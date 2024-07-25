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
#include "ffx_denoiser_shaderblobs.h"
#include "denoiser/ffx_denoiser_private.h"

 #include <ffx_denoiser_prepare_shadow_mask_pass_permutations.h>
 #include <ffx_denoiser_prepare_shadow_mask_pass_wave64_permutations.h>
 #include <ffx_denoiser_prepare_shadow_mask_pass_16bit_permutations.h>
 #include <ffx_denoiser_prepare_shadow_mask_pass_wave64_16bit_permutations.h>

 #include <ffx_denoiser_filter_soft_shadows_0_pass_permutations.h>
 #include <ffx_denoiser_filter_soft_shadows_0_pass_wave64_permutations.h>
 #include <ffx_denoiser_filter_soft_shadows_0_pass_16bit_permutations.h>
 #include <ffx_denoiser_filter_soft_shadows_0_pass_wave64_16bit_permutations.h>


 #include <ffx_denoiser_filter_soft_shadows_1_pass_permutations.h>
 #include <ffx_denoiser_filter_soft_shadows_1_pass_wave64_permutations.h>
 #include <ffx_denoiser_filter_soft_shadows_1_pass_16bit_permutations.h>
 #include <ffx_denoiser_filter_soft_shadows_1_pass_wave64_16bit_permutations.h>

 #include <ffx_denoiser_filter_soft_shadows_2_pass_permutations.h>
 #include <ffx_denoiser_filter_soft_shadows_2_pass_wave64_permutations.h>
 #include <ffx_denoiser_filter_soft_shadows_2_pass_16bit_permutations.h>
 #include <ffx_denoiser_filter_soft_shadows_2_pass_wave64_16bit_permutations.h>


 #include <ffx_denoiser_shadows_tile_classification_pass_permutations.h>
 #include <ffx_denoiser_shadows_tile_classification_pass_wave64_permutations.h>
 #include <ffx_denoiser_shadows_tile_classification_pass_16bit_permutations.h>
 #include <ffx_denoiser_shadows_tile_classification_pass_wave64_16bit_permutations.h>

#include <ffx_denoiser_reproject_reflections_pass_permutations.h>
#include <ffx_denoiser_reproject_reflections_pass_wave64_permutations.h>
#include <ffx_denoiser_reproject_reflections_pass_16bit_permutations.h>
#include <ffx_denoiser_reproject_reflections_pass_wave64_16bit_permutations.h>

#include <ffx_denoiser_prefilter_reflections_pass_permutations.h>
#include <ffx_denoiser_prefilter_reflections_pass_wave64_permutations.h>
#include <ffx_denoiser_prefilter_reflections_pass_16bit_permutations.h>
#include <ffx_denoiser_prefilter_reflections_pass_wave64_16bit_permutations.h>

#include <ffx_denoiser_resolve_temporal_reflections_pass_permutations.h>
#include <ffx_denoiser_resolve_temporal_reflections_pass_wave64_permutations.h>
#include <ffx_denoiser_resolve_temporal_reflections_pass_16bit_permutations.h>
#include <ffx_denoiser_resolve_temporal_reflections_pass_wave64_16bit_permutations.h>

#include <string.h> // for memset

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)

#define POPULATE_PERMUTATION_KEY(options, key) \
key.index = 0;                                 \
key.FFX_DENOISER_OPTION_INVERTED_DEPTH = FFX_CONTAINS_FLAG(options, DENOISER_SHADER_PERMUTATION_DEPTH_INVERTED);      \


 static FfxShaderBlob denoiserGetPrepareShadowMaskPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
 {

     ffx_denoiser_prepare_shadow_mask_pass_PermutationKey key;
     POPULATE_PERMUTATION_KEY(permutationOptions, key);

     if (isWave64) {

         if (is16bit) {

             const int32_t tableIndex = g_ffx_denoiser_prepare_shadow_mask_pass_wave64_16bit_IndirectionTable[key.index];
             return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_prepare_shadow_mask_pass_wave64_16bit_PermutationInfo, tableIndex);
         }
         else {

             const int32_t tableIndex = g_ffx_denoiser_prepare_shadow_mask_pass_wave64_IndirectionTable[key.index];
             return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_prepare_shadow_mask_pass_wave64_PermutationInfo, tableIndex);
         }
     }
     else {

         if (is16bit) {

             const int32_t tableIndex = g_ffx_denoiser_prepare_shadow_mask_pass_16bit_IndirectionTable[key.index];
             return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_prepare_shadow_mask_pass_16bit_PermutationInfo, tableIndex);
         }
         else {

             const int32_t tableIndex = g_ffx_denoiser_prepare_shadow_mask_pass_IndirectionTable[key.index];
             return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_prepare_shadow_mask_pass_PermutationInfo, tableIndex);
         }
     }
 }


 static FfxShaderBlob denoiserGetShadowsTileClassificationPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
 {

     ffx_denoiser_shadows_tile_classification_pass_PermutationKey key;
     POPULATE_PERMUTATION_KEY(permutationOptions, key);

     if (isWave64) {

         if (is16bit) {

             const int32_t tableIndex = g_ffx_denoiser_shadows_tile_classification_pass_wave64_16bit_IndirectionTable[key.index];
             return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_shadows_tile_classification_pass_wave64_16bit_PermutationInfo, tableIndex);
         }
         else {

             const int32_t tableIndex = g_ffx_denoiser_shadows_tile_classification_pass_wave64_IndirectionTable[key.index];
             return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_shadows_tile_classification_pass_wave64_PermutationInfo, tableIndex);
         }
     }
     else {

         if (is16bit) {

             const int32_t tableIndex = g_ffx_denoiser_shadows_tile_classification_pass_16bit_IndirectionTable[key.index];
             return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_shadows_tile_classification_pass_16bit_PermutationInfo, tableIndex);
         }
         else {

             const int32_t tableIndex = g_ffx_denoiser_shadows_tile_classification_pass_IndirectionTable[key.index];
             return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_shadows_tile_classification_pass_PermutationInfo, tableIndex);
         }
     }
 }


 static FfxShaderBlob denoiserGetFilterSoftShadows0PermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
 {

     ffx_denoiser_filter_soft_shadows_0_pass_PermutationKey key;
     POPULATE_PERMUTATION_KEY(permutationOptions, key);

     if (isWave64)
     {
         const int32_t tableIndex = g_ffx_denoiser_filter_soft_shadows_0_pass_wave64_16bit_IndirectionTable[key.index];
         return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_filter_soft_shadows_0_pass_wave64_16bit_PermutationInfo, tableIndex);
     }
     else
     {
         const int32_t tableIndex = g_ffx_denoiser_filter_soft_shadows_0_pass_16bit_IndirectionTable[key.index];
         return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_filter_soft_shadows_0_pass_16bit_PermutationInfo, tableIndex);
     }
 }


 static FfxShaderBlob denoiserGetFilterSoftShadows1PermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
 {

     ffx_denoiser_filter_soft_shadows_1_pass_PermutationKey key;
     POPULATE_PERMUTATION_KEY(permutationOptions, key);

     if (isWave64)
     {
         const int32_t tableIndex = g_ffx_denoiser_filter_soft_shadows_1_pass_wave64_16bit_IndirectionTable[key.index];
         return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_filter_soft_shadows_1_pass_wave64_16bit_PermutationInfo, tableIndex);
     }
     else
     {
         const int32_t tableIndex = g_ffx_denoiser_filter_soft_shadows_1_pass_16bit_IndirectionTable[key.index];
         return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_filter_soft_shadows_1_pass_16bit_PermutationInfo, tableIndex);
     }
 }

 static FfxShaderBlob denoiserGetFilterSoftShadows2PermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool)
 {

     ffx_denoiser_filter_soft_shadows_2_pass_PermutationKey key;
     POPULATE_PERMUTATION_KEY(permutationOptions, key);

     if (isWave64)
     {
         const int32_t tableIndex = g_ffx_denoiser_filter_soft_shadows_2_pass_wave64_16bit_IndirectionTable[key.index];
         return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_filter_soft_shadows_2_pass_wave64_16bit_PermutationInfo, tableIndex);
     }
     else
     {
         const int32_t tableIndex = g_ffx_denoiser_filter_soft_shadows_2_pass_16bit_IndirectionTable[key.index];
         return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_filter_soft_shadows_2_pass_16bit_PermutationInfo, tableIndex);
     }
 }


static FfxShaderBlob denoiserGetReprojectReflectionsPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_denoiser_reproject_reflections_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_denoiser_reproject_reflections_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_reproject_reflections_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_denoiser_reproject_reflections_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_reproject_reflections_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_denoiser_reproject_reflections_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_reproject_reflections_pass_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_denoiser_reproject_reflections_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_reproject_reflections_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob denoiserGetPrefilterReflectionsPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_denoiser_prefilter_reflections_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_denoiser_prefilter_reflections_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_prefilter_reflections_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_denoiser_prefilter_reflections_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_prefilter_reflections_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_denoiser_prefilter_reflections_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_prefilter_reflections_pass_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_denoiser_prefilter_reflections_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_prefilter_reflections_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob denoiserGetResolveTemporalReflectionsPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_denoiser_resolve_temporal_reflections_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64) {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_denoiser_resolve_temporal_reflections_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_resolve_temporal_reflections_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_denoiser_resolve_temporal_reflections_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_resolve_temporal_reflections_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else {

        if (is16bit) {

            const int32_t tableIndex = g_ffx_denoiser_resolve_temporal_reflections_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_resolve_temporal_reflections_pass_16bit_PermutationInfo, tableIndex);
        }
        else {

            const int32_t tableIndex = g_ffx_denoiser_resolve_temporal_reflections_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_denoiser_resolve_temporal_reflections_pass_PermutationInfo, tableIndex);
        }
    }
}

FfxErrorCode denoiserGetPermutationBlobByIndex(
    FfxDenoiserPass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {

    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, DENOISER_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit = FFX_CONTAINS_FLAG(permutationOptions, DENOISER_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId) {

        case FFX_DENOISER_PASS_PREPARE_SHADOW_MASK:
        {
            FfxShaderBlob blob = denoiserGetPrepareShadowMaskPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_DENOISER_PASS_SHADOWS_TILE_CLASSIFICATION:
        {
            FfxShaderBlob blob = denoiserGetShadowsTileClassificationPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_DENOISER_PASS_FILTER_SOFT_SHADOWS_0:
        {
            FfxShaderBlob blob = denoiserGetFilterSoftShadows0PermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_DENOISER_PASS_FILTER_SOFT_SHADOWS_1:
        {
            FfxShaderBlob blob = denoiserGetFilterSoftShadows1PermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_DENOISER_PASS_FILTER_SOFT_SHADOWS_2:
        {
            FfxShaderBlob blob = denoiserGetFilterSoftShadows2PermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_DENOISER_PASS_REPROJECT_REFLECTIONS:
        {
            FfxShaderBlob blob = denoiserGetReprojectReflectionsPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_DENOISER_PASS_PREFILTER_REFLECTIONS:
        {
            FfxShaderBlob blob = denoiserGetPrefilterReflectionsPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_DENOISER_PASS_RESOLVE_TEMPORAL_REFLECTIONS:
        {
            FfxShaderBlob blob = denoiserGetResolveTemporalReflectionsPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
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

FfxErrorCode denoiserIsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, DENOISER_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
