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
#include "ffx_fsr2_shaderblobs.h"
#include "fsr2/ffx_fsr2_private.h"

#include <ffx_fsr2_autogen_reactive_pass_permutations.h>
#include <ffx_fsr2_accumulate_pass_permutations.h>
#include <ffx_fsr2_compute_luminance_pyramid_pass_permutations.h>
#include <ffx_fsr2_depth_clip_pass_permutations.h>
#include <ffx_fsr2_lock_pass_permutations.h>
#include <ffx_fsr2_reconstruct_previous_depth_pass_permutations.h>
#include <ffx_fsr2_rcas_pass_permutations.h>
#include <ffx_fsr2_tcr_autogen_pass_permutations.h>

#include <ffx_fsr2_autogen_reactive_pass_wave64_permutations.h>
#include <ffx_fsr2_accumulate_pass_wave64_permutations.h>
#include <ffx_fsr2_compute_luminance_pyramid_pass_wave64_permutations.h>
#include <ffx_fsr2_depth_clip_pass_wave64_permutations.h>
#include <ffx_fsr2_lock_pass_wave64_permutations.h>
#include <ffx_fsr2_reconstruct_previous_depth_pass_wave64_permutations.h>
#include <ffx_fsr2_rcas_pass_wave64_permutations.h>
#include <ffx_fsr2_tcr_autogen_pass_wave64_permutations.h>

#include <ffx_fsr2_autogen_reactive_pass_16bit_permutations.h>
#include <ffx_fsr2_accumulate_pass_16bit_permutations.h>
#include <ffx_fsr2_compute_luminance_pyramid_pass_16bit_permutations.h>
#include <ffx_fsr2_depth_clip_pass_16bit_permutations.h>
#include <ffx_fsr2_lock_pass_16bit_permutations.h>
#include <ffx_fsr2_reconstruct_previous_depth_pass_16bit_permutations.h>
#include <ffx_fsr2_rcas_pass_16bit_permutations.h>
#include <ffx_fsr2_tcr_autogen_pass_16bit_permutations.h>

#include <ffx_fsr2_autogen_reactive_pass_wave64_16bit_permutations.h>
#include <ffx_fsr2_accumulate_pass_wave64_16bit_permutations.h>
#include <ffx_fsr2_compute_luminance_pyramid_pass_wave64_16bit_permutations.h>
#include <ffx_fsr2_depth_clip_pass_wave64_16bit_permutations.h>
#include <ffx_fsr2_lock_pass_wave64_16bit_permutations.h>
#include <ffx_fsr2_reconstruct_previous_depth_pass_wave64_16bit_permutations.h>
#include <ffx_fsr2_rcas_pass_wave64_16bit_permutations.h>
#include <ffx_fsr2_tcr_autogen_pass_wave64_16bit_permutations.h>

#include <string.h> // for memset

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)
#define POPULATE_PERMUTATION_KEY(options, key)                                                                \
key.index = 0;                                                                                                \
key.FFX_FSR2_OPTION_REPROJECT_USE_LANCZOS_TYPE = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_USE_LANCZOS_TYPE);                     \
key.FFX_FSR2_OPTION_HDR_COLOR_INPUT = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_HDR_COLOR_INPUT);                 \
key.FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_LOW_RES_MOTION_VECTORS);   \
key.FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_JITTER_MOTION_VECTORS);   \
key.FFX_FSR2_OPTION_INVERTED_DEPTH = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_DEPTH_INVERTED);                   \
key.FFX_FSR2_OPTION_APPLY_SHARPENING = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_ENABLE_SHARPENING);

static FfxShaderBlob fsr2GetTcrAutogenPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_fsr2_tcr_autogen_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_tcr_autogen_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_tcr_autogen_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr2_tcr_autogen_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_tcr_autogen_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_tcr_autogen_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_tcr_autogen_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr2_tcr_autogen_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_tcr_autogen_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob fsr2GetDepthClipPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_fsr2_depth_clip_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_depth_clip_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_depth_clip_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr2_depth_clip_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_depth_clip_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_depth_clip_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_depth_clip_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr2_depth_clip_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_depth_clip_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob fsr2GetReconstructPreviousDepthPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_fsr2_reconstruct_previous_depth_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_reconstruct_previous_depth_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_reconstruct_previous_depth_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr2_reconstruct_previous_depth_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_reconstruct_previous_depth_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_reconstruct_previous_depth_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_reconstruct_previous_depth_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr2_reconstruct_previous_depth_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_reconstruct_previous_depth_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob fsr2GetLockPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_fsr2_lock_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_lock_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_lock_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr2_lock_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_lock_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_lock_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_lock_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr2_lock_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_lock_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob fsr2GetAccumulatePassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_fsr2_accumulate_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_accumulate_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_accumulate_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr2_accumulate_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_accumulate_pass_wave64_PermutationInfo, tableIndex);
        }
    } else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_accumulate_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_accumulate_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_fsr2_accumulate_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_accumulate_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob fsr2GetRCASPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16Bit)
{

    ffx_fsr2_rcas_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);
#if defined(_GAMING_XBOX)
    if (is16Bit) {
        if (isWave64) {

            const int32_t tableIndex = g_ffx_fsr2_rcas_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_rcas_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_fsr2_rcas_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_rcas_pass_16bit_PermutationInfo, tableIndex);
        }
    }
    else
#else
    FFX_UNUSED(is16Bit);
#endif

    if (isWave64) {
        
        const int32_t tableIndex = g_ffx_fsr2_rcas_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_rcas_pass_wave64_PermutationInfo, tableIndex);

    } else {

        const int32_t tableIndex = g_ffx_fsr2_rcas_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_rcas_pass_PermutationInfo, tableIndex);

    }
}

static FfxShaderBlob fsr2GetComputeLuminancePyramidPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{

    ffx_fsr2_compute_luminance_pyramid_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        const int32_t tableIndex = g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_PermutationInfo, tableIndex);
    } else {

        const int32_t tableIndex = g_ffx_fsr2_compute_luminance_pyramid_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_compute_luminance_pyramid_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob fsr2GetAutogenReactivePassPermutationBlobByIndex(
    uint32_t permutationOptions,
    bool isWave64,
    bool is16bit) {

    ffx_fsr2_autogen_reactive_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_autogen_reactive_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_autogen_reactive_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_fsr2_autogen_reactive_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_autogen_reactive_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_fsr2_autogen_reactive_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_autogen_reactive_pass_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_fsr2_autogen_reactive_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_fsr2_autogen_reactive_pass_PermutationInfo, tableIndex);
        }
    }
}

FfxErrorCode fsr2GetPermutationBlobByIndex(
    FfxFsr2Pass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {

    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, FSR2_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit = FFX_CONTAINS_FLAG(permutationOptions, FSR2_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId) {

        case FFX_FSR2_PASS_DEPTH_CLIP:
        {
            FfxShaderBlob blob = fsr2GetDepthClipPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FSR2_PASS_RECONSTRUCT_PREVIOUS_DEPTH:
        {
            FfxShaderBlob blob = fsr2GetReconstructPreviousDepthPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FSR2_PASS_LOCK:
        {
            FfxShaderBlob blob = fsr2GetLockPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FSR2_PASS_ACCUMULATE:
        case FFX_FSR2_PASS_ACCUMULATE_SHARPEN:
        {
            FfxShaderBlob blob = fsr2GetAccumulatePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FSR2_PASS_RCAS:
        {
            FfxShaderBlob blob = fsr2GetRCASPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FSR2_PASS_COMPUTE_LUMINANCE_PYRAMID:
        {
            FfxShaderBlob blob = fsr2GetComputeLuminancePyramidPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FSR2_PASS_GENERATE_REACTIVE:
        {
            FfxShaderBlob blob = fsr2GetAutogenReactivePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FSR2_PASS_TCR_AUTOGENERATE:
        {
            FfxShaderBlob blob = fsr2GetTcrAutogenPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
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

FfxErrorCode fsr2IsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, FSR2_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
