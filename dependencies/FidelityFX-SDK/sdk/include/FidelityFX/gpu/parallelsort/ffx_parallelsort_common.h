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

FfxUInt32 FfxNumBlocksPerThreadGroup()
{
    return NumBlocksPerThreadGroup();
}

FfxUInt32 FfxNumThreadGroups()
{
    return NumThreadGroups();
}

FfxUInt32 FfxNumThreadGroupsWithAdditionalBlocks()
{
    return NumThreadGroupsWithAdditionalBlocks();
}

FfxUInt32 FfxNumReduceThreadgroupPerBin()
{
    return NumReduceThreadgroupPerBin();
}

FfxUInt32 FfxNumKeys()
{
    return NumKeys();
}

FfxUInt32 FfxLoadKey(FfxUInt32 index)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)
    return LoadSourceKey(index);
#else 
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)
}

void FfxStoreKey(FfxUInt32 index, FfxUInt32 value)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)
    StoreDestKey(index, value);
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)
}

FfxUInt32 FfxLoadPayload(FfxUInt32 index)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
    return LoadSourcePayload(index);
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
}

void FfxStorePayload(FfxUInt32 index, FfxUInt32 value)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)
    StoreDestPayload(index, value);
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)
}

FfxUInt32 FfxLoadSum(FfxUInt32 index)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
    return LoadSumTable(index);
#else 
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
}

void FfxStoreSum(FfxUInt32 index, FfxUInt32 value)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
    StoreSumTable(index, value);
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
}

void FfxStoreReduce(FfxUInt32 index, FfxUInt32 value)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)
    StoreReduceTable(index, value);
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)
}

FfxUInt32 FfxLoadScanSource(FfxUInt32 index)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)
    return LoadScanSource(index);
#else 
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)
}

void FfxStoreScanDest(FfxUInt32 index, FfxUInt32 value)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)
    StoreScanDest(index, value);
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)
}

FfxUInt32 FfxLoadScanScratch(FfxUInt32 index)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)
    return LoadScanScratch(index);
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)
}
