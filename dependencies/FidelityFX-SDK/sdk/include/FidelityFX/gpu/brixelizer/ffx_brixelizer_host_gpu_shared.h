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

#ifndef FFX_BRIXELIZER_HOST_GPU_SHARED_H
#define FFX_BRIXELIZER_HOST_GPU_SHARED_H

#include "../ffx_core.h"

#define FFX_BRIXELIZER_MAX_CASCADES             24
#define FFX_BRIXELIZER_MAX_INSTANCES            (FfxUInt32(1) << FfxUInt32(16))
#define FFX_BRIXELIZER_CASCADE_RESOLUTION       64
#define FFX_BRIXELIZER_MAX_INSTANCES            (FfxUInt32(1) << FfxUInt32(16))
#define FFX_BRIXELIZER_INVALID_ID               0x00ffffffu
#define FFX_BRIXELIZER_UNINITIALIZED_ID         0xffffffffu
#define FFX_BRIXELIZER_INVALID_BOTTOM_AABB_NODE 0x7fff
#define FFX_BRIXELIZER_MAX_BRICKS_X8            (1 << 18)
#define FFX_BRIXELIZER_MAX_BRICKS               FFX_BRIXELIZER_MAX_BRICKS_X8
#define FFX_BRIXELIZER_BRICK_AABBS_STRIDE       (sizeof(FfxUInt32))
#define FFX_BRIXELIZER_BRICK_AABBS_SIZE         (FFX_BRIXELIZER_MAX_BRICKS_X8 * FFX_BRIXELIZER_BRICK_AABBS_STRIDE)
#define FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE   ((16 * 16 * 16) * sizeof(FfxUInt32) + (4 * 4 * 4 + 1) * sizeof(FfxFloat32x3) * 2)
#define FFX_BRIXELIZER_CASCADE_AABB_TREE_STRIDE (sizeof(FfxUInt32))
#define FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE   (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * sizeof(FfxUInt32))
#define FFX_BRIXELIZER_CASCADE_BRICK_MAP_STRIDE (sizeof(FfxUInt32))

#define FFX_BRIXELIZER_BRICK_ID_MASK 0xffffff

#define FFX_BRIXELIZER_CASCADE_DEGREE     FfxUInt32(6)
#define FFX_BRIXELIZER_CASCADE_WRAP_MASK  FfxUInt32(63)

#define FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE FfxUInt32(512)

#if defined(FFX_CPU)
    #define FFX_BRIXELIZER_CONST static const
#elif defined(FFX_HLSL)
    #define FFX_BRIXELIZER_CONST static const
#elif defined(FFX_GLSL)
    #define FFX_BRIXELIZER_CONST const
#else
    FFX_STATIC_ASSERT(0)
#endif

#ifdef FFX_CPU
    #define FFX_BRIXELIZER_BEGIN_STRUCT(name)        typedef struct name {
    #define FFX_BRIXELIZER_END_STRUCT(name)          } name;
    #define FFX_BRIXELIZER_BEGIN_ENUM(name)          typedef enum name {
    #define FFX_BRIXELIZER_END_ENUM(name)            } name;
    #define FFX_BRIXELIZER_ENUM_VALUE(name, value)   name = value,
#else
    #define FFX_BRIXELIZER_BEGIN_STRUCT(name)        struct name {
    #define FFX_BRIXELIZER_END_STRUCT(name)          };
    #define FFX_BRIXELIZER_BEGIN_ENUM(name)
    #define FFX_BRIXELIZER_END_ENUM(name)
    #define FFX_BRIXELIZER_ENUM_VALUE(name, value)   FFX_BRIXELIZER_CONST FfxUInt32 name = value;
#endif

/// An enumeration of flags which can be specified for different options when doing a cascade update.
///
/// @ingroup Brixelizer
FFX_BRIXELIZER_BEGIN_ENUM(FfxBrixelizerCascadeUpdateFlags)
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_CASCADE_UPDATE_FLAG_NONE,  FfxUInt32(0))                  ///< No flags.
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_CASCADE_UPDATE_FLAG_RESET, FfxUInt32(1) << FfxUInt32(0))  ///< Reset the cascade. This clears and frees all bricks currently in the cascade ready to rebuild the cascade completely.
FFX_BRIXELIZER_END_ENUM(FfxBrixelizerCascadeUpdateFlags)

/// An enumeration of the different possible debug outputs for the Brixelizer debug visualization.
///
/// @ingroup Brixelizer
FFX_BRIXELIZER_BEGIN_ENUM(FfxBrixelizerTraceDebugModes)
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_TRACE_DEBUG_MODE_DISTANCE,    0)    ///< Display a visualisation of the distance to hit, with closer hits in blue and further hits in green.
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_TRACE_DEBUG_MODE_UVW,         1)    ///< Display the UVW coordinates of hits.
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_TRACE_DEBUG_MODE_ITERATIONS,  2)    ///< Display a heatmap visualizing number of iterations in the scene.
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_TRACE_DEBUG_MODE_GRAD,        3)    ///< Display the normals at hits.
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_TRACE_DEBUG_MODE_BRICK_ID,    4)    ///< Display each brick in its own color.
    FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_TRACE_DEBUG_MODE_CASCADE_ID,  5)    ///< Display each cascade in itw own color.
FFX_BRIXELIZER_END_ENUM(FfxBrixelizerTraceDebugModes)

/// A structure of parameters describing a cascade. This structure is primarily for Brixelizer
/// internal use.
///
/// @ingroup Brixelizer
FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerCascadeInfo)
    FfxFloat32x3 grid_min;
    FfxFloat32   voxel_size;

    FfxFloat32x3 grid_max;
    FfxUInt32    flags;                              ///< the latest build_flags; see FfxBrixelizerCascadeUpdateFlags

    FfxUInt32x3  clipmap_offset;
    FfxUInt32    pad00;

    FfxInt32x3   clipmap_invalidation_offset;
    FfxUInt32    pad33;

    FfxInt32x3   ioffset;
    FfxUInt32    index;

    FfxFloat32x3 grid_mid;
    FfxUInt32    is_enabled;

    FfxUInt32x2  rel_grid_min_fp16;
    FfxUInt32x2  rel_grid_max_fp16;

    FfxUInt32    pad11;
    FfxUInt32    pad22;
    FfxFloat32   ivoxel_size;
    FfxUInt32    is_initialized;
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerCascadeInfo)

/// A structure of parameters describing the Brixelizer context. This structure is primarily
/// for Brixelizer internal use.
///
/// @ingroup Brixelizer
FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerContextInfo)
    FfxUInt32  num_bricks;
    FfxUInt32  frame_index;
    FfxFloat32 imesh_unit;
    FfxFloat32 mesh_unit;

    FfxBrixelizerCascadeInfo cascades[FFX_BRIXELIZER_MAX_CASCADES];
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerContextInfo)

// =============================================================================
// Debug/context counters
// =============================================================================
// Important to match the first a couple of counters with the FfxBrixelizerScratchCounters to share code
#define MEMBER_LIST \
    MEMBER(brickCount,        BRICK_COUNT,       0)    \
    MEMBER(dirtyBricks,       DIRTY_BRICKS,      1)    \
    MEMBER(freeBricks,        FREE_BRICKS,       2)    \
    MEMBER(clearBricks,       CLEAR_BRICKS,      3)    \
    MEMBER(mergeBricks,       MERGE_BRICKS,      4)    \
    MEMBER(numDebugAABBs,     NUM_DEBUG_AABBS,   5)

/// A structure containing all the counters used by the Brixelizer context. These can
/// be read back from the context after processing each update for analysis.
/// <c><i>brickCount</i></c> gives the total number of bricks allocated.
/// <c><i>dirtyBricks</i></c> gives the total number of bricks requiring an eikonal pass for completion.
/// <c><i>freeBricks</i></c> gives the total number of free bricks. This is the maximum number of bricks which can be allocated within a frame.
/// <c><i>clearBricks</i></c> gives the total number of bricks to be cleared in a frame. Bricks are cleared by having all distance values reset to 1.
/// <c><i>mergeBricks</i></c> gives the total number of bricks to be merged in a frame.
/// <c><i>numDebugAABBs</i></c> gives the total number of debug AABBs requested to be drawn in a debug visualization.
///
/// @ingroup Brixelizer
FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerDebugCounters)
#define MEMBER(camel_name, _upper_name, _index) FfxUInt32 camel_name;
    MEMBER_LIST
#undef MEMBER
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerDebugCounters)

FFX_BRIXELIZER_BEGIN_ENUM(FfxBrixelizerContextCounterIndex)
#define MEMBER(_camel_name, upper_name, index) FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_CONTEXT_COUNTER_##upper_name, index)
    MEMBER_LIST
#undef MEMBER
FFX_BRIXELIZER_END_ENUM(FfxBrixelizerContextCounterIndex)

#define MEMBER(_camel_name, _upper_name, _index) + 1
FFX_BRIXELIZER_CONST FfxUInt32 FFX_BRIXELIZER_NUM_CONTEXT_COUNTERS = 0 MEMBER_LIST;
#undef MEMBER
#undef MEMBER_LIST

// =============================================================================
// Scratch counters
// =============================================================================
#define MEMBER_LIST                                      \
    MEMBER(triangles,          TRIANGLES,            0)  \
    MEMBER(maxTriangles,       MAX_TRIANGLES,        1)  \
    MEMBER(references,         REFERENCES,           2)  \
    MEMBER(maxReferences,      MAX_REFERENCES,       3)  \
    MEMBER(groupIndex,         GROUP_INDEX,          4)  \
    MEMBER(compressionBricks,  COMPRESSION_BRICKS,   5)  \
    MEMBER(storageOffset,      STORAGE_OFFSET,       6)  \
    MEMBER(storageSize,        STORAGE_SIZE,         7)  \
    MEMBER(numBricksAllocated, NUM_BRICKS_ALLOCATED, 8)  \
    MEMBER(clearBricks,        CLEAR_BRICKS,         9)

/// A structure containing the counters used by the Brixelizer context for each cascade
/// update. This can be readback and used for analysis after each update. The following
/// members contain useful information for analysing Brixelizer resource usage.
///
/// <c><i>triangles</i></c> is used to store the total amount of storage space requested within the triangle buffer during an update. This is useful for determining a sensible value of <c><i>triangleSwapSize</i></c> in either <c><i>FfxBrixelizerRawCascadeUpdateDescription</i></c> or <c><i>FfxBrixelizerUpdateDescription</i></c>.
/// <c><i>references</i></c> is used to store the total number of reference allocations requested by Brixelizer during an update. This is useful for determining a sensible value of <c><i>maxReferences</i></c> in either <c><i>FfxBrixelizerRawCascadeUpdateDescription</i></c> or <c><i>FfxBrixelizerUpdateDescription</i></c>. 
/// <c><i>numBricksAllocated</i></c> is used to store the number of brick allocations requested in an update. This is useful for determining a sensible value of <c><i>maxBricksPerBake</i></c> in either <c><i>FfxBrixelizerRawCascadeUpdateDescription</i></c> or <c><i>FfxBrixelizerUpdateDescription</i></c>. 
///
/// The following counters are used internally by Brixelizer.
///
/// <c><i>maxTriangles</i></c> is used to store the storage size of the triangle buffer.
/// <c><i>maxReferences</i></c> is used to store the maxmimum number of references that can be stored.
/// <c><i>groupIndex</i></c> is used as a global atomic for wavefront synchronisation.
/// <c><i>compressionBricks</i></c> the number of bricks to compress (i.e. calculate AABBs for) this udpate.
/// <c><i>storageOffset</i></c> the next free position in the bricks scratch buffer.
/// <c><i>storageSize</i></c> the size of the bricks scratch buffer.
/// <c><i>clearBricks</i></c> the amount of bricks in the scratch buffer to initialize.
///
/// @ingroup Brixelizer
FFX_BRIXELIZER_BEGIN_STRUCT(FfxBrixelizerScratchCounters)
#define MEMBER(camel_name, _upper_name, _index) FfxUInt32 camel_name;
    MEMBER_LIST
#undef MEMBER
FFX_BRIXELIZER_END_STRUCT(FfxBrixelizerScratchCounters)

FFX_BRIXELIZER_BEGIN_ENUM(FfxBrixelizerScratchCounterIndex)
#define MEMBER(_camel_name, upper_name, index) FFX_BRIXELIZER_ENUM_VALUE(FFX_BRIXELIZER_SCRATCH_COUNTER_##upper_name, index)
    MEMBER_LIST
#undef MEMBER
FFX_BRIXELIZER_END_ENUM(FfxBrixelizerScratchCounterIndex)

#define MEMBER(_camel_name, _upper_name, _index) + 1
FFX_BRIXELIZER_CONST FfxUInt32 FFX_BRIXELIZER_NUM_SCRATCH_COUNTERS = 0 MEMBER_LIST;
#undef MEMBER
#undef MEMBER_LIST

#endif
