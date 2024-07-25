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

#include <FidelityFX/host/ffx_util.h>
#include "ffx_parallelsort_shaderblobs.h"
#include "parallelsort/ffx_parallelsort_private.h"

#include <ffx_parallelsort_setup_indirect_args_pass_permutations.h>
#include <ffx_parallelsort_sum_pass_permutations.h>
#include <ffx_parallelsort_reduce_pass_permutations.h>
#include <ffx_parallelsort_scan_pass_permutations.h>
#include <ffx_parallelsort_scan_add_pass_permutations.h>
#include <ffx_parallelsort_scatter_pass_permutations.h>

#include <ffx_parallelsort_setup_indirect_args_pass_wave64_permutations.h>
#include <ffx_parallelsort_sum_pass_wave64_permutations.h>
#include <ffx_parallelsort_reduce_pass_wave64_permutations.h>
#include <ffx_parallelsort_scan_pass_wave64_permutations.h>
#include <ffx_parallelsort_scan_add_pass_wave64_permutations.h>
#include <ffx_parallelsort_scatter_pass_wave64_permutations.h>

#include <string.h> // for memset

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)

#define POPULATE_PERMUTATION_KEY(options, key)                                                                              \
key.index = 0;                                                                                                              \
key.FFX_PARALLELSORT_OPTION_HAS_PAYLOAD = FFX_CONTAINS_FLAG(options, PARALLELSORT_SHADER_PERMUTATION_HAS_PAYLOAD);

static FfxShaderBlob parallelSortGetSetupIndirectArgsPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{

    ffx_parallelsort_setup_indirect_args_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        const int32_t tableIndex = g_ffx_parallelsort_setup_indirect_args_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_setup_indirect_args_pass_wave64_PermutationInfo, tableIndex);
        
    } else {

        const int32_t tableIndex = g_ffx_parallelsort_setup_indirect_args_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_setup_indirect_args_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob parallelSortGetSumPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{

    ffx_parallelsort_sum_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        const int32_t tableIndex = g_ffx_parallelsort_sum_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_sum_pass_wave64_PermutationInfo, tableIndex);

    }
    else {

        const int32_t tableIndex = g_ffx_parallelsort_sum_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_sum_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob parallelSortGetReducePassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{

    ffx_parallelsort_reduce_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        const int32_t tableIndex = g_ffx_parallelsort_reduce_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_reduce_pass_wave64_PermutationInfo, tableIndex);

    }
    else {

        const int32_t tableIndex = g_ffx_parallelsort_reduce_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_reduce_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob parallelSortGetScanPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{

    ffx_parallelsort_scan_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        const int32_t tableIndex = g_ffx_parallelsort_scan_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_scan_pass_wave64_PermutationInfo, tableIndex);

    }
    else {

        const int32_t tableIndex = g_ffx_parallelsort_scan_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_scan_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob parallelSortGetScanAddPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{

    ffx_parallelsort_scan_add_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        const int32_t tableIndex = g_ffx_parallelsort_scan_add_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_scan_add_pass_wave64_PermutationInfo, tableIndex);

    }
    else {

        const int32_t tableIndex = g_ffx_parallelsort_scan_add_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_scan_add_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob parallelSortGetScatterPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{

    ffx_parallelsort_scatter_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        const int32_t tableIndex = g_ffx_parallelsort_scatter_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_scatter_pass_wave64_PermutationInfo, tableIndex);

    }
    else {

        const int32_t tableIndex = g_ffx_parallelsort_scatter_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_parallelsort_scatter_pass_PermutationInfo, tableIndex);
    }
}

FfxErrorCode parallelSortGetPermutationBlobByIndex(
    FfxParallelSortPass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {

    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, PARALLELSORT_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit = FFX_CONTAINS_FLAG(permutationOptions, PARALLELSORT_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId) {

        case FFX_PARALLELSORT_PASS_SETUP_INDIRECT_ARGS:
        {
            FfxShaderBlob blob = parallelSortGetSetupIndirectArgsPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_PARALLELSORT_PASS_SUM:
        {
            FfxShaderBlob blob = parallelSortGetSumPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_PARALLELSORT_PASS_REDUCE:
        {
            FfxShaderBlob blob = parallelSortGetReducePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_PARALLELSORT_PASS_SCAN:
        {
            FfxShaderBlob blob = parallelSortGetScanPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_PARALLELSORT_PASS_SCAN_ADD:
        {
            FfxShaderBlob blob = parallelSortGetScanAddPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_PARALLELSORT_PASS_SCATTER:
        {
            FfxShaderBlob blob = parallelSortGetScatterPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        default:
            FFX_ASSERT_FAIL("Should never reach here.");
            break;
    }

    // return an empty blob
    memset(outBlob, 0, sizeof(FfxShaderBlob));
    return FFX_OK;
}

FfxErrorCode parallelSortIsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, PARALLELSORT_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
