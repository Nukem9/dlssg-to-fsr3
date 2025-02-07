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
#include "ffx_cacao_shaderblobs.h"
#include "cacao/ffx_cacao_private.h"

#include <ffx_cacao_apply_non_smart_half_pass_permutations.h>
#include <ffx_cacao_apply_non_smart_pass_permutations.h>
#include <ffx_cacao_apply_pass_permutations.h>
#include <ffx_cacao_clear_load_counter_pass_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_1_pass_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_2_pass_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_3_pass_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_4_pass_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_5_pass_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_6_pass_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_7_pass_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_8_pass_permutations.h>
#include <ffx_cacao_generate_importance_map_pass_permutations.h>
#include <ffx_cacao_generate_importance_map_a_pass_permutations.h>
#include <ffx_cacao_generate_importance_map_b_pass_permutations.h>
#include <ffx_cacao_generate_q0_pass_permutations.h>
#include <ffx_cacao_generate_q1_pass_permutations.h>
#include <ffx_cacao_generate_q2_pass_permutations.h>
#include <ffx_cacao_generate_q3_pass_permutations.h>
#include <ffx_cacao_generate_q3_base_pass_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_and_mips_pass_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_half_pass_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_pass_permutations.h>
#include <ffx_cacao_prepare_downsampled_normals_from_input_normals_pass_permutations.h>
#include <ffx_cacao_prepare_downsampled_normals_pass_permutations.h>
#include <ffx_cacao_prepare_native_depths_and_mips_pass_permutations.h>
#include <ffx_cacao_prepare_native_depths_half_pass_permutations.h>
#include <ffx_cacao_prepare_native_depths_pass_permutations.h>
#include <ffx_cacao_prepare_native_normals_from_input_normals_pass_permutations.h>
#include <ffx_cacao_prepare_native_normals_pass_permutations.h>
#include <ffx_cacao_upscale_bilateral_5x5_pass_permutations.h>

#include <ffx_cacao_apply_non_smart_half_pass_wave64_permutations.h>
#include <ffx_cacao_apply_non_smart_pass_wave64_permutations.h>
#include <ffx_cacao_apply_pass_wave64_permutations.h>
#include <ffx_cacao_clear_load_counter_pass_wave64_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_1_pass_wave64_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_2_pass_wave64_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_3_pass_wave64_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_4_pass_wave64_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_5_pass_wave64_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_6_pass_wave64_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_7_pass_wave64_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_8_pass_wave64_permutations.h>
#include <ffx_cacao_generate_importance_map_pass_wave64_permutations.h>
#include <ffx_cacao_generate_importance_map_a_pass_wave64_permutations.h>
#include <ffx_cacao_generate_importance_map_b_pass_wave64_permutations.h>
#include <ffx_cacao_generate_q0_pass_wave64_permutations.h>
#include <ffx_cacao_generate_q1_pass_wave64_permutations.h>
#include <ffx_cacao_generate_q2_pass_wave64_permutations.h>
#include <ffx_cacao_generate_q3_pass_wave64_permutations.h>
#include <ffx_cacao_generate_q3_base_pass_wave64_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_and_mips_pass_wave64_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_half_pass_wave64_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_pass_wave64_permutations.h>
#include <ffx_cacao_prepare_downsampled_normals_from_input_normals_pass_wave64_permutations.h>
#include <ffx_cacao_prepare_downsampled_normals_pass_wave64_permutations.h>
#include <ffx_cacao_prepare_native_depths_and_mips_pass_wave64_permutations.h>
#include <ffx_cacao_prepare_native_depths_half_pass_wave64_permutations.h>
#include <ffx_cacao_prepare_native_depths_pass_wave64_permutations.h>
#include <ffx_cacao_prepare_native_normals_from_input_normals_pass_wave64_permutations.h>
#include <ffx_cacao_prepare_native_normals_pass_wave64_permutations.h>
#include <ffx_cacao_upscale_bilateral_5x5_pass_wave64_permutations.h>

#include <ffx_cacao_apply_non_smart_half_pass_16bit_permutations.h>
#include <ffx_cacao_apply_non_smart_pass_16bit_permutations.h>
#include <ffx_cacao_apply_pass_16bit_permutations.h>
#include <ffx_cacao_clear_load_counter_pass_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_1_pass_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_2_pass_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_3_pass_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_4_pass_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_5_pass_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_6_pass_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_7_pass_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_8_pass_16bit_permutations.h>
#include <ffx_cacao_generate_importance_map_pass_16bit_permutations.h>
#include <ffx_cacao_generate_importance_map_a_pass_16bit_permutations.h>
#include <ffx_cacao_generate_importance_map_b_pass_16bit_permutations.h>
#include <ffx_cacao_generate_q0_pass_16bit_permutations.h>
#include <ffx_cacao_generate_q1_pass_16bit_permutations.h>
#include <ffx_cacao_generate_q2_pass_16bit_permutations.h>
#include <ffx_cacao_generate_q3_pass_16bit_permutations.h>
#include <ffx_cacao_generate_q3_base_pass_16bit_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_and_mips_pass_16bit_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_half_pass_16bit_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_pass_16bit_permutations.h>
#include <ffx_cacao_prepare_downsampled_normals_from_input_normals_pass_16bit_permutations.h>
#include <ffx_cacao_prepare_downsampled_normals_pass_16bit_permutations.h>
#include <ffx_cacao_prepare_native_depths_and_mips_pass_16bit_permutations.h>
#include <ffx_cacao_prepare_native_depths_half_pass_16bit_permutations.h>
#include <ffx_cacao_prepare_native_depths_pass_16bit_permutations.h>
#include <ffx_cacao_prepare_native_normals_from_input_normals_pass_16bit_permutations.h>
#include <ffx_cacao_prepare_native_normals_pass_16bit_permutations.h>
#include <ffx_cacao_upscale_bilateral_5x5_pass_16bit_permutations.h>

#include <ffx_cacao_apply_non_smart_half_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_apply_non_smart_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_apply_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_clear_load_counter_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_1_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_2_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_3_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_4_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_5_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_6_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_7_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_edge_sensitive_blur_8_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_generate_importance_map_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_generate_importance_map_a_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_generate_importance_map_b_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_generate_q0_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_generate_q1_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_generate_q2_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_generate_q3_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_generate_q3_base_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_and_mips_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_half_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_prepare_downsampled_depths_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_prepare_downsampled_normals_from_input_normals_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_prepare_downsampled_normals_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_prepare_native_depths_and_mips_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_prepare_native_depths_half_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_prepare_native_depths_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_prepare_native_normals_from_input_normals_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_prepare_native_normals_pass_wave64_16bit_permutations.h>
#include <ffx_cacao_upscale_bilateral_5x5_pass_wave64_16bit_permutations.h>

#include <string.h> // for memset

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)
#define POPULATE_PERMUTATION_KEY(options, key)                                                                              \
key.index = 0;                                                                                                              \
key.FFX_CACAO_OPTION_APPLY_SMART = FFX_CONTAINS_FLAG(options, CACAO_SHADER_PERMUTATION_APPLY_SMART);                        \


static FfxShaderBlob cacaoGetApplyNonSmartPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_cacao_apply_non_smart_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);
    if (isWave64){
        if (is16bit) {
            const int32_t tableIndex = g_ffx_cacao_apply_non_smart_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_apply_non_smart_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_apply_non_smart_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_apply_non_smart_pass_wave64_PermutationInfo, tableIndex);
        }
    }else{
        if (is16bit) {
            const int32_t tableIndex = g_ffx_cacao_apply_non_smart_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_apply_non_smart_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_apply_non_smart_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_apply_non_smart_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob cacaoGetApplyNonSmartHalfPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_cacao_apply_non_smart_half_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_apply_non_smart_half_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_apply_non_smart_half_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_apply_non_smart_half_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_apply_non_smart_half_pass_wave64_PermutationInfo, tableIndex);
        }
    }else{
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_apply_non_smart_half_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_apply_non_smart_half_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_apply_non_smart_half_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_apply_non_smart_half_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob cacaoGetApplyPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{
    ffx_cacao_apply_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_apply_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_apply_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_apply_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_apply_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetClearLoadCounterPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_clear_load_counter_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_clear_load_counter_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_clear_load_counter_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_clear_load_counter_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_clear_load_counter_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetEdgeSensitiveBlur1PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_edge_sensitive_blur_1_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_1_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_1_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_1_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_1_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetEdgeSensitiveBlur2PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_edge_sensitive_blur_2_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);
    
    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_2_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_2_pass_wave64_PermutationInfo, tableIndex);
    }else{        
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_2_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_2_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetEdgeSensitiveBlur3PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_edge_sensitive_blur_3_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_3_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_3_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_3_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_3_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetEdgeSensitiveBlur4PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_edge_sensitive_blur_4_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_4_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_4_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_4_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_4_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetEdgeSensitiveBlur5PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_edge_sensitive_blur_5_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_5_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_5_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_5_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_5_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetEdgeSensitiveBlur6PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_edge_sensitive_blur_6_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_6_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_6_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_6_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_6_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetEdgeSensitiveBlur7PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_edge_sensitive_blur_7_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_7_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_7_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_7_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_7_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetEdgeSensitiveBlur8PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_edge_sensitive_blur_8_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_8_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_8_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_edge_sensitive_blur_8_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_edge_sensitive_blur_8_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetGenerateImportanceMapPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_generate_importance_map_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_generate_importance_map_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_importance_map_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_generate_importance_map_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_importance_map_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetGenerateImportanceMapAPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_generate_importance_map_a_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_generate_importance_map_a_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_importance_map_a_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_generate_importance_map_a_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_importance_map_a_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetGenerateImportanceMapBPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_generate_importance_map_b_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_generate_importance_map_b_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_importance_map_b_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_generate_importance_map_b_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_importance_map_b_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetGenerateQ0PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_generate_q0_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_generate_q0_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_q0_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_generate_q0_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_q0_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetGenerateQ1PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_generate_q1_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_generate_q1_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_q1_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_generate_q1_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_q1_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetGenerateQ2PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_generate_q2_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_generate_q2_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_q2_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_generate_q2_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_q2_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetGenerateQ3PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_generate_q3_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_generate_q3_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_q3_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_generate_q3_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_q3_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetGenerateQ3BasePassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_generate_q3_base_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_generate_q3_base_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_q3_base_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_generate_q3_base_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_generate_q3_base_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetPrepareDownsampledDepthsAndMipsPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_prepare_downsampled_depths_and_mips_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_depths_and_mips_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_depths_and_mips_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_depths_and_mips_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_depths_and_mips_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetPrepareDownsampledDepthsHalfPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_cacao_prepare_downsampled_depths_half_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_depths_half_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_depths_half_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_depths_half_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_depths_half_pass_wave64_PermutationInfo, tableIndex);
        }
    }else{
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_depths_half_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_depths_half_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_depths_half_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_depths_half_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob cacaoGetPrepareDownsampledDepthsPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_cacao_prepare_downsampled_depths_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_depths_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_depths_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_depths_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_depths_pass_wave64_PermutationInfo, tableIndex);
        }
    }else{
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_depths_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_depths_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_depths_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_depths_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob cacaoGetPrepareDownsampledNormalsFromInputNormalsPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_prepare_downsampled_normals_from_input_normals_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_normals_from_input_normals_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_normals_from_input_normals_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_normals_from_input_normals_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_normals_from_input_normals_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetPrepareDownsampledNormalsPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_prepare_downsampled_normals_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_normals_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_normals_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_prepare_downsampled_normals_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_downsampled_normals_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetPrepareNativeDepthsAndMipsPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_prepare_native_depths_and_mips_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_prepare_native_depths_and_mips_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_depths_and_mips_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_prepare_native_depths_and_mips_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_depths_and_mips_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetPrepareNativeDepthsHalfPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_cacao_prepare_native_depths_half_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_prepare_native_depths_half_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_depths_half_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_prepare_native_depths_half_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_depths_half_pass_wave64_PermutationInfo, tableIndex);
        }
    }else{
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_prepare_native_depths_half_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_depths_half_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_prepare_native_depths_half_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_depths_half_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob cacaoGetPrepareNativeDepthsPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_cacao_prepare_native_depths_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_prepare_native_depths_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_depths_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_prepare_native_depths_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_depths_pass_wave64_PermutationInfo, tableIndex);
        }
    }else{
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_prepare_native_depths_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_depths_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_prepare_native_depths_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_depths_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob cacaoGetPrepareNativeNormalsFromInputNormalsPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_prepare_native_normals_from_input_normals_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_prepare_native_normals_from_input_normals_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_normals_from_input_normals_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_prepare_native_normals_from_input_normals_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_normals_from_input_normals_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetPrepareNativeNormalsPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64)
{

    ffx_cacao_prepare_native_normals_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        const int32_t tableIndex = g_ffx_cacao_prepare_native_normals_pass_wave64_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_normals_pass_wave64_PermutationInfo, tableIndex);
    }else{
        const int32_t tableIndex = g_ffx_cacao_prepare_native_normals_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_prepare_native_normals_pass_PermutationInfo, tableIndex);
    }
}

static FfxShaderBlob cacaoGetUpscaleBilateral5x5PassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{

    ffx_cacao_upscale_bilateral_5x5_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if(isWave64){
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_upscale_bilateral_5x5_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_upscale_bilateral_5x5_pass_wave64_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_upscale_bilateral_5x5_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_upscale_bilateral_5x5_pass_wave64_PermutationInfo, tableIndex);
        }
    }else{
        if (is16bit) {

            const int32_t tableIndex = g_ffx_cacao_upscale_bilateral_5x5_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_upscale_bilateral_5x5_pass_16bit_PermutationInfo, tableIndex);
        } else {

            const int32_t tableIndex = g_ffx_cacao_upscale_bilateral_5x5_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_cacao_upscale_bilateral_5x5_pass_PermutationInfo, tableIndex);
        }
    }
}

FfxErrorCode cacaoGetPermutationBlobByIndex(
    FfxCacaoPass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {

    bool is16bit = FFX_CONTAINS_FLAG(permutationOptions, CACAO_SHADER_PERMUTATION_ALLOW_FP16);
    bool forceWave64 = FFX_CONTAINS_FLAG(permutationOptions, CACAO_SHADER_PERMUTATION_FORCE_WAVE64);

    switch (passId) {

        case FFX_CACAO_PASS_APPLY_NON_SMART_HALF:
        {
            FfxShaderBlob blob = cacaoGetApplyNonSmartHalfPassPermutationBlobByIndex(permutationOptions, forceWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_APPLY_NON_SMART:
        {
            FfxShaderBlob blob = cacaoGetApplyNonSmartPassPermutationBlobByIndex(permutationOptions, forceWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_APPLY:
        {
            FfxShaderBlob blob = cacaoGetApplyPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_CLEAR_LOAD_COUNTER:
        {
            FfxShaderBlob blob = cacaoGetClearLoadCounterPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_1:
        {
            FfxShaderBlob blob = cacaoGetEdgeSensitiveBlur1PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_2:
        {
            FfxShaderBlob blob = cacaoGetEdgeSensitiveBlur2PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_3:
        {
            FfxShaderBlob blob = cacaoGetEdgeSensitiveBlur3PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_4:
        {
            FfxShaderBlob blob = cacaoGetEdgeSensitiveBlur4PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_5:
        {
            FfxShaderBlob blob = cacaoGetEdgeSensitiveBlur5PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_6:
        {
            FfxShaderBlob blob = cacaoGetEdgeSensitiveBlur6PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_7:
        {
            FfxShaderBlob blob = cacaoGetEdgeSensitiveBlur7PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_8:
        {
            FfxShaderBlob blob = cacaoGetEdgeSensitiveBlur8PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_GENERATE_IMPORTANCE_MAP:
        {
            FfxShaderBlob blob = cacaoGetGenerateImportanceMapPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_POST_PROCESS_IMPORTANCE_MAP_A:
        {
            FfxShaderBlob blob = cacaoGetGenerateImportanceMapAPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_POST_PROCESS_IMPORTANCE_MAP_B:
        {
            FfxShaderBlob blob = cacaoGetGenerateImportanceMapBPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_GENERATE_Q0:
        {
            FfxShaderBlob blob = cacaoGetGenerateQ0PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_GENERATE_Q1:
        {
            FfxShaderBlob blob = cacaoGetGenerateQ1PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_GENERATE_Q2:
        {
            FfxShaderBlob blob = cacaoGetGenerateQ2PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_GENERATE_Q3:
        {
            FfxShaderBlob blob = cacaoGetGenerateQ3PassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_GENERATE_Q3_BASE:
        {
            FfxShaderBlob blob = cacaoGetGenerateQ3BasePassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_PREPARE_DOWNSAMPLED_DEPTHS_AND_MIPS:
        {
            FfxShaderBlob blob = cacaoGetPrepareDownsampledDepthsAndMipsPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_PREPARE_DOWNSAMPLED_DEPTHS_HALF:
        {
            FfxShaderBlob blob = cacaoGetPrepareDownsampledDepthsHalfPassPermutationBlobByIndex(permutationOptions, forceWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_PREPARE_DOWNSAMPLED_DEPTHS:
        {
            FfxShaderBlob blob = cacaoGetPrepareDownsampledDepthsPassPermutationBlobByIndex(permutationOptions, forceWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_PREPARE_DOWNSAMPLED_NORMALS_FROM_INPUT_NORMALS:
        {
            FfxShaderBlob blob = cacaoGetPrepareDownsampledNormalsFromInputNormalsPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_PREPARE_DOWNSAMPLED_NORMALS:
        {
            FfxShaderBlob blob = cacaoGetPrepareDownsampledNormalsPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_PREPARE_NATIVE_DEPTHS_AND_MIPS:
        {
            FfxShaderBlob blob = cacaoGetPrepareNativeDepthsAndMipsPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_PREPARE_NATIVE_DEPTHS_HALF:
        {
            FfxShaderBlob blob = cacaoGetPrepareNativeDepthsHalfPassPermutationBlobByIndex(permutationOptions, forceWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_PREPARE_NATIVE_DEPTHS:
        {
            FfxShaderBlob blob = cacaoGetPrepareNativeDepthsPassPermutationBlobByIndex(permutationOptions, forceWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_PREPARE_NATIVE_NORMALS_FROM_INPUT_NORMALS:
        {
            FfxShaderBlob blob = cacaoGetPrepareNativeNormalsFromInputNormalsPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_PREPARE_NATIVE_NORMALS:
        {
            FfxShaderBlob blob = cacaoGetPrepareNativeNormalsPassPermutationBlobByIndex(permutationOptions, forceWave64);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }

        case FFX_CACAO_PASS_UPSCALE_BILATERAL_5X5:
        {
            FfxShaderBlob blob = cacaoGetUpscaleBilateral5x5PassPermutationBlobByIndex(permutationOptions, forceWave64, is16bit);
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

FfxErrorCode cacaoIsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, CACAO_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
