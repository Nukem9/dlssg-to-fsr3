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

#define DENOISER_SHADOWS_BIND_SRV_INPUT_HIT_MASK_RESULTS 0

#define DENOISER_SHADOWS_BIND_UAV_SHADOW_MASK 0

#define DENOISER_SHADOWS_BIND_CB0_DENOISER_SHADOWS 0

#include "denoiser/ffx_denoiser_shadows_callbacks_hlsl.h"
#include "denoiser/ffx_denoiser_shadows_prepare.h"

#ifndef FFX_DENOISER_SHADOWS_THREAD_GROUP_WIDTH
#define FFX_DENOISER_SHADOWS_THREAD_GROUP_WIDTH 8
#endif // #ifndef FFX_DENOISER_SHADOWS_THREAD_GROUP_WIDTH
#ifndef FFX_DENOISER_SHADOWS_THREAD_GROUP_HEIGHT
#define FFX_DENOISER_SHADOWS_THREAD_GROUP_HEIGHT 4
#endif // FFX_DENOISER_SHADOWS_THREAD_GROUP_HEIGHT
#ifndef FFX_DENOISER_SHADOWS_THREAD_GROUP_DEPTH
#define FFX_DENOISER_SHADOWS_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_DENOISER_SHADOWS_THREAD_GROUP_DEPTH
#ifndef FFX_DENOISER_SHADOWS_NUM_THREADS
#define FFX_DENOISER_SHADOWS_NUM_THREADS [numthreads(FFX_DENOISER_SHADOWS_THREAD_GROUP_WIDTH, FFX_DENOISER_SHADOWS_THREAD_GROUP_HEIGHT, FFX_DENOISER_SHADOWS_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_DENOISER_SHADOWS_NUM_THREADS

FFX_PREFER_WAVE64
FFX_DENOISER_SHADOWS_NUM_THREADS
FFX_DENOISER_SHADOWS_EMBED_PREPARE_SHADOW_MASK_ROOTSIG_CONTENT
void CS(uint2 gtid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    FFX_DNSR_Shadows_PrepareShadowMask(gtid, gid);
}
