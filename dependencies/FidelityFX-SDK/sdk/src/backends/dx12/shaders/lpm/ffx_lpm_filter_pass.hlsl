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

// LPM pass 1
// SRV  0 : LPM_InputColor  : r_input_color
// UAV  0 : LPM_OutputColor : rw_output_color
// CB   0 : cbLPM

#define LPM_BIND_SRV_INPUT_COLOR               0
#define LPM_BIND_UAV_OUTPUT_COLOR              0

#define LPM_BIND_CB_LPM                        0

#include "lpm/ffx_lpm_callbacks_hlsl.h"
#include "lpm/ffx_lpm_filter.h"

#ifndef FFX_LPM_THREAD_GROUP_WIDTH
#define FFX_LPM_THREAD_GROUP_WIDTH 64
#endif // #ifndef FFX_LPM_THREAD_GROUP_WIDTH
#ifndef FFX_LPM_THREAD_GROUP_HEIGHT
#define FFX_LPM_THREAD_GROUP_HEIGHT 1
#endif // FFX_LPM_THREAD_GROUP_HEIGHT
#ifndef FFX_LPM_THREAD_GROUP_DEPTH
#define FFX_LPM_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_LPM_THREAD_GROUP_DEPTH
#ifndef FFX_LPM_NUM_THREADS
#define FFX_LPM_NUM_THREADS [numthreads(FFX_LPM_THREAD_GROUP_WIDTH, FFX_LPM_THREAD_GROUP_HEIGHT, FFX_LPM_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_LPM_NUM_THREADS

FFX_PREFER_WAVE64
FFX_LPM_NUM_THREADS
FFX_LPM_EMBED_ROOTSIG_CONTENT
void CS(uint3 LocalThreadId : SV_GroupThreadID, uint3 WorkGroupId : SV_GroupID, uint3 Dtid : SV_DispatchThreadID)
{
    LPMFilter(LocalThreadId, WorkGroupId, Dtid);
}
