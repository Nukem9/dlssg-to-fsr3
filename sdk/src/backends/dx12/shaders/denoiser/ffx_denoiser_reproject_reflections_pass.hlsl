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

#define DENOISER_BIND_SRV_INPUT_DEPTH_HIERARCHY    2
#define DENOISER_BIND_SRV_INPUT_MOTION_VECTORS     3
#define DENOISER_BIND_SRV_INPUT_NORMAL             4
#define DENOISER_BIND_SRV_RADIANCE                 7
#define DENOISER_BIND_SRV_RADIANCE_HISTORY         8
#define DENOISER_BIND_SRV_VARIANCE                 9
#define DENOISER_BIND_SRV_SAMPLE_COUNT             10
#define DENOISER_BIND_SRV_EXTRACTED_ROUGHNESS      12
#define DENOISER_BIND_SRV_DEPTH_HISTORY            13
#define DENOISER_BIND_SRV_NORMAL_HISTORY           14
#define DENOISER_BIND_SRV_ROUGHNESS_HISTORY        15

#define DENOISER_BIND_UAV_VARIANCE                        1
#define DENOISER_BIND_UAV_SAMPLE_COUNT                    2
#define DENOISER_BIND_UAV_AVERAGE_RADIANCE                3
#define DENOISER_BIND_UAV_DENOISER_TILE_LIST              5
#define DENOISER_BIND_UAV_REPROJECTED_RADIANCE            9

#define DENOISER_BIND_CB_DENOISER   0

#include "denoiser/ffx_denoiser_reflections_callbacks_hlsl.h"
#include "denoiser/ffx_denoiser_reflections_reproject.h"

#ifndef FFX_DENOISER_THREAD_GROUP_WIDTH
#define FFX_DENOISER_THREAD_GROUP_WIDTH 8
#endif // #ifndef FFX_DENOISER_THREAD_GROUP_WIDTH
#ifndef FFX_DENOISER_THREAD_GROUP_HEIGHT
#define FFX_DENOISER_THREAD_GROUP_HEIGHT 8
#endif // FFX_DENOISER_THREAD_GROUP_HEIGHT
#ifndef FFX_DENOISER_THREAD_GROUP_DEPTH
#define FFX_DENOISER_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_DENOISER_THREAD_GROUP_DEPTH
#ifndef FFX_DENOISER_NUM_THREADS
#define FFX_DENOISER_NUM_THREADS [numthreads(FFX_DENOISER_THREAD_GROUP_WIDTH, FFX_DENOISER_THREAD_GROUP_HEIGHT, FFX_DENOISER_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_DENOISER_NUM_THREADS

FFX_PREFER_WAVE64
FFX_DENOISER_NUM_THREADS
FFX_DENOISER_EMBED_ROOTSIG_CONTENT
void CS(FfxUInt32 LocalThreadIndex : SV_GroupIndex, FfxUInt32 WorkGroupId : SV_GroupID, FfxInt32x2 GroupThreadId : SV_GroupThreadID )
{
    Reproject(LocalThreadIndex, WorkGroupId, GroupThreadId);
}
