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

// FSR1 pass 2 (optional RCAS)
// SRV  0 : FSR1_InternalUpscaled       : r_internal_upscaled_color
// UAV  0 : FSR1_UpscaledOutput         : rw_upscaled_output
// CB   0 : cbFSR1

#define FSR1_BIND_SRV_INTERNAL_UPSCALED_COLOR   0
#define FSR1_BIND_UAV_UPSCALED_OUTPUT           0
#define FSR1_BIND_CB_FSR1                       0

#include "fsr1/ffx_fsr1_callbacks_hlsl.h"
#include "fsr1/ffx_fsr1_rcas.h"

#ifndef FFX_FSR1_THREAD_GROUP_WIDTH
#define FFX_FSR1_THREAD_GROUP_WIDTH 64
#endif // #ifndef FFX_FSR1_THREAD_GROUP_WIDTH
#ifndef FFX_FSR1_THREAD_GROUP_HEIGHT
#define FFX_FSR1_THREAD_GROUP_HEIGHT 1
#endif // FFX_FSR1_THREAD_GROUP_HEIGHT
#ifndef FFX_FSR1_THREAD_GROUP_DEPTH
#define FFX_FSR1_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_FSR1_THREAD_GROUP_DEPTH
#ifndef FFX_FSR1_NUM_THREADS
#define FFX_FSR1_NUM_THREADS [numthreads(FFX_FSR1_THREAD_GROUP_WIDTH, FFX_FSR1_THREAD_GROUP_HEIGHT, FFX_FSR1_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_FSR1_NUM_THREADS

FFX_PREFER_WAVE64
FFX_FSR1_NUM_THREADS
FFX_FSR1_EMBED_ROOTSIG_CONTENT
void CS(uint3 LocalThreadId : SV_GroupThreadID, uint3 WorkGroupId : SV_GroupID, uint3 Dtid : SV_DispatchThreadID)
{
    RCAS(LocalThreadId, WorkGroupId, Dtid);
}
