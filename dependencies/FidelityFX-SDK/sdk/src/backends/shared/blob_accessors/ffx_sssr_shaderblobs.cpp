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
#include "ffx_sssr_shaderblobs.h"
#include "sssr/ffx_sssr_private.h"

#include <ffx_sssr_prepare_blue_noise_texture_pass_permutations.h>
#include <ffx_sssr_intersect_pass_permutations.h>
#include <ffx_sssr_classify_tiles_pass_permutations.h>
#include <ffx_sssr_prepare_indirect_args_pass_permutations.h>
#include <ffx_sssr_depth_downsample_pass_permutations.h>

#include <ffx_sssr_prepare_blue_noise_texture_pass_wave64_permutations.h>
#include <ffx_sssr_intersect_pass_wave64_permutations.h>
#include <ffx_sssr_classify_tiles_pass_wave64_permutations.h>
#include <ffx_sssr_prepare_indirect_args_pass_wave64_permutations.h>
#include <ffx_sssr_depth_downsample_pass_wave64_permutations.h>

#include <ffx_sssr_prepare_blue_noise_texture_pass_16bit_permutations.h>
#include <ffx_sssr_intersect_pass_16bit_permutations.h>
#include <ffx_sssr_classify_tiles_pass_16bit_permutations.h>
#include <ffx_sssr_prepare_indirect_args_pass_16bit_permutations.h>
#include <ffx_sssr_depth_downsample_pass_16bit_permutations.h>

#include <ffx_sssr_prepare_blue_noise_texture_pass_wave64_16bit_permutations.h>
#include <ffx_sssr_intersect_pass_wave64_16bit_permutations.h>
#include <ffx_sssr_classify_tiles_pass_wave64_16bit_permutations.h>
#include <ffx_sssr_prepare_indirect_args_pass_wave64_16bit_permutations.h>
#include <ffx_sssr_depth_downsample_pass_wave64_16bit_permutations.h>

#include <string.h> // for memset

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)
#define POPULATE_PERMUTATION_KEY(options, key)                                                            \
key.index = 0;                                                                                            \
key.FFX_SSSR_OPTION_INVERTED_DEPTH = FFX_CONTAINS_FLAG(options, SSSR_SHADER_PERMUTATION_DEPTH_INVERTED);  \


static FfxShaderBlob sssrGetClassifyTilesPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_sssr_classify_tiles_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_sssr_classify_tiles_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_classify_tiles_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_sssr_classify_tiles_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_classify_tiles_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_sssr_classify_tiles_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_classify_tiles_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_sssr_classify_tiles_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_classify_tiles_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob sssrGetPrepareBlueNoiseTexturePassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_sssr_prepare_blue_noise_texture_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_sssr_prepare_blue_noise_texture_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_prepare_blue_noise_texture_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_sssr_prepare_blue_noise_texture_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_prepare_blue_noise_texture_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_sssr_prepare_blue_noise_texture_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_prepare_blue_noise_texture_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_sssr_prepare_blue_noise_texture_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_prepare_blue_noise_texture_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob sssrGetPrepareIndirectArgsPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_sssr_prepare_indirect_args_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_sssr_prepare_indirect_args_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_prepare_indirect_args_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_sssr_prepare_indirect_args_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_prepare_indirect_args_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_sssr_prepare_indirect_args_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_prepare_indirect_args_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_sssr_prepare_indirect_args_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_prepare_indirect_args_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob sssrGetIntersectionPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_sssr_intersect_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_sssr_intersect_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_intersect_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_sssr_intersect_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_intersect_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_sssr_intersect_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_intersect_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_sssr_intersect_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_intersect_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob sssrGetDepthDownsamplePassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_sssr_depth_downsample_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_sssr_depth_downsample_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_depth_downsample_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_sssr_depth_downsample_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_depth_downsample_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_sssr_depth_downsample_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_depth_downsample_pass_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_sssr_depth_downsample_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_sssr_depth_downsample_pass_PermutationInfo, tableIndex);
        }
    }
}

FfxErrorCode sssrGetPermutationBlobByIndex(
    FfxSssrPass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {

    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, SSSR_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit = FFX_CONTAINS_FLAG(permutationOptions, SSSR_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId) {

        case FFX_SSSR_PASS_CLASSIFY_TILES:
        {
            FfxShaderBlob blob = sssrGetClassifyTilesPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_SSSR_PASS_PREPARE_BLUE_NOISE_TEXTURE:
        {
            FfxShaderBlob blob = sssrGetPrepareBlueNoiseTexturePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_SSSR_PASS_PREPARE_INDIRECT_ARGS:
        {
            FfxShaderBlob blob = sssrGetPrepareIndirectArgsPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_SSSR_PASS_INTERSECTION:
        {
            FfxShaderBlob blob = sssrGetIntersectionPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_SSSR_PASS_DEPTH_DOWNSAMPLE:
        {
            FfxShaderBlob blob = sssrGetDepthDownsamplePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
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

FfxErrorCode sssrIsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, SSSR_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
