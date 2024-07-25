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

#define FFX_WAVE 1

#define SSSR_BIND_SRV_INPUT_COLOR              0
#define SSSR_BIND_SRV_INPUT_NORMAL             1
#define SSSR_BIND_SRV_INPUT_ENVIRONMENT_MAP    2
#define SSSR_BIND_SRV_DEPTH_HIERARCHY          3
#define SSSR_BIND_SRV_EXTRACTED_ROUGHNESS      4
#define SSSR_BIND_SRV_BLUE_NOISE_TEXTURE       5

#define SSSR_BIND_UAV_RADIANCE    0
#define SSSR_BIND_UAV_RAY_LIST    1
#define SSSR_BIND_UAV_RAY_COUNTER 2

#define SSSR_BIND_CB_SSSR   0

#include "sssr/ffx_sssr_callbacks_hlsl.h"
#include "sssr/ffx_sssr_intersect.h"

#ifndef FFX_SSSR_THREAD_GROUP_WIDTH
#define FFX_SSSR_THREAD_GROUP_WIDTH 8
#endif // #ifndef FFX_SSSR_THREAD_GROUP_WIDTH
#ifndef FFX_SSSR_THREAD_GROUP_HEIGHT
#define FFX_SSSR_THREAD_GROUP_HEIGHT 8
#endif // FFX_SSSR_THREAD_GROUP_HEIGHT
#ifndef FFX_SSSR_THREAD_GROUP_DEPTH
#define FFX_SSSR_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_SSSR_THREAD_GROUP_DEPTH
#ifndef FFX_SSSR_NUM_THREADS
#define FFX_SSSR_NUM_THREADS [numthreads(FFX_SSSR_THREAD_GROUP_WIDTH, FFX_SSSR_THREAD_GROUP_HEIGHT, FFX_SSSR_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_SSSR_NUM_THREADS

FFX_PREFER_WAVE64
FFX_SSSR_NUM_THREADS
FFX_SSSR_EMBED_ROOTSIG_CONTENT
void CS(FfxUInt32 LocalThreadIndex : SV_GroupIndex, FfxUInt32 WorkGroupId : SV_GroupID)
{
    Intersect(LocalThreadIndex, WorkGroupId);
}
