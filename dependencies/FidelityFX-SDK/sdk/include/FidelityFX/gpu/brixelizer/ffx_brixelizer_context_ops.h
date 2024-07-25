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

#ifndef FFX_BRIXELIZER_CONTEXT_OPS_H
#define FFX_BRIXELIZER_CONTEXT_OPS_H

#include "ffx_brixelizer_host_gpu_shared_private.h"
#include "ffx_brixelizer_brick_common_private.h"
#include "ffx_brixelizer_build_common.h"
#include "ffx_brixelizer_common_private.h"

void FfxBrixelizerAppendClearBrick(FfxUInt32 brick_id)
{
    FfxUInt32 offset;
    IncrementContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_CLEAR_BRICKS, FfxUInt32(1), offset);
    StoreBricksClearList(offset, brick_id);
}

void FfxBrixelizerAppendDirtyBrick(FfxUInt32 brick_id)
{
    FfxUInt32 offset;
    IncrementContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_DIRTY_BRICKS, FfxUInt32(1), offset);
    StoreBricksDirtyList(offset, brick_id);
}

void FfxBrixelizerClearCounters()
{
    for (FfxUInt32 i = 0; i < FFX_BRIXELIZER_NUM_CONTEXT_COUNTERS; i++) {
        StoreContextCounter(i, FfxUInt32(0));
    }
}

void FfxBrixelizerAddBrickToFreeList(FfxUInt32 brick_id)
{
    FfxUInt32 offset;
    IncrementContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_FREE_BRICKS, FfxUInt32(1), offset);
    StoreBricksFreeList(offset, brick_id);
}

FfxFloat32 FfxBrixelizerLoadBrixelDist(FfxUInt32 brick_id, FfxInt32x3 coord)
{
    if (any(FFX_GREATER_THAN(coord, FFX_BROADCAST_INT32X3(7))) || any(FFX_LESS_THAN(coord, FFX_BROADCAST_INT32X3(0)))) return FfxFloat32(1.0);
    return LoadSDFAtlas(FfxBrixelizerGetSDFAtlasOffset(brick_id) + coord);
}

void FfxBrixelizerStoreBrixelDist(FfxUInt32 brick_id, FfxUInt32x3 coord, FfxFloat32 dist)
{
    StoreSDFAtlas(FfxBrixelizerGetSDFAtlasOffset(brick_id) + coord, clamp(dist, FfxFloat32(0.0), FfxFloat32(1.0)));
}

void FfxBrixelizerClearBrick(FfxUInt32 brick_id, FfxUInt32x3 voxel_coord)
{
    FfxBrixelizerStoreBrixelDist(brick_id, voxel_coord, FfxFloat32(1.0));
}

FfxFloat32 FfxBrixelizerEikonalMin(FfxFloat32 a, FfxFloat32 b)
{
    return FfxBrixelizerUnsignedMin(a, b);
}

// Eikonal solver, enforce |gradient|==d
// Generic form for n dimensions:
// U = 1/n * (sum(U_i) + sqrt(sum(U_i) * sum(U_i) - n * (sum(U_i * U_i) - D)))
// when sqrt is non real then fall back to min of lower dimensions
FfxFloat32 FfxBrixelizerEikonal1D(FfxFloat32 x, FfxFloat32 y, FfxFloat32 z, FfxFloat32 d)
{
    FfxFloat32 xyz = FfxBrixelizerEikonalMin(x, FfxBrixelizerEikonalMin(y, z));
    return (xyz + d * FfxBrixelizerGetSign(xyz));
}

FfxFloat32 FfxBrixelizerEikonal2D(FfxFloat32 x, FfxFloat32 y, FfxFloat32 d)
{
    const FfxFloat32 k  = FfxFloat32(1.0) / FfxFloat32(2.0);
    FfxFloat32       xy = x + y;
    FfxFloat32       v  = xy * xy - FfxFloat32(2.0) * (x * x + y * y - d * d);
    if (v < FfxFloat32(0.0)) return FfxFloat32(1.0);
    return k * (xy + sqrt(v) * FfxBrixelizerGetSign(xy));
}

FfxFloat32 FfxBrixelizerEikonal3D(FfxFloat32 x, FfxFloat32 y, FfxFloat32 z, FfxFloat32 d)
{
    const FfxFloat32 k   = FfxFloat32(1.0) / FfxFloat32(3.0);
    FfxFloat32       xyz = x + y + z;
    FfxFloat32       v   = xyz * xyz - FfxFloat32(3.0) * (x * x + y * y + z * z - d * d);
    if (v < FfxFloat32(0.0)) return FfxFloat32(1.0);
    return k * (xyz + sqrt(v) * FfxBrixelizerGetSign(xyz));
}

FFX_GROUPSHARED FfxFloat32 lds_eikonal_sdf_cache[512];
FfxFloat32 FfxBrixelizerLDSLoadSDF(FfxInt32x3 coord)
{
    if (any(FFX_LESS_THAN(coord, FFX_BROADCAST_INT32X3(0))) || any(FFX_GREATER_THAN(coord, FFX_BROADCAST_INT32X3(7)))) return FfxFloat32(1.0);
    return lds_eikonal_sdf_cache[FfxUInt32(coord.x) + FfxUInt32(coord.y) * FfxUInt32(8) + FfxUInt32(coord.z) * FfxUInt32(8 * 8)];
}

void FfxBrixelizerLDSStoreSDF(FfxInt32x3 coord, FfxFloat32 sdf)
{
    lds_eikonal_sdf_cache[FfxUInt32(coord.x) + FfxUInt32(coord.y) * FfxUInt32(8) + FfxUInt32(coord.z) * FfxUInt32(8 * 8)] = sdf;
}

// Collects a list of bricks to clear for indirect args
// those don't have a brick_id->voxel_id mapping are considered free
// those that got eikonal counter are considered dirty as they had something baked in them
// DEFINE_ENTRY(64, 1, 1, CollectClearBricks)
void FfxBrixelizerCollectClearBricks(FfxUInt32 brick_offset)
{
    if (brick_offset >= GetContextInfoNumBricks()) return;

    FfxUInt32 brick_id        = FfxBrixelizerMakeBrickID(brick_offset);
    FfxUInt32 dim             = 8;
    FfxUInt32 voxel_id        = FfxBrixelizerLoadBrickVoxelID(brick_id);
    FfxUInt32 voxel_idx       = FfxBrixelizerVoxelGetIndex(voxel_id);
    FfxUInt32 cascade_id      = FfxBrixelizerGetVoxelCascade(voxel_id);
    FfxUInt32 eikonal_counter = LoadBricksEikonalCounters(brick_id);

    // @TODO cleanup this mess
    if (GetBuildInfoDoInitialization() > 0) {
        StoreBricksEikonalCounters(brick_id, 0);
        FfxBrixelizerMarkBrickFree(brick_id);
        FfxBrixelizerAddBrickToFreeList(brick_id);
        FfxBrixelizerAppendClearBrick(brick_id);
    } else {
#ifdef FFX_BRIXELIZER_DEBUG_FORCE_REBUILD
        StoreBricksEikonalCounters(brick_id, 0);
        FfxBrixelizerMarkBrickFree(brick_id);
        FfxBrixelizerAddBrickToFreeList(brick_id);
        FfxBrixelizerAppendClearBrick(brick_id);

#else // !FFX_BRIXELIZER_DEBUG_FORCE_REBUILD

        if (FfxBrixelizerIsInvalidID(voxel_id)) {
            FfxBrixelizerAddBrickToFreeList(brick_id);
            if (eikonal_counter > 0) { // Means there's been some baking using this brick so need to clear
                FfxBrixelizerAppendClearBrick(brick_id);
                StoreBricksEikonalCounters(brick_id, 0);
            }
        }

#endif // !FFX_BRIXELIZER_DEBUG_FORCE_REBUILD
    }
}

void FfxBrixelizerPrepareClearBricks()
{
    FfxUInt32 tier_cnt = LoadContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_CLEAR_BRICKS);
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_CLEAR_BRICKS_32 + FfxUInt32(0), tier_cnt * FfxUInt32(1 << (3)));
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_CLEAR_BRICKS_32 + FfxUInt32(1), FfxUInt32(1));
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_CLEAR_BRICKS_32 + FfxUInt32(2), FfxUInt32(1));
}

void FfxBrixelizerClearBrickEntry(FfxUInt32 gtid, FfxUInt32 group_id)
{
    FfxUInt32  brick_offset = group_id.x >> (3);
    FfxUInt32  stamp_id     = group_id.x & ((1 << (3)) - 1);
    FfxUInt32  brick_id     = LoadBricksClearList(brick_offset);
    FfxUInt32  brick_dim    = 8;
    FfxUInt32x3 local_coord  = FfxBrixelizerUnflattenPOT(gtid, 2) + FfxBrixelizerUnflattenPOT(stamp_id, 1) * FfxUInt32(4);

    FfxBrixelizerClearBrick(brick_id, local_coord);
}

void FfxBrixelizerCollectDirtyBricks(FfxUInt32 brick_offset)
{
    FfxUInt32 brick_id        = FfxBrixelizerMakeBrickID(brick_offset);
    FfxUInt32 eikonal_counter = LoadBricksEikonalCounters(brick_id);
    FfxUInt32 dim             = 8;
    FfxUInt32 voxel_id        = FfxBrixelizerLoadBrickVoxelID(brick_id);

    if (FfxBrixelizerIsValidID(voxel_id)) {
        FfxUInt32               voxel_idx         = FfxBrixelizerVoxelGetIndex(voxel_id);
        FfxUInt32               cascade_idx       = FfxBrixelizerGetVoxelCascade(voxel_id);
        FfxUInt32x3             clipmap_offset    = GetContextInfoCascadeClipmapOffset(cascade_idx);
        FfxUInt32x3             voxel_offset      = FfxBrixelizerUnflattenPOT(voxel_idx, FFX_BRIXELIZER_CASCADE_DEGREE);
        FfxUInt32               wrapped_voxel_idx = FfxBrixelizerFlattenPOT(FfxBrixelizerWrapCoords(FfxInt32x3(clipmap_offset), FFX_BRIXELIZER_CASCADE_WRAP_MASK, voxel_offset), FFX_BRIXELIZER_CASCADE_DEGREE);
        if (eikonal_counter < 16) {
            FfxBrixelizerAppendDirtyBrick(brick_id);
            StoreBricksEikonalCounters(brick_id, eikonal_counter + 16);
        }
    }
}

// DEFINE_ENTRY(512, 1, 1, Eikonal)
void FfxBrixelizerEikonal(FfxUInt32 local_coord_packed, FfxUInt32 brick_offset)
{
    FfxUInt32   brick_id     = LoadBricksDirtyList(brick_offset);
    FfxUInt32   voxel_id     = FfxBrixelizerLoadBrickVoxelID(brick_id);
    FfxUInt32x3 local_coord  = FfxBrixelizerUnflattenPOT(local_coord_packed, 3);
    if (FfxBrixelizerIsValidID(voxel_id)) {
        FfxFloat32 cell_distance = FfxFloat32(1.0) / (FfxFloat32(8));
        FfxFloat32 e             = FfxBrixelizerLoadBrixelDist(brick_id, FfxInt32x3(local_coord));

        FfxBrixelizerLDSStoreSDF(FfxInt32x3(local_coord), e);
        FFX_GROUP_MEMORY_BARRIER;
        for (FfxUInt32 j = FfxUInt32(0); j < FfxUInt32(3); j++) {
            for (FfxUInt32 i = FfxUInt32(0); i < FfxUInt32(4); i++) {
                FfxUInt32 d = FfxUInt32(1) << (FfxUInt32(3) - i);
                FfxFloat32x3 min_dists = FFX_BROADCAST_FLOAT32X3(FfxFloat32(1.0));
                min_dists.x      = FfxBrixelizerEikonalMin(FfxBrixelizerLDSLoadSDF(FfxInt32x3(local_coord) + FfxInt32x3(d, 0, 0)), FfxBrixelizerLDSLoadSDF(FfxInt32x3(local_coord) + FfxInt32x3(-d, 0, 0)));
                min_dists.y      = FfxBrixelizerEikonalMin(FfxBrixelizerLDSLoadSDF(FfxInt32x3(local_coord) + FfxInt32x3(0, d, 0)), FfxBrixelizerLDSLoadSDF(FfxInt32x3(local_coord) + FfxInt32x3(0, -d, 0)));
                min_dists.z      = FfxBrixelizerEikonalMin(FfxBrixelizerLDSLoadSDF(FfxInt32x3(local_coord) + FfxInt32x3(0, 0, d)), FfxBrixelizerLDSLoadSDF(FfxInt32x3(local_coord) + FfxInt32x3(0, 0, -d)));
                min_dists        = abs(min_dists);
                FfxFloat32 e10   = FfxBrixelizerEikonal1D(min_dists.x, min_dists.y, min_dists.z, d * cell_distance);
                FfxFloat32 e20   = FfxBrixelizerEikonal2D(min_dists.x, min_dists.y, d * cell_distance);
                FfxFloat32 e21   = FfxBrixelizerEikonal2D(min_dists.x, min_dists.z, d * cell_distance);
                FfxFloat32 e22   = FfxBrixelizerEikonal2D(min_dists.z, min_dists.y, d * cell_distance);
                FfxFloat32 e30   = FfxBrixelizerEikonal3D(min_dists.x, min_dists.y, min_dists.z, d * cell_distance);
                e                = FfxBrixelizerEikonalMin(e, FfxBrixelizerEikonalMin(e10, FfxBrixelizerEikonalMin(e20, FfxBrixelizerEikonalMin(e21, FfxBrixelizerEikonalMin(e22, e30)))));
                e                = FfxBrixelizerEikonalMin(e, e10);
                FfxBrixelizerLDSStoreSDF(FfxInt32x3(local_coord), e);
            }
        }

        FfxBrixelizerStoreBrixelDist(brick_id, local_coord, e);
    }
}

void FfxBrixelizerMergeBricks(FfxUInt32 gtid, FfxUInt32 group_id)
{
    FfxUInt32   cnt          = LoadContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_MERGE_BRICKS);
    FfxUInt32   merge_idx    = group_id / FfxUInt32(8);
    FfxUInt32   stamp_idx    = group_id % FfxUInt32(8);
    FfxUInt32   voxel_idx    = gtid + stamp_idx * FfxUInt32(64);
    FfxUInt32x3 voxel_offset = FfxBrixelizerUnflattenPOT(voxel_idx, FfxUInt32(3));
    if (merge_idx >= cnt) return;
    FfxUInt32  brick_A         = LoadBricksMergeList(merge_idx * FfxUInt32(2) + FfxUInt32(0));
    FfxUInt32  brick_B         = LoadBricksMergeList(merge_idx * FfxUInt32(2) + FfxUInt32(1));
    FfxUInt32x3 atlas_offset_A = FfxBrixelizerGetSDFAtlasOffset(brick_A);
    FfxUInt32x3 atlas_offset_B = FfxBrixelizerGetSDFAtlasOffset(brick_B);
    {
        FfxFloat32 sdf_val_A = LoadSDFAtlas(atlas_offset_A + voxel_offset);
        FfxFloat32 sdf_val_B = LoadSDFAtlas(atlas_offset_B + voxel_offset);
        StoreSDFAtlas(atlas_offset_B + voxel_offset, min(sdf_val_A, sdf_val_B));
    }
}

void FfxBrixelizerMergeCascades(FfxUInt32 voxel_idx)
{
    // Assume the same clipmap state so no need to wrap the coords
    FfxUInt32 brick_A = LoadCascadeBrickMapArrayUniform(GetBuildInfo().src_cascade_A, voxel_idx);
    FfxUInt32 brick_B = LoadCascadeBrickMapArrayUniform(GetBuildInfo().src_cascade_B, voxel_idx);
    if (FfxBrixelizerIsValidID(brick_A) && FfxBrixelizerIsValidID(brick_B)) {
        FfxUInt32 offset;
        IncrementContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_MERGE_BRICKS, FfxUInt32(1), /* out */ offset);
        StoreBricksMergeList(offset * FfxUInt32(2) + FfxUInt32(0), brick_A);
        StoreBricksMergeList(offset * FfxUInt32(2) + FfxUInt32(1), brick_B);
        StoreCascadeBrickMapArrayUniform(GetBuildInfo().dst_cascade, voxel_idx, brick_B); // we store the 2nd brick and merge into it

        FfxUInt32   brick_A_aabb_pack = LoadBricksAABB(FfxBrixelizerBrickGetIndex(brick_A));
        FfxUInt32x3 brick_A_aabb_umin = FfxBrixelizerUnflattenPOT(brick_A_aabb_pack & ((FfxUInt32(1) << FfxUInt32(9)) - FfxUInt32(1)), FfxUInt32(3));
        FfxUInt32x3 brick_A_aabb_umax = FfxBrixelizerUnflattenPOT((brick_A_aabb_pack >> FfxUInt32(9)) & ((FfxUInt32(1) << FfxUInt32(9)) - FfxUInt32(1)), FfxUInt32(3));

        FfxUInt32   brick_B_aabb_pack = LoadBricksAABB(FfxBrixelizerBrickGetIndex(brick_B));
        FfxUInt32x3 brick_B_aabb_umin = FfxBrixelizerUnflattenPOT(brick_B_aabb_pack & ((FfxUInt32(1) << FfxUInt32(9)) - FfxUInt32(1)), FfxUInt32(3));
        FfxUInt32x3 brick_B_aabb_umax = FfxBrixelizerUnflattenPOT((brick_B_aabb_pack >> FfxUInt32(9)) & ((FfxUInt32(1) << FfxUInt32(9)) - FfxUInt32(1)), FfxUInt32(3));

        brick_B_aabb_umin = min(brick_A_aabb_umin, brick_B_aabb_umin);
        brick_B_aabb_umax = max(brick_A_aabb_umax, brick_B_aabb_umax);
        brick_B_aabb_pack = FfxBrixelizerFlattenPOT(brick_B_aabb_umin, 3) | (FfxBrixelizerFlattenPOT(brick_B_aabb_umax, 3) << 9);

        StoreBricksAABB(FfxBrixelizerBrickGetIndex(brick_B), brick_B_aabb_pack);
    } else if (brick_B == FFX_BRIXELIZER_UNINITIALIZED_ID || brick_A == FFX_BRIXELIZER_UNINITIALIZED_ID) {
        StoreCascadeBrickMapArrayUniform(GetBuildInfo().dst_cascade, voxel_idx, FFX_BRIXELIZER_UNINITIALIZED_ID);
    } else if (FfxBrixelizerIsValidID(brick_B)) {
        StoreCascadeBrickMapArrayUniform(GetBuildInfo().dst_cascade, voxel_idx, brick_B);
    } else {
        StoreCascadeBrickMapArrayUniform(GetBuildInfo().dst_cascade, voxel_idx, brick_A);
    }
}

void FfxBrixelizerPrepareEikonalArgs()
{
    FfxUInt32 tier_cnt = LoadContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_DIRTY_BRICKS);
    StoreContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_DIRTY_BRICKS, 0);
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_EIKONAL_32 + 0, tier_cnt);
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_EIKONAL_32 + FfxUInt32(1), 1);
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_EIKONAL_32 + 2, 1);
}

void FfxBrixelizerPrepareMergeBricksArgs()
{
    FfxUInt32 cnt = LoadContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_MERGE_BRICKS);
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_MERGE_BRICKS_32 + FfxUInt32(0), cnt * FfxUInt32(8)); // 2^3 of 4^3 lanes
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_MERGE_BRICKS_32 + FfxUInt32(1), FfxUInt32(1));
    StoreIndirectArgs(FFX_BRIXELIZER_INDIRECT_OFFSETS_MERGE_BRICKS_32 + FfxUInt32(2), FfxUInt32(1));
}

#endif // ifndef FFX_BRIXELIZER_CONTEXT_OPS_H