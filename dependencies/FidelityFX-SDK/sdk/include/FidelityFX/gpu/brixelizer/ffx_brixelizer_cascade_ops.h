// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2024 Advanced Micro Devices, Inc.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef FFX_BRIXELIZER_CASCADE_OPS_H
#define FFX_BRIXELIZER_CASCADE_OPS_H

#include "ffx_brixelizer_host_gpu_shared_private.h"
#include "ffx_brixelizer_brick_common_private.h"
#include "ffx_brixelizer_build_common.h"
#include "ffx_brixelizer_common_private.h"
#include "ffx_brixelizer_mesh_common.h"

void FfxBrixelizerClearRefCounter(FfxUInt32 idx)
{
    StoreScratchCR1RefCounter(idx, FfxUInt32(0));
    StoreScratchVoxelAllocationFailCounter(idx, FfxUInt32(0));
}

// Search for n where a[n] <= offset and a[n+1] > offset
#define LOWER_BOUND(offset, total_count)                                                                                         \
    {                                                                                                                            \
        FfxUInt32 cursor = FfxUInt32(0);                                                                                         \
        FfxUInt32 size   = total_count;                                                                                          \
        while (size > FfxUInt32(0)) {                                                                                            \
            FfxUInt32 size_half = size >> FfxUInt32(1);                                                                          \
            FfxUInt32 mid       = cursor + size_half;                                                                            \
            if (LOWER_BOUND_LOAD(mid) > offset)                                                                                  \
                size = size_half;                                                                                                \
            else {                                                                                                               \
                cursor = mid + FfxUInt32(1);                                                                                     \
                size   = size - size_half - FfxUInt32(1);                                                                        \
            }                                                                                                                    \
        }                                                                                                                        \
                                                                                                                                 \
        LOWER_BOUND_RESULT = max(cursor, FfxUInt32(1)) - FfxUInt32(1);                                                           \
    }

FfxUInt32x3 WrapCoords(FfxUInt32x3 voxel_coord)
{
    return (voxel_coord + GetCascadeInfoClipmapOffset()) & FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_WRAP_MASK);
}

FfxUInt32  WrapFlatCoords(FfxUInt32 voxel_idx)
{
    return FfxBrixelizerFlattenPOT((FfxBrixelizerUnflattenPOT(voxel_idx, FFX_BRIXELIZER_CASCADE_DEGREE) + GetCascadeInfoClipmapOffset()) & FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_WRAP_MASK), FFX_BRIXELIZER_CASCADE_DEGREE);
}

void MarkFailed(FfxUInt32 flat_voxel_idx)
{
    StoreScratchVoxelAllocationFailCounter(flat_voxel_idx, FfxUInt32(1));
}

FfxBoolean  IsBuildable(FfxUInt32 voxel_idx)
{
    return LoadCascadeBrickMap(WrapFlatCoords(voxel_idx)) == FFX_BRIXELIZER_UNINITIALIZED_ID;
}

FfxBoolean  CanBuildThisVoxel(FfxUInt32 flat_voxel_idx)
{
    if (!IsBuildable(flat_voxel_idx)) return false;
    return true;
}

void AddReferenceOrMarkVoxelFailed(FfxUInt32 voxel_idx, FfxUInt32 triangle_id)
{
    if (!CanBuildThisVoxel(voxel_idx)) {
        return;
    }

    FfxUInt32 local_ref_idx;
    IncrementScratchCR1RefCounter(voxel_idx, FfxUInt32(1), local_ref_idx);
    FfxBrixelizerTriangleReference ref;
    ref.voxel_idx     = voxel_idx;
    ref.triangle_id   = triangle_id;
    ref.local_ref_idx = local_ref_idx;
    FfxUInt32 coarse_ref_offset;
    IncrementScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_REFERENCES, FfxUInt32(1), coarse_ref_offset);
    FfxUInt32 max_references = LoadScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_MAX_REFERENCES);
    if (coarse_ref_offset < max_references) {
        StoreScratchCR1Reference(coarse_ref_offset, ref);
    } else {
        MarkFailed(voxel_idx);
    }
}

FfxUInt32 GetReferenceOffset(FfxUInt32 voxel_idx)
{
    FfxUInt32 group_scan_id    = voxel_idx / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE;
    FfxUInt32 group_scan_value = LoadVoxelReferenceGroupSum(group_scan_id);
    FfxUInt32 local_scan_value = LoadScratchCR1RefCounterScan(voxel_idx);
    return group_scan_value + local_scan_value;
}

struct FfxBrixelizerCRItemPacked {
    FfxUInt32 pack0;
    FfxUInt32 pack1;
};

struct FfxBrixelizerCRItem {
    FfxUInt32x3 bounds_min;
    FfxUInt32x3 bounds_max;
};

FFX_GROUPSHARED FfxUInt32            gs_ffx_brixelizer_voxelizer_items_ref_count[FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE];
FFX_GROUPSHARED FfxUInt32            gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE];
FFX_GROUPSHARED FfxBrixelizerCRItemPacked gs_ffx_brixelizer_voxelizer_items[FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE];
FFX_GROUPSHARED FfxUInt32            gs_ffx_brixelizer_voxelizer_item_counter;
FFX_GROUPSHARED FfxUInt32            gs_ffx_brixelizer_voxelizer_ref_counter;
FFX_GROUPSHARED FfxUInt32            gs_ffx_brixelizer_voxelizer_ref_offset;
FFX_GROUPSHARED FfxUInt32            gs_ffx_brixelizer_triangle_offset_global;
FFX_GROUPSHARED FfxUInt32            gs_ffx_brixelizer_triangle_offset;
FFX_GROUPSHARED FfxBoolean           gs_ffx_brixelizer_voxelizer_has_space;

void FfxBrixelizerCRStoreItem(FfxInt32 index, FfxBrixelizerCRItem item)
{
    FfxUInt32 pack0 = ((item.bounds_min.x & FfxUInt32(0x3ff)) << FfxUInt32(0))  |
                      ((item.bounds_min.y & FfxUInt32(0x3ff)) << FfxUInt32(10)) |
                      ((item.bounds_min.z & FfxUInt32(0x3ff)) << FfxUInt32(20));
    FfxUInt32 pack1 = ((item.bounds_max.x & FfxUInt32(0x3ff)) << FfxUInt32(0))  |
                      ((item.bounds_max.y & FfxUInt32(0x3ff)) << FfxUInt32(10)) |
                      ((item.bounds_max.z & FfxUInt32(0x3ff)) << FfxUInt32(20));
    FfxBrixelizerCRItemPacked packed;
    packed.pack0                     = pack0;
    packed.pack1                     = pack1;
    gs_ffx_brixelizer_voxelizer_items[index] = packed;
}

FfxBrixelizerCRItem FfxBrixelizerCRLoadItem(FfxInt32 index)
{
    FfxBrixelizerCRItemPacked pack = gs_ffx_brixelizer_voxelizer_items[index];
    FfxBrixelizerCRItem       item;
    item.bounds_min.x = (pack.pack0 >> FfxUInt32(0))  & FfxUInt32(0x3ff);
    item.bounds_min.y = (pack.pack0 >> FfxUInt32(10)) & FfxUInt32(0x3ff);
    item.bounds_min.z = (pack.pack0 >> FfxUInt32(20)) & FfxUInt32(0x3ff);
    item.bounds_max.x = (pack.pack1 >> FfxUInt32(0))  & FfxUInt32(0x3ff);
    item.bounds_max.y = (pack.pack1 >> FfxUInt32(10)) & FfxUInt32(0x3ff);
    item.bounds_max.z = (pack.pack1 >> FfxUInt32(20)) & FfxUInt32(0x3ff);
    return item;
}

struct FfxBrixelizerCRVoxelTriangleBounds {
    FfxFloat32x3 bound_min;
    FfxFloat32x3 bound_max;
    FfxUInt32x3  ubound_min;
    FfxUInt32x3  ubound_max;
};

FfxBrixelizerTriangle FetchTriangle(FfxBrixelizerBasicMeshInfo instance_info, FfxUInt32 instance_id, FfxUInt32 job_idx, FfxUInt32 triangle_index)
{
    FfxBrixelizerTrianglePos pos = FfxBrixelizerFetchTriangle(instance_info, instance_id, triangle_index);

    FfxBrixelizerTriangle tri;
    tri.face3          = pos.face3;
    tri.job_idx        = job_idx;
    tri.triangle_index = triangle_index;
    tri.wp0            = FfxFloat32x3(pos.wp0 - GetCascadeInfoGridMin());
    tri.wp1            = FfxFloat32x3(pos.wp1 - GetCascadeInfoGridMin());
    tri.wp2            = FfxFloat32x3(pos.wp2 - GetCascadeInfoGridMin());

    return tri;
}

FfxBoolean GetTriangleBounds(FfxUInt32 instance_id, FfxUInt32 job_idx, FfxBrixelizerBasicMeshInfo instance_info, FfxUInt32 triangle_index, FFX_PARAMETER_OUT FfxBrixelizerTriangle tri,
                       FFX_PARAMETER_OUT FfxBrixelizerCRVoxelTriangleBounds tvbounds)
{
    FfxUInt32 job_num_triangles = instance_info.triangleCount;
    if (triangle_index < job_num_triangles) {
        tri = FetchTriangle(instance_info, instance_id, job_idx, triangle_index);
        FfxFloat32 inflation_size = FfxFloat32(GetCascadeInfoVoxelSize() / FfxFloat32(7.0));
        tvbounds.bound_min = FfxFloat32x3(min(tri.wp0.x, min(tri.wp1.x, tri.wp2.x)),
                                          min(tri.wp0.y, min(tri.wp1.y, tri.wp2.y)),
                                          min(tri.wp0.z, min(tri.wp1.z, tri.wp2.z)));
        tvbounds.bound_max = FfxFloat32x3(max(tri.wp0.x, max(tri.wp1.x, tri.wp2.x)),
                                          max(tri.wp0.y, max(tri.wp1.y, tri.wp2.y)),
                                          max(tri.wp0.z, max(tri.wp1.z, tri.wp2.z)));

        FfxFloat32x3 bounds_min;

        bounds_min.x = tvbounds.bound_min.x > FfxFloat32(0.0) ? tvbounds.bound_min.x : tvbounds.bound_min.x - FfxFloat32(1.0);
        bounds_min.y = tvbounds.bound_min.y > FfxFloat32(0.0) ? tvbounds.bound_min.y : tvbounds.bound_min.y - FfxFloat32(1.0);
        bounds_min.z = tvbounds.bound_min.z > FfxFloat32(0.0) ? tvbounds.bound_min.z : tvbounds.bound_min.z - FfxFloat32(1.0);

        tvbounds.ubound_min  = min(
            FFX_BROADCAST_INT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION - FfxUInt32(1)),
            max(
                FFX_BROADCAST_INT32X3(0),
                FfxInt32x3((bounds_min - FFX_BROADCAST_FLOAT32X3(inflation_size)) / FfxFloat32(GetCascadeInfoVoxelSize()))
                )
        );

        FfxFloat32x3 bounds_max;

        bounds_max.x = tvbounds.bound_max.x > FfxFloat32(0.0) ? tvbounds.bound_max.x : tvbounds.bound_max.x - FfxFloat32(1.0);
        bounds_max.y = tvbounds.bound_max.y > FfxFloat32(0.0) ? tvbounds.bound_max.y : tvbounds.bound_max.y - FfxFloat32(1.0);
        bounds_max.z = tvbounds.bound_max.z > FfxFloat32(0.0) ? tvbounds.bound_max.z : tvbounds.bound_max.z - FfxFloat32(1.0);

        tvbounds.ubound_max  = min(
                                  FFX_BROADCAST_INT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION - FfxUInt32(1)),
                                  max(
                                      FFX_BROADCAST_INT32X3(0),
                                      FfxInt32x3((bounds_max + FFX_BROADCAST_FLOAT32X3(inflation_size)) / FfxFloat32(GetCascadeInfoVoxelSize()))
                                      )
                                  ) +
                              FFX_BROADCAST_INT32X3(1);
        return all(FFX_LESS_THAN_EQUAL(tvbounds.bound_min, FfxFloat32x3(GetCascadeInfoGridMax() - GetCascadeInfoGridMin()) + FFX_BROADCAST_FLOAT32X3(inflation_size))) &&
               all(FFX_GREATER_THAN_EQUAL(tvbounds.bound_max, FFX_BROADCAST_FLOAT32X3(0.0) + FFX_BROADCAST_FLOAT32X3(-inflation_size)));
    }
    return false;
}

void FfxBrixelizerStoreTriangleCenter(FfxUInt32 triangle_id_swap_offset, FfxBrixelizerTriangle tri)
{
    StoreScratchIndexSwapFloat3(triangle_id_swap_offset / FFX_BRIXELIZER_SIZEOF_UINT, (tri.wp0 + tri.wp1 + tri.wp2) / FfxFloat32(3.0));
}

FfxFloat32x3 FfxBrixelizerLoadTriangleCenter(FfxUInt32 triangle_id_swap_offset)
{
    return LoadScratchIndexSwapFloat3(triangle_id_swap_offset / FFX_BRIXELIZER_SIZEOF_UINT);
}

struct FfxBrixelizerTrianglePartial {
    FfxFloat32x3 wp0;
    FfxFloat32x3 wp1;
    FfxFloat32x3 wp2;
};

struct FfxBrixelizerTrianglePartialCompressed {
    FfxUInt32x2 wp0xy;
    FfxUInt32x2 ed0;
    FfxUInt32x2 ed1;
};

// Compress to f32x3 v0 and f16x3 e0, e1 and store
void FfxBrixelizerStoreTrianglePartial(FfxUInt32 triangle_id_swap_offset, FfxBrixelizerTriangle tri)
{
    FfxBrixelizerTrianglePartialCompressed trip;
    trip.wp0xy      = ffxAsUInt32(tri.wp0.xy);
    FfxFloat32x3 e0 = tri.wp1 - tri.wp0;
    FfxFloat32x3 e1 = tri.wp2 - tri.wp0;
    FfxFloat32x4 v0 = FfxFloat32x4(e0.xyz, e1.x);
    trip.ed0        = ffxPackF32x2(v0);
    trip.ed1.x      = ffxPackF32(e1.yz);
    trip.ed1.y      = ffxAsUInt32(tri.wp0.z);
    StoreScratchIndexSwapUInt2((triangle_id_swap_offset / FFX_BRIXELIZER_SIZEOF_UINT) + FfxUInt32(0), trip.wp0xy);
    StoreScratchIndexSwapUInt2((triangle_id_swap_offset / FFX_BRIXELIZER_SIZEOF_UINT) + FfxUInt32(2), trip.ed0);
    StoreScratchIndexSwapUInt2((triangle_id_swap_offset / FFX_BRIXELIZER_SIZEOF_UINT) + FfxUInt32(4), trip.ed1);
}

void FfxBrixelizerLoadTrianglePartial(FfxUInt32 triangle_id_swap_offset, FFX_PARAMETER_OUT FfxBrixelizerTrianglePartial tri)
{
    FfxBrixelizerTrianglePartialCompressed tripc;
    tripc.wp0xy     = LoadScratchIndexSwapUInt2((triangle_id_swap_offset / FFX_BRIXELIZER_SIZEOF_UINT) + FfxUInt32(0));
    tripc.ed0       = LoadScratchIndexSwapUInt2((triangle_id_swap_offset / FFX_BRIXELIZER_SIZEOF_UINT) + FfxUInt32(2));
    tripc.ed1       = LoadScratchIndexSwapUInt2((triangle_id_swap_offset / FFX_BRIXELIZER_SIZEOF_UINT) + FfxUInt32(4));
    tri.wp0.xy      = ffxAsFloat(tripc.wp0xy.xy);
    tri.wp0.z       = ffxAsFloat(tripc.ed1.y);
    FfxFloat32x4 v0 = ffxUnpackF32x2(tripc.ed0);
    FfxFloat32x2 v1 = ffxUnpackF32(tripc.ed1.x);
    tri.wp1         = tri.wp0 + FfxFloat32x3(v0.xyz);
    tri.wp2         = tri.wp0 + FfxFloat32x3(v0.w, v1.xy);
}

// Integer scan
FFX_GROUPSHARED FfxUInt32 gs_ffx_brixelizer_scan_buffer[FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE];
FFX_GROUPSHARED FfxUInt32 gs_ffx_brixelizer_scan_group_id;
FfxUInt32 GroupScanExclusiveAdd(FfxUInt32 gid, FfxUInt32 group_size)
{
    FfxUInt32 sum = FfxUInt32(0);
    for (FfxUInt32 stride = FfxUInt32(1); stride <= (group_size >> FfxUInt32(1)); stride <<= FfxUInt32(1)) {
        if (gid < group_size / (FfxUInt32(2) * stride)) {
            gs_ffx_brixelizer_scan_buffer[FfxUInt32(2) * (gid + FfxUInt32(1)) * stride - FfxUInt32(1)] += gs_ffx_brixelizer_scan_buffer[(FfxUInt32(2) * gid + FfxUInt32(1)) * stride - FfxUInt32(1)];
        }
        FFX_GROUP_MEMORY_BARRIER;
    }

    if (gid == FfxUInt32(0)) {
        sum = gs_ffx_brixelizer_scan_buffer[group_size - FfxUInt32(1)];
        gs_ffx_brixelizer_scan_buffer[group_size - FfxUInt32(1)] = FfxUInt32(0);
    }
    FFX_GROUP_MEMORY_BARRIER;

    for (FfxUInt32 stride = (group_size >> FfxUInt32(1)); stride > FfxUInt32(0); stride >>= FfxUInt32(1)) {
        if (gid < group_size / (FfxUInt32(2) * stride)) {
            FfxUInt32 tmp = gs_ffx_brixelizer_scan_buffer[(FfxUInt32(2) * gid + FfxUInt32(1)) * stride - FfxUInt32(1)];
            gs_ffx_brixelizer_scan_buffer[(FfxUInt32(2) * gid + FfxUInt32(1)) * stride - FfxUInt32(1)] = gs_ffx_brixelizer_scan_buffer[FfxUInt32(2) * (gid + FfxUInt32(1)) * stride - FfxUInt32(1)];
            gs_ffx_brixelizer_scan_buffer[FfxUInt32(2) * (gid + FfxUInt32(1)) * stride - FfxUInt32(1)] = gs_ffx_brixelizer_scan_buffer[FfxUInt32(2) * (gid + FfxUInt32(1)) * stride - FfxUInt32(1)] + tmp;
        }
        FFX_GROUP_MEMORY_BARRIER;
    }

    return sum;
}

FfxUInt32 LoadJobTriangleCountScan(FfxUInt32 job_idx)
{
    return LoadScratchJobCountersScan(job_idx) + LoadGlobalJobTriangleCounterScan(job_idx / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE);
}

FfxUInt32 JobLowerBound(FfxUInt32 triangle_offset, FfxUInt32 total_job_count)
{
#define LOWER_BOUND_LOAD(mid) LoadJobTriangleCountScan(mid)

    FfxUInt32 LOWER_BOUND_RESULT;

    LOWER_BOUND(triangle_offset, total_job_count);

#undef LOWER_BOUND_LOAD

    return LOWER_BOUND_RESULT;
}

FfxUInt32 JobLowerBoundBySubvoxel(FfxUInt32 subvoxel_offset, FfxUInt32 total_job_count)
{
#define LOWER_BOUND_LOAD(mid) LoadJobIndex(mid)

    FfxUInt32 LOWER_BOUND_RESULT;

    LOWER_BOUND(subvoxel_offset, total_job_count);

#undef LOWER_BOUND_LOAD

    return LOWER_BOUND_RESULT;
}

// One group performs global scan for all the other groups
#define GROUP_SCAN(gid, total_group_count, group_size, LoadGlobal, StoreGlobal)                                                                      \
    {                                                                                                                                                \
        FFX_GROUP_MEMORY_BARRIER;                                                                                                           \
        if (gid == FfxUInt32(0)) gs_ffx_brixelizer_scan_group_id = FfxUInt32(0);                                                                             \
        FFX_GROUP_MEMORY_BARRIER;                                                                                                           \
        for (FfxUInt32 cursor = FfxUInt32(0); cursor < total_group_count; cursor += group_size) {                                                    \
            FFX_GROUP_MEMORY_BARRIER;                                                                                                       \
            if (gid + cursor < total_group_count)                                                                                                    \
                gs_ffx_brixelizer_scan_buffer[gid] = LoadGlobal(gid + cursor);                                                                               \
            else                                                                                                                                     \
                gs_ffx_brixelizer_scan_buffer[gid] = FfxUInt32(0);                                                                                           \
            FFX_GROUP_MEMORY_BARRIER;                                                                                                       \
            FfxUInt32 sum = GroupScanExclusiveAdd(gid, group_size);                                                                                  \
                                                                                                                                                     \
            if (gid + cursor < total_group_count) StoreGlobal(gid + cursor, gs_ffx_brixelizer_scan_buffer[gid] + gs_ffx_brixelizer_scan_group_id);                   \
                                                                                                                                                     \
            FFX_GROUP_MEMORY_BARRIER;                                                                                                       \
                                                                                                                                                     \
            if (gid == FfxUInt32(0)) gs_ffx_brixelizer_scan_group_id += sum;                                                                                 \
        }                                                                                                                                            \
    }

// Used for group scan macros
FfxUInt32 StampLowerBound(FfxUInt32 item_id)
{
#define LOWER_BOUND_LOAD(mid) LoadScratchCR1StampScan(mid)

    FfxUInt32 LOWER_BOUND_RESULT;

    LOWER_BOUND(item_id, FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION);

#undef LOWER_BOUND_LOAD

    return LOWER_BOUND_RESULT;
}

void AddBrickToCompressionList(FfxUInt32 brick_id)
{
    FfxUInt32 offset;
    IncrementScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_COMPRESSION_BRICKS, FfxUInt32(1), offset);
    StoreScratchBricksCompressionList(offset, brick_id);
}

FfxUInt32 AllocateBrick()
{
    FfxUInt32 brick_idx;
    IncrementScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_NUM_BRICKS_ALLOCATED, FfxUInt32(1), brick_idx);
    if (brick_idx > GetBuildInfo().max_bricks_per_bake) {
        return FFX_BRIXELIZER_INVALID_ID;
    }

    FfxUInt32 val;
    IncrementContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_BRICK_COUNT, FfxUInt32(1), val);
    if (val >= LoadContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_FREE_BRICKS)) return FFX_BRIXELIZER_INVALID_ID;
    FfxUInt32 brick_id = LoadBricksFreeList(val);
    return brick_id;
}

void MapBrickToVoxel(FfxUInt32 brick_id, FfxUInt32 voxel_id)
{
    voxel_id |= (GetCascadeInfoIndex()) << FFX_BRIXELIZER_CASCADE_ID_SHIFT;
    StoreBricksVoxelMap(FfxBrixelizerBrickGetIndex(brick_id), voxel_id);
}

FfxUInt32 BrickGetStorageOffset(FfxUInt32 brick_id)
{
    return LoadScratchBricksStorageOffsets(FfxBrixelizerBrickGetIndex(brick_id));
}

FfxUInt32 AllocateStorage(FfxUInt32 brick_id)
{
    FfxUInt32 dim  = FfxUInt32(8);
    FfxUInt32 size = dim * dim * dim * FfxUInt32(4);
    FfxUInt32 offset;
    IncrementScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_STORAGE_OFFSET, size, offset);
    if (offset + size > LoadScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_STORAGE_SIZE)) {
        StoreScratchBricksStorageOffsets(FfxBrixelizerBrickGetIndex(brick_id), FFX_BRIXELIZER_INVALID_ALLOCATION);
        return FFX_BRIXELIZER_INVALID_ALLOCATION;
    }
    StoreScratchBricksStorageOffsets(FfxBrixelizerBrickGetIndex(brick_id), offset);
    return offset;
}

void AppendClearBrick(FfxUInt32 brick_id)
{
    FfxUInt32 offset;
    IncrementScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_CLEAR_BRICKS, 1, offset);
    StoreScratchBricksClearList(offset, brick_id);
}

// Utilities for 32 scratch space for atomics to work
FfxFloat32 LoadBrixelData32(FfxUInt32 brick_id, FfxInt32x3 coord)
{
    FfxInt32 brick_dim = FfxInt32(8);
    if (any(FFX_GREATER_THAN_EQUAL(coord, FFX_BROADCAST_INT32X3(brick_dim))) || any(FFX_LESS_THAN(coord, FFX_BROADCAST_INT32X3(0)))) return FfxFloat32(1.0);

    const FfxUInt32 brick_size = brick_dim * brick_dim * brick_dim * FFX_BRIXELIZER_SIZEOF_UINT;
    FfxUInt32       offset     = FfxBrixelizerFlattenPOT(coord, 3);
    offset += BrickGetStorageOffset(brick_id) / FFX_BRIXELIZER_SIZEOF_UINT;
    FfxUInt32 uval = LoadScratchBricksStorage(offset);
    return FfxBrixelizerUnpackDistance(uval);
}

void BrickInterlockedMin32(FfxUInt32 brick_id, FfxInt32x3 coord, FfxUInt32 uval)
{
    FfxInt32 brick_dim = 8;
    if (any(FFX_GREATER_THAN_EQUAL(coord, FFX_BROADCAST_INT32X3(brick_dim))) || any(FFX_LESS_THAN(coord, FFX_BROADCAST_INT32X3(0)))) return;

    const FfxUInt32 brick_size = brick_dim * brick_dim * brick_dim * FFX_BRIXELIZER_SIZEOF_UINT;
    FfxUInt32       offset     = FfxBrixelizerFlattenPOT(coord, 3) * FFX_BRIXELIZER_SIZEOF_UINT;
    offset += BrickGetStorageOffset(brick_id);
    MinScratchBricksStorage(offset / FFX_BRIXELIZER_SIZEOF_UINT, uval);
}

void BrickInterlockedMin32(FfxUInt32 brick_id, FfxInt32x3 coord, FfxFloat32 fval)
{
    BrickInterlockedMin32(brick_id, coord, FfxBrixelizerPackDistance(fval));
}

void ClearBrixelData32(FfxUInt32 brick_id, FfxInt32x3 coord)
{
    FfxInt32 brick_dim = 8;
    if (any(FFX_GREATER_THAN_EQUAL(coord, FFX_BROADCAST_INT32X3(brick_dim))) || any(FFX_LESS_THAN(coord, FFX_BROADCAST_INT32X3(0)))) return;

    FfxUInt32 offset = FfxBrixelizerFlattenPOT(coord, 3) * FFX_BRIXELIZER_SIZEOF_UINT;
    offset += BrickGetStorageOffset(brick_id);
    StoreScratchBricksStorage(offset / FFX_BRIXELIZER_SIZEOF_UINT, FfxBrixelizerPackDistance(FfxFloat32(1.0)));
}

void InitializeIndirectArgs(FfxUInt32 subvoxel_count)
{
    {
        FfxUInt32 tier_cnt = LoadScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_CLEAR_BRICKS);
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_CLEAR_BRICKS_32 + 0, tier_cnt * 8);
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_CLEAR_BRICKS_32 + 1, 1);
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_CLEAR_BRICKS_32 + 2, 1);
    }
    {
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_EMIT_SDF_32 + 0, subvoxel_count);
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_EMIT_SDF_32 + 1, 1);
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_EMIT_SDF_32 + 2, 1);
    }
    {
        FfxUInt32 tier_cnt = LoadScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_COMPRESSION_BRICKS);
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_COMPRESS_32 + 0, tier_cnt);
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_COMPRESS_32 + 1, 1);
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_COMPRESS_32 + 2, 1);
    }
    {
        FfxUInt32 total_cell_count   = FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION;
        FfxUInt32 total_references   = min(LoadScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_REFERENCES), LoadScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_MAX_REFERENCES));
        FfxUInt32 total_thread_count = max(total_cell_count, total_references);

        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_COMPACT_REFERENCES_32 + 0, (total_thread_count + FFX_BRIXELIZER_STATIC_CONFIG_COMPACT_REFERENCES_GROUP_SIZE - 1) / FFX_BRIXELIZER_STATIC_CONFIG_COMPACT_REFERENCES_GROUP_SIZE);
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_COMPACT_REFERENCES_32 + 1, 1);
        StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_COMPACT_REFERENCES_32 + 2, 1);
    }
}

void InitializeJobIndirectArgs(FfxUInt32 num_triangles)
{
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_VOXELIZE_32 + 0, (num_triangles + FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE - 1) / FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE);
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_VOXELIZE_32 + 1, 1);
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_VOXELIZE_32 + 2, 1);
}

void FfxBrixelizerClearBuildCounters()
{
    for (FfxUInt32 i = FfxUInt32(0); i < FFX_BRIXELIZER_NUM_SCRATCH_COUNTERS; i++) {
        StoreScratchCounter(i, FfxUInt32(0));
    }
    FfxUInt32 storage_size;
    GetScratchBricksStorageDimensions(storage_size);
    StoreScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_STORAGE_SIZE, storage_size);
    GetScratchIndexSwapDimensions(storage_size);
    StoreScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_MAX_TRIANGLES, storage_size);
    GetScratchMaxReferences(storage_size);
    StoreScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_MAX_REFERENCES, storage_size);
    for (FfxUInt32 i = 0; i < FFX_BROADCAST_UINT32(FFX_BRIXELIZER_NUM_INDIRECT_OFFSETS) * FFX_BROADCAST_UINT32(FFX_BRIXELIZER_STATIC_CONFIG_INDIRECT_DISPATCH_STRIDE32); i++) {
        StoreIndirectArgs(i, FfxUInt32(0));
    }
}

void FfxBrixelizerResetCascade(FfxUInt32 tid)
{
    if (tid < FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION) {
        FfxUInt32 brick_id = LoadCascadeBrickMap(tid);
        if (FfxBrixelizerIsValidID(brick_id)) {
            FfxBrixelizerMarkBrickFree(brick_id);
        }
        StoreCascadeBrickMap(tid, FFX_BRIXELIZER_UNINITIALIZED_ID);
    }
}

void FfxBrixelizerInitializeCascade(FfxUInt32 tid)
{
    if (tid < FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION) {
        FfxUInt32 brick_id = LoadCascadeBrickMap(tid);
        if (brick_id == FFX_BRIXELIZER_UNINITIALIZED_ID) {
            StoreCascadeBrickMap(tid, FFX_BRIXELIZER_INVALID_ID);
        }
    }
}

void FfxBrixelizerMarkCascadeUninitialized(FfxUInt32 tid)
{
    if (tid < FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION) {
            StoreCascadeBrickMap(tid, FFX_BRIXELIZER_UNINITIALIZED_ID);
    }
}

void FfxBrixelizerFreeCascade(FfxUInt32 tid)
{
    if (tid < FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION) {
        FfxUInt32 brick_id = LoadCascadeBrickMap(tid);
        if (FfxBrixelizerIsValidID(brick_id)) {
            FfxBrixelizerMarkBrickFree(brick_id);
        }
        StoreCascadeBrickMap(tid, FFX_BRIXELIZER_UNINITIALIZED_ID);
    }
}

void FfxBrixelizerScrollCascade(FfxUInt32 tid)
{
    if (all(FFX_LESS_THAN(FfxBrixelizerUnflattenPOT(tid, FFX_BRIXELIZER_CASCADE_DEGREE), FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION)))) {
        FfxInt32x3 voxel_coord = to_int3(FfxBrixelizerUnflattenPOT(tid, FFX_BRIXELIZER_CASCADE_DEGREE));
#ifdef FFX_BRIXELIZER_DEBUG_FORCE_REBUILD
        FfxUInt32 voxel_idx = FfxBrixelizerFlattenPOT(voxel_coord, FFX_BRIXELIZER_CASCADE_DEGREE);
        FfxUInt32 brick_id  = LoadCascadeBrickMap(WrapFlatCoords(voxel_idx));
        StoreCascadeBrickMap(WrapFlatCoords(voxel_idx), FFX_BRIXELIZER_UNINITIALIZED_ID);
#else // !FFX_BRIXELIZER_DEBUG_FORCE_REBUILD

        // Scrolling clipmap update
        if (any(FFX_LESS_THAN(voxel_coord, -GetCascadeInfoClipmapInvalidationOffset())) || any(FFX_GREATER_THAN_EQUAL(voxel_coord, FFX_BROADCAST_INT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION) - GetCascadeInfoClipmapInvalidationOffset()))) {
            FfxUInt32 voxel_idx = FfxBrixelizerFlattenPOT(voxel_coord, FFX_BRIXELIZER_CASCADE_DEGREE);
            FfxUInt32 brick_id  = LoadCascadeBrickMap(WrapFlatCoords(voxel_idx));
            if (FfxBrixelizerIsValidID(brick_id)) {
                FfxBrixelizerMarkBrickFree(brick_id);
            }
            StoreCascadeBrickMap(WrapFlatCoords(voxel_idx), FFX_BRIXELIZER_UNINITIALIZED_ID);
        }

#endif // !FFX_BRIXELIZER_DEBUG_FORCE_REBUILD
    }
}

void FfxBrixelizerClearRefCounters(FfxUInt32 tid)
{
    FfxUInt32x3 voxel_coord = FfxBrixelizerUnflattenPOT(tid, FFX_BRIXELIZER_CASCADE_DEGREE);
    if (all(FFX_LESS_THAN(voxel_coord, FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION)))) {
        FfxUInt32 voxel_idx = FfxBrixelizerFlattenPOT(voxel_coord, FFX_BRIXELIZER_CASCADE_DEGREE);
        FfxBrixelizerClearRefCounter(voxel_idx);
    }
}

void FfxBrixelizerClearJobCounter(FfxUInt32 tid)
{
    if (tid < GetBuildInfoNumJobs()) StoreScratchJobCounter(tid, FfxUInt32(0));
}

void FfxBrixelizerInvalidateJobAreas(FfxUInt32 gtid, FfxUInt32 group_id)
{

    FfxUInt32 thread_subvoxel_offset = group_id * FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE + gtid;
    FfxUInt32 job_idx                = JobLowerBoundBySubvoxel(thread_subvoxel_offset, GetBuildInfoNumJobs());

    if (job_idx < GetBuildInfoNumJobs()) {
        FfxBrixelizerBrixelizationJob job         = LoadBrixelizationJob(job_idx);
        FfxUInt32               subvoxel_id = thread_subvoxel_offset - LoadJobIndex(job_idx);
        FfxInt32x3              dim         = FfxInt32x3(job.aabbMax - job.aabbMin);

        ffxassert(all(job.aabbMax > FFX_BROADCAST_UINT32X3(0)));
        ffxassert(all(job.aabbMin >= FFX_BROADCAST_UINT32X3(0)));
        ffxassert(all(job.aabbMin < FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION)));
        ffxassert(all(job.aabbMax > job.aabbMin));
        ffxassert(all(job.aabbMax <= FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION)));

        if (FFX_HAS_FLAG(job.flags, FFX_BRIXELIZER_JOB_FLAG_INVALIDATE)) {
            if (subvoxel_id < dim.x * dim.y * dim.z) {
                FfxUInt32x3 subvoxel_coord     = FfxBrixelizerUnflatten(subvoxel_id, dim);
                FfxUInt32x3 global_voxel_coord = subvoxel_coord + job.aabbMin;
                ffxassert(all(global_voxel_coord >= FFX_BROADCAST_UINT32X3(0)) && all(global_voxel_coord < FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION)));
                FfxUInt32 brick_id = LoadCascadeBrickMap(WrapFlatCoords(FfxBrixelizerFlattenPOT(global_voxel_coord, FFX_BRIXELIZER_CASCADE_DEGREE)));
                if (brick_id != FFX_BRIXELIZER_UNINITIALIZED_ID) {
                    FfxBrixelizerMarkBrickFree(brick_id);
                    StoreCascadeBrickMap(WrapFlatCoords(FfxBrixelizerFlattenPOT(global_voxel_coord, FFX_BRIXELIZER_CASCADE_DEGREE)), FFX_BRIXELIZER_UNINITIALIZED_ID);
                }
            }
        } else {
        }
    }
}

void FfxBrixelizerCoarseCulling(FfxUInt32 gtid, FfxUInt32 group_id)
{
    FfxUInt32 thread_subvoxel_offset = group_id * FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE + gtid;
    FfxUInt32 job_idx                = JobLowerBoundBySubvoxel(thread_subvoxel_offset, GetBuildInfoNumJobs());

    FfxBoolean needs_rebuild = false;

    if (job_idx < GetBuildInfoNumJobs()) {
        FfxBrixelizerBrixelizationJob job         = LoadBrixelizationJob(job_idx);
        FfxUInt32               subvoxel_id = thread_subvoxel_offset - LoadJobIndex(job_idx);
        FfxInt32x3              dim         = FfxInt32x3(job.aabbMax - job.aabbMin);

        ffxassert(all(job.aabbMax > FFX_BROADCAST_UINT32X3(0)));
        ffxassert(all(job.aabbMin >= FFX_BROADCAST_UINT32X3(0)));
        ffxassert(all(job.aabbMin < FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION)));
        ffxassert(all(job.aabbMax > job.aabbMin));
        ffxassert(all(job.aabbMax <= FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION)));
        if (FFX_HAS_FLAG(job.flags, FFX_BRIXELIZER_JOB_FLAG_INVALIDATE)) {
        } else {
            if (subvoxel_id < dim.x * dim.y * dim.z) {
                FfxUInt32x3 subvoxel_coord     = FfxBrixelizerUnflatten(subvoxel_id, dim);
                FfxUInt32x3 global_voxel_coord = subvoxel_coord + job.aabbMin;
                ffxassert(all(global_voxel_coord >= FFX_BROADCAST_UINT32X3(0)) && all(global_voxel_coord < FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION)));
                FfxUInt32  voxel_idx          = FfxBrixelizerFlattenPOT(global_voxel_coord, FFX_BRIXELIZER_CASCADE_DEGREE);
                FfxBoolean this_needs_rebuild = CanBuildThisVoxel(voxel_idx);
#ifdef FFX_BRIXELIZER_DEBUG_FORCE_REBUILD
                this_needs_rebuild = true;
#else // !FFX_BRIXELIZER_DEBUG_FORCE_REBUILD

#endif // !FFX_BRIXELIZER_DEBUG_FORCE_REBUILD

                needs_rebuild = this_needs_rebuild;
            }
        }
    }
    if (needs_rebuild) {
        IncrementScratchJobCounter(job_idx, 1);
    }
}

void FfxBrixelizerScanJobs(FfxUInt32 job_idx, FfxUInt32 gtid, FfxUInt32 group_id)
{
    FfxBoolean              is_touched    = job_idx < GetBuildInfoNumJobs() && LoadScratchJobCounter(job_idx) > 0;
    FfxBrixelizerBrixelizationJob job           = LoadBrixelizationJob(job_idx);
    FfxBrixelizerInstanceInfo     instance_info = LoadInstanceInfo(job.instanceIdx);

    ffxassert((job.flags & FFX_BRIXELIZER_JOB_FLAG_INVALIDATE) == 0);
    // Scan triangle counts so that later we can map thread_id -> job_idx
    {
        gs_ffx_brixelizer_scan_buffer[gtid] = is_touched ? instance_info.triangleCount : 0;
        FFX_GROUP_MEMORY_BARRIER;
        FfxUInt32 sum = GroupScanExclusiveAdd(gtid, FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE);
        if (job_idx < GetBuildInfoNumJobs()) StoreScratchJobCountersScan(job_idx, gs_ffx_brixelizer_scan_buffer[gtid]);

        if (gtid == 0) // The first thread stores the sum
            StoreGlobalJobTriangleCounterScan(group_id, sum);
    }
    if (gtid == 0) IncrementScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_GROUP_INDEX, 1, gs_ffx_brixelizer_scan_group_id);

    FfxUInt32 total_group_count = (GetBuildInfoNumJobs() + FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE - 1) / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE;

    FFX_GROUP_MEMORY_BARRIER;                      // Wait for gs_ffx_brixelizer_scan_group_id
    if (total_group_count - 1 == gs_ffx_brixelizer_scan_group_id) { // the last group does the rest of the scans

        GROUP_SCAN(gtid,
                   total_group_count,
                   FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE,
                   LoadGlobalJobTriangleCounterScan,
                   StoreGlobalJobTriangleCounterScan);

        if (gtid == 0) {
            StoreScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_GROUP_INDEX, FfxUInt32(0));
            InitializeJobIndirectArgs(gs_ffx_brixelizer_scan_group_id);
        }
    }
}

void FfxBrixelizerVoxelize(FfxUInt32 gtid, FfxUInt32 group_id)
{
    if (gtid == 0) {
        gs_ffx_brixelizer_voxelizer_item_counter = 0;
        gs_ffx_brixelizer_voxelizer_ref_counter  = 0;
        gs_ffx_brixelizer_voxelizer_ref_offset   = 0;
        gs_ffx_brixelizer_triangle_offset_global = 0;
        gs_ffx_brixelizer_triangle_offset        = 0;
    }
    FFX_GROUP_MEMORY_BARRIER; // Wait for initialization

    FfxUInt32                     thread_triangle_offset = group_id * FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE + gtid;
    FfxUInt32                     job_idx                = JobLowerBound(thread_triangle_offset, GetBuildInfoNumJobs());
    FfxBoolean                    is_touched             = job_idx < GetBuildInfoNumJobs() && LoadScratchJobCounter(job_idx) > 0;
    FfxUInt32                     triangle_index         = thread_triangle_offset - LoadJobTriangleCountScan(job_idx);
    FfxBrixelizerTriangle               tri;
    FfxBrixelizerCRVoxelTriangleBounds tvbounds;
    FfxBoolean                    collides = false;
    if (is_touched) {
        FfxBrixelizerBrixelizationJob job           = LoadBrixelizationJob(job_idx);
        FfxBrixelizerInstanceInfo     instance_info = LoadInstanceInfo(job.instanceIdx);
        if (triangle_index < instance_info.triangleCount) {
            collides = GetTriangleBounds(job.instanceIdx, job_idx, ffxBrixelizerInstanceInfoGetMeshInfo(instance_info), triangle_index, /* out */ tri,
                                         /* out */ tvbounds);
        }
    }

    ffxassert(!collides || all(tvbounds.ubound_max <= FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION)));
    ffxassert(!collides || all(tvbounds.ubound_min < FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION)));

    FfxUInt32 item_offset;

    if (collides) {
        FFX_ATOMIC_ADD_RETURN(gs_ffx_brixelizer_voxelizer_item_counter, 1, item_offset);

        FfxBrixelizerCRItem item;
        item.bounds_min = tvbounds.ubound_min;
        item.bounds_max = tvbounds.ubound_max;

        FfxBrixelizerCRStoreItem(FfxInt32(item_offset), item);

        FfxUInt32x3 dim = tvbounds.ubound_max - tvbounds.ubound_min;

#if defined(FFX_BRIXELIZER_VOXELIZER_2D)
        FfxUInt32 num_refs = dim.x * dim.z;
#else  // !defined(FFX_BRIXELIZER_VOXELIZER_2D)
        FfxUInt32 num_refs = dim.x * dim.y * dim.z;
#endif // !defined(FFX_BRIXELIZER_VOXELIZER_2D)
        FFX_ATOMIC_ADD(gs_ffx_brixelizer_voxelizer_ref_counter, num_refs);
        gs_ffx_brixelizer_voxelizer_items_ref_count[item_offset]                = num_refs;
        gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_offset] = 0;
    }

    FFX_GROUP_MEMORY_BARRIER; // Wait for gs_ffx_brixelizer_voxelizer_ref_counter

    if (gs_ffx_brixelizer_voxelizer_item_counter == FfxUInt32(0)) return; // scalar

#if defined(FFX_BRIXELIZER_VOXELIZER_CHECK_BRICKS)
    {
        FfxUInt32 item_id  = FfxUInt32(0);
        FfxUInt32 ref_scan = FfxUInt32(0);
        for (FfxUInt32 ref_id = gtid; ref_id < gs_ffx_brixelizer_voxelizer_ref_counter; ref_id += FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE) {
            while (ref_id >= gs_ffx_brixelizer_voxelizer_items_ref_count[item_id] + ref_scan) {
                ref_scan += gs_ffx_brixelizer_voxelizer_items_ref_count[item_id];
                item_id++;
            }
            if (ref_id >= gs_ffx_brixelizer_voxelizer_ref_counter) break;
            FfxUInt32           local_ref_id = ref_id - ref_scan;
            FfxBrixelizerCRItem item         = FfxBrixelizerCRLoadItem(item_id);
            FfxUInt32x3          dim          = item.bounds_max - item.bounds_min;

#    if defined(FFX_BRIXELIZER_VOXELIZER_2D)
            FfxUInt32x2 local_voxel_coord = FfxBrixelizerUnflatten(local_ref_id, dim.xz);

            for (FfxUInt32 y = FfxUInt32(0); y < dim.y; y++) {
                FfxUInt32x3 global_voxel_coord = FfxUInt32x3(local_voxel_coord.x, y, local_voxel_coord.y) + item.bounds_min;
                FfxUInt32  voxel_idx          = FfxBrixelizerFlattenPOT(global_voxel_coord, FFX_BRIXELIZER_CASCADE_DEGREE);
                if (CanBuildThisVoxel(voxel_idx)) {
                    FFX_ATOMIC_OR(gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_id], FfxUInt32(1));
                }
            }
#    else  // !defined(FFX_BRIXELIZER_VOXELIZER_2D)
            FfxUInt32x3 local_voxel_coord  = FfxBrixelizerUnflatten(local_ref_id, dim);
            FfxUInt32x3 global_voxel_coord = local_voxel_coord + item.bounds_min;
            FfxUInt32  voxel_idx          = FfxBrixelizerFlattenPOT(global_voxel_coord, FFX_BRIXELIZER_CASCADE_DEGREE);
            if (CanBuildThisVoxel(voxel_idx)) {
                FFX_ATOMIC_OR(gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_id], FfxUInt32(1));
            }
#    endif // !defined(FFX_BRIXELIZER_VOXELIZER_2D)
        }
    }

#endif // defined(FFX_BRIXELIZER_VOXELIZER_CHECK_BRICKS)

    FFX_GROUP_MEMORY_BARRIER;

    const FfxUInt32 MAX_STORAGE = LoadScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_MAX_TRIANGLES);

    FfxFloat32x3 bounds           = tvbounds.bound_max - tvbounds.bound_min;
    FfxFloat32   aabb_max_dim     = ffxMax3(bounds.x, bounds.y, bounds.z);
    FfxFloat32   voxel_size_ratio = aabb_max_dim / GetCascadeInfoVoxelSize();
    FfxBoolean   small_triangle   = voxel_size_ratio < FfxFloat32(1.0e-1); // 1/10th of a brick is small enough for the point approximation

    FfxUInt32 triangle_size = FfxUInt32(0);
    if (small_triangle)
        triangle_size = FfxUInt32(12);
    else
        triangle_size = FfxUInt32(24);

#if defined(FFX_BRIXELIZER_VOXELIZER_CHECK_BRICKS)
    FfxUInt32 hit_cnt = gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_offset];
#else  // ! defined(FFX_BRIXELIZER_VOXELIZER_CHECK_BRICKS)
    FfxUInt32 hit_cnt = 1;
#endif // ! defined(FFX_BRIXELIZER_VOXELIZER_CHECK_BRICKS)
    FfxUInt32 local_triangle_swap_offset;

    if (collides && hit_cnt != FfxUInt32(0)) FFX_ATOMIC_ADD_RETURN(gs_ffx_brixelizer_triangle_offset, triangle_size, local_triangle_swap_offset);

    FFX_GROUP_MEMORY_BARRIER;

    if (gtid == FfxUInt32(0)) {
        IncrementScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_TRIANGLES, /* in */ gs_ffx_brixelizer_triangle_offset,
                                /* out */ gs_ffx_brixelizer_triangle_offset_global);
        // Check that there's enough swap space for the triangles
        gs_ffx_brixelizer_voxelizer_has_space = gs_ffx_brixelizer_triangle_offset_global + gs_ffx_brixelizer_triangle_offset <= MAX_STORAGE;
    }

    FFX_GROUP_MEMORY_BARRIER;

    // Swap only triangles that have enough resources to get voxelized
    if (collides && gs_ffx_brixelizer_voxelizer_has_space) {
        if (hit_cnt != FfxUInt32(0)) {
            FfxUInt32 triangle_id_swap_offset = local_triangle_swap_offset + gs_ffx_brixelizer_triangle_offset_global;
            if (small_triangle) {
                FfxBrixelizerStoreTriangleCenter(triangle_id_swap_offset, tri);
                triangle_id_swap_offset |= FFX_BRIXELIZER_TRIANGLE_SMALL_FLAG;
            } else
                FfxBrixelizerStoreTrianglePartial(triangle_id_swap_offset, tri);
            gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_offset] = triangle_id_swap_offset;
        } else {
            gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_offset] = FfxUInt32(-1);
        }
    }

    FFX_GROUP_MEMORY_BARRIER;

    {
        FfxUInt32 item_id  = FfxUInt32(0);
        FfxUInt32 ref_scan = FfxUInt32(0);
        for (FfxUInt32 ref_id = gtid; ref_id < gs_ffx_brixelizer_voxelizer_ref_counter; ref_id += FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE) {
            while (ref_id >= gs_ffx_brixelizer_voxelizer_items_ref_count[item_id] + ref_scan) {
                ref_scan += gs_ffx_brixelizer_voxelizer_items_ref_count[item_id];
                item_id++;
            }
            if (ref_id >= gs_ffx_brixelizer_voxelizer_ref_counter) break;
            if (gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_id] == FfxUInt32(-1)) continue; // Skip if culled

            FfxUInt32   local_ref_id = ref_id - ref_scan;
            FfxBrixelizerCRItem item         = FfxBrixelizerCRLoadItem(FfxInt32(item_id));
            FfxUInt32x3 dim          = item.bounds_max - item.bounds_min;
            FfxUInt32   num_cells    = dim.x * dim.y * dim.z;

#if defined(FFX_BRIXELIZER_VOXELIZER_2D)
            FfxUInt32x2 local_voxel_coord = FfxBrixelizerUnflatten(local_ref_id, dim.xz);

            // Only cull if the number of cells is more than N
            FfxBoolean                   check_range = !FfxBrixelizerTriangleIsSmall(gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_id]) && num_cells > FfxUInt32(1);
            FfxBrixelizerTrianglePartial tri;
            FfxFloat32x3                 e0;
            FfxFloat32x3                 e1;
            FfxFloat32x3                 e2;
            FfxFloat32x3                 gn;
            if (check_range) {
                FfxBrixelizerLoadTrianglePartial(FfxBrixelizerTriangleIDGetOffset(gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_id]), /* out */ tri);
                e0 = tri.wp1.xyz - tri.wp0.xyz;
                e1 = tri.wp2.xyz - tri.wp1.xyz;
                e2 = tri.wp0.xyz - tri.wp2.xyz;
                gn = normalize(cross(e2, e0));
            }
            for (FfxUInt32 y = FfxUInt32(0); y < dim.y; y++) {
                FfxUInt32x3 global_voxel_coord = FfxUInt32x3(local_voxel_coord.x, y, local_voxel_coord.y) + item.bounds_min;
                FfxUInt32   voxel_idx          = FfxBrixelizerFlattenPOT(global_voxel_coord, FFX_BRIXELIZER_CASCADE_DEGREE);
                if (check_range) {
                    FfxFloat32x3 voxel_offset = GetCascadeInfoVoxelSize() * (FfxFloat32x3(global_voxel_coord) + FFX_BROADCAST_FLOAT32X3(0.5));
                    FfxFloat32  dist         = abs(dot(gn, (voxel_offset - tri.wp0)));
                    if (dist > GetCascadeInfoVoxelSize() * FfxFloat32(2.0)) continue;
                    dist = CalculateDistanceToTriangle(voxel_offset, tri.wp0, tri.wp1, tri.wp2);
                    if (dist > GetCascadeInfoVoxelSize() * FfxFloat32(2.0)) continue;
                }
                if (!gs_ffx_brixelizer_voxelizer_has_space) {
                    MarkFailed(voxel_idx);
                } else {
                    AddReferenceOrMarkVoxelFailed(voxel_idx, gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_id]);
                }
            }
#else  // !defined(FFX_BRIXELIZER_VOXELIZER_2D)
            FfxUInt32x3 local_voxel_coord  = FfxBrixelizerUnflatten(local_ref_id, dim);
            FfxUInt32x3 global_voxel_coord = local_voxel_coord + item.bounds_min;
            FfxUInt32  voxel_idx          = FfxBrixelizerFlattenPOT(global_voxel_coord, FFX_BRIXELIZER_CASCADE_DEGREE);
            if (!gs_ffx_brixelizer_voxelizer_has_space) {
                MarkFailed(voxel_idx);
            } else {
                AddReferenceOrMarkVoxelFailed(voxel_idx, gs_ffx_brixelizer_voxelizer_items_triangle_id_swap_offsets[item_id]);
            }
#endif // !defined(FFX_BRIXELIZER_VOXELIZER_2D)
        }
    }
}

void FfxBrixelizerScanReferences(FfxUInt32 voxel_flat_id, FfxUInt32 gtid, FfxUInt32 group_id)
{
    FfxUInt32 total_cell_count  = (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION);
    FfxUInt32 total_group_count = (total_cell_count + FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE - FfxUInt32(1)) / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE;
    FfxUInt32 ref_count = voxel_flat_id < total_cell_count ? LoadScratchCR1RefCounter(voxel_flat_id) : FfxUInt32(0);

    FfxUInt32 failed_at_voxelizer = LoadScratchVoxelAllocationFailCounter(voxel_flat_id);

    if (failed_at_voxelizer != FfxUInt32(0)) {
        ref_count = FfxUInt32(0);
        FfxBrixelizerClearRefCounter(voxel_flat_id);
    }

    FfxUInt32 brick_id = LoadCascadeBrickMap(WrapFlatCoords(voxel_flat_id));

    // Brick allocation/deallocation logic
    if (ref_count > 0) {
        if (brick_id == FFX_BRIXELIZER_UNINITIALIZED_ID) { // Allocate a new brick
            brick_id = AllocateBrick();
            if (FfxBrixelizerIsInvalidID(brick_id)) {
                ref_count = FfxUInt32(0);
                FfxBrixelizerClearRefCounter(voxel_flat_id);
                StoreCascadeBrickMap(WrapFlatCoords(voxel_flat_id), FFX_BRIXELIZER_UNINITIALIZED_ID);
            } else {
                ffxassert(FfxBrixelizerIsValidID(brick_id));
                FfxUInt32 storage_alloc_offset = AllocateStorage(brick_id);
                if (storage_alloc_offset == FFX_BRIXELIZER_INVALID_ALLOCATION) {
                    ref_count = FfxUInt32(0);
                    FfxBrixelizerClearRefCounter(voxel_flat_id);
                    FfxBrixelizerMarkBrickFree(brick_id);
                    StoreCascadeBrickMap(WrapFlatCoords(voxel_flat_id), FFX_BRIXELIZER_UNINITIALIZED_ID);
                    brick_id = FFX_BRIXELIZER_INVALID_ID;
                } else {
                    AppendClearBrick(brick_id);
                    AddBrickToCompressionList(brick_id);
                    StoreCascadeBrickMap(WrapFlatCoords(voxel_flat_id), brick_id);
                }
            }
        } else { // Already have an assigned brick
            ref_count = FfxUInt32(0); // No need to rebuild
            FfxBrixelizerClearRefCounter(voxel_flat_id);
        }
    } else {
        if (failed_at_voxelizer == FfxUInt32(0) && // Restart next frame
            brick_id == FFX_BRIXELIZER_UNINITIALIZED_ID) {
            brick_id = FFX_BRIXELIZER_INVALID_ID;
            StoreCascadeBrickMap(WrapFlatCoords(voxel_flat_id), FFX_BRIXELIZER_INVALID_ID);
            FfxBrixelizerClearRefCounter(voxel_flat_id);
        }
    }

    if (FfxBrixelizerIsValidID(brick_id) && brick_id != FFX_BRIXELIZER_UNINITIALIZED_ID) {
        MapBrickToVoxel(brick_id, voxel_flat_id); // Update mapping
    }

    ////////////////////////////////////////////////////
    {
        // Scan the ref counts for sorting
        gs_ffx_brixelizer_scan_buffer[gtid] = ref_count;
        FFX_GROUP_MEMORY_BARRIER;
        FfxUInt32 ref_sum = GroupScanExclusiveAdd(gtid, FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE);
        StoreScratchCR1RefCounterScan(voxel_flat_id, gs_ffx_brixelizer_scan_buffer[gtid]);

        if (gtid == FfxUInt32(0)) // The first thread stores the sum
            StoreVoxelReferenceGroupSum(group_id, ref_sum);
    }
    ////////////////////////////////////////////////////
    {
        // Scan the stamp counts for work distribution
        FfxUInt32 stamp_count = FfxUInt32(0);
        if ((ref_count > 0) && FfxBrixelizerIsValidID(brick_id)) {
            stamp_count = ((ref_count + FFX_BRIXELIZER_STATIC_CONFIG_EMIT_SDF_REFS_PER_GROUP - FfxUInt32(1)) / FFX_BRIXELIZER_STATIC_CONFIG_EMIT_SDF_REFS_PER_GROUP);
        }
        gs_ffx_brixelizer_scan_buffer[gtid] = stamp_count;
        FFX_GROUP_MEMORY_BARRIER;
        FfxUInt32 stamp_sum = GroupScanExclusiveAdd(gtid, FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE);
        StoreScratchCR1StampScan(voxel_flat_id, gs_ffx_brixelizer_scan_buffer[gtid]);
        if (gtid == FfxUInt32(0)) // The first thread stores the sum
            StoreStampGroupSum(group_id, stamp_sum);
    }

    if (gtid == FfxUInt32(0)) IncrementScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_GROUP_INDEX, FfxUInt32(1), gs_ffx_brixelizer_scan_group_id);

    FFX_GROUP_MEMORY_BARRIER;                            // Wait for gs_ffx_brixelizer_scan_group_id
    if (total_group_count - FfxUInt32(1) == gs_ffx_brixelizer_scan_group_id) { // the last group does the rest of the scans

        GROUP_SCAN(gtid,
                   total_group_count,
                   FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE,
                   LoadVoxelReferenceGroupSum,
                   StoreVoxelReferenceGroupSum);

        GROUP_SCAN(gtid,
                   total_group_count,
                   FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE,
                   LoadStampGroupSum,
                   StoreStampGroupSum);

        if (gtid == FfxUInt32(0)) {
            StoreScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_GROUP_INDEX, FfxUInt32(0));
            InitializeIndirectArgs(gs_ffx_brixelizer_scan_group_id);
        }
    }
}

void FfxBrixelizerCompactReferences(FfxUInt32 tid)
{
    FfxUInt32 total_cell_count  = (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION);
    FfxUInt32 total_group_count = (total_cell_count + FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE - FfxUInt32(1)) / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE;

    FfxUInt32 total_references = min(LoadScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_REFERENCES), LoadScratchCounter(FFX_BRIXELIZER_SCRATCH_COUNTER_MAX_REFERENCES));
    if (tid < total_references) {
        FfxBrixelizerTriangleReference ref       = LoadScratchCR1Reference(tid);
        FfxUInt32                     voxel_id  = ref.voxel_idx;
        FfxUInt32                     ref_count = LoadScratchCR1RefCounter(voxel_id);
        if (ref_count > 0) {
            ffxassert(ref.local_ref_idx < ref_count);
            FfxUInt32 offset = GetReferenceOffset(voxel_id) + ref.local_ref_idx;
            StoreScratchCR1CompactedReferences(offset, ref.triangle_id);
        }
    }

    if (tid < total_cell_count) {
        FfxUInt32 group_scan_id    = tid / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE;
        FfxUInt32 group_scan_value = LoadStampGroupSum(group_scan_id);
        FfxUInt32 local_scan_value = LoadScratchCR1StampScan(tid);
        FfxUInt32 stamp_offset     = group_scan_value + local_scan_value;
        StoreScratchCR1StampScan(tid, stamp_offset);
    }
}

void FfxBrixelizerEmitSDF(FfxUInt32 ref_id_offset, FfxUInt32 global_stamp_id)
{
    FfxUInt32 voxel_id        = StampLowerBound(global_stamp_id);
    FfxUInt32 brick_id        = LoadCascadeBrickMap(WrapFlatCoords(voxel_id));
    FfxUInt32 ref_count       = LoadScratchCR1RefCounter(voxel_id);

    if (FfxBrixelizerIsInvalidID(brick_id) || ref_count == FfxUInt32(0)) return;

    FfxUInt32 refbatch_count       = (ref_count + FFX_BRIXELIZER_STATIC_CONFIG_EMIT_SDF_REFS_PER_GROUP - 1) / FFX_BRIXELIZER_STATIC_CONFIG_EMIT_SDF_REFS_PER_GROUP;
    FfxUInt32 global_stamp_offset  = LoadScratchCR1StampScan(voxel_id);
    FfxUInt32 voxel_stamp_id       = global_stamp_id - global_stamp_offset;
    FfxUInt32 refbatch_id          = voxel_stamp_id % refbatch_count;
    FfxUInt32 voxel_ref_offset     = GetReferenceOffset(voxel_id);
    FfxUInt32 refbatch_item_offset = refbatch_id * FFX_BRIXELIZER_STATIC_CONFIG_EMIT_SDF_REFS_PER_GROUP;
    FfxUInt32 start_ref_id         = voxel_ref_offset + refbatch_item_offset;
    FfxUInt32 end_ref_id           = voxel_ref_offset + min(refbatch_item_offset + FFX_BRIXELIZER_STATIC_CONFIG_EMIT_SDF_REFS_PER_GROUP, ref_count);

    FfxUInt32x3 stamp_min = FfxUInt32x3(0, 0, 0);
    FfxUInt32x3 stamp_max = stamp_min + FFX_BROADCAST_UINT32X3(8);

    const FfxFloat32 brick_width      = FfxFloat32(8.0);
    FfxUInt32x3      voxel_coord      = FfxBrixelizerUnflattenPOT(voxel_id, FFX_BRIXELIZER_CASCADE_DEGREE);
    FfxFloat32       brixel_size      = FfxFloat32(GetCascadeInfoVoxelSize() / (brick_width - FfxFloat32(FfxFloat32(1.0))));
    FfxFloat32       half_brixel_size = FfxFloat32(brixel_size / FfxFloat32(FfxFloat32(2.0)));
    FfxFloat32x3     brick_min        = to_float3(voxel_coord) * FfxFloat32(GetCascadeInfoVoxelSize()) - FFX_BROADCAST_FLOAT32X3(FfxFloat32(half_brixel_size));
    FfxFloat32x3     brick_max        = brick_min + FFX_BROADCAST_FLOAT32X3(brixel_size * brick_width);
    FfxFloat32       clamped_dist     = ffxAsFloat(FfxUInt32(-1));
    for (FfxUInt32 ref_id = start_ref_id + ref_id_offset; ref_id < end_ref_id; ref_id += FFX_BRIXELIZER_STATIC_CONFIG_EMIT_SDF_GROUP_SIZE) {
        FfxUInt32 triangle_id = LoadScratchCR1CompactedReferences(ref_id);

        if (FfxBrixelizerTriangleIsSmall(triangle_id)) {
            FfxFloat32x3 center = FfxBrixelizerLoadTriangleCenter(FfxBrixelizerTriangleIDGetOffset(triangle_id));
            FfxFloat32x3 COORD  = (center - brick_min) / (brixel_size);
            FfxFloat32x3 VOXEL  = clamp(floor(COORD), FFX_BROADCAST_FLOAT32X3(0.0), FFX_BROADCAST_FLOAT32X3(7.0));
            FfxFloat32x3 p      = VOXEL + FFX_BROADCAST_FLOAT32X3(FfxFloat32(0.5));
            FfxFloat32  dist   = dot2(p - COORD) * FfxFloat32(0.25) * FfxFloat32(0.25);
            clamped_dist  = FfxFloat32(1.0) * clamp(dist, FfxFloat32(0.0), FfxFloat32(1.0));
            BrickInterlockedMin32(brick_id, FfxInt32x3(VOXEL), clamped_dist);
        } else {

            FfxBrixelizerTrianglePartial tri;
            FfxBrixelizerLoadTrianglePartial(FfxBrixelizerTriangleIDGetOffset(triangle_id), /* out */ tri);

            FfxFloat32x3      TRIANGLE_VERTEX_0 = (tri.wp0 - brick_min) / (brixel_size);
            FfxFloat32x3      TRIANGLE_VERTEX_1 = (tri.wp1 - brick_min) / (brixel_size);
            FfxFloat32x3      TRIANGLE_VERTEX_2 = (tri.wp2 - brick_min) / (brixel_size);
            const FfxFloat32  TRIANGLE_OFFSET   = FfxFloat32(FfxFloat32(0.0));
            FfxFloat32x3      TRIANGLE_MIN      = min(TRIANGLE_VERTEX_0, min(TRIANGLE_VERTEX_1, TRIANGLE_VERTEX_2));
            FfxFloat32x3      TRIANGLE_MAX      = max(TRIANGLE_VERTEX_0, max(TRIANGLE_VERTEX_1, TRIANGLE_VERTEX_2));
            FfxFloat32x3      TRIANGLE_AABB_MIN;
            FfxFloat32x3      TRIANGLE_AABB_MAX;

            TRIANGLE_AABB_MIN.x = (floor(TRIANGLE_MIN.x < FfxFloat32(0.0) ? TRIANGLE_MIN.x - FfxFloat32(1.0) : TRIANGLE_MIN.x)) - TRIANGLE_OFFSET;
            TRIANGLE_AABB_MIN.y = (floor(TRIANGLE_MIN.y < FfxFloat32(0.0) ? TRIANGLE_MIN.y - FfxFloat32(1.0) : TRIANGLE_MIN.y)) - TRIANGLE_OFFSET;
            TRIANGLE_AABB_MIN.z = (floor(TRIANGLE_MIN.z < FfxFloat32(0.0) ? TRIANGLE_MIN.z - FfxFloat32(1.0) : TRIANGLE_MIN.z)) - TRIANGLE_OFFSET;

            TRIANGLE_AABB_MAX.x = (floor(TRIANGLE_MAX.x < FfxFloat32(0.0) ? TRIANGLE_MAX.x - FfxFloat32(1.0) : TRIANGLE_MAX.x)) + (FfxFloat32(1.0) + TRIANGLE_OFFSET);
            TRIANGLE_AABB_MAX.y = (floor(TRIANGLE_MAX.y < FfxFloat32(0.0) ? TRIANGLE_MAX.y - FfxFloat32(1.0) : TRIANGLE_MAX.y)) + (FfxFloat32(1.0) + TRIANGLE_OFFSET);
            TRIANGLE_AABB_MAX.z = (floor(TRIANGLE_MAX.z < FfxFloat32(0.0) ? TRIANGLE_MAX.z - FfxFloat32(1.0) : TRIANGLE_MAX.z)) + (FfxFloat32(1.0) + TRIANGLE_OFFSET);

            TRIANGLE_AABB_MIN = max(TRIANGLE_AABB_MIN, FfxFloat32x3(stamp_min));
            TRIANGLE_AABB_MAX = min(TRIANGLE_AABB_MAX, FfxFloat32x3(stamp_max));

            if (all(FFX_EQUAL(TRIANGLE_AABB_MIN, TRIANGLE_AABB_MAX))) continue;

            FfxFloat32x3 a            = TRIANGLE_VERTEX_0;
            FfxFloat32x3 b            = TRIANGLE_VERTEX_1;
            FfxFloat32x3 c            = TRIANGLE_VERTEX_2;
            FfxFloat32x3 ba           = TRIANGLE_VERTEX_1 - TRIANGLE_VERTEX_0;
            FfxFloat32x3 ac           = TRIANGLE_VERTEX_0 - TRIANGLE_VERTEX_2;
            FfxFloat32x3 cb           = TRIANGLE_VERTEX_2 - TRIANGLE_VERTEX_1;
            FfxFloat32x3 nor          = cross(ba, ac);
            FfxFloat32x3 cross_ba_nor = cross(ba, nor);
            FfxFloat32x3 cross_cb_nor = cross(cb, nor);
            FfxFloat32x3 cross_ac_nor = cross(ac, nor);
            FfxFloat32   dot2_ba      = dot2(ba);
            FfxFloat32   dot2_cb      = dot2(cb);
            FfxFloat32   dot2_ac      = dot2(ac);
            FfxFloat32   dot2_nor     = dot2(nor);

#define FFX_BRIXELIZER_TRIANGLE_VOXELIZER_THIN_LAYER

#define FFX_BRIXELIZER_TRIANGLE_VOXELIZER_BODY                                                                                                                                             \
    {                                                                                                                                                                              \
        FfxFloat32x3 p    = VOXEL + FFX_BROADCAST_FLOAT32X3(FfxFloat32(0.5));                                                                                                                            \
        FfxFloat32   dist = CalculateDistanceToTriangleSquared(ba, p - a, c - b, p - b, ac, p - c, nor, cross_ba_nor, cross_cb_nor, cross_ac_nor, dot2_ba, dot2_cb, dot2_ac, dot2_nor) * \
                     FfxFloat32(0.25) * FfxFloat32(0.25);                                                                                                                                    \
        clamped_dist = FfxFloat32(1.0) * clamp(dist, FfxFloat32(0.0), FfxFloat32(1.0));                                                                                                           \
        BrickInterlockedMin32(brick_id, FfxInt32x3(VOXEL), clamped_dist);                                                                                                               \
    }

            // for FFX_BRIXELIZER_TRIANGLE_VOXELIZER_THIN_LAYER:
            //   Basically a simple 2D loop over 2D AABB of a triangle selected by VX_CRD_2 and VX_CRD_0
            //   Then 1D loop for the depth layer of that triangle for VX_CRD_1
            //   Sensitive to the selection of the major axis
            // else:
            //   Iterates 3D AABB of a triangle

            // Macros allow to change the major plane easily
            // For thin layer there's a major plane for the outer 2D iteration and then the one axis left for 1D iteration
            #if !defined(VX_CRD_0)
            #    define VX_CRD_0 x
            #endif // !defined(VX_CRD_0)

            #if !defined(VX_CRD_1)
            #    define VX_CRD_1 y
            #endif // !defined(VX_CRD_1)

            #if !defined(VX_CRD_2)
            #    define VX_CRD_2 z
            #endif // !defined(VX_CRD_2)

            { // Everything is in grid space

                // 3 2d edge normals with offsets for edge functions for 3 projections xy, yz, xz
                brixelizerreal3   de_xy;
                brixelizerreal3x2 ne_xy;
                brixelizerreal3   de_xz;
                brixelizerreal3x2 ne_xz;
                brixelizerreal3   de_yz;
                brixelizerreal3x2 ne_yz;

                brixelizerreal3 gn; // triangle plane normal

                // Need to offset the edge functions by the grid alignment
                FfxBrixelizerGet2DEdges(              //
                    /* out */ de_xy,            //
                    /* out */ ne_xy,            //
                    /* out */ de_xz,            //
                    /* out */ ne_xz,            //
                    /* out */ de_yz,            //
                    /* out */ ne_yz,            //
                    /* out */ gn,               //
                    brixelizerreal3(TRIANGLE_VERTEX_0), //
                    brixelizerreal3(TRIANGLE_VERTEX_1), //
                    brixelizerreal3(TRIANGLE_VERTEX_2), //
                    TRIANGLE_OFFSET,            //
                    false
                );

                brixelizerreal3 VOXEL;

                // Some duplication but with the other ordering, only one is used though
                brixelizerreal3   de_yx = de_xy;
                brixelizerreal3x2 ne_yx = ne_xy;
                brixelizerreal3   de_zx = de_xz;
                brixelizerreal3x2 ne_zx = ne_xz;
                brixelizerreal3   de_zy = de_yz;
                brixelizerreal3x2 ne_zy = ne_yz;

            #define _CONCAT(a, b) a##b
            #define CONCAT(a, b) _CONCAT(a, b)
            #define VX_CRD_02 CONCAT(VX_CRD_0, VX_CRD_2)
            #define VX_CRD_01 CONCAT(VX_CRD_0, VX_CRD_1)
            #define VX_CRD_12 CONCAT(VX_CRD_1, VX_CRD_2)
            #define VX_DE_01 CONCAT(de_, VX_CRD_01)
            #define VX_DE_02 CONCAT(de_, VX_CRD_02)
            #define VX_DE_12 CONCAT(de_, VX_CRD_12)
            #define VX_NE_02 CONCAT(ne_, VX_CRD_02)
            #define VX_NE_01 CONCAT(ne_, VX_CRD_01)
            #define VX_NE_12 CONCAT(ne_, VX_CRD_12)

                // Just one row of the voxelizer along the 1st
            #if defined(FFX_BRIXELIZER_TRIANGLE_VOXELIZER_ONE_ROW)

                // gn = normalize(cross(e2, e0));
                if (gn.VX_CRD_1 < brixelizerreal(0.0)) {
                    gn = -gn; // make normal point in +z direction
                }
                brixelizerreal ny_inv = brixelizerreal(brixelizerreal(1.0) / max(gn.VX_CRD_1, brixelizerreal(1.0e-4)));
                brixelizerreal d_tri  = -dot(gn, brixelizerreal3(TRIANGLE_VERTEX_0));
                // 2 plane equation offsets with grid alignment
                brixelizerreal d_tri_proj_min = -FfxBrixelizerOffsetByMax(d_tri, gn.VX_CRD_01, TRIANGLE_OFFSET) * ny_inv;
                brixelizerreal d_tri_proj_max = -FfxBrixelizerOffsetByMin(d_tri, gn.VX_CRD_01, TRIANGLE_OFFSET) * ny_inv;

                VOXEL.VX_CRD_2 = TRIANGLE_AABB_MIN.VX_CRD_2;
                {
                    VOXEL.VX_CRD_0 = TRIANGLE_AABB_MIN.VX_CRD_0;
                    {
                        if (FfxBrixelizerEvalEdge(VOXEL.VX_CRD_02, VX_DE_02, VX_NE_02)) // 2D triangle test
                        {
                            // Now figure out the 3rd coordinate range [min, max]
                            // By doing range analysis on the evaluation of the plane equation on the 4 corners of the row
                            brixelizerreal y00   = -((VOXEL.VX_CRD_0 + brixelizerreal(0.0)) * gn.VX_CRD_0 + (VOXEL.VX_CRD_2 + brixelizerreal(0.0)) * gn.VX_CRD_2) * ny_inv;
                            brixelizerreal y01   = -((VOXEL.VX_CRD_0 + brixelizerreal(0.0)) * gn.VX_CRD_0 + (VOXEL.VX_CRD_2 + brixelizerreal(1.0)) * gn.VX_CRD_2) * ny_inv;
                            brixelizerreal y10   = -((VOXEL.VX_CRD_0 + brixelizerreal(1.0)) * gn.VX_CRD_0 + (VOXEL.VX_CRD_2 + brixelizerreal(0.0)) * gn.VX_CRD_2) * ny_inv;
                            brixelizerreal y11   = -((VOXEL.VX_CRD_0 + brixelizerreal(1.0)) * gn.VX_CRD_0 + (VOXEL.VX_CRD_2 + brixelizerreal(1.0)) * gn.VX_CRD_2) * ny_inv;
                            brixelizerreal min_y = floor(min(y00, min(y01, min(y10, y11))) - d_tri * ny_inv);
                            min_y        = max(TRIANGLE_AABB_MIN.VX_CRD_1, min_y);
                            brixelizerreal max_y = floor(max(y00, max(y01, max(y10, y11))) - d_tri * ny_inv) + brixelizerreal(1.0);
                            max_y        = min(TRIANGLE_AABB_MAX.VX_CRD_1, max_y);

                            // brixelizerreal min_y = floor(min(y00, min(y01, min(y10, y11 + d_tri_proj_min))) + d_tri_proj_min);
                            // min_y        = max(TRIANGLE_AABB_MIN.VX_CRD_1, min_y);
                            // brixelizerreal max_y = floor(max(y00, max(y01, max(y10, y11))) + d_tri_proj_max) + brixelizerreal(1.0);
                            // max_y        = min(TRIANGLE_AABB_MAX.VX_CRD_1, max_y);

                            for (VOXEL.VX_CRD_1 = min_y; VOXEL.VX_CRD_1 < max_y; VOXEL.VX_CRD_1 += brixelizerreal(1.0)) {
                                // for (VOXEL.VX_CRD_1 = TRIANGLE_AABB_MIN.VX_CRD_1; VOXEL.VX_CRD_1 < TRIANGLE_AABB_MAX.VX_CRD_1; VOXEL.VX_CRD_1 += brixelizerreal(1.0)) {
                                // if (FfxBrixelizerEvalEdge(VOXEL.VX_CRD_01, VX_DE_01, VX_NE_01) && // the rest of the 2D triangle tests
                                // FfxBrixelizerEvalEdge(VOXEL.VX_CRD_12, VX_DE_12, VX_NE_12))   //
                                { FFX_BRIXELIZER_TRIANGLE_VOXELIZER_BODY; }
                            }
                        }
                    }
                }

                // Outer loop iterates on the 2d bounding box
            #elif defined(FFX_BRIXELIZER_TRIANGLE_VOXELIZER_THIN_LAYER)
                // gn = normalize(cross(e2, e0));
                if (gn.VX_CRD_1 < brixelizerreal(0.0)) {
                    gn = -gn; // make normal point in +z direction
                }
                brixelizerreal ny_inv         = brixelizerreal(brixelizerreal(1.0) / max(gn.VX_CRD_1, brixelizerreal(1.0e-4)));
                brixelizerreal d_tri          = -dot(gn, brixelizerreal3(TRIANGLE_VERTEX_0));
                brixelizerreal d_tri_proj_min = -FfxBrixelizerOffsetByMax(d_tri, gn.VX_CRD_01, TRIANGLE_OFFSET) * ny_inv;
                brixelizerreal d_tri_proj_max = -FfxBrixelizerOffsetByMin(d_tri, gn.VX_CRD_01, TRIANGLE_OFFSET) * ny_inv;
                // For thin layer we iterate N^2 and project a point on two planes to find the lower/upper bound for the 3rd inner loop
                for (VOXEL.VX_CRD_2 = TRIANGLE_AABB_MIN.VX_CRD_2; VOXEL.VX_CRD_2 < TRIANGLE_AABB_MAX.VX_CRD_2; VOXEL.VX_CRD_2 += brixelizerreal(1.0)) {
                    for (VOXEL.VX_CRD_0 = TRIANGLE_AABB_MIN.VX_CRD_0; VOXEL.VX_CRD_0 < TRIANGLE_AABB_MAX.VX_CRD_0; VOXEL.VX_CRD_0 += brixelizerreal(1.0)) {
                        if (FfxBrixelizerEvalEdge(VOXEL.VX_CRD_02, VX_DE_02, VX_NE_02)) // 2D triangle test
                        {
                            // Now figure out the 3rd coordinate range [min, max]
                            // By doing range analysis on the evaluation of 4 corners of the grid
                            brixelizerreal y00   = -((VOXEL.VX_CRD_0 + brixelizerreal(0.0)) * gn.VX_CRD_0 + (VOXEL.VX_CRD_2 + brixelizerreal(0.0)) * gn.VX_CRD_2) * ny_inv;
                            brixelizerreal y01   = -((VOXEL.VX_CRD_0 + brixelizerreal(0.0)) * gn.VX_CRD_0 + (VOXEL.VX_CRD_2 + brixelizerreal(1.0)) * gn.VX_CRD_2) * ny_inv;
                            brixelizerreal y10   = -((VOXEL.VX_CRD_0 + brixelizerreal(1.0)) * gn.VX_CRD_0 + (VOXEL.VX_CRD_2 + brixelizerreal(0.0)) * gn.VX_CRD_2) * ny_inv;
                            brixelizerreal y11   = -((VOXEL.VX_CRD_0 + brixelizerreal(1.0)) * gn.VX_CRD_0 + (VOXEL.VX_CRD_2 + brixelizerreal(1.0)) * gn.VX_CRD_2) * ny_inv;
                            brixelizerreal min_y = floor(min(y00 + d_tri_proj_min, min(y01 + d_tri_proj_min, min(y10 + d_tri_proj_min, y11 + d_tri_proj_min))));
                            min_y        = max(TRIANGLE_AABB_MIN.VX_CRD_1, min_y);
                            brixelizerreal max_y = floor(max(y00 + d_tri_proj_max, max(y01 + d_tri_proj_max, max(y10 + d_tri_proj_max, y11 + d_tri_proj_max)))) + brixelizerreal(1.0);
                            max_y        = min(TRIANGLE_AABB_MAX.VX_CRD_1, max_y);

                            for (VOXEL.VX_CRD_1 = min_y; VOXEL.VX_CRD_1 < max_y; VOXEL.VX_CRD_1 += brixelizerreal(1.0)) {
                                if (FfxBrixelizerEvalEdge(VOXEL.VX_CRD_01, VX_DE_01, VX_NE_01) && // the rest of the 2D triangle tests
                                    FfxBrixelizerEvalEdge(VOXEL.VX_CRD_12, VX_DE_12, VX_NE_12))   //
                                {
                                    FFX_BRIXELIZER_TRIANGLE_VOXELIZER_BODY;
                                }
                            }
                        }
                    }
                }
            #else // !FFX_BRIXELIZER_TRIANGLE_VOXELIZER_THIN_LAYER
                // For thick layer we iterate N^3
                for (VOXEL.VX_CRD_2 = TRIANGLE_AABB_MIN.VX_CRD_2; VOXEL.VX_CRD_2 < TRIANGLE_AABB_MAX.VX_CRD_2; VOXEL.VX_CRD_2 += brixelizerreal(1.0)) {
                    for (VOXEL.VX_CRD_0 = TRIANGLE_AABB_MIN.VX_CRD_0; VOXEL.VX_CRD_0 < TRIANGLE_AABB_MAX.VX_CRD_0; VOXEL.VX_CRD_0 += brixelizerreal(1.0)) {
                        if (FfxBrixelizerEvalEdge(VOXEL.VX_CRD_02, VX_DE_02, VX_NE_02)) //
                        {
                            for (VOXEL.VX_CRD_1 = TRIANGLE_AABB_MIN.VX_CRD_1; VOXEL.VX_CRD_1 < TRIANGLE_AABB_MAX.VX_CRD_1; VOXEL.VX_CRD_1 += brixelizerreal(1.0)) {
                                if (FfxBrixelizerEvalEdge(VOXEL.VX_CRD_01, VX_DE_01, VX_NE_01) && //
                                    FfxBrixelizerEvalEdge(VOXEL.VX_CRD_12, VX_DE_12, VX_NE_12))   //
                                {
                                    FFX_BRIXELIZER_TRIANGLE_VOXELIZER_BODY;
                                }
                            }
                        }
                    }
                }
            #endif
            }

#undef FFX_BRIXELIZER_TRIANGLE_VOXELIZER_BODY

#undef FFX_BRIXELIZER_TRIANGLE_VOXELIZER_THIN_LAYER
        }
    }
}

FFX_GROUPSHARED FfxUInt32x3 lds_aabb_tree_min;
FFX_GROUPSHARED FfxUInt32x3 lds_aabb_tree_max;
// Build AABB tree for 64^3 for 4^3 stamp
void FfxBrixelizerBuildTreeAABB(FfxUInt32x3 gid, FfxUInt32x3 group_id)
{
    FfxUInt32 layer_idx = GetBuildInfoTreeIteration();

    if (all(FFX_EQUAL(gid, FFX_BROADCAST_UINT32X3(0)))) {
        lds_aabb_tree_min = FFX_BROADCAST_UINT32X3(FfxUInt32(-1));
        lds_aabb_tree_max = FFX_BROADCAST_UINT32X3(FfxUInt32(0));
    }

    FFX_GROUP_MEMORY_BARRIER;

    if (layer_idx == FfxUInt32(0)) { // bottom level 16^16^16 of 4^4^4
        FfxUInt32x3 child_coord         = gid.xyz;
        FfxUInt32x3 node_offset         = group_id.xyz * FfxUInt32(4);
        FfxUInt32x3 voxel_coord         = node_offset + child_coord;
        FfxUInt32   brick_id            = LoadCascadeBrickMap(FfxBrixelizerFlattenPOT(WrapCoords(voxel_coord), FFX_BRIXELIZER_CASCADE_DEGREE));
        FfxBoolean  full_or_unitialized = brick_id != FFX_BRIXELIZER_INVALID_ID; // It's a valid brick or an uninitialized one
        if (full_or_unitialized) {
            FfxUInt32  brick_aabb_pack = 0x3FE00;
            if (brick_id != FFX_BRIXELIZER_UNINITIALIZED_ID) {
                brick_aabb_pack = LoadBricksAABB(FfxBrixelizerBrickGetIndex(brick_id));
            }
            FfxUInt32x3 brick_aabb_umin = FfxBrixelizerUnflattenPOT(brick_aabb_pack & ((FfxUInt32(1) << FfxUInt32(9)) - FfxUInt32(1)), FfxUInt32(3));
            FfxUInt32x3 brick_aabb_umax = FfxBrixelizerUnflattenPOT((brick_aabb_pack >> FfxUInt32(9)) & ((FfxUInt32(1) << FfxUInt32(9)) - FfxUInt32(1)), FfxUInt32(3));
            FFX_ATOMIC_MIN(lds_aabb_tree_min.x, child_coord.x * FfxUInt32(8) + brick_aabb_umin.x);
            FFX_ATOMIC_MIN(lds_aabb_tree_min.y, child_coord.y * FfxUInt32(8) + brick_aabb_umin.y);
            FFX_ATOMIC_MIN(lds_aabb_tree_min.z, child_coord.z * FfxUInt32(8) + brick_aabb_umin.z);
            FFX_ATOMIC_MAX(lds_aabb_tree_max.x, child_coord.x * FfxUInt32(8) + brick_aabb_umax.x);
            FFX_ATOMIC_MAX(lds_aabb_tree_max.y, child_coord.y * FfxUInt32(8) + brick_aabb_umax.y);
            FFX_ATOMIC_MAX(lds_aabb_tree_max.z, child_coord.z * FfxUInt32(8) + brick_aabb_umax.z);
        }
        FFX_GROUP_MEMORY_BARRIER;
        if (all(FFX_EQUAL(gid, FFX_BROADCAST_UINT32X3(0)))) {
            FfxUInt32 flat_stamp_idx = FfxBrixelizerFlattenPOT(group_id.xyz, FfxUInt32(4));
            FfxUInt32 min_pack       = FfxBrixelizerFlattenPOT(lds_aabb_tree_min.xyz & FfxUInt32(0x1f), FfxUInt32(5));
            FfxUInt32 max_pack       = FfxBrixelizerFlattenPOT(lds_aabb_tree_max.xyz & FfxUInt32(0x1f), FfxUInt32(5));
            StoreCascadeAABBTreeUInt(flat_stamp_idx, min_pack | (max_pack << FfxUInt32(16)));
        }
    } else if (layer_idx == FfxUInt32(1)) { // mid level 4^4^4 of 4^4^4
        FfxUInt32x3 child_coord      = gid.xyz * FfxUInt32(4) + group_id.xyz * FfxUInt32(16);
        FfxUInt32  child_idx        = FfxBrixelizerFlattenPOT(child_coord / FfxUInt32(4), FfxUInt32(4));
        FfxUInt32  bottom_aabb_node = LoadCascadeAABBTreeUInt(child_idx);
        FfxUInt32x3 aabb_min         = FfxBrixelizerUnflattenPOT(bottom_aabb_node & FfxUInt32(0x7fff), FfxUInt32(5));
        FfxUInt32x3 aabb_max         = FfxBrixelizerUnflattenPOT((bottom_aabb_node >> FfxUInt32(16)) & FfxUInt32(0x7fff), FfxUInt32(5));
        if (bottom_aabb_node != FFX_BRIXELIZER_INVALID_BOTTOM_AABB_NODE) {
            FFX_ATOMIC_MIN(lds_aabb_tree_min.x, child_coord.x * FfxUInt32(8) + aabb_min.x);
            FFX_ATOMIC_MIN(lds_aabb_tree_min.y, child_coord.y * FfxUInt32(8) + aabb_min.y);
            FFX_ATOMIC_MIN(lds_aabb_tree_min.z, child_coord.z * FfxUInt32(8) + aabb_min.z);
            FFX_ATOMIC_MAX(lds_aabb_tree_max.x, child_coord.x * FfxUInt32(8) + aabb_max.x);
            FFX_ATOMIC_MAX(lds_aabb_tree_max.y, child_coord.y * FfxUInt32(8) + aabb_max.y);
            FFX_ATOMIC_MAX(lds_aabb_tree_max.z, child_coord.z * FfxUInt32(8) + aabb_max.z);
        }
        FFX_GROUP_MEMORY_BARRIER;
        if (all(FFX_EQUAL(gid, FFX_BROADCAST_UINT32X3(0)))) {
            if (lds_aabb_tree_min.x == FfxUInt32(-1)) { // TODO(Dihara): Check this!!!!!
                StoreCascadeAABBTreeFloat3(FfxUInt32(16 * 16 * 16) + 6 * FfxBrixelizerFlattenPOT(group_id, 2) + 0, FfxFloat32x3(0.0, 0.0, 0.0));
                StoreCascadeAABBTreeFloat3(FfxUInt32(16 * 16 * 16) + 6 * FfxBrixelizerFlattenPOT(group_id, 2) + 3, FfxFloat32x3(0.0, 0.0, 0.0));
            } else {
                FfxFloat32x3 world_aabb_min = FfxFloat32x3(lds_aabb_tree_min.xyz) * GetCascadeInfoVoxelSize() / FfxFloat32(8.0) + GetCascadeInfoGridMin();
                FfxFloat32x3 world_aabb_max = FfxFloat32x3(lds_aabb_tree_max.xyz + FFX_BROADCAST_UINT32X3(1)) * GetCascadeInfoVoxelSize() / FfxFloat32(8.0) + GetCascadeInfoGridMin();
                StoreCascadeAABBTreeFloat3(FfxUInt32(16 * 16 * 16) + 3 * (2 * FfxBrixelizerFlattenPOT(group_id, 2) + FfxUInt32(0)), FfxFloat32x3(world_aabb_min));
                StoreCascadeAABBTreeFloat3(FfxUInt32(16 * 16 * 16) + 3 * (2 * FfxBrixelizerFlattenPOT(group_id, 2) + FfxUInt32(1)), FfxFloat32x3(world_aabb_max));
            }
        }
    } else if (layer_idx == FfxUInt32(2)) { // toP level 4^4^4
        FfxUInt32x3  child_coord    = gid.xyz;
        FfxUInt32   child_idx      = FfxBrixelizerFlattenPOT(child_coord, FfxUInt32(2));
        FfxFloat32x3 stamp_aabb_min = LoadCascadeAABBTreeFloat3(FfxUInt32(16 * 16 * 16) + (FfxUInt32(2) * child_idx + FfxUInt32(0)) * 3);
        FfxFloat32x3 stamp_aabb_max = LoadCascadeAABBTreeFloat3(FfxUInt32(16 * 16 * 16) + (FfxUInt32(2) * child_idx + FfxUInt32(1)) * 3);
        FfxUInt32x3  voxel_aabb_min = FfxUInt32x3(max(FFX_BROADCAST_FLOAT32X3(0.0), stamp_aabb_min - GetCascadeInfoGridMin()) / (GetCascadeInfoVoxelSize() / FfxFloat32(8.0)));
        FfxUInt32x3  voxel_aabb_max = FfxUInt32x3(max(FFX_BROADCAST_FLOAT32X3(0.0), stamp_aabb_max - GetCascadeInfoGridMin()) / (GetCascadeInfoVoxelSize() / FfxFloat32(8.0)));
        if (ffxAsUInt32(stamp_aabb_min.x) != ffxAsUInt32(stamp_aabb_max.x)) {
            FFX_ATOMIC_MIN(lds_aabb_tree_min.x, voxel_aabb_min.x);
            FFX_ATOMIC_MIN(lds_aabb_tree_min.y, voxel_aabb_min.y);
            FFX_ATOMIC_MIN(lds_aabb_tree_min.z, voxel_aabb_min.z);
            FFX_ATOMIC_MAX(lds_aabb_tree_max.x, voxel_aabb_max.x);
            FFX_ATOMIC_MAX(lds_aabb_tree_max.y, voxel_aabb_max.y);
            FFX_ATOMIC_MAX(lds_aabb_tree_max.z, voxel_aabb_max.z);
        }
        FFX_GROUP_MEMORY_BARRIER;
        if (all(FFX_EQUAL(gid, FFX_BROADCAST_UINT32X3(0)))) {
            if (lds_aabb_tree_min.x == FfxUInt32(-1)) { // TODO(Dihara): Check this!!!!!
                StoreCascadeAABBTreeFloat3(FfxUInt32(16 * 16 * 16) + FfxUInt32(4 * 4 * 4) * 6 + 0, FfxFloat32x3(0.0, 0.0, 0.0));
                StoreCascadeAABBTreeFloat3(FfxUInt32(16 * 16 * 16) + FfxUInt32(4 * 4 * 4) * 6 + 3, FfxFloat32x3(0.0, 0.0, 0.0));
            } else {
                FfxFloat32x3 world_aabb_min = FfxFloat32x3(lds_aabb_tree_min.xyz) * GetCascadeInfoVoxelSize() / FfxFloat32(8.0) + GetCascadeInfoGridMin();
                FfxFloat32x3 world_aabb_max = FfxFloat32x3(lds_aabb_tree_max.xyz) * GetCascadeInfoVoxelSize() / FfxFloat32(8.0) + GetCascadeInfoGridMin();
                StoreCascadeAABBTreeFloat3(FfxUInt32(16 * 16 * 16) + 3 * FfxUInt32(2 * 4 * 4 * 4 + 0), world_aabb_min);
                StoreCascadeAABBTreeFloat3(FfxUInt32(16 * 16 * 16) + 3 * FfxUInt32(2 * 4 * 4 * 4 + 1), world_aabb_max);
            }
        }
    }
}

void FfxBrixelizerClearBrickStorage(FfxUInt32 gtid, FfxUInt32 group_id)
{
    FfxUInt32   brick_offset = group_id >> FfxUInt32(3);
    FfxUInt32   stamp_id     = group_id & FfxUInt32(7);
    FfxUInt32   brick_id     = LoadScratchBricksClearList(brick_offset);
    FfxUInt32   brick_dim    = FfxUInt32(8);
    FfxUInt32x3 local_coord  = FfxBrixelizerUnflattenPOT(gtid, FfxUInt32(2)) + FfxBrixelizerUnflattenPOT(stamp_id, FfxUInt32(1)) * FfxUInt32(4);

    ClearBrixelData32(brick_id, FfxInt32x3(local_coord));
}

FFX_GROUPSHARED FfxUInt32x3 lds_brick_aabb_min;
FFX_GROUPSHARED FfxUInt32x3 lds_brick_aabb_max;
void FfxBrixelizerCompressBrick(FfxUInt32 gtid, FfxUInt32 brick_map_offset)
{
    FfxUInt32 brick_id         = LoadScratchBricksCompressionList(brick_map_offset);
    FfxUInt32 voxel_id         = FfxBrixelizerLoadBrickVoxelID(brick_id);
    FfxUInt32 voxel_idx        = FfxBrixelizerVoxelGetIndex(voxel_id);
    FfxUInt32 cascade_id       = FfxBrixelizerGetVoxelCascade(voxel_id);

    if (gtid == 0) {
        lds_brick_aabb_max = FFX_BROADCAST_UINT32X3(0);
        lds_brick_aabb_min = FFX_BROADCAST_UINT32X3(0xffffffffu);
    }
    FfxUInt32x3 local_coord = FfxBrixelizerUnflattenPOT(gtid, FfxUInt32(3));
    FfxFloat32 val = LoadBrixelData32(brick_id, FfxInt32x3(local_coord));

    FFX_GROUP_MEMORY_BARRIER;
    if (val < FfxFloat32(1.0 / 8.0)) {
        FFX_ATOMIC_MAX(lds_brick_aabb_max.x, local_coord.x);
        FFX_ATOMIC_MAX(lds_brick_aabb_max.y, local_coord.y);
        FFX_ATOMIC_MAX(lds_brick_aabb_max.z, local_coord.z);
        FFX_ATOMIC_MIN(lds_brick_aabb_min.x, local_coord.x);
        FFX_ATOMIC_MIN(lds_brick_aabb_min.y, local_coord.y);
        FFX_ATOMIC_MIN(lds_brick_aabb_min.z, local_coord.z);
    }
    FFX_GROUP_MEMORY_BARRIER;
    if (gtid == FfxUInt32(0)) {
        if (lds_brick_aabb_min.x == FfxUInt32(0xffffffff)) { // free brick
            FfxBrixelizerMarkBrickFree(brick_id);
            StoreCascadeBrickMap(WrapFlatCoords(voxel_idx), FFX_BRIXELIZER_INVALID_ID);
        } else {
            FfxUInt32 pack0 = FfxBrixelizerFlattenPOT(min(FFX_BROADCAST_UINT32X3(7), lds_brick_aabb_min), FfxUInt32(3));
            FfxUInt32 pack1 = FfxBrixelizerFlattenPOT(min(FFX_BROADCAST_UINT32X3(7), lds_brick_aabb_max), FfxUInt32(3));
            StoreBricksAABB(FfxBrixelizerBrickGetIndex(brick_id), pack0 | (pack1 << FfxUInt32(9)));
        }
    }

    if (lds_brick_aabb_min.x != 0xffffffffu) {
        if (abs(val) > FfxFloat32(0.9999)) return;
        val = (FfxBrixelizerGetSign(val) * sqrt(abs(val)) * FfxFloat32(4.0)) / FfxFloat32(8 - 1);
        ffxassert(val >= -FfxFloat32(1.0) && val <= FfxFloat32(1.0));
        StoreSDFAtlas(FfxBrixelizerGetSDFAtlasOffset(brick_id) + local_coord, clamp(val, FfxFloat32(0.0), FfxFloat32(1.0)));
    }
}

#endif // ifndef FFX_BRIXELIZER_CASCADE_OPS_H