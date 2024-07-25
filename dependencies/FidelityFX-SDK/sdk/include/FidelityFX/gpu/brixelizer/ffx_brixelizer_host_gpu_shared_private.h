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

#ifndef FFX_BRIXELIZER_HOST_GPU_SHARED_PRIVATE_H
#define FFX_BRIXELIZER_HOST_GPU_SHARED_PRIVATE_H

#include "../ffx_core.h"
#include "ffx_brixelizer_host_gpu_shared.h"

FFX_BRIXELIZER_BEGIN_ENUM(FfxBrixelizerInstanceFlagsInternal) 
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_INSTANCE_FLAG_USE_U16_INDEX,             (FfxUInt32(1) << FfxUInt32(0)))
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_INSTANCE_FLAG_USE_RGBA16_VERTEX,         (FfxUInt32(1) << FfxUInt32(2)))
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_INSTANCE_FLAG_USE_INDEXLESS_QUAD_LIST,   (FfxUInt32(1) << FfxUInt32(3)))
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_INSTANCE_FLAG_UV_FORMAT_RG16_UNORM,      (FfxUInt32(1) << FfxUInt32(4)))
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_INSTANCE_FLAG_UV_FORMAT_RG32_FLOAT,      (FfxUInt32(1) << FfxUInt32(5)))
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_INSTANCE_FLAG_COLOR_FORMAT_RGBA32_FLOAT, (FfxUInt32(1) << FfxUInt32(6)))
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_INSTANCE_FLAG_COLOR_FORMAT_RGBA8_UNORM,  (FfxUInt32(1) << FfxUInt32(7)))
FFX_BRIXELIZER_END_ENUM(FfxBrixelizerInstanceFlagsInternal)

#define FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE          FfxUInt32(256)
#define FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE    FfxUInt32(256)
#define FFX_BRIXELIZER_STATIC_CONFIG_MAX_VERTEX_BUFFERS            FfxUInt32(8192)
#define FFX_BRIXELIZER_STATIC_CONFIG_COMPACT_REFERENCES_GROUP_SIZE FfxUInt32(64)
#define FFX_BRIXELIZER_STATIC_CONFIG_EMIT_SDF_GROUP_SIZE           FfxUInt32(32)
#define FFX_BRIXELIZER_STATIC_CONFIG_EMIT_SDF_REFS_PER_GROUP       FFX_BRIXELIZER_STATIC_CONFIG_EMIT_SDF_GROUP_SIZE
#define FFX_BRIXELIZER_STATIC_CONFIG_INDIRECT_DISPATCH_STRIDE32    FfxUInt32(4)
#define FFX_BRIXELIZER_STATIC_CONFIG_INDIRECT_DISPATCH_STRIDE      FfxUInt32(16)

// index deliberately starts at 1
#define MEMBER_LIST                                                                                                 \
    MEMBER(CompactReferences,               COMPACT_REFERENCES,                   1)                                \
    MEMBER(EmitSDF,                         EMIT_SDF,                             2)                                \
    MEMBER(Voxelize,                        VOXELIZE,                             3)                                \
    MEMBER(Compress,                        COMPRESS,                             4)                                \
    MEMBER(Eikonal,                         EIKONAL,                              5)                                \
    MEMBER(ClearBricks,                     CLEAR_BRICKS,                         6)                                \
    MEMBER(MergeBricks,                     MERGE_BRICKS,                         7)                                \
    MEMBER(InstanceOpsTrianglePass1,        INSTANCE_OPS_TRIANGLE_PASS_1,         8)                                \
    MEMBER(InstanceOpsDeferredReleaseBrits, INSTANCE_OPS_DEFERRED_RELEASE_BRITS,  9)                                \
    MEMBER(ClearBrits,                      CLEAR_BRITS,                         10)

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerIndirectOffsets)
        FfxUInt32x4 pad;
#define MEMBER(pascal_name, _upper_name, _index) FfxUInt32x4 pascal_name;
        MEMBER_LIST
#undef MEMBER
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerIndirectOffsets)

#define MEMBER(_pascal_name, _upper_name, _index) + 1
FFX_BRIXELIZER_CONST FfxUInt32 FFX_BRIXELIZER_NUM_INDIRECT_OFFSETS = 1 MEMBER_LIST;
#undef MEMBER

FFX_BRIXELIZER_BEGIN_ENUM(FfxBrixelizerIndirectOffsetsEnum)
#define MEMBER(_pascal_name, upper_name, index) FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_INDIRECT_OFFSETS_##upper_name, index * FFX_BRIXELIZER_STATIC_CONFIG_INDIRECT_DISPATCH_STRIDE)
    MEMBER_LIST
#undef MEMBER
#define MEMBER(_pascal_name, upper_name, index) FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_INDIRECT_OFFSETS_##upper_name##_32, index * FFX_BRIXELIZER_STATIC_CONFIG_INDIRECT_DISPATCH_STRIDE32)
    MEMBER_LIST
#undef MEMBER
FFX_BRIXELIZER_END_ENUM(FfxBrixelizerIndirectOffsetsEnum)

#undef MEMBER_LIST

FFX_BRIXELIZER_BEGIN_ENUM(FfxBrixelizerJobFlagsInternal)
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_JOB_FLAG_NONE,       FfxUInt32(0))
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_JOB_FLAG_INVALIDATE, (FfxUInt32(1) << FfxUInt32(0)))
FFX_BRIXELIZER_END_ENUM(FfxBrixelizerJobFlagsInternal)

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerBasicMeshInfo)
    FfxUInt32 flags;
    FfxUInt32 indexBufferID;
    FfxUInt32 indexBufferOffset;
    FfxUInt32 vertexBufferID;
    FfxUInt32 vertexBufferOffset;
    FfxUInt32 vertexCount;
    FfxUInt32 vertexStride;
    FfxUInt32 triangleCount;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerBasicMeshInfo)

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerInstanceInfo)
    FfxFloat32x3 aabbMin; // voxel aligned
    FfxUInt32    pack0;   // | 16: vertex buffer id |  16: index buffer id |

    FfxFloat32x3 aabbMax; // voxel aligned
    FfxUInt32    pack1;   // ! 6: vertex stride | 10: flags | | |

    FfxUInt32    indexBufferOffset;
    FfxUInt32    vertexBufferOffset;
    FfxUInt32    vertexCount;
    FfxUInt32    triangleCount;

    FfxUInt32x3 padding;
    FfxUInt32   index;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerInstanceInfo)

#ifdef FFX_GPU
FfxBrixelizerBasicMeshInfo ffxBrixelizerInstanceInfoGetMeshInfo(FFX_PARAMETER_IN FfxBrixelizerInstanceInfo instance_info)
{
    FfxBrixelizerBasicMeshInfo info;
    info.flags              = (instance_info.pack1 >> 16) & FfxUInt32(0x1ff);
    info.triangleCount      = instance_info.triangleCount;
    info.indexBufferID      = (instance_info.pack0 >> 0) & FfxUInt32(0xffff);
    info.indexBufferOffset  = instance_info.indexBufferOffset;
    info.vertexBufferID     = (instance_info.pack0 >> 16) & FfxUInt32(0xffff);
    info.vertexBufferOffset = instance_info.vertexBufferOffset;
    info.vertexStride       = (instance_info.pack1 >> 26) & 0x3f;
    return info;
}
#endif

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerBrixelizationJob)
    FfxUInt32x3 aabbMin; // in local cascade voxel size
    FfxUInt32   instanceIdx;

    FfxUInt32x3 aabbMax; // in local cascade voxel size
    FfxUInt32   flags;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerBrixelizationJob)

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerIndexRef)
    FfxUInt32 surface_id;
    FfxUInt32 instance_id;
    FfxUInt32 triangle_index;
    FfxUInt32 pad0;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerIndexRef)

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerTriangleReference)
    FfxUInt32 voxel_idx;
    FfxUInt32 triangle_id;
    FfxUInt32 local_ref_idx;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerTriangleReference)

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerInstanceReference)
    FfxUInt32 voxel_idx;
    FfxUInt32 instance_id;
    FfxUInt32 local_ref_idx;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerInstanceReference)

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerTriangle)
    FfxUInt32   job_idx;
    FfxUInt32   triangle_index;
    FfxUInt32x3 face3;

    FfxFloat32x3 wp0;
    FfxFloat32x3 wp1;
    FfxFloat32x3 wp2;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerTriangle)

#define FFX_BRIXELIZER_TRIANGLE_SMALL_FLAG (FfxUInt32(1) << FfxUInt32(31))
#define FFX_BRIXELIZER_TRIANGLE_OFFSET_MASK (0x7fffffffu)

#define FFX_BRIXELIZER_INVALID_ALLOCATION 0x00ffffffu
#define FFX_BRIXELIZER_VOXEL_ID_MASK 0xffffff
#define FFX_BRIXELIZER_CASCADE_ID_SHIFT 24

#ifdef FFX_GPU
FfxBoolean FfxBrixelizerTriangleIsSmall(FfxUInt32 id)     { return ((id & FFX_BRIXELIZER_TRIANGLE_SMALL_FLAG) != 0); }
FfxUInt32  FfxBrixelizerTriangleIDGetOffset(FfxUInt32 id) { return id & FFX_BRIXELIZER_TRIANGLE_OFFSET_MASK; }
FfxBoolean FfxBrixelizerIsInvalidID(FfxUInt32 id)         { return (id & FFX_BRIXELIZER_INVALID_ID) == FFX_BRIXELIZER_INVALID_ID; }
#endif

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerBuildInfo)
    FfxInt32     tree_iteration;
    FfxUInt32    max_bricks_per_bake;
    FfxUInt32    num_jobs;
    FfxUInt32    cascade_index;

    FfxUInt32    cascade_shift; // [1, 2, 4] for 1x, 2x and 4x
    FfxUInt32    is_dynamic;
    FfxUInt32    do_initialization;
    FfxUInt32    num_job_voxels;

    FfxUInt32    src_cascade_A;
    FfxUInt32    src_cascade_B;
    FfxUInt32    dst_cascade;
    FfxUInt32    merge_instance_idx;

    FfxUInt32x3  _padding;
    FfxUInt32    build_flags;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerBuildInfo)

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerDebugAABB)
    FfxFloat32x3 color;
    FfxFloat32x3 aabbMin;
    FfxFloat32x3 aabbMax;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerDebugAABB)

FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerDebugInfo)
    FfxFloat32x4x4       inv_view; ///< Stored in row-major order.

    FfxFloat32x4x4       inv_proj; ///< Stored in row-major order.

    FfxFloat32           t_min;
    FfxFloat32           t_max;
    FfxFloat32           preview_sdf_solve_eps;
    FfxUInt32            start_cascade_idx;

    FfxUInt32            end_cascade_idx;
    FfxUInt32            debug_state;
    FfxUInt32            max_aabbs;
    FfxUInt32            _padding;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerDebugInfo)

#endif // FFX_BRIXELIZER_HOST_GPU_SHARED_PRIVATE_H