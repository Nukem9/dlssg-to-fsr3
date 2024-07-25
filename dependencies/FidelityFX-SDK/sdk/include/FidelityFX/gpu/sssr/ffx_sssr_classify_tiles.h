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

#include "ffx_sssr_common.h"

FfxBoolean IsBaseRay(FfxUInt32x2 dispatch_thread_id, FfxUInt32 samples_per_quad) {
    switch (samples_per_quad) {
    case 1:
        return ((dispatch_thread_id.x & 1) | (dispatch_thread_id.y & 1)) == 0; // Deactivates 3 out of 4 rays
    case 2:
        return (dispatch_thread_id.x & 1) == (dispatch_thread_id.y & 1); // Deactivates 2 out of 4 rays. Keeps diagonal.
    default: // case 4:
        return true;
    }
}

FFX_GROUPSHARED FfxUInt32 g_TileCount;

void ClassifyTiles(FfxUInt32x2 dispatch_thread_id, FfxUInt32x2 group_thread_id, FfxFloat32 roughness)
{
    g_TileCount = 0;

    FfxBoolean is_first_lane_of_wave = ffxWaveIsFirstLane();

    // First we figure out on a per thread basis if we need to shoot a reflection ray.
    // Disable offscreen pixels
    FfxBoolean needs_ray = !(dispatch_thread_id.x >= RenderSize().x || dispatch_thread_id.y >= RenderSize().y);

    // Dont shoot a ray on very rough surfaces.
    FfxBoolean is_reflective_surface = IsReflectiveSurface(dispatch_thread_id, roughness);
    FfxBoolean is_glossy_reflection = IsGlossyReflection(roughness);
    needs_ray = needs_ray && is_glossy_reflection && is_reflective_surface;

    // Also we dont need to run the denoiser on mirror reflections.
    FfxBoolean needs_denoiser = needs_ray && !IsMirrorReflection(roughness);

    // Decide which ray to keep
    FfxBoolean is_base_ray = IsBaseRay(dispatch_thread_id, SamplesPerQuad());
    needs_ray = needs_ray && (!needs_denoiser || is_base_ray); // Make sure to not deactivate mirror reflection rays.

    if (TemporalVarianceGuidedTracingEnabled() && needs_denoiser && !needs_ray) {
        FfxBoolean has_temporal_variance = FFX_SSSR_LoadVarianceHistory(FfxInt32x3(dispatch_thread_id, 0)) > VarianceThreshold();
        needs_ray = needs_ray || has_temporal_variance;
    }

    FFX_GROUP_MEMORY_BARRIER; // Wait until g_TileCount is cleared - allow some computations before and after

    // Now we know for each thread if it needs to shoot a ray and wether or not a denoiser pass has to run on this pixel.

    if (is_glossy_reflection && is_reflective_surface)
    {
        FFX_ATOMIC_ADD(g_TileCount, 1);
    }

    // Next we have to figure out for which pixels that ray is creating the values for. Thus, if we have to copy its value horizontal, vertical or across.
    FfxBoolean require_copy = !needs_ray && needs_denoiser; // Our pixel only requires a copy if we want to run a denoiser on it but don't want to shoot a ray for it.
    FfxBoolean copy_horizontal  = ffxWaveReadAtLaneIndexB1(require_copy, ffxWaveLaneIndex() ^ 1) && (SamplesPerQuad() != 4) && is_base_ray; // QuadReadAcrossX
    FfxBoolean copy_vertical    = ffxWaveReadAtLaneIndexB1(require_copy, ffxWaveLaneIndex() ^ 2) && (SamplesPerQuad() == 1) && is_base_ray; // QuadReadAcrossY
    FfxBoolean copy_diagonal    = ffxWaveReadAtLaneIndexB1(require_copy, ffxWaveLaneIndex() ^ 3) && (SamplesPerQuad() == 1) && is_base_ray; // QuadReadAcrossDiagonal

    // Thus, we need to compact the rays and append them all at once to the ray list.
    FfxUInt32 local_ray_index_in_wave = ffxWavePrefixCountBits(needs_ray);
    FfxUInt32 wave_ray_count = ffxWaveActiveCountBits(needs_ray);
    FfxUInt32 base_ray_index;
    if (is_first_lane_of_wave) {
        IncrementRayCounter(wave_ray_count, base_ray_index);
    }
    base_ray_index = ffxWaveReadLaneFirstU1(base_ray_index);
    if (needs_ray) {
        FfxInt32 ray_index = FfxInt32(base_ray_index + local_ray_index_in_wave);
        StoreRay(ray_index, dispatch_thread_id, copy_horizontal, copy_vertical, copy_diagonal);
    }

    FfxFloat32x4 intersection_output = FfxFloat32x4(0.0f, 0.0f, 0.0f, 0.0f);
    if (is_reflective_surface && !is_glossy_reflection)
    {
        // Fall back to environment map without preparing a ray
        FfxFloat32x2 uv = (dispatch_thread_id + 0.5) * InverseRenderSize();
        FfxFloat32x3 world_space_normal = FFX_SSSR_LoadWorldSpaceNormal(FfxInt32x2(dispatch_thread_id));
        FfxFloat32  z = FFX_SSSR_LoadDepth(FfxInt32x2(dispatch_thread_id), 0);
        FfxFloat32x3 screen_uv_space_ray_origin = FfxFloat32x3(uv, z);
        FfxFloat32x3 view_space_ray = ScreenSpaceToViewSpace(screen_uv_space_ray_origin);
        FfxFloat32x3 view_space_ray_direction = normalize(view_space_ray);
        FfxFloat32x3 view_space_surface_normal = FFX_MATRIX_MULTIPLY(ViewMatrix(), FfxFloat32x4(world_space_normal, 0)).xyz;
        FfxFloat32x3 view_space_reflected_direction = reflect(view_space_ray_direction, view_space_surface_normal);
        FfxFloat32x3 world_space_reflected_direction = FFX_MATRIX_MULTIPLY(InvView(), FfxFloat32x4(view_space_reflected_direction, 0)).xyz;

        intersection_output.xyz = FFX_SSSR_SampleEnvironmentMap(world_space_reflected_direction, sqrt(roughness));
    }

    FFX_SSSR_StoreRadiance(dispatch_thread_id, intersection_output);

    FFX_GROUP_MEMORY_BARRIER; // Wait until g_TileCount

    if ((group_thread_id.x == 0) && (group_thread_id.y == 0) && g_TileCount > 0)
    {
        FfxUInt32 tile_offset;
        IncrementDenoiserTileCounter(tile_offset);
        StoreDenoiserTile(FfxInt32(tile_offset), FfxInt32x2(dispatch_thread_id.xy));
    }

}
