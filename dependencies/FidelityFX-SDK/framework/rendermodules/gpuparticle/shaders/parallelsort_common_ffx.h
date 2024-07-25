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

#if __cplusplus

#pragma once
#include "misc/assert.h"
struct SetupIndirectCB
{
    uint32_t maxThreadGroups;
    uint32_t shift;
};

#endif  // __cplusplus

#ifndef __cplusplus

#include "fidelityfx/ffx_core.h"

#if FFX_PARALLELSORT_OPTION_HAS_PAYLOAD
#define FFX_PARALLELSORT_COPY_VALUE 1
#endif  // FFX_PARALLELSORT_OPTION_HAS_PAYLOAD

#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif  // #if defined(FFX_PREFER_WAVE64)

#pragma warning(disable : 3205)  // conversion from larger type to smaller

#define FFX_DECLARE_SRV_REGISTER(regIndex)      t##regIndex
#define FFX_DECLARE_UAV_REGISTER(regIndex)      u##regIndex
#define FFX_DECLARE_CB_REGISTER(regIndex)       b##regIndex
#define FFX_PARALLELSORT_DECLARE_SRV(regIndex)  register(FFX_DECLARE_SRV_REGISTER(regIndex))
#define FFX_PARALLELSORT_DECLARE_UAV(regIndex)  register(FFX_DECLARE_UAV_REGISTER(regIndex))
#define FFX_PARALLELSORT_DECLARE_CB(regIndex)   register(FFX_DECLARE_CB_REGISTER(regIndex))

// Note that this MUST be kept in sync with sdk\include\FidelityFX\gpu\parallelsort\ffx_parallelsort_callbacks_hlsl.h
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
cbuffer cbParallelSort : FFX_PARALLELSORT_DECLARE_CB(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
#elif defined(FFX_PARALLELSORT_BIND_UAV_INDIRECT_CONSTANT_ARGS)
struct ParallelSortConstantData
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT) || defined(FFX_PARALLELSORT_BIND_UAV_INDIRECT_CONSTANT_ARGS)
{
    FfxUInt32 numKeys;                              ///< The number of keys to sort
    FfxInt32  numBlocksPerThreadGroup;              ///< How many blocks of keys each thread group needs to process
    FfxUInt32 numThreadGroups;                      ///< How many thread groups are being run concurrently for sort
    FfxUInt32 numThreadGroupsWithAdditionalBlocks;  ///< How many thread groups need to process additional block data
    FfxUInt32 numReduceThreadgroupPerBin;           ///< How many thread groups are summed together for each reduced bin entry
    FfxUInt32 numScanValues;                        ///< How many values to perform scan prefix (+ add) on
    FfxUInt32 shift;                                ///< What bits are being sorted (4 bit increments)
    FfxUInt32 padding;                              ///< Padding - unused
};
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT) || defined(FFX_PARALLELSORT_BIND_UAV_INDIRECT_CONSTANT_ARGS)

#if defined(FFX_PARALLELSORT_BIND_CB_SETUP_INDIRECT_ARGS)
cbuffer cbSetupIndirect : FFX_PARALLELSORT_DECLARE_CB(FFX_PARALLELSORT_BIND_CB_SETUP_INDIRECT_ARGS)
{
    FfxUInt32 maxThreadGroups;
    FfxUInt32 shift;
};
#endif  // #if defined(FFX_PARALLELSORT_BIND_CB_SETUP_INDIRECT_ARGS)

FfxUInt32 MaxThreadGroups()
{
#if defined(FFX_PARALLELSORT_BIND_CB_SETUP_INDIRECT_ARGS)
    return maxThreadGroups;
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_SETUP_INDIRECT_ARGS)
}

FfxUInt32 NumKeys()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return numKeys;

#else
    return 0;
#endif  // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxInt32 NumBlocksPerThreadGroup()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return numBlocksPerThreadGroup;
#else
    return 0;
#endif  // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxUInt32 NumThreadGroups()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return numThreadGroups;
#else
    return 0;
#endif  // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxUInt32 NumThreadGroupsWithAdditionalBlocks()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return numThreadGroupsWithAdditionalBlocks;
#else
    return 0;
#endif  // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxUInt32 NumReduceThreadgroupPerBin()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return numReduceThreadgroupPerBin;
#else
    return 0;
#endif  // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxUInt32 NumScanValues()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return numScanValues;
#else
    return 0;
#endif  // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

FfxUInt32 ShiftBit()
{
#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    return shift;
#elif defined(FFX_PARALLELSORT_BIND_CB_SETUP_INDIRECT_ARGS)
    return shift;
#else
    return 0;
#endif // #if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
}

// UAV declarations
#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)
RWStructuredBuffer<FfxUInt32> rw_source_keys : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)

#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)
RWStructuredBuffer<FfxUInt32> rw_dest_keys : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)

#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
RWStructuredBuffer<FfxUInt32> rw_source_payloads : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)

#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)
RWStructuredBuffer<FfxUInt32> rw_dest_payloads : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)

#if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
RWStructuredBuffer<FfxUInt32> rw_sum_table : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)

#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)
RWStructuredBuffer<FfxUInt32> rw_reduce_table : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)
RWStructuredBuffer<FfxUInt32> rw_scan_source : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)
RWStructuredBuffer<FfxUInt32> rw_scan_dest : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)
RWStructuredBuffer<FfxUInt32> rw_scan_scratch : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)

#if defined(FFX_PARALLELSORT_BIND_UAV_INDIRECT_CONSTANT_ARGS)
RWStructuredBuffer<ParallelSortConstantData> rw_indirect_constant_args : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_INDIRECT_CONSTANT_ARGS);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_INDIRECT_CONSTANT_ARGS)

#if defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)
RWStructuredBuffer<FfxUInt32> rw_count_scatter_args : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)

#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)
RWStructuredBuffer<FfxUInt32> rw_reduce_scan_args : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS);
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)


// SRV declarations
#if defined(FFX_PARALLELSORT_BIND_SRV_KEY_COUNT_ARGS)
StructuredBuffer<FfxUInt32> r_key_count_args : FFX_PARALLELSORT_DECLARE_SRV(FFX_PARALLELSORT_BIND_SRV_KEY_COUNT_ARGS);
#endif // #if defined(FFX_PARALLELSORT_BIND_SRV_KEY_COUNT_ARGS)


FfxUInt32 LoadSourceKey(FfxUInt32 index)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)
    return rw_source_keys[index];
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)
    return 0;
}

void StoreDestKey(FfxUInt32 index, FfxUInt32 value)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)
    rw_dest_keys[index] = value;
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)
}

FfxUInt32 LoadSourcePayload(FfxUInt32 index)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
    return rw_source_payloads[index];
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
    return 0;
}

void StoreDestPayload(FfxUInt32 index, FfxUInt32 value)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)
    rw_dest_payloads[index] = value;
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)
}

FfxUInt32 LoadSumTable(FfxUInt32 index)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
    return rw_sum_table[index];
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
    return 0;
}

void StoreSumTable(FfxUInt32 index, FfxUInt32 value)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
    rw_sum_table[index] = value;
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
}

void StoreReduceTable(FfxUInt32 index, FfxUInt32 value)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)
    rw_reduce_table[index] = value;
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)
}

FfxUInt32 LoadScanSource(FfxUInt32 index)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)
    return rw_scan_source[index];
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)
    return 0;
}

void StoreScanDest(FfxUInt32 index, FfxUInt32 value)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)
    rw_scan_dest[index] = value;
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)
}

FfxUInt32 LoadScanScratch(FfxUInt32 index)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)
    return rw_scan_scratch[index];
#endif  // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)
    return 0;
}

void StoreCountScatterArgs(FfxUInt32 x, FfxUInt32 y, FfxUInt32 z)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)
    rw_count_scatter_args[0] = x;
    rw_count_scatter_args[1] = y;
    rw_count_scatter_args[2] = z;
#endif  // defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)
}

void StoreReduceScanArgs(FfxUInt32 x, FfxUInt32 y, FfxUInt32 z)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)
    rw_reduce_scan_args[0] = x;
    rw_reduce_scan_args[1] = y;
    rw_reduce_scan_args[2] = z;
#endif  // defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)
}

FfxUInt32 ReadKeyCountArgs()
{
#if defined(FFX_PARALLELSORT_BIND_SRV_KEY_COUNT_ARGS)
    return r_key_count_args[0];
#endif  // #if defined(FFX_PARALLELSORT_BIND_SRV_KEY_COUNT_ARGS)
    return 0;
}

void StoreIndirectConstantData(FfxUInt32 x, FfxUInt32 numKeys, FfxInt32 numBlocksPerThreadGroup, FfxUInt32 numThreadGroups,
                               FfxUInt32 numThreadGroupsWithAdditionalBlocks, FfxUInt32 numReduceThreadgroupPerBin, FfxUInt32 numScanValues,
                               FfxUInt32 shift)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_INDIRECT_CONSTANT_ARGS)
    rw_indirect_constant_args[x].numKeys                                = numKeys;
    rw_indirect_constant_args[x].numBlocksPerThreadGroup                = numBlocksPerThreadGroup;
    rw_indirect_constant_args[x].numThreadGroups                        = numThreadGroups;
    rw_indirect_constant_args[x].numThreadGroupsWithAdditionalBlocks    = numThreadGroupsWithAdditionalBlocks;
    rw_indirect_constant_args[x].numReduceThreadgroupPerBin             = numReduceThreadgroupPerBin;
    rw_indirect_constant_args[x].numScanValues                          = numScanValues;
    rw_indirect_constant_args[x].shift                                  = shift;
#endif  // defined(FFX_PARALLELSORT_BIND_UAV_INDIRECT_CONSTANT_ARGS)
}

#include "fidelityfx/parallelsort/ffx_parallelsort_common.h"
#include "fidelityfx/parallelsort/ffx_parallelsort.h"

// Callbacks for individual parallelsort passes
void FfxParallelSortSetupIndirectArgs(FfxUInt32 LocalThreadId)
{
#if defined(FFX_PARALLELSORT_BIND_UAV_INDIRECT_CONSTANT_ARGS)
    // This pass works a little differrently then the one for standard parallel sort owing to the fact
    // that the data size is not fixed as it comes from spawned particles over time
    ParallelSortConstantData constData;
    constData.numKeys   = ReadKeyCountArgs();
    constData.shift     = ShiftBit();

    FfxUInt32 BlockSize = FFX_PARALLELSORT_ELEMENTS_PER_THREAD * FFX_PARALLELSORT_THREADGROUP_SIZE;
    FfxUInt32 NumBlocks = (ReadKeyCountArgs() + BlockSize - 1) / BlockSize;

    // Figure out data distribution
    FfxUInt32 NumThreadGroupsToRun  = MaxThreadGroups();
    FfxUInt32 BlocksPerThreadGroup  = (NumBlocks / NumThreadGroupsToRun);
    constData.numThreadGroupsWithAdditionalBlocks = NumBlocks % NumThreadGroupsToRun;

    if (NumBlocks < NumThreadGroupsToRun)
    {
        BlocksPerThreadGroup                            = 1;
        NumThreadGroupsToRun                            = NumBlocks;
        constData.numThreadGroupsWithAdditionalBlocks   = 0;
    }

    constData.numThreadGroups         = NumThreadGroupsToRun;
    constData.numBlocksPerThreadGroup = BlocksPerThreadGroup;

    // Calculate the number of thread groups to run for reduction (each thread group can process BlockSize number of entries)
    FfxUInt32 NumReducedThreadGroupsToRun =
        FFX_PARALLELSORT_SORT_BIN_COUNT * ((BlockSize > NumThreadGroupsToRun) ? 1 : (NumThreadGroupsToRun + BlockSize - 1) / BlockSize);
    constData.numReduceThreadgroupPerBin = NumReducedThreadGroupsToRun / FFX_PARALLELSORT_SORT_BIN_COUNT;
    constData.numScanValues = NumReducedThreadGroupsToRun;  // The number of reduce thread groups becomes our scan count (as each thread group writes out 1 value that needs scan prefix)

    // Setup indirect constant buffer
    StoreIndirectConstantData(0,
                              constData.numKeys,
                              constData.numBlocksPerThreadGroup,
                              constData.numThreadGroups,
                              constData.numThreadGroupsWithAdditionalBlocks,
                              constData.numReduceThreadgroupPerBin,
                              constData.numScanValues,
                              constData.shift);

    // Setup dispatch arguments
    StoreCountScatterArgs(NumThreadGroupsToRun, 1, 1);
    StoreReduceScanArgs(NumReducedThreadGroupsToRun, 1, 1);
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_INDIRECT_CONSTANT_ARGS)
}

void FfxParallelSortCount(FfxUInt32 LocalID, FfxUInt32 GroupID)
{
    ffxParallelSortCountUInt(LocalID, GroupID, ShiftBit());
}

void FfxParallelSortReduce(FfxUInt32 LocalID, FfxUInt32 GroupID)
{
    ffxParallelSortReduceCount(LocalID, GroupID);
}

void FfxParallelSortScan(FfxUInt32 LocalID, FfxUInt32 GroupID)
{
    FfxUInt32 BaseIndex = FFX_PARALLELSORT_ELEMENTS_PER_THREAD * FFX_PARALLELSORT_THREADGROUP_SIZE * GroupID;
    ffxParallelSortScanPrefix(NumScanValues(), LocalID, GroupID, 0, BaseIndex, false);
}

void FfxParallelSortScanAdd(FfxUInt32 LocalID, FfxUInt32 GroupID)
{
    // When doing adds, we need to access data differently because reduce
    // has a more specialized access pattern to match optimized count
    // Access needs to be done similarly to reduce
    // Figure out what bin data we are reducing
    FfxUInt32 BinID     = GroupID / NumReduceThreadgroupPerBin();
    FfxUInt32 BinOffset = BinID * NumThreadGroups();

    // Get the base index for this thread group
    FfxUInt32 BaseIndex = (GroupID % NumReduceThreadgroupPerBin()) * FFX_PARALLELSORT_ELEMENTS_PER_THREAD * FFX_PARALLELSORT_THREADGROUP_SIZE;

    ffxParallelSortScanPrefix(NumThreadGroups(), LocalID, GroupID, BinOffset, BaseIndex, true);
}

void FfxParallelSortScatter(FfxUInt32 LocalID, FfxUInt32 GroupID)
{
    ffxParallelSortScatterUInt(LocalID, GroupID, ShiftBit());
}

#endif  // !__cplusplus
