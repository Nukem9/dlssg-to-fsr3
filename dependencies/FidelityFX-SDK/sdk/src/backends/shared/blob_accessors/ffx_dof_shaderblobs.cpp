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
#include "ffx_dof_shaderblobs.h"
#include "dof/ffx_dof_private.h"

#include <ffx_dof_downsample_depth_pass_permutations.h>
#include <ffx_dof_downsample_color_pass_permutations.h>
#include <ffx_dof_dilate_pass_permutations.h>
#include <ffx_dof_blur_pass_permutations.h>
#include <ffx_dof_composite_pass_permutations.h>

#include <ffx_dof_downsample_depth_pass_wave64_permutations.h>
#include <ffx_dof_downsample_color_pass_wave64_permutations.h>
#include <ffx_dof_dilate_pass_wave64_permutations.h>
#include <ffx_dof_blur_pass_wave64_permutations.h>
#include <ffx_dof_composite_pass_wave64_permutations.h>

#include <ffx_dof_downsample_depth_pass_16bit_permutations.h>
#include <ffx_dof_downsample_color_pass_16bit_permutations.h>
#include <ffx_dof_dilate_pass_16bit_permutations.h>
#include <ffx_dof_blur_pass_16bit_permutations.h>
#include <ffx_dof_composite_pass_16bit_permutations.h>

#include <ffx_dof_downsample_depth_pass_wave64_16bit_permutations.h>
#include <ffx_dof_downsample_color_pass_wave64_16bit_permutations.h>
#include <ffx_dof_dilate_pass_wave64_16bit_permutations.h>
#include <ffx_dof_blur_pass_wave64_16bit_permutations.h>
#include <ffx_dof_composite_pass_wave64_16bit_permutations.h>

#include <cstring>  // for memset
#include <array>

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif  // #if defined(POPULATE_PERMUTATION_KEY)
#define POPULATE_PERMUTATION_KEY(options, key)                                                                   \
    key.index                             = 0;                                                                   \
    key.FFX_DOF_OPTION_MAX_RING_MERGE_LOG = FFX_CONTAINS_FLAG(options, DOF_SHADER_PERMUTATION_MERGE_RINGS);      \
    key.FFX_DOF_OPTION_COMBINE_IN_PLACE   = FFX_CONTAINS_FLAG(options, DOF_SHADER_PERMUTATION_COMBINE_IN_PLACE); \
    key.FFX_DOF_OPTION_REVERSE_DEPTH      = FFX_CONTAINS_FLAG(options, DOF_SHADER_PERMUTATION_REVERSE_DEPTH);

static FfxShaderBlob dofGetDsDepthPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_dof_downsample_depth_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_dof_downsample_depth_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_downsample_depth_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_dof_downsample_depth_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_downsample_depth_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_dof_downsample_depth_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_downsample_depth_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_dof_downsample_depth_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_downsample_depth_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob dofGetDsColorPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_dof_downsample_color_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_dof_downsample_color_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_downsample_color_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_dof_downsample_color_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_downsample_color_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_dof_downsample_color_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_downsample_color_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_dof_downsample_color_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_downsample_color_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob dofGetDilatePassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_dof_dilate_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_dof_dilate_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_dilate_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_dof_dilate_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_dilate_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_dof_dilate_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_dilate_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_dof_dilate_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_dilate_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob dofGetBlurPassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_dof_blur_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_dof_blur_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_blur_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_dof_blur_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_blur_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_dof_blur_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_blur_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_dof_blur_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_blur_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob dofGetCompositePassPermutationBlobByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_dof_composite_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_dof_composite_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_composite_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_dof_composite_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_composite_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_dof_composite_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_composite_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_dof_composite_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_dof_composite_pass_PermutationInfo, tableIndex);
        }
    }
}

using FpPassHandler                                              = FfxShaderBlob (*)(uint32_t, bool, bool);
static const FpPassHandler g_pass_getter_map[FFX_DOF_PASS_COUNT] = {dofGetDsDepthPassPermutationBlobByIndex,
                                                                    dofGetDsColorPassPermutationBlobByIndex,
                                                                    dofGetDilatePassPermutationBlobByIndex,
                                                                    dofGetBlurPassPermutationBlobByIndex,
                                                                    dofGetCompositePassPermutationBlobByIndex};

FfxErrorCode dofGetPermutationBlobByIndex(FfxDofPass passId, uint32_t permutationOptions, FfxShaderBlob* outBlob)
{
    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, DOF_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit  = FFX_CONTAINS_FLAG(permutationOptions, DOF_SHADER_PERMUTATION_USE_FP16);

    if (passId >= FFX_DOF_PASS_COUNT)
    {
        FFX_ASSERT_FAIL("Invalid pass id");
        // return an empty blob
        memset(outBlob, 0, sizeof(FfxShaderBlob));
        return FFX_ERROR_INVALID_ENUM;
    }
    FfxShaderBlob blob = (g_pass_getter_map[passId])(permutationOptions, isWave64, is16bit);
    memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
    return FFX_OK;
}

FfxErrorCode dofIsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, DOF_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
