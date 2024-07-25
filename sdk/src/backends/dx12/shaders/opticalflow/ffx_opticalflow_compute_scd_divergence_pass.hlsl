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

#define FFX_OPTICALFLOW_BIND_UAV_OPTICAL_FLOW_SCD_HISTOGRAM            0
#define FFX_OPTICALFLOW_BIND_UAV_OPTICAL_FLOW_SCD_PREVIOUS_HISTOGRAM   1
#define FFX_OPTICALFLOW_BIND_UAV_OPTICAL_FLOW_SCD_TEMP                 2
#define FFX_OPTICALFLOW_BIND_UAV_OPTICAL_FLOW_SCD_OUTPUT               3

#define FFX_OPTICALFLOW_BIND_CB_COMMON                                   0

#include "opticalflow/ffx_opticalflow_callbacks_hlsl.h"
#include "opticalflow/ffx_opticalflow_compute_scd_divergence.h"

#ifndef FFX_OPTICALFLOW_THREAD_GROUP_WIDTH
#define FFX_OPTICALFLOW_THREAD_GROUP_WIDTH 256
#endif // #ifndef FFX_OPTICALFLOW_THREAD_GROUP_WIDTH
#ifndef FFX_OPTICALFLOW_THREAD_GROUP_HEIGHT
#define FFX_OPTICALFLOW_THREAD_GROUP_HEIGHT 1
#endif // #ifndef FFX_OPTICALFLOW_THREAD_GROUP_HEIGHT
#ifndef FFX_OPTICALFLOW_THREAD_GROUP_DEPTH
#define FFX_OPTICALFLOW_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_OPTICALFLOW_THREAD_GROUP_DEPTH
#ifndef FFX_OPTICALFLOW_NUM_THREADS
#define FFX_OPTICALFLOW_NUM_THREADS [numthreads(FFX_OPTICALFLOW_THREAD_GROUP_WIDTH, FFX_OPTICALFLOW_THREAD_GROUP_HEIGHT, FFX_OPTICALFLOW_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_OPTICALFLOW_NUM_THREADS

FFX_OPTICALFLOW_NUM_THREADS
FFX_OPTICALFLOW_EMBED_ROOTSIG_CONTENT
FFX_PREFER_WAVE64
void CS(int3 iGlobalId      : SV_DispatchThreadID,
        int2 iLocalId       : SV_GroupThreadID,
        int2 iGroupId       : SV_GroupID,
        int  iLocalIndex    : SV_GroupIndex)
{
    int2 iGroupSize = int2(FFX_OPTICALFLOW_THREAD_GROUP_WIDTH, FFX_OPTICALFLOW_THREAD_GROUP_HEIGHT);
    ComputeSCDHistogramsDivergence(iGlobalId, iLocalId, iLocalIndex, iGroupId, iGroupSize);
}
