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

#include "ffx_brixelizer_shaderblobs.h"
#include <FidelityFX/host/ffx_util.h>
#include "brixelizer/ffx_brixelizer_raw_private.h"

#include <ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_build_tree_aabb_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_brick_storage_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_build_counters_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_job_counter_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_ref_counters_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_coarse_culling_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_compact_references_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_compress_brick_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_emit_sdf_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_free_cascade_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_initialize_cascade_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_reset_cascade_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_scan_jobs_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_scan_references_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_scroll_cascade_pass_permutations.h>
#include <ffx_brixelizer_cascade_ops_voxelize_pass_permutations.h>
#include <ffx_brixelizer_context_ops_clear_brick_pass_permutations.h>
#include <ffx_brixelizer_context_ops_clear_counters_pass_permutations.h>
#include <ffx_brixelizer_context_ops_collect_clear_bricks_pass_permutations.h>
#include <ffx_brixelizer_context_ops_collect_dirty_bricks_pass_permutations.h>
#include <ffx_brixelizer_context_ops_eikonal_pass_permutations.h>
#include <ffx_brixelizer_context_ops_merge_bricks_pass_permutations.h>
#include <ffx_brixelizer_context_ops_merge_cascades_pass_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_clear_bricks_pass_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_eikonal_args_pass_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_permutations.h>
#include <ffx_brixelizer_debug_visualization_pass_permutations.h>
#include <ffx_brixelizer_debug_draw_instance_aabbs_permutations.h>
#include <ffx_brixelizer_debug_draw_aabb_tree_permutations.h>

#include <ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_build_tree_aabb_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_brick_storage_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_build_counters_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_job_counter_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_ref_counters_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_coarse_culling_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_compact_references_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_compress_brick_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_emit_sdf_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_free_cascade_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_initialize_cascade_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_reset_cascade_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_scan_jobs_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_scan_references_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_scroll_cascade_pass_wave64_permutations.h>
#include <ffx_brixelizer_cascade_ops_voxelize_pass_wave64_permutations.h>
#include <ffx_brixelizer_context_ops_clear_brick_pass_wave64_permutations.h>
#include <ffx_brixelizer_context_ops_clear_counters_pass_wave64_permutations.h>
#include <ffx_brixelizer_context_ops_collect_clear_bricks_pass_wave64_permutations.h>
#include <ffx_brixelizer_context_ops_collect_dirty_bricks_pass_wave64_permutations.h>
#include <ffx_brixelizer_context_ops_eikonal_pass_wave64_permutations.h>
#include <ffx_brixelizer_context_ops_merge_bricks_pass_wave64_permutations.h>
#include <ffx_brixelizer_context_ops_merge_cascades_pass_wave64_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_clear_bricks_pass_wave64_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_eikonal_args_pass_wave64_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_wave64_permutations.h>
#include <ffx_brixelizer_debug_visualization_pass_wave64_permutations.h>
#include <ffx_brixelizer_debug_draw_instance_aabbs_wave64_permutations.h>
#include <ffx_brixelizer_debug_draw_aabb_tree_wave64_permutations.h>

#include <ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_build_tree_aabb_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_brick_storage_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_build_counters_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_job_counter_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_ref_counters_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_coarse_culling_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_compact_references_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_compress_brick_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_emit_sdf_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_free_cascade_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_initialize_cascade_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_reset_cascade_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_scan_jobs_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_scan_references_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_scroll_cascade_pass_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_voxelize_pass_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_clear_brick_pass_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_clear_counters_pass_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_collect_clear_bricks_pass_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_collect_dirty_bricks_pass_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_eikonal_pass_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_merge_bricks_pass_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_merge_cascades_pass_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_clear_bricks_pass_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_eikonal_args_pass_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_16bit_permutations.h>
#include <ffx_brixelizer_debug_visualization_pass_16bit_permutations.h>
#include <ffx_brixelizer_debug_draw_instance_aabbs_16bit_permutations.h>
#include <ffx_brixelizer_debug_draw_aabb_tree_16bit_permutations.h>

#include <ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_build_tree_aabb_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_brick_storage_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_build_counters_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_job_counter_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_clear_ref_counters_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_coarse_culling_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_compact_references_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_compress_brick_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_emit_sdf_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_free_cascade_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_initialize_cascade_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_reset_cascade_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_scan_jobs_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_scan_references_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_scroll_cascade_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_cascade_ops_voxelize_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_clear_brick_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_clear_counters_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_collect_clear_bricks_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_collect_dirty_bricks_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_eikonal_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_merge_bricks_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_merge_cascades_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_clear_bricks_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_eikonal_args_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_debug_visualization_pass_wave64_16bit_permutations.h>
#include <ffx_brixelizer_debug_draw_instance_aabbs_wave64_16bit_permutations.h>
#include <ffx_brixelizer_debug_draw_aabb_tree_wave64_16bit_permutations.h>

#include <string.h> // for memset

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)

#define POPULATE_PERMUTATION_KEY(key) key.index = 0;                           

static FfxShaderBlob brixelizerGetCascadeMarkCascadeUninitializedPassPermutationByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_mark_cascade_uninitialized_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeBuildTreeAABBPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_build_tree_aabb_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_build_tree_aabb_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_build_tree_aabb_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_build_tree_aabb_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_build_tree_aabb_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_build_tree_aabb_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_build_tree_aabb_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_build_tree_aabb_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_build_tree_aabb_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeClearBrickStoragePassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_build_tree_aabb_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_brick_storage_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_brick_storage_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_brick_storage_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_brick_storage_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_brick_storage_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_brick_storage_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_brick_storage_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_brick_storage_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeClearBuildCountersPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_clear_build_counters_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_build_counters_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_build_counters_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_build_counters_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_build_counters_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_build_counters_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_build_counters_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_build_counters_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_build_counters_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeClearJobCounterPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_clear_job_counter_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_job_counter_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_job_counter_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_job_counter_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_job_counter_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_job_counter_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_job_counter_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_job_counter_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_job_counter_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeClearRefCountersPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_clear_ref_counters_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_ref_counters_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_ref_counters_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_ref_counters_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_ref_counters_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_ref_counters_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_ref_counters_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_clear_ref_counters_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_clear_ref_counters_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeCoarseCullingPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_coarse_culling_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_coarse_culling_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_coarse_culling_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_coarse_culling_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_coarse_culling_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_coarse_culling_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_coarse_culling_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_coarse_culling_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_coarse_culling_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeCompactReferencesPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_compact_references_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_compact_references_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_compact_references_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_compact_references_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_compact_references_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_compact_references_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_compact_references_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_compact_references_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_compact_references_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeCompressBrickPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_compress_brick_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_compress_brick_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_compress_brick_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_compress_brick_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_compress_brick_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_compress_brick_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_compress_brick_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_compress_brick_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_compress_brick_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeEmitSDFPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_emit_sdf_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_emit_sdf_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_emit_sdf_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_emit_sdf_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_emit_sdf_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_emit_sdf_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_emit_sdf_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_emit_sdf_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_emit_sdf_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeFreeCascadePassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_free_cascade_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_free_cascade_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_free_cascade_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_free_cascade_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_free_cascade_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_free_cascade_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_free_cascade_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_free_cascade_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_free_cascade_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeInitializeCascadePassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_initialize_cascade_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_initialize_cascade_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_initialize_cascade_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_initialize_cascade_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_initialize_cascade_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_initialize_cascade_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_initialize_cascade_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_initialize_cascade_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_initialize_cascade_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeInvalidateJobAreasPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_invalidate_job_areas_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeResetCascadePassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_reset_cascade_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_reset_cascade_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_reset_cascade_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_reset_cascade_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_reset_cascade_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_reset_cascade_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_reset_cascade_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_reset_cascade_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_reset_cascade_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeScanJobsPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_scan_jobs_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scan_jobs_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scan_jobs_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scan_jobs_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scan_jobs_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scan_jobs_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scan_jobs_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scan_jobs_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scan_jobs_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeScanReferencesPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_scan_references_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scan_references_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scan_references_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scan_references_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scan_references_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scan_references_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scan_references_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scan_references_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scan_references_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeScrollCascadePassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_scroll_cascade_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scroll_cascade_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scroll_cascade_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scroll_cascade_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scroll_cascade_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scroll_cascade_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scroll_cascade_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_scroll_cascade_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_scroll_cascade_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetCascadeVoxelizePassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_cascade_ops_voxelize_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_voxelize_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_voxelize_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_voxelize_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_voxelize_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_voxelize_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_voxelize_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_cascade_ops_voxelize_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_cascade_ops_voxelize_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextClearBrickPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_context_ops_clear_brick_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_clear_brick_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_clear_brick_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_clear_brick_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_clear_brick_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_clear_brick_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_clear_brick_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_clear_brick_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_clear_brick_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextClearCountersPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_context_ops_clear_counters_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_clear_counters_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_clear_counters_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_clear_counters_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_clear_counters_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_clear_counters_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_clear_counters_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_clear_counters_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_clear_counters_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextCollectClearBricksPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_context_ops_collect_clear_bricks_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_collect_clear_bricks_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_collect_clear_bricks_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_collect_clear_bricks_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_collect_clear_bricks_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_collect_clear_bricks_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_collect_clear_bricks_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_collect_clear_bricks_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_collect_clear_bricks_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextCollectDirtyBricksPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_context_ops_collect_dirty_bricks_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_collect_dirty_bricks_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_collect_dirty_bricks_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_collect_dirty_bricks_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_collect_dirty_bricks_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_collect_dirty_bricks_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_collect_dirty_bricks_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_collect_dirty_bricks_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_collect_dirty_bricks_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextEikonalPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_context_ops_eikonal_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_eikonal_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_eikonal_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_eikonal_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_eikonal_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_eikonal_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_eikonal_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_eikonal_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_eikonal_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextMergeBricksPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_context_ops_merge_bricks_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_merge_bricks_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_merge_bricks_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_merge_bricks_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_merge_bricks_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_merge_bricks_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_merge_bricks_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_merge_bricks_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_merge_bricks_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextMergeCascadesPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_context_ops_merge_cascades_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_merge_cascades_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_merge_cascades_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_merge_cascades_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_merge_cascades_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_merge_cascades_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_merge_cascades_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_merge_cascades_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_merge_cascades_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextPrepareClearBricksPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_context_ops_prepare_clear_bricks_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_clear_bricks_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_clear_bricks_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_clear_bricks_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_clear_bricks_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_clear_bricks_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_clear_bricks_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_clear_bricks_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_clear_bricks_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextPrepareEikonalArgsPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_context_ops_prepare_eikonal_args_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_eikonal_args_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_eikonal_args_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_eikonal_args_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_eikonal_args_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_eikonal_args_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_eikonal_args_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_eikonal_args_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_eikonal_args_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextPrepareMergeBricksArgsPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_context_ops_prepare_merge_bricks_args_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetContextDebugVisualizationPassPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{

    ffx_brixelizer_debug_visualization_pass_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_visualization_pass_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_visualization_pass_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_visualization_pass_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_visualization_pass_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_visualization_pass_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_visualization_pass_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_visualization_pass_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_visualization_pass_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetDebugDrawInstanceAABBsPermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{
    ffx_brixelizer_debug_draw_instance_aabbs_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_draw_instance_aabbs_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_draw_instance_aabbs_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_draw_instance_aabbs_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_draw_instance_aabbs_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_draw_instance_aabbs_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_draw_instance_aabbs_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_draw_instance_aabbs_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_draw_instance_aabbs_PermutationInfo, tableIndex);
        }
    }
}

static FfxShaderBlob brixelizerGetDebugDrawAABBTreePermutationBlobByIndex(uint32_t, bool isWave64, bool is16bit)
{
    ffx_brixelizer_debug_draw_aabb_tree_PermutationKey key;
    POPULATE_PERMUTATION_KEY(key);

    if (isWave64)
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_draw_aabb_tree_wave64_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_draw_aabb_tree_wave64_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_draw_aabb_tree_wave64_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_draw_aabb_tree_wave64_PermutationInfo, tableIndex);
        }
    }
    else
    {
        if (is16bit)
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_draw_aabb_tree_16bit_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_draw_aabb_tree_16bit_PermutationInfo, tableIndex);
        }
        else
        {
            const int32_t tableIndex = g_ffx_brixelizer_debug_draw_aabb_tree_IndirectionTable[key.index];
            return POPULATE_SHADER_BLOB_FFX(g_ffx_brixelizer_debug_draw_aabb_tree_PermutationInfo, tableIndex);
        }
    }
}


FfxErrorCode brixelizerGetPermutationBlobByIndex(
    FfxBrixelizerPass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {
    
    bool isWave64 = FFX_CONTAINS_FLAG(permutationOptions, BRIXELIZER_SHADER_PERMUTATION_FORCE_WAVE64);
    bool is16bit  = FFX_CONTAINS_FLAG(permutationOptions, BRIXELIZER_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId) {

        case FFX_BRIXELIZER_PASS_CASCADE_MARK_UNINITIALIZED: 
        {
            FfxShaderBlob blob = brixelizerGetCascadeMarkCascadeUninitializedPassPermutationByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_BUILD_TREE_AABB: 
        {
            FfxShaderBlob blob = brixelizerGetCascadeBuildTreeAABBPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_CLEAR_BRICK_STORAGE:
        {
            FfxShaderBlob blob = brixelizerGetCascadeClearBrickStoragePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_CLEAR_BUILD_COUNTERS:
        {
            FfxShaderBlob blob = brixelizerGetCascadeClearBuildCountersPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_CLEAR_JOB_COUNTER:
        {
            FfxShaderBlob blob = brixelizerGetCascadeClearJobCounterPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_CLEAR_REF_COUNTERS:
        {
            FfxShaderBlob blob = brixelizerGetCascadeClearRefCountersPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_COARSE_CULLING:
        {
            FfxShaderBlob blob = brixelizerGetCascadeCoarseCullingPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_COMPACT_REFERENCES:
        {
            FfxShaderBlob blob = brixelizerGetCascadeCompactReferencesPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_COMPRESS_BRICK:
        {
            FfxShaderBlob blob = brixelizerGetCascadeCompressBrickPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_EMIT_SDF:
        {
            FfxShaderBlob blob = brixelizerGetCascadeEmitSDFPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_FREE_CASCADE:
        {
            FfxShaderBlob blob = brixelizerGetCascadeFreeCascadePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_INITIALIZE_CASCADE:
        {
            FfxShaderBlob blob = brixelizerGetCascadeInitializeCascadePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_INVALIDATE_JOB_AREAS:
        {
            FfxShaderBlob blob = brixelizerGetCascadeInvalidateJobAreasPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_RESET_CASCADE:
        {
            FfxShaderBlob blob = brixelizerGetCascadeResetCascadePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_SCAN_JOBS:
        {
            FfxShaderBlob blob = brixelizerGetCascadeScanJobsPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_SCAN_REFERENCES:
        {
            FfxShaderBlob blob = brixelizerGetCascadeScanReferencesPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_SCROLL_CASCADE:
        {
            FfxShaderBlob blob = brixelizerGetCascadeScrollCascadePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CASCADE_VOXELIZE:
        {
            FfxShaderBlob blob = brixelizerGetCascadeVoxelizePassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CONTEXT_CLEAR_BRICK:
        {
            FfxShaderBlob blob = brixelizerGetContextClearBrickPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CONTEXT_CLEAR_COUNTERS:
        {
            FfxShaderBlob blob = brixelizerGetContextClearCountersPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CONTEXT_COLLECT_CLEAR_BRICKS:
        {
            FfxShaderBlob blob = brixelizerGetContextCollectClearBricksPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CONTEXT_COLLECT_DIRTY_BRICKS:
        {
            FfxShaderBlob blob = brixelizerGetContextCollectDirtyBricksPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CONTEXT_EIKONAL:
        {
            FfxShaderBlob blob = brixelizerGetContextEikonalPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CONTEXT_MERGE_BRICKS:
        {
            FfxShaderBlob blob = brixelizerGetContextMergeBricksPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CONTEXT_MERGE_CASCADES:
        {
            FfxShaderBlob blob = brixelizerGetContextMergeCascadesPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_CLEAR_BRICKS:
        {
            FfxShaderBlob blob = brixelizerGetContextPrepareClearBricksPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_EIKONAL_ARGS:
        {
            FfxShaderBlob blob = brixelizerGetContextPrepareEikonalArgsPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_MERGE_BRICKS_ARGS:
        {
            FfxShaderBlob blob = brixelizerGetContextPrepareMergeBricksArgsPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_DEBUG_VISUALIZATION:
        {
            FfxShaderBlob blob = brixelizerGetContextDebugVisualizationPassPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_DEBUG_INSTANCE_AABBS:
        {
            FfxShaderBlob blob = brixelizerGetDebugDrawInstanceAABBsPermutationBlobByIndex(permutationOptions, isWave64, is16bit);
            memcpy(outBlob, &blob, sizeof(FfxShaderBlob));
            return FFX_OK;
        }
        case FFX_BRIXELIZER_PASS_DEBUG_AABB_TREE:
        {
            FfxShaderBlob blob = brixelizerGetDebugDrawAABBTreePermutationBlobByIndex(permutationOptions, isWave64, is16bit);
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

FfxErrorCode brixelizerIsWave64(uint32_t permutationOptions, bool& isWave64)
{
    isWave64 = FFX_CONTAINS_FLAG(permutationOptions, BRIXELIZER_SHADER_PERMUTATION_FORCE_WAVE64);
    return FFX_OK;
}
