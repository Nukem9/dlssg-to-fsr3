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
#include "ffx_blur_shaderblobs.h"
#include "blur/ffx_blur_private.h"

#include <ffx_blur_pass_permutations.h>
#include <ffx_blur_pass_wave64_permutations.h>
#include <ffx_blur_pass_16bit_permutations.h>
#include <ffx_blur_pass_wave64_16bit_permutations.h>

#include <string.h> // for memset

static FfxShaderBlob blurGetBlurPassPermutationBlobByKernelSize(
    uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_blur_pass_PermutationKey key;
    key.index = 0;

    if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_KERNEL_0))
        key.FFX_BLUR_OPTION_KERNEL_PERMUTATION = 0;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_KERNEL_1))
        key.FFX_BLUR_OPTION_KERNEL_PERMUTATION = 1;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_KERNEL_2))
        key.FFX_BLUR_OPTION_KERNEL_PERMUTATION = 2;
    else
        FFX_ASSERT_FAIL("Unknown kernel permutation.");

    if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_3x3_KERNEL))
        key.FFX_BLUR_OPTION_KERNEL_DIMENSION = 0;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_5x5_KERNEL))
        key.FFX_BLUR_OPTION_KERNEL_DIMENSION = 1;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_7x7_KERNEL))
        key.FFX_BLUR_OPTION_KERNEL_DIMENSION = 2;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_9x9_KERNEL))
        key.FFX_BLUR_OPTION_KERNEL_DIMENSION = 3;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_11x11_KERNEL))
        key.FFX_BLUR_OPTION_KERNEL_DIMENSION = 4;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_13x13_KERNEL))
        key.FFX_BLUR_OPTION_KERNEL_DIMENSION = 5;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_15x15_KERNEL))
        key.FFX_BLUR_OPTION_KERNEL_DIMENSION = 6;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_17x17_KERNEL))
        key.FFX_BLUR_OPTION_KERNEL_DIMENSION = 7;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_19x19_KERNEL))
        key.FFX_BLUR_OPTION_KERNEL_DIMENSION = 8;
    else if (FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_21x21_KERNEL))
        key.FFX_BLUR_OPTION_KERNEL_DIMENSION = 9;
    else
        FFX_ASSERT_FAIL("Unknown kernel size permutation.");

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_blur_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_blur_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_blur_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_blur_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_blur_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_blur_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_blur_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_blur_pass_PermutationInfo, tableIndex);
        }
    }
}

FfxErrorCode blurGetPermutationBlobByIndex(
    FfxBlurPass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {

    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit = FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId)
    {
        case FFX_BLUR_PASS_BLUR:
        {
            FfxShaderBlob blob =
                blurGetBlurPassPermutationBlobByKernelSize(permutationOptions, isWave64, is16bit);
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

FfxErrorCode blurIsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, BLUR_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
