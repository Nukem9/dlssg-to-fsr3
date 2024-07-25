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

#define BRIXELIZER_BIND_SRV_CONTEXT_COUNTERS                 0
#define BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS        0

#define BRIXELIZER_BIND_SAMPLER_CLAMP_LINEAR_SAMPLER         0
#define BRIXELIZER_BIND_SAMPLER_CLAMP_NEAREST_SAMPLER        1
#define BRIXELIZER_BIND_SAMPLER_WRAP_LINEAR_SAMPLER          2
#define BRIXELIZER_BIND_SAMPLER_WRAP_NEAREST_SAMPLER         3

#include "ffx_brixelizergi_callbacks_hlsl.h"
#include "ffx_brixelizergi_radiance_cache_update.h"

#ifndef FFX_BRIXELIZER_GI_THREAD_GROUP_WIDTH
#define FFX_BRIXELIZER_GI_THREAD_GROUP_WIDTH 1
#endif  // #ifndef FFX_BRIXELIZER_GI_THREAD_GROUP_WIDTH
#ifndef FFX_BRIXELIZER_GI_THREAD_GROUP_HEIGHT
#define FFX_BRIXELIZER_GI_THREAD_GROUP_HEIGHT 1
#endif  // FFX_BRIXELIZER_GI_THREAD_GROUP_HEIGHT
#ifndef FFX_BRIXELIZER_GI_THREAD_GROUP_DEPTH
#define FFX_BRIXELIZER_GI_THREAD_GROUP_DEPTH 1
#endif  // #ifndef FFX_BRIXELIZER_GI_THREAD_GROUP_DEPTH
#ifndef FFX_BRIXELIZER_GI_NUM_THREADS
#define FFX_BRIXELIZER_GI_NUM_THREADS [numthreads(FFX_BRIXELIZER_GI_THREAD_GROUP_WIDTH, FFX_BRIXELIZER_GI_THREAD_GROUP_HEIGHT, FFX_BRIXELIZER_GI_THREAD_GROUP_DEPTH)]
#endif  // #ifndef FFX_BRIXELIZER_GI_NUM_THREADS

FFX_PREFER_WAVE64
FFX_BRIXELIZER_GI_NUM_THREADS
void CS(uint3 tid : SV_DispatchThreadID)
{
	FfxBrixelizerGIPrepareClearCache(tid);
}