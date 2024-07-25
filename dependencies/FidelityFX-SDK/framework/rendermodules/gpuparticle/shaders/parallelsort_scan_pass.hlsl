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

#define FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE           0
#define FFX_PARALLELSORT_BIND_UAV_SCAN_DEST             1

#define FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT          0

#define FFX_GPU
#define FFX_HLSL

#include "parallelsort_common_ffx.h"

#ifndef FFX_PARALLELSORT_THREAD_GROUP_WIDTH
#define FFX_PARALLELSORT_THREAD_GROUP_WIDTH FFX_PARALLELSORT_THREADGROUP_SIZE
#endif // #ifndef FFX_PARALLELSORT_THREAD_GROUP_WIDTH

#ifndef FFX_PARALLELSORT_THREAD_GROUP_HEIGHT
#define FFX_PARALLELSORT_THREAD_GROUP_HEIGHT 1
#endif // FFX_PARALLELSORT_THREAD_GROUP_HEIGHT

#ifndef FFX_PARALLELSORT_THREAD_GROUP_DEPTH
#define FFX_PARALLELSORT_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_PARALLELSORT_THREAD_GROUP_DEPTH

#ifndef FFX_PARALLELSORT_NUM_THREADS
#define FFX_PARALLELSORT_NUM_THREADS [numthreads(FFX_PARALLELSORT_THREAD_GROUP_WIDTH, FFX_PARALLELSORT_THREAD_GROUP_HEIGHT, FFX_PARALLELSORT_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_PARALLELSORT_NUM_THREADS

FFX_PARALLELSORT_NUM_THREADS
void CS(uint LocalID : SV_GroupThreadID, uint GroupID : SV_GroupID)
{
    FfxParallelSortScan(LocalID, GroupID);
}
