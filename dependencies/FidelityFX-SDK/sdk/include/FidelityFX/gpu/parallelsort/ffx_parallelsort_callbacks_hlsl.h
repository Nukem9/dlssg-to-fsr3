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
#ifdef __hlsl_dx_compiler
#pragma dxc diagnostic push
#pragma dxc diagnostic ignored "-Wambig-lit-shift"
#endif //__hlsl_dx_compiler
#include "ffx_core.h"
#ifdef __hlsl_dx_compiler
#pragma dxc diagnostic pop
#endif //__hlsl_dx_compiler

#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#pragma warning(disable: 3205)  // conversion from larger type to smaller

#define FFX_DECLARE_UAV_REGISTER(regIndex)  u##regIndex
#define FFX_DECLARE_CB_REGISTER(regIndex)   b##regIndex
#define FFX_PARALLELSORT_DECLARE_UAV(regIndex)  register(FFX_DECLARE_UAV_REGISTER(regIndex))
#define FFX_PARALLELSORT_DECLARE_CB(regIndex)   register(FFX_DECLARE_CB_REGISTER(regIndex))

#if defined(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    cbuffer cbParallelSort : FFX_PARALLELSORT_DECLARE_CB(FFX_PARALLELSORT_BIND_CB_PARALLEL_SORT)
    {
        FfxUInt32 numKeys;
        FfxInt32  numBlocksPerThreadGroup;
        FfxUInt32 numThreadGroups;
        FfxUInt32 numThreadGroupsWithAdditionalBlocks;
        FfxUInt32 numReduceThreadgroupPerBin;
        FfxUInt32 numScanValues;
        FfxUInt32 shift;
        FfxUInt32 padding;
    };
#else
    #define numKeys 0
    #define numBlocksPerThreadGroup 0
    #define numThreadGroups 0
    #define numThreadGroupsWithAdditionalBlocks 0
    #define numReduceThreadgroupPerBin 0
    #define numScanValues 0
    #define shift 0
    #define padding 0
#endif

#define FFX_PARALLELSORT_CONSTANT_BUFFER_1_SIZE 8  // Number of 32-bit float/uint values in the constant buffer.

#define FFX_PARALLELSORT_ROOTSIG_STRINGIFY(p) FFX_PARALLELSORT_ROOTSIG_STR(p)
#define FFX_PARALLELSORT_ROOTSIG_STR(p) #p
#define FFX_PARALLELSORT_ROOTSIG [RootSignature( "DescriptorTable(UAV(u0, numDescriptors = " FFX_PARALLELSORT_ROOTSIG_STRINGIFY(FFX_PARALLELSORT_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                                 "CBV(b0)")]

#if defined(FFX_PARALLELSORT_EMBED_ROOTSIG)
#define FFX_PARALLELSORT_EMBED_ROOTSIG_CONTENT FFX_PARALLELSORT_ROOTSIG
#else
#define FFX_PARALLELSORT_EMBED_ROOTSIG_CONTENT
#endif // #if FFX_PARALLELSORT_EMBED_ROOTSIG

FfxUInt32 NumKeys()
{
    return numKeys;
}

FfxInt32 NumBlocksPerThreadGroup()
{
    return numBlocksPerThreadGroup;
}

FfxUInt32 NumThreadGroups()
{
    return numThreadGroups;
}

FfxUInt32 NumThreadGroupsWithAdditionalBlocks()
{
    return numThreadGroupsWithAdditionalBlocks;
}

FfxUInt32 NumReduceThreadgroupPerBin()
{
    return numReduceThreadgroupPerBin;
}

FfxUInt32 NumScanValues()
{
    return numScanValues;
}

FfxUInt32 ShiftBit()
{
    return shift;
}

    #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)
        RWStructuredBuffer<FfxUInt32>                               rw_source_keys                  : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)

    #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)
        RWStructuredBuffer<FfxUInt32>                               rw_dest_keys                    : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)

    #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
        RWStructuredBuffer<FfxUInt32>                               rw_source_payloads              : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
    
    #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)
        RWStructuredBuffer<FfxUInt32>                               rw_dest_payloads               : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)

    #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
        RWStructuredBuffer<FfxUInt32>                               rw_sum_table                    : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)

    #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)
        RWStructuredBuffer<FfxUInt32>                               rw_reduce_table                 : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)

    #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)
        RWStructuredBuffer<FfxUInt32>                               rw_scan_source                  : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)

    #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)
        RWStructuredBuffer<FfxUInt32>                               rw_scan_dest                    : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)

    #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)
        RWStructuredBuffer<FfxUInt32>                               rw_scan_scratch                 : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)

    // UAV declarations
    #if defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)
        RWStructuredBuffer<FfxUInt32>                               rw_count_scatter_args           : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)

    #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)
        RWStructuredBuffer<FfxUInt32>                               rw_reduce_scan_args             : FFX_PARALLELSORT_DECLARE_UAV(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS);
    #endif // #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)

#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)
    FfxUInt32 LoadSourceKey(FfxUInt32 index)
    {
        return rw_source_keys[index];
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_KEYS)

#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)
    void StoreDestKey(FfxUInt32 index, FfxUInt32 value)
    {
        rw_dest_keys[index] = value;
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_KEYS)

#if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)
    FfxUInt32 LoadSourcePayload(FfxUInt32 index)
    {
        return rw_source_payloads[index];
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SOURCE_PAYLOADS)

#if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)
    void StoreDestPayload(FfxUInt32 index, FfxUInt32 value)
    {
        rw_dest_payloads[index] = value;
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_DEST_PAYLOADS)

#if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
    FfxUInt32 LoadSumTable(FfxUInt32 index)
    {
        return rw_sum_table[index];
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)

#if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)
    void StoreSumTable(FfxUInt32 index, FfxUInt32 value)
    {
        rw_sum_table[index] = value;
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SUM_TABLE)

#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)
    void StoreReduceTable(FfxUInt32 index, FfxUInt32 value)
    {
        rw_reduce_table[index] = value;
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_TABLE)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)
    FfxUInt32 LoadScanSource(FfxUInt32 index)
    {
        return rw_scan_source[index];
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SOURCE)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)
    void StoreScanDest(FfxUInt32 index, FfxUInt32 value)
    {
        rw_scan_dest[index] = value;
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_DEST)

#if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)
    FfxUInt32 LoadScanScratch(FfxUInt32 index)
    {
        return rw_scan_scratch[index];
    }
#endif // #if defined(FFX_PARALLELSORT_BIND_UAV_SCAN_SCRATCH)

#if defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)
    void StoreCountScatterArgs(FfxUInt32 x, FfxUInt32 y, FfxUInt32 z)
    {
        rw_count_scatter_args[0] = x;
        rw_count_scatter_args[1] = y;
        rw_count_scatter_args[2] = z;
    }
#endif // defined(FFX_PARALLELSORT_BIND_UAV_COUNT_SCATTER_ARGS)

#if defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)
    void StoreReduceScanArgs(FfxUInt32 x, FfxUInt32 y, FfxUInt32 z)
    {
        rw_reduce_scan_args[0] = x;
        rw_reduce_scan_args[1] = y;
        rw_reduce_scan_args[2] = z;
    }
#endif // defined(FFX_PARALLELSORT_BIND_UAV_REDUCE_SCAN_ARGS)

#endif // #if defined(FFX_GPU)
