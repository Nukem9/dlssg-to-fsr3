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

#include "ffx_brixelizergi_shaderblobs.h"

#include <FidelityFX/host/ffx_util.h>
#include <FidelityFX/gpu/brixelizergi/ffx_brixelizergi_resources.h>
#include "brixelizergi/ffx_brixelizergi_private.h"

#include <ffx_brixelizergi_blur_x_permutations.h>
#include <ffx_brixelizergi_blur_y_permutations.h>
#include <ffx_brixelizergi_clear_cache_permutations.h>
#include <ffx_brixelizergi_emit_irradiance_cache_permutations.h>
#include <ffx_brixelizergi_emit_primary_ray_radiance_permutations.h>
#include <ffx_brixelizergi_fill_screen_probes_permutations.h>
#include <ffx_brixelizergi_interpolate_screen_probes_permutations.h>
#include <ffx_brixelizergi_prepare_clear_cache_permutations.h>
#include <ffx_brixelizergi_project_screen_probes_permutations.h>
#include <ffx_brixelizergi_propagate_sh_permutations.h>
#include <ffx_brixelizergi_reproject_gi_permutations.h>
#include <ffx_brixelizergi_reproject_screen_probes_permutations.h>
#include <ffx_brixelizergi_spawn_screen_probes_permutations.h>
#include <ffx_brixelizergi_specular_pre_trace_permutations.h>
#include <ffx_brixelizergi_specular_trace_permutations.h>
#include <ffx_brixelizergi_debug_visualization_permutations.h>
#include <ffx_brixelizergi_generate_disocclusion_mask_permutations.h>
#include <ffx_brixelizergi_downsample_permutations.h>
#include <ffx_brixelizergi_upsample_permutations.h>

#include <ffx_brixelizergi_blur_x_wave64_permutations.h>
#include <ffx_brixelizergi_blur_y_wave64_permutations.h>
#include <ffx_brixelizergi_clear_cache_wave64_permutations.h>
#include <ffx_brixelizergi_emit_irradiance_cache_wave64_permutations.h>
#include <ffx_brixelizergi_emit_primary_ray_radiance_wave64_permutations.h>
#include <ffx_brixelizergi_fill_screen_probes_wave64_permutations.h>
#include <ffx_brixelizergi_interpolate_screen_probes_wave64_permutations.h>
#include <ffx_brixelizergi_prepare_clear_cache_wave64_permutations.h>
#include <ffx_brixelizergi_project_screen_probes_wave64_permutations.h>
#include <ffx_brixelizergi_propagate_sh_wave64_permutations.h>
#include <ffx_brixelizergi_reproject_gi_wave64_permutations.h>
#include <ffx_brixelizergi_reproject_screen_probes_wave64_permutations.h>
#include <ffx_brixelizergi_spawn_screen_probes_wave64_permutations.h>
#include <ffx_brixelizergi_specular_pre_trace_wave64_permutations.h>
#include <ffx_brixelizergi_specular_trace_wave64_permutations.h>
#include <ffx_brixelizergi_debug_visualization_wave64_permutations.h>
#include <ffx_brixelizergi_generate_disocclusion_mask_wave64_permutations.h>
#include <ffx_brixelizergi_downsample_wave64_permutations.h>
#include <ffx_brixelizergi_upsample_wave64_permutations.h>

#include <ffx_brixelizergi_blur_x_16bit_permutations.h>
#include <ffx_brixelizergi_blur_y_16bit_permutations.h>
#include <ffx_brixelizergi_clear_cache_16bit_permutations.h>
#include <ffx_brixelizergi_emit_irradiance_cache_16bit_permutations.h>
#include <ffx_brixelizergi_emit_primary_ray_radiance_16bit_permutations.h>
#include <ffx_brixelizergi_fill_screen_probes_16bit_permutations.h>
#include <ffx_brixelizergi_interpolate_screen_probes_16bit_permutations.h>
#include <ffx_brixelizergi_prepare_clear_cache_16bit_permutations.h>
#include <ffx_brixelizergi_project_screen_probes_16bit_permutations.h>
#include <ffx_brixelizergi_propagate_sh_16bit_permutations.h>
#include <ffx_brixelizergi_reproject_gi_16bit_permutations.h>
#include <ffx_brixelizergi_reproject_screen_probes_16bit_permutations.h>
#include <ffx_brixelizergi_spawn_screen_probes_16bit_permutations.h>
#include <ffx_brixelizergi_specular_pre_trace_16bit_permutations.h>
#include <ffx_brixelizergi_specular_trace_16bit_permutations.h>
#include <ffx_brixelizergi_debug_visualization_16bit_permutations.h>
#include <ffx_brixelizergi_generate_disocclusion_mask_16bit_permutations.h>
#include <ffx_brixelizergi_downsample_16bit_permutations.h>
#include <ffx_brixelizergi_upsample_16bit_permutations.h>

#include <ffx_brixelizergi_blur_x_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_blur_y_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_clear_cache_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_emit_irradiance_cache_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_emit_primary_ray_radiance_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_fill_screen_probes_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_interpolate_screen_probes_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_prepare_clear_cache_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_project_screen_probes_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_propagate_sh_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_reproject_gi_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_reproject_screen_probes_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_spawn_screen_probes_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_specular_pre_trace_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_specular_trace_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_debug_visualization_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_generate_disocclusion_mask_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_downsample_wave64_16bit_permutations.h>
#include <ffx_brixelizergi_upsample_wave64_16bit_permutations.h>

#include <string.h> // for memset

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)

#define POPULATE_PERMUTATION_KEY(options, key)                                                                                 \
key.index = 0;                                                                                                                 \
key.FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED   = FFX_CONTAINS_FLAG(options, BRIXELIZER_GI_SHADER_PERMUTATION_DEPTH_INVERTED);   \
key.FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR = FFX_CONTAINS_FLAG(options, BRIXELIZER_GI_SHADER_PERMUTATION_DISABLE_SPECULAR); \
key.FFX_BRIXELIZER_GI_OPTION_DISABLE_DENOISER = FFX_CONTAINS_FLAG(options, BRIXELIZER_GI_SHADER_PERMUTATION_DISABLE_DENOISER);  

static FfxShaderBlob brixelizerGIGetBlurXPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_blur_x_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_blur_x_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_blur_x_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_blur_x_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_blur_x_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_blur_x_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_blur_x_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_blur_x_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_blur_x_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetBlurYPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_blur_y_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_blur_y_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_blur_y_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_blur_y_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_blur_y_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_blur_y_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_blur_y_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_blur_y_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_blur_y_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetClearCachePassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_clear_cache_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_clear_cache_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_clear_cache_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_clear_cache_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_clear_cache_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_clear_cache_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_clear_cache_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_clear_cache_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_clear_cache_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetEmitIrradianceCachePassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_emit_irradiance_cache_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_emit_irradiance_cache_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_emit_irradiance_cache_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_emit_irradiance_cache_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_emit_irradiance_cache_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_emit_irradiance_cache_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_emit_irradiance_cache_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_emit_irradiance_cache_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_emit_irradiance_cache_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetEmitPrimaryRayRadiancePassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_emit_primary_ray_radiance_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_emit_primary_ray_radiance_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_emit_primary_ray_radiance_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_emit_primary_ray_radiance_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_emit_primary_ray_radiance_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_emit_primary_ray_radiance_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_emit_primary_ray_radiance_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_emit_primary_ray_radiance_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_emit_primary_ray_radiance_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetFillScreenProbesPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_fill_screen_probes_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_fill_screen_probes_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_fill_screen_probes_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_fill_screen_probes_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_fill_screen_probes_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_fill_screen_probes_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_fill_screen_probes_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_fill_screen_probes_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_fill_screen_probes_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetInterpolateScreenProbesPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_interpolate_screen_probes_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_interpolate_screen_probes_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_interpolate_screen_probes_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_interpolate_screen_probes_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_interpolate_screen_probes_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_interpolate_screen_probes_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_interpolate_screen_probes_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_interpolate_screen_probes_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_interpolate_screen_probes_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetPrepareClearCachePassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_prepare_clear_cache_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_prepare_clear_cache_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_prepare_clear_cache_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_prepare_clear_cache_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_prepare_clear_cache_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_prepare_clear_cache_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_prepare_clear_cache_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_prepare_clear_cache_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_prepare_clear_cache_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetProjectScreenProbesPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_project_screen_probes_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_project_screen_probes_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_project_screen_probes_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_project_screen_probes_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_project_screen_probes_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_project_screen_probes_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_project_screen_probes_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_project_screen_probes_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_project_screen_probes_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetPropagateSHPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_propagate_sh_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_propagate_sh_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_propagate_sh_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_propagate_sh_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_propagate_sh_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_propagate_sh_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_propagate_sh_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_propagate_sh_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_propagate_sh_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetReprojectGIPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_reproject_gi_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_reproject_gi_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_reproject_gi_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_reproject_gi_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_reproject_gi_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_reproject_gi_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_reproject_gi_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_reproject_gi_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_reproject_gi_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetReprojectScreenProbesPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_reproject_screen_probes_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_reproject_screen_probes_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_reproject_screen_probes_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_reproject_screen_probes_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_reproject_screen_probes_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_reproject_screen_probes_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_reproject_screen_probes_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_reproject_screen_probes_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_reproject_screen_probes_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetSpawnScreenProbesPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_spawn_screen_probes_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_spawn_screen_probes_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_spawn_screen_probes_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_spawn_screen_probes_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_spawn_screen_probes_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_spawn_screen_probes_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_spawn_screen_probes_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_spawn_screen_probes_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_spawn_screen_probes_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetSpecularPreTracePassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_specular_pre_trace_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_specular_pre_trace_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_specular_pre_trace_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_specular_pre_trace_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_specular_pre_trace_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_specular_pre_trace_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_specular_pre_trace_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_specular_pre_trace_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_specular_pre_trace_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetSpecularTracePassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_specular_trace_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_specular_trace_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_specular_trace_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_specular_trace_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_specular_trace_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_specular_trace_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_specular_trace_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_specular_trace_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_specular_trace_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetDebugVisualizationPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_debug_visualization_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_debug_visualization_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_debug_visualization_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_debug_visualization_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_debug_visualization_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_debug_visualization_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_debug_visualization_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_debug_visualization_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_debug_visualization_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetGenerateDisocclusionMaskPassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_generate_disocclusion_mask_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_generate_disocclusion_mask_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_generate_disocclusion_mask_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_generate_disocclusion_mask_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_generate_disocclusion_mask_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_generate_disocclusion_mask_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_generate_disocclusion_mask_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_generate_disocclusion_mask_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_generate_disocclusion_mask_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetDownsamplePassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_downsample_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_downsample_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_downsample_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_downsample_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_downsample_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_downsample_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_downsample_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_downsample_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_downsample_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGIGetUpsamplePassPermutationByIndex(uint32_t permutationOptions, bool isWave64, bool is16bit)
{
    ffx_brixelizergi_upsample_PermutationKey key;
    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_upsample_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_upsample_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_upsample_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_upsample_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizergi_upsample_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_upsample_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizergi_upsample_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizergi_upsample_PermutationInfo, tableIndex);
        }
    }
}

FfxErrorCode brixelizerGIGetPermutationBlobByIndex(
    FfxBrixelizerGIPass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {

    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, BRIXELIZER_GI_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit  = FFX_CONTAINS_FLAG(permutationOptions, BRIXELIZER_GI_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId) {

        case FFX_BRIXELIZER_GI_PASS_BLUR_X:
        {
            FfxShaderBlob blob = brixelizerGIGetBlurXPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_BLUR_Y:
        {
            FfxShaderBlob blob = brixelizerGIGetBlurYPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_CLEAR_CACHE:
        {
            FfxShaderBlob blob = brixelizerGIGetClearCachePassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_EMIT_IRRADIANCE_CACHE:
        {
            FfxShaderBlob blob = brixelizerGIGetEmitIrradianceCachePassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_EMIT_PRIMARY_RAY_RADIANCE:
        {
            FfxShaderBlob blob = brixelizerGIGetEmitPrimaryRayRadiancePassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_FILL_SCREEN_PROBES:
        {
            FfxShaderBlob blob = brixelizerGIGetFillScreenProbesPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_INTERPOLATE_SCREEN_PROBES:
        {
            FfxShaderBlob blob = brixelizerGIGetInterpolateScreenProbesPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_PREPARE_CLEAR_CACHE:
        {
            FfxShaderBlob blob = brixelizerGIGetPrepareClearCachePassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_PROJECT_SCREEN_PROBES:
        {
            FfxShaderBlob blob = brixelizerGIGetProjectScreenProbesPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_PROPAGATE_SH:
        {
            FfxShaderBlob blob = brixelizerGIGetPropagateSHPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_REPROJECT_GI:
        {
            FfxShaderBlob blob = brixelizerGIGetReprojectGIPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_REPROJECT_SCREEN_PROBES:
        {
            FfxShaderBlob blob = brixelizerGIGetReprojectScreenProbesPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_SPAWN_SCREEN_PROBES:
        {
            FfxShaderBlob blob = brixelizerGIGetSpawnScreenProbesPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_SPECULAR_PRE_TRACE:
        {
            FfxShaderBlob blob = brixelizerGIGetSpecularPreTracePassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_SPECULAR_TRACE:
        {
            FfxShaderBlob blob = brixelizerGIGetSpecularTracePassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_DEBUG_VISUALIZATION:
        {
            FfxShaderBlob blob = brixelizerGIGetDebugVisualizationPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_GENERATE_DISOCCLUSION_MASK:
        {
            FfxShaderBlob blob = brixelizerGIGetGenerateDisocclusionMaskPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_DOWNSAMPLE:
        {
            FfxShaderBlob blob = brixelizerGIGetDownsamplePassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_GI_PASS_UPSAMPLE:
        {
            FfxShaderBlob blob = brixelizerGIGetUpsamplePassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        default:
            FFX_ASSERT_FAIL("Should never reach here.");
            break;
    }

    // return an empty blob
    memset(&outBlob, 0, sizeof(FfxShaderBlob));
    return FFX_OK;
}

FfxErrorCode brixelizerGIIsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, BRIXELIZER_GI_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
