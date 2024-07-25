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

#define FFX_FRAMEINTERPOLATION_BIND_SRV_GAME_MOTION_VECTOR_FIELD_X              0
#define FFX_FRAMEINTERPOLATION_BIND_SRV_GAME_MOTION_VECTOR_FIELD_Y              1

#define FFX_FRAMEINTERPOLATION_BIND_UAV_COUNTERS                                0
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_0             1
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_1             2
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_2             3
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_3             4
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_4             5
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_5             6
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_6             7
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_7             8
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_8             9
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_9             10
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_10            11
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_11            12
#define FFX_FRAMEINTERPOLATION_BIND_UAV_INPAINTING_PYRAMID_MIPMAP_12            13

#define FFX_FRAMEINTERPOLATION_BIND_CB_FRAMEINTERPOLATION                       0

#ifdef FFX_HALF
    #undef FFX_HALF
    #define FFX_HALF 0
#endif

#define FFX_FRAMEINTERPOLATION_BIND_CB_INPAINTING_PYRAMID                       1

#include "frameinterpolation/ffx_frameinterpolation_callbacks_hlsl.h"
#include "frameinterpolation/ffx_frameinterpolation_common.h"
#include "frameinterpolation/ffx_frameinterpolation_compute_game_vector_field_inpainting_pyramid.h"

#ifndef FFX_FRAMEINTERPOLATION_THREAD_GROUP_WIDTH
#define FFX_FRAMEINTERPOLATION_THREAD_GROUP_WIDTH 256
#endif // #ifndef FFX_FRAMEINTERPOLATION_THREAD_GROUP_WIDTH
#ifndef FFX_FRAMEINTERPOLATION_THREAD_GROUP_HEIGHT
#define FFX_FRAMEINTERPOLATION_THREAD_GROUP_HEIGHT 1
#endif // #ifndef FFX_FRAMEINTERPOLATION_THREAD_GROUP_HEIGHT
#ifndef FFX_FRAMEINTERPOLATION_THREAD_GROUP_DEPTH
#define FFX_FRAMEINTERPOLATION_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_FRAMEINTERPOLATION_THREAD_GROUP_DEPTH
#ifndef FFX_FRAMEINTERPOLATION_NUM_THREADS
#define FFX_FRAMEINTERPOLATION_NUM_THREADS [numthreads(FFX_FRAMEINTERPOLATION_THREAD_GROUP_WIDTH, FFX_FRAMEINTERPOLATION_THREAD_GROUP_HEIGHT, FFX_FRAMEINTERPOLATION_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_FRAMEINTERPOLATION_NUM_THREADS

FFX_FRAMEINTERPOLATION_NUM_THREADS
void CS(FfxUInt32x3 WorkGroupId : SV_GroupID, FfxUInt32 LocalThreadIndex : SV_GroupIndex)
{
    computeFrameinterpolationGameVectorFieldInpaintingPyramid(WorkGroupId, LocalThreadIndex);
}
