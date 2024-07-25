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

#ifndef DECLARATIONS_H
#define DECLARATIONS_H

#ifndef MAX_TEXTURES_COUNT
#define MAX_TEXTURES_COUNT            1000
#endif // !MAX_TEXTURES_COUNT

#define MAX_SHADOW_MAP_TEXTURES_COUNT 15

#define RAYTRACING_INFO_BEGIN_SLOT 20
#define RAYTRACING_INFO_MATERIAL   20
#define RAYTRACING_INFO_INSTANCE   21
#define RAYTRACING_INFO_SURFACE_ID 22
#define RAYTRACING_INFO_SURFACE    23

#define TEXTURE_BEGIN_SLOT    50
#define SHADOW_MAP_BEGIN_SLOT 25
#define SAMPLER_BEGIN_SLOT    10

#define INDEX_BUFFER_BEGIN_SLOT  1050
#define VERTEX_BUFFER_BEGIN_SLOT 21050

#define MAX_BUFFER_COUNT 20000

#define DECLARE_SRV_REGISTER(regIndex)     t##regIndex
#define DECLARE_SAMPLER_REGISTER(regIndex) s##regIndex

#define DECLARE_SRV(regIndex)     register(DECLARE_SRV_REGISTER(regIndex))
#define DECLARE_SAMPLER(regIndex) register(DECLARE_SAMPLER_REGISTER(regIndex))

#define SURFACE_INFO_INDEX_TYPE_U32 0
#define SURFACE_INFO_INDEX_TYPE_U16 1

#define RAY_COUNTER_SW_OFFSET 0
#define RAY_COUNTER_SW_HISTORY_OFFSET 4
#define RAY_COUNTER_DENOISE_OFFSET 8
#define RAY_COUNTER_DENOISE_HISTORY_OFFSET 12
#define RAY_COUNTER_HW_OFFSET 16
#define RAY_COUNTER_HW_HISTORY_OFFSET 20

#define INDIRECT_ARGS_SW_OFFSET 0
#define INDIRECT_ARGS_DENOISE_OFFSET 12
#define INDIRECT_ARGS_APPLY_OFFSET 24
#define INDIRECT_ARGS_HW_OFFSET 36


// Use hitcounter feedback
#define HSR_FLAGS_USE_HIT_COUNTER (1 << 0)
// Traverse in screen space
#define HSR_FLAGS_USE_SCREEN_SPACE (1 << 1)
// Traverse using HW ray tracing
#define HSR_FLAGS_USE_RAY_TRACING (1 << 2)
// Iterate BVH to search for the opaque fragment
#define HSR_FLAGS_RESOLVE_TRANSPARENT (1 << 3)
// Grab radiance from screen space shaded image for ray traced intersections, when possible
#define HSR_FLAGS_SHADING_USE_SCREEN (1 << 5)
// defines HSR_SHADING_USE_SCREEN

// Extra flags for debugging
#define HSR_FLAGS_FLAG_0 (1 << 9)
#define HSR_FLAGS_FLAG_1 (1 << 10)
#define HSR_FLAGS_FLAG_2 (1 << 11)
#define HSR_FLAGS_FLAG_3 (1 << 12)

// Visualization tweaking
#define HSR_FLAGS_SHOW_DEBUG_TARGET (1 << 13)
#define HSR_FLAGS_SHOW_INTERSECTION (1 << 14)
#define HSR_FLAGS_SHOW_REFLECTION_TARGET (1 << 15)
#define HSR_FLAGS_APPLY_REFLECTIONS (1 << 16)
#define HSR_FLAGS_INTERSECTION_ACCUMULATE (1 << 17)

#define HSR_FLAGS_VISUALIZE_WAVES (1 << 18)
#define HSR_FLAGS_VISUALIZE_AVG_RADIANCE (1 << 19)
#define HSR_FLAGS_VISUALIZE_VARIANCE (1 << 20)
#define HSR_FLAGS_VISUALIZE_NUM_SAMPLES (1 << 21)
#define HSR_FLAGS_VISUALIZE_RAY_LENGTH (1 << 23)
#define HSR_FLAGS_VISUALIZE_REPROJECTION (1 << 25)
#define HSR_FLAGS_VISUALIZE_TRANSPARENT_QUERY (1 << 26)
#define HSR_FLAGS_VISUALIZE_HIT_COUNTER (1 << 27)
#define HSR_FLAGS_VISUALIZE_PRIMARY_RAYS (1 << 28)

#endif
