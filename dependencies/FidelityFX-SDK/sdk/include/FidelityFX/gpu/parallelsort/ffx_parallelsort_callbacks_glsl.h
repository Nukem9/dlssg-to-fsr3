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

#include "ffx_parallelsort_resources.h"

#if defined(FFX_GPU)
#include "ffx_core.h"

#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#pragma warning(disable: 3205)  // conversion from larger type to smaller

#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT, std140) uniform cbParallelSort_t
    {
        FfxUInt32 numKeys;
        FfxInt32  numBlocksPerThreadGroup;
        FfxUInt32 numThreadGroups;
        FfxUInt32 numThreadGroupsWithAdditionalBlocks;
        FfxUInt32 numReduceThreadgroupPerBin;
        FfxUInt32 numScanValues;
        FfxUInt32 shiftBit;
        FfxUInt32 padding;
    } cbParallelSort;
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
  
FfxUInt32 NumKeys()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return cbParallelSort.numKeys;
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxInt32 NumBlocksPerThreadGroup()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return cbParallelSort.numBlocksPerThreadGroup;
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxUInt32 NumThreadGroups()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return cbParallelSort.numThreadGroups;
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxUInt32 NumThreadGroupsWithAdditionalBlocks()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return cbParallelSort.numThreadGroupsWithAdditionalBlocks;
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)   
}

FfxUInt32 NumReduceThreadgroupPerBin()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return cbParallelSort.numReduceThreadgroupPerBin;
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxUInt32 NumScanValues()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return cbParallelSort.numScanValues;
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxUInt32 ShiftBit()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return cbParallelSort.shiftBit;
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}


#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS, std430)  coherent buffer ParallelSortSrcKeys_t { uint source_keys[]; } rw_source_keys;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)

#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_DEST_KEYS, std430)  coherent buffer ParallelSortDstKeys_t { uint dest_keys[]; } rw_dest_keys;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)

#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS, std430)  coherent buffer ParallelSortSrcPayload_t { uint source_payloads[]; } rw_source_payloads;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
    
#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS, std430)  coherent buffer ParallelSortDstPayload_t { uint dest_payloads[]; } rw_dest_payloads;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)

#if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_SUM_TABLE, std430)  coherent buffer ParallelSortSumTable_t { uint sum_table[]; } rw_sum_table;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)

#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE, std430)  coherent buffer ParallelSortReduceTable_t { uint reduce_table[]; } rw_reduce_table;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE, std430)  coherent buffer ParallelSortScanSrc_t { uint scan_source[]; } rw_scan_source;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_SCAN_DEST, std430)  coherent buffer ParallelSortScanDst_t { uint scan_dest[]; } rw_scan_dest;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH, std430)  coherent buffer ParallelSortScanScratch_t { uint scan_scratch[]; } rw_scan_scratch;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)

// UAV declarations
#if defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS, std430)  coherent buffer ParallelSortCountScatterArgs_t { uint count_scatter_args[]; } rw_count_scatter_args;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)

#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)
    layout(set = 0, binding = FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS, std430)  coherent buffer ParallelSortReduceScanArgs_t { uint reduce_scan_args[]; } rw_reduce_scan_args;
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)

#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)
    FfxUInt32 LoadSourceKey(FfxUInt32 index)
    {
        return rw_source_keys.source_keys[index];
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)

#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)
    void StoreDestKey(FfxUInt32 index, FfxUInt32 value)
    {
        rw_dest_keys.dest_keys[index] = value;
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)

#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
    FfxUInt32 LoadSourcePayload(FfxUInt32 index)
    {
        return rw_source_payloads.source_payloads[index];
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)

#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)
    void StoreDestPayload(FfxUInt32 index, FfxUInt32 value)
    {
        rw_dest_payloads.dest_payloads[index] = value;
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)

#if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
    FfxUInt32 LoadSumTable(FfxUInt32 index)
    {
        return rw_sum_table.sum_table[index];
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)

#if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
    void StoreSumTable(FfxUInt32 index, FfxUInt32 value)
    {
        rw_sum_table.sum_table[index] = value;
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)

#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)
    void StoreReduceTable(FfxUInt32 index, FfxUInt32 value)
    {
        rw_reduce_table.reduce_table[index] = value;
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)
    FfxUInt32 LoadScanSource(FfxUInt32 index)
    {
        return rw_scan_source.scan_source[index];
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)
    void StoreScanDest(FfxUInt32 index, FfxUInt32 value)
    {
        rw_scan_dest.scan_dest[index] = value;
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)
    FfxUInt32 LoadScanScratch(FfxUInt32 index)
    {
        return rw_scan_scratch.scan_scratch[index];
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)

#if defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)
    void StoreCountScatterArgs(FfxUInt32 x, FfxUInt32 y, FfxUInt32 z)
    {
        rw_count_scatter_args.count_scatter_args[0] = x;
        rw_count_scatter_args.count_scatter_args[1] = y;
        rw_count_scatter_args.count_scatter_args[2] = z;
    }
#endif // defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)

#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)
    void StoreReduceScanArgs(FfxUInt32 x, FfxUInt32 y, FfxUInt32 z)
    {
        rw_reduce_scan_args.reduce_scan_args[0] = x;
        rw_reduce_scan_args.reduce_scan_args[1] = y;
        rw_reduce_scan_args.reduce_scan_args[2] = z;
    }
#endif // defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)

#endif // #if defined(FFX_GPU)
