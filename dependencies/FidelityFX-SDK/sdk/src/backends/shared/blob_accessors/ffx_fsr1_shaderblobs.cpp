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
#include "ffx_fsr1_shaderblobs.h"
#include "fsr1/ffx_fsr1_private.h"

#include <ffx_fsr1_easu_pass_permutations.h>
#include <ffx_fsr1_rcas_pass_permutations.h>

#include <ffx_fsr1_easu_pass_wave64_permutations.h>
#include <ffx_fsr1_rcas_pass_wave64_permutations.h>

#include <ffx_fsr1_easu_pass_16bit_permutations.h>
#include <ffx_fsr1_rcas_pass_16bit_permutations.h>

#include <ffx_fsr1_easu_pass_wave64_16bit_permutations.h>
#include <ffx_fsr1_rcas_pass_wave64_16bit_permutations.h>

#include <string.h> // for memset

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)
#define POPULATE_PERMUTATION_KEY(options, key)                                                                              \
key.index = 0;                                                                                                              \
key.FFX_FSR1_OPTION_APPLY_RCAS = FFX_CONTAINS_FLAG(options, FSR1_SHADER_PERMUTATION_APPLY_RCAS);                            \
key.FFX_FSR1_OPTION_RCAS_PASSTHROUGH_ALPHA = FFX_CONTAINS_FLAG(options, FSR1_SHADER_PERMUTATION_RCAS_PASSTHROUGH_ALPHA);    \
key.FFX_FSR1_OPTION_SRGB_CONVERSIONS = FFX_CONTAINS_FLAG(options, FSR1_SHADER_PERMUTATION_SRGB_CONVERSIONS);

static FfxShaderBlob fsr1GetEasuPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_fsr1_easu_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr1_easu_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr1_easu_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr1_easu_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr1_easu_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr1_easu_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr1_easu_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr1_easu_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr1_easu_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob fsr1GetRcasPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_fsr1_rcas_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr1_rcas_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr1_rcas_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr1_rcas_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr1_rcas_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr1_rcas_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr1_rcas_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr1_rcas_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr1_rcas_pass_PermutationInfo, tableIndex);
        }
    }
}

FfxErrorCode fsr1GetPermutationBlobByIndex(
    FfxFsr1Pass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {

    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, FSR1_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit = FFX_CONTAINS_FLAG(permutationOptions, FSR1_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId) {

        case FFX_FSR1_PASS_EASU:
        case FFX_FSR1_PASS_EASU_RCAS:
        {
            FfxShaderBlob blob = fsr1GetEasuPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FSR1_PASS_RCAS:
        {
            FfxShaderBlob blob = fsr1GetRcasPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
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

FfxErrorCode fsr1IsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, FSR1_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
