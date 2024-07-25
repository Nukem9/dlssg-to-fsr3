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

#define FFX_CPU
#include <FidelityFX/host/ffx_util.h>
#include "ffx_frameinterpolation_shaderblobs.h"
#include "frameinterpolation/ffx_frameinterpolation_private.h"

#include <ffx_frameinterpolation_reconstruct_and_dilate_pass_permutations.h>
#include <ffx_frameinterpolation_disocclusion_mask_pass_permutations.h>
#include <ffx_frameinterpolation_reconstruct_previous_depth_pass_permutations.h>
#include <ffx_frameinterpolation_setup_pass_permutations.h>
#include <ffx_frameinterpolation_game_motion_vector_field_pass_permutations.h>
#include <ffx_frameinterpolation_optical_flow_vector_field_pass_permutations.h>
#include <ffx_frameinterpolation_pass_permutations.h>
#include <ffx_frameinterpolation_Compute_Game_Vector_Field_Inpainting_Pyramid_pass_permutations.h>
#include <ffx_frameinterpolation_Compute_Inpainting_Pyramid_pass_permutations.h>
#include <ffx_frameinterpolation_Inpainting_pass_permutations.h>
#include <ffx_frameinterpolation_debug_view_pass_permutations.h>

#include <ffx_frameinterpolation_reconstruct_and_dilate_pass_wave64_permutations.h>
#include <ffx_frameinterpolation_disocclusion_mask_pass_wave64_permutations.h>
#include <ffx_frameinterpolation_reconstruct_previous_depth_pass_wave64_permutations.h>
#include <ffx_frameinterpolation_setup_pass_wave64_permutations.h>
#include <ffx_frameinterpolation_game_motion_vector_field_pass_wave64_permutations.h>
#include <ffx_frameinterpolation_optical_flow_vector_field_pass_wave64_permutations.h>
#include <ffx_frameinterpolation_pass_wave64_permutations.h>
#include <ffx_frameinterpolation_Compute_Game_Vector_Field_Inpainting_Pyramid_pass_wave64_permutations.h>
#include <ffx_frameinterpolation_Compute_Inpainting_Pyramid_pass_wave64_permutations.h>
#include <ffx_frameinterpolation_Inpainting_pass_wave64_permutations.h>
#include <ffx_frameinterpolation_debug_view_pass_wave64_permutations.h>

#include <ffx_frameinterpolation_reconstruct_and_dilate_pass_16bit_permutations.h>
#include <ffx_frameinterpolation_disocclusion_mask_pass_16bit_permutations.h>
#include <ffx_frameinterpolation_reconstruct_previous_depth_pass_16bit_permutations.h>
#include <ffx_frameinterpolation_setup_pass_16bit_permutations.h>
#include <ffx_frameinterpolation_game_motion_vector_field_pass_16bit_permutations.h>
#include <ffx_frameinterpolation_optical_flow_vector_field_pass_16bit_permutations.h>
#include <ffx_frameinterpolation_pass_16bit_permutations.h>
#include <ffx_frameinterpolation_Compute_Game_Vector_Field_Inpainting_Pyramid_pass_16bit_permutations.h>
#include <ffx_frameinterpolation_Compute_Inpainting_Pyramid_pass_16bit_permutations.h>
#include <ffx_frameinterpolation_Inpainting_pass_16bit_permutations.h>
#include <ffx_frameinterpolation_debug_view_pass_16bit_permutations.h>

#include <ffx_frameinterpolation_reconstruct_and_dilate_pass_wave64_16bit_permutations.h>
#include <ffx_frameinterpolation_disocclusion_mask_pass_wave64_16bit_permutations.h>
#include <ffx_frameinterpolation_reconstruct_previous_depth_pass_wave64_16bit_permutations.h>
#include <ffx_frameinterpolation_setup_pass_wave64_16bit_permutations.h>
#include <ffx_frameinterpolation_game_motion_vector_field_pass_wave64_16bit_permutations.h>
#include <ffx_frameinterpolation_optical_flow_vector_field_pass_wave64_16bit_permutations.h>
#include <ffx_frameinterpolation_pass_wave64_16bit_permutations.h>
#include <ffx_frameinterpolation_Compute_Game_Vector_Field_Inpainting_Pyramid_pass_wave64_16bit_permutations.h>
#include <ffx_frameinterpolation_Compute_Inpainting_Pyramid_pass_wave64_16bit_permutations.h>
#include <ffx_frameinterpolation_Inpainting_pass_wave64_16bit_permutations.h>
#include <ffx_frameinterpolation_debug_view_pass_wave64_16bit_permutations.h>

#include <string.h> // for memset

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)
#define POPULATE_PERMUTATION_KEY(options, key)                                                                \
    key.index = 0;\
    key.FFX_FRAMEINTERPOLATION_OPTION_LOW_RES_MOTION_VECTORS    = FFX_CONTAINS_FLAG(options, FRAMEINTERPOLATION_SHADER_PERMUTATION_LOW_RES_MOTION_VECTORS); \
    key.FFX_FRAMEINTERPOLATION_OPTION_JITTER_MOTION_VECTORS     = FFX_CONTAINS_FLAG(options, FRAMEINTERPOLATION_SHADER_PERMUTATION_JITTER_MOTION_VECTORS);  \
    key.FFX_FRAMEINTERPOLATION_OPTION_INVERTED_DEPTH            = FFX_CONTAINS_FLAG(options, FRAMEINTERPOLATION_SHADER_PERMUTATION_DEPTH_INVERTED);

static FfxShaderBlob FrameInterpolationGetReconstructAndDilatePermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_reconstruct_and_dilate_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_reconstruct_and_dilate_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_reconstruct_and_dilate_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_reconstruct_and_dilate_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_reconstruct_and_dilate_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob FrameInterpolationGetSetupPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_setup_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_setup_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_setup_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_setup_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_setup_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob FrameInterpolationGetGameMotionVectorFieldPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_game_motion_vector_field_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_game_motion_vector_field_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_game_motion_vector_field_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_game_motion_vector_field_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_game_motion_vector_field_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob FrameInterpolationGetOpticalFlowVectorFieldPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_optical_flow_vector_field_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_optical_flow_vector_field_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_optical_flow_vector_field_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_optical_flow_vector_field_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_optical_flow_vector_field_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob FrameInterpolationGetReconstructPrevDepthPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_reconstruct_previous_depth_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_reconstruct_previous_depth_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_reconstruct_previous_depth_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_reconstruct_previous_depth_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_reconstruct_previous_depth_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob FrameInterpolationGetDisocclusionMaskPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_disocclusion_mask_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_disocclusion_mask_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_disocclusion_mask_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_disocclusion_mask_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_disocclusion_mask_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob FrameInterpolationGetComputeInpaintingPyramidPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_compute_inpainting_pyramid_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_compute_inpainting_pyramid_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_compute_inpainting_pyramid_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_compute_inpainting_pyramid_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_compute_inpainting_pyramid_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob FrameInterpolationGetFiPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob FrameInterpolationGetComputeGameVectorFieldInpaintingPyramidPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_compute_game_vector_field_inpainting_pyramid_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_compute_game_vector_field_inpainting_pyramid_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_compute_game_vector_field_inpainting_pyramid_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_compute_game_vector_field_inpainting_pyramid_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_compute_game_vector_field_inpainting_pyramid_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob FrameInterpolationGetInpaintingPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_inpainting_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_inpainting_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_inpainting_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_inpainting_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_inpainting_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob FrameInterpolationGetDebugViewPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
{
    ffx_frameinterpolation_inpainting_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_debug_view_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_debug_view_pass_wave64_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_frameinterpolation_debug_view_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_frameinterpolation_debug_view_pass_PermutationInfo, tableIndex);
    }
}

FfxErrorCode frameInterpolationGetPermutationBlobByIndex(FfxFrameInterpolationPass passId,
    FfxBindStage,
    uint32_t permutationOptions, FfxShaderBlob* outBlob)
{

    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, FRAMEINTERPOLATION_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit  = FFX_CONTAINS_FLAG(permutationOptions, FRAMEINTERPOLATION_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId) {

        case FFX_FRAMEINTERPOLATION_PASS_RECONSTRUCT_AND_DILATE:
        {
            FfxShaderBlob blob = FrameInterpolationGetReconstructAndDilatePermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FRAMEINTERPOLATION_PASS_SETUP:
        {
            FfxShaderBlob blob = FrameInterpolationGetSetupPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FRAMEINTERPOLATION_PASS_RECONSTRUCT_PREV_DEPTH:
        {
            FfxShaderBlob blob = FrameInterpolationGetReconstructPrevDepthPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FRAMEINTERPOLATION_PASS_GAME_MOTION_VECTOR_FIELD:
        {
            FfxShaderBlob blob = FrameInterpolationGetGameMotionVectorFieldPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FRAMEINTERPOLATION_PASS_OPTICAL_FLOW_VECTOR_FIELD:
        {
            FfxShaderBlob blob = FrameInterpolationGetOpticalFlowVectorFieldPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FRAMEINTERPOLATION_PASS_DISOCCLUSION_MASK:
        {
            FfxShaderBlob blob = FrameInterpolationGetDisocclusionMaskPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FRAMEINTERPOLATION_PASS_GAME_VECTOR_FIELD_INPAINTING_PYRAMID:
        {
            FfxShaderBlob blob = FrameInterpolationGetComputeGameVectorFieldInpaintingPyramidPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FRAMEINTERPOLATION_PASS_INPAINTING_PYRAMID:
        {
            FfxShaderBlob blob = FrameInterpolationGetComputeInpaintingPyramidPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FRAMEINTERPOLATION_PASS_INTERPOLATION:
        {
            FfxShaderBlob blob = FrameInterpolationGetFiPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FRAMEINTERPOLATION_PASS_INPAINTING:
        {
            FfxShaderBlob blob = FrameInterpolationGetInpaintingPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_FRAMEINTERPOLATION_PASS_DEBUG_VIEW:
        {
            FfxShaderBlob blob = FrameInterpolationGetDebugViewPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
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

FfxErrorCode frameInterpolationIsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, FRAMEINTERPOLATION_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
