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
#include "ffx_spd_shaderblobs.h"
#include "spd/ffx_spd_private.h"

#include <ffx_spd_downsample_pass_permutations.h>
#include <ffx_spd_downsample_pass_wave64_permutations.h>
#include <ffx_spd_downsample_pass_16bit_permutations.h>
#include <ffx_spd_downsample_pass_wave64_16bit_permutations.h>

#include <string.h> // for memset

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)

#define POPULATE_PERMUTATION_KEY(options, key)                                                                              \
key.index = 0;                                                                                                              \
key.FFX_SPD_OPTION_LINEAR_SAMPLE = FFX_CONTAINS_FLAG(options, SPD_SHADER_PERMUTATION_LINEAR_SAMPLE);                            \
key.FFX_SPD_OPTION_WAVE_INTEROP_LDS = FFX_CONTAINS_FLAG(options, SPD_SHADER_PERMUTATION_WAVE_INTEROP_LDS);

static FfxShaderBlob spdGetDownsamplePassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_spd_downsample_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (FFX_CONTAINS_FLAG(permutationOptions, SPD_SHADER_PERMUTATION_DOWNSAMPLE_FILTER_MEAN))
    {
        key.FFX_SPD_OPTION_DOWNSAMPLE_FILTER = 0;
    }
    else if (FFX_CONTAINS_FLAG(permutationOptions, SPD_SHADER_PERMUTATION_DOWNSAMPLE_FILTER_MIN))
    {
        key.FFX_SPD_OPTION_DOWNSAMPLE_FILTER = 1;
    }
    else if (FFX_CONTAINS_FLAG(permutationOptions, SPD_SHADER_PERMUTATION_DOWNSAMPLE_FILTER_MAX))
    {
        key.FFX_SPD_OPTION_DOWNSAMPLE_FILTER = 2;
    }

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_spd_downsample_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_spd_downsample_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_spd_downsample_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_spd_downsample_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_spd_downsample_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_spd_downsample_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_spd_downsample_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_spd_downsample_pass_PermutationInfo, tableIndex);
        }
    }
}

FfxErrorCode spdGetPermutationBlobByIndex(
    FfxSpdPass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {

    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, SPD_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit = FFX_CONTAINS_FLAG(permutationOptions, SPD_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId) {

        case FFX_SPD_PASS_DOWNSAMPLE:
        {
            FfxShaderBlob blob = spdGetDownsamplePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
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

FfxErrorCode spdIsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, SPD_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
