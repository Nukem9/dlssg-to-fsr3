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

#include "ffx_core.h"

#if FFX_PARALLELSORT_OPTION_HAS_PAYLOAD
    #define FFX_PARALLELSORT_COPY_VALUE 1
#endif // FFX_PARALLELSORT_OPTION_HAS_PAYLOAD

#include "parallelsort/ffx_parallelsort.h"

void FfxParallelSortScanAdd(FfxUInt32 LocalID, FfxUInt32 GroupID)
{
    // When doing adds, we need to access data differently because reduce 
    // has a more specialized access pattern to match optimized count
    // Access needs to be done similarly to reduce
    // Figure out what bin data we are reducing
    FfxUInt32 BinID = GroupID / NumReduceThreadgroupPerBin();
    FfxUInt32 BinOffset = BinID * NumThreadGroups();

    // Get the base index for this thread group
    FfxUInt32 BaseIndex = (GroupID % NumReduceThreadgroupPerBin()) * FFX_PARALLELSORT_ELEMENTS_PER_THREAD * FFX_PARALLELSORT_THREADGROUP_SIZE;
    
    ffxParallelSortScanPrefix(NumThreadGroups(), LocalID, GroupID, BinOffset, BaseIndex, true);
}
