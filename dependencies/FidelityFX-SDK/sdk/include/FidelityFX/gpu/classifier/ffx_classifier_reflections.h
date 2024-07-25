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

#define TILE_CLASS_FULL_SW 0
#define TILE_CLASS_HALF_SW 1
#define TILE_CLASS_FULL_HW 2

#define FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED

#include "ffx_classifier_reflections_common.h"

FfxFloat32x2 Hash22(FfxFloat32x2 p)
{
    FfxFloat32x3 p3 = ffxFract(FfxFloat32x3(p.xyx) * FfxFloat32x3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return ffxFract((p3.xx + p3.yz) * p3.zy);
}

FfxFloat32x2 GetRandom(FfxUInt32x2 index)
{
    FfxFloat32   v   = 0.152f;
    FfxFloat32x2 pos = (FfxFloat32x2(index) * v + FfxFloat32(FrameIndex()) / 60.0f * 1500.0f + 50.0f);
    return Hash22(pos);
}

FfxFloat32x2 GetRandomLastFrame(FfxUInt32x2 index)
{
    FfxFloat32   v   = 0.152f;
    FfxFloat32x2 pos = (FfxFloat32x2(index) * v + FfxFloat32(FrameIndex() - 1) / 60.0f * 1500.0f + 50.0f);
    return Hash22(pos);
}

FfxBoolean IsSW(FfxFloat32 hitcounter, FfxFloat32 misscounter, FfxFloat32 rnd)
{
    // Turn a random tile full hybrid once in a while to get the opportunity for testing HiZ traversal
    return rnd <= (+HybridSpawnRate() + hitcounter - misscounter * HybridMissWeight());
}

FfxBoolean IsConverged(FfxUInt32x2 pixel_coordinate, FfxFloat32x2 uv)
{
    FfxFloat32x2 motion_vector = LoadMotionVector(FfxInt32x2(pixel_coordinate));
    ;
    return SampleVarianceHistory(uv - motion_vector) < VRTVarianceThreshold();
}

// In case no ray is traced we need to clear the buffers
void FillEnvironment(FfxUInt32x2 ray_coord, FfxFloat32 factor)
{
    // Fall back to the environment probe
    FfxUInt32x2  screen_size                     = FfxUInt32x2(ReflectionWidth(), ReflectionHeight());
    FfxFloat32x2 uv                              = (ray_coord + 0.5) * InverseRenderSize();
    FfxFloat32x3 world_space_normal              = LoadWorldSpaceNormal(FfxInt32x2(ray_coord));
    FfxFloat32   roughness                       = LoadRoughnessFromMaterialParametersInput(FfxUInt32x3(ray_coord, 0));
    FfxFloat32   z                               = GetInputDepth(ray_coord);
    FfxFloat32x3 screen_uv_space_ray_origin      = FfxFloat32x3(uv, z);
    FfxFloat32x3 view_space_ray                  = ScreenSpaceToViewSpace(screen_uv_space_ray_origin);
    FfxFloat32x3 view_space_ray_direction        = normalize(view_space_ray);
    FfxFloat32x3 view_space_surface_normal       = FFX_MATRIX_MULTIPLY(ViewMatrix(), FfxFloat32x4(world_space_normal, 0)).xyz;
    FfxFloat32x3 view_space_reflected_direction  = reflect(view_space_ray_direction, view_space_surface_normal);
    FfxFloat32x3 world_space_reflected_direction = FFX_MATRIX_MULTIPLY(InvView(), FfxFloat32x4(view_space_reflected_direction, 0)).xyz;
    FfxFloat32x3 world_space_ray_origin          = FFX_MATRIX_MULTIPLY(InvView(), FfxFloat32x4(view_space_ray, 1)).xyz;

    FfxFloat32x3 env_sample = SampleEnvironmentMap(world_space_reflected_direction, sqrt(roughness));

    if (!any(isnan(env_sample)))
        StoreRadiance(ray_coord, env_sample.xyzz * factor);
    else
        StoreRadiance(ray_coord, (0.0f).xxxx);
}

void ZeroBuffers(FfxUInt32x2 dispatch_thread_id)
{
    StoreRadiance(dispatch_thread_id, (0.0f).xxxx);
}

FfxFloat32x2 GetSurfaceReprojection(FfxFloat32x2 uv, FfxFloat32x2 motion_vector)
{
    // Reflector position reprojection
    FfxFloat32x2 history_uv = uv - motion_vector;
    return history_uv;
}

FfxBoolean IsBaseRay(FfxUInt32x2 dispatch_thread_id, FfxUInt32 samples_per_quad)
{
    switch (samples_per_quad)
    {
    case 1:
        return ((dispatch_thread_id.x & 1) | (dispatch_thread_id.y & 1)) == 0;  // Deactivates 3 out of 4 rays
    case 2:
        return (dispatch_thread_id.x & 1) == (dispatch_thread_id.y & 1);  // Deactivates 2 out of 4 rays. Keeps diagonal.
    default:                                                              // case 4:
        return true;
    }
}

FFX_GROUPSHARED FfxUInt32 g_TileCount;
FFX_GROUPSHARED FfxInt32  g_TileClass;
FFX_GROUPSHARED FfxUInt32 g_SWCount;
FFX_GROUPSHARED FfxUInt32 g_SWCountTotal;
FFX_GROUPSHARED FfxUInt32 g_base_ray_index_sw;

void ClassifyTiles(FfxUInt32x2  dispatch_thread_id,
                   FfxUInt32x2  group_thread_id,
                   FfxFloat32   roughness,
                   FfxFloat32x3 view_space_surface_normal,
                   FfxFloat32   depth,
                   FfxInt32x2   screen_size,
                   FfxUInt32    samples_per_quad,
                   FfxBoolean   enable_temporal_variance_guided_tracing,
                   FfxBoolean   enable_hitcounter,
                   FfxBoolean   enable_screen_space_tracing,
                   FfxBoolean   enable_hw_ray_tracing)
{
    FfxUInt32  flat_group_thread_id  = group_thread_id.x + group_thread_id.y * 8;
    FfxBoolean is_first_lane_of_wave = ffxWaveIsFirstLane();

    if (group_thread_id.x == 0 && group_thread_id.y == 0)
    {
        // Initialize group shared variables
        g_TileCount         = 0;
        g_SWCount           = 0;
        g_SWCountTotal      = 0;
        g_base_ray_index_sw = 0;

#ifdef FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED
        // Initialize per 8x8 tile hit counter
        if (enable_hitcounter)
        {
            // In case we do hybrid
            if (enable_screen_space_tracing && enable_hw_ray_tracing)
            {
                // Feedback counters
                // See Intersect.hlsl
                FfxUInt32 hitcounter = 0;

                // Use surface motion vectors of one of the 8x8 pixels in the tile to reproject statistics from the previous frame
                // Helps a lot in movement to sustain temoporal coherence
#define FFX_CLASSIFIER_CLASSIFICATION_REPROJECT_HITCOUNTER
#ifdef FFX_CLASSIFIER_CLASSIFICATION_REPROJECT_HITCOUNTER
                {
                    // Grab motion vector from a random point in the subgroup
                    FfxFloat32x2 xi                      = GetRandom(dispatch_thread_id.xy / 8);
                    FfxInt32x2   mix                     = FfxInt32x2(xi * 8.0f);
                    FfxFloat32x2 motion_vector           = LoadMotionVector(FfxInt32x2(dispatch_thread_id) + mix);
                    FfxFloat32x2 uv8                     = (FfxFloat32x2(dispatch_thread_id.xy + mix)) / FFX_DNSR_Reflections_RoundUp8(screen_size);
                    FfxFloat32x2 surface_reprojection_uv = GetSurfaceReprojection(uv8, motion_vector);
                    hitcounter = LoadHitCounterHistory(FfxUInt32x2(surface_reprojection_uv * (FFX_DNSR_Reflections_RoundUp8(screen_size) / 8)));
                }
#endif  // FFX_CLASSIFIER_CLASSIFICATION_REPROJECT_HITCOUNTER

                // Use 3x3 region to grab the biggest success rate and create a safe band of hybrid rays to hide artefacts in movements
#define FFX_CLASSIFIER_CLASSIFICATION_SAFEBAND
#ifdef FFX_CLASSIFIER_CLASSIFICATION_SAFEBAND
                FfxUInt32 same_pixel_hitcounter = 0;
                // We need a safe band for some geometry not in the BVH to avoid fireflies
                const FfxInt32 radius = 1;
                for (FfxInt32 y = -radius; y <= radius; y++)
                {
                    for (FfxInt32 x = -radius; x <= radius; x++)
                    {
                        FfxUInt32 pt = LoadHitCounterHistory(dispatch_thread_id.xy / 8 + FfxInt32x2(x, y));
                        if (FFX_Hitcounter_GetSWHits(pt) > FFX_Hitcounter_GetSWHits(same_pixel_hitcounter))
                            same_pixel_hitcounter = pt;
                    }
                }
#else   // FFX_CLASSIFIER_CLASSIFICATION_SAFEBAND
                FfxUInt32 same_pixel_hitcounter = LoadHitCounterHistory(dispatch_thread_id.xy / 8);
#endif  // FFX_CLASSIFIER_CLASSIFICATION_SAFEBAND

                // Again compare with the same pixel and Pick the one with the biggest success rate
                if (FFX_Hitcounter_GetSWHits(hitcounter) < FFX_Hitcounter_GetSWHits(same_pixel_hitcounter))
                    hitcounter = same_pixel_hitcounter;

                FfxFloat32 rnd              = GetRandom(dispatch_thread_id.xy / 8).x;
                FfxFloat32 rnd_last         = GetRandomLastFrame(dispatch_thread_id.xy / 8).x;
                FfxFloat32 sw_hitcount_new  = FfxFloat32(FFX_Hitcounter_GetSWHits(hitcounter));
                FfxFloat32 sw_hitcount_old  = FfxFloat32(FFX_Hitcounter_GetOldSWHits(hitcounter));
                FfxFloat32 sw_misscount_new = FfxFloat32(FFX_Hitcounter_GetSWMisses(hitcounter));
                FfxFloat32 sw_misscount_old = FfxFloat32(FFX_Hitcounter_GetOldSWMisses(hitcounter));
                FfxBoolean new_class        = IsSW(sw_hitcount_new, sw_misscount_new, rnd);
                FfxBoolean old_class        = IsSW(sw_hitcount_old, sw_misscount_old, rnd_last);

                // To make transition less obvious we do and extra checkerboard stage
                if (new_class == old_class)
                {
                    if (new_class)
                    {
                        g_TileClass = TILE_CLASS_FULL_SW;
                    }
                    else
                    {
                        g_TileClass = TILE_CLASS_FULL_HW;
                    }
                }
                else
                {
                    g_TileClass = TILE_CLASS_HALF_SW;
                }
                sw_hitcount_old  = sw_hitcount_new;
                sw_misscount_old = sw_misscount_new;
                StoreHitCounter(dispatch_thread_id.xy / 8,
                                        (FfxUInt32(clamp(sw_hitcount_old, 0.0f, 255.0f)) << 8) | (FfxUInt32(clamp(sw_misscount_old, 0.0f, 255.0f)) << 24));
            }
        }
        else
        {
            g_TileClass = TILE_CLASS_FULL_SW;
        }
#endif  // FFX_HYBRID_REFLECTIONS
    }
    FFX_GROUP_MEMORY_BARRIER;

    // First we figure out on a per thread basis if we need to shoot a reflection ray
    FfxBoolean is_on_screen = (dispatch_thread_id.x < screen_size.x) && (dispatch_thread_id.y < screen_size.y);
    // Allow for additional engine side checks. For example engines could additionally only cast reflection rays for specific depth ranges
    FfxBoolean is_surface = !IsBackground(depth);
    // Don't shoot a ray on very rough surfaces
    FfxBoolean is_glossy_reflection = is_surface && IsGlossyReflection(roughness);
    FfxBoolean needs_ray            = is_on_screen && is_glossy_reflection;

    // Decide which ray to keep
    FfxBoolean is_base_ray  = IsBaseRay(dispatch_thread_id, samples_per_quad);
    FfxBoolean is_converged = true;
    if (enable_temporal_variance_guided_tracing)
    {
        FfxFloat32x2 uv = (dispatch_thread_id + 0.5) / screen_size;
        is_converged    = IsConverged(dispatch_thread_id, uv);
    }

    needs_ray = needs_ray && (is_base_ray || !is_converged);

    // Extra check for back-facing rays, fresnel, mirror etc.
    if (abs(view_space_surface_normal.z) > ReflectionsBackfacingThreshold())
    {
        FillEnvironment(dispatch_thread_id, IBLFactor());
        needs_ray = false;
    }

    // We need denoiser even for mirrors since ssr/hw transition ends up creating poping tile firefies.
    FfxBoolean needs_denoiser = is_glossy_reflection;

    // Next we have to figure out for which pixels that ray is creating the values for. Thus, if we have to copy its value horizontal, vertical or across.
    FfxBoolean require_copy =
        !needs_ray && needs_denoiser;  // Our pixel only requires a copy if we want to run a denoiser on it but don't want to shoot a ray for it.
    
    FfxBoolean copy_horizontal = FfxBoolean(ffxWaveXorU1(FfxUInt32(require_copy), 1)) && (samples_per_quad != 4) && is_base_ray;  // QuadReadAcrossX
    FfxBoolean copy_vertical   = FfxBoolean(ffxWaveXorU1(FfxUInt32(require_copy), 2)) && (samples_per_quad == 1) && is_base_ray;  // QuadReadAcrossY
    FfxBoolean copy_diagonal   = FfxBoolean(ffxWaveXorU1(FfxUInt32(require_copy), 3)) && (samples_per_quad == 1) && is_base_ray;  // QuadReadAcrossDiagonal

    FfxBoolean needs_sw_ray = true;

    // In case there's only software rays we don't do hybridization
#ifdef FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED
    needs_sw_ray = needs_ray && enable_screen_space_tracing;

    FfxBoolean needs_hw_ray = false;
    if (enable_hw_ray_tracing && roughness < RTRoughnessThreshold())
    {
        FfxBoolean checkerboard = ((group_thread_id.x ^ group_thread_id.y) & 1) == 0;
        needs_sw_ray            = needs_sw_ray && ((g_TileClass == TILE_CLASS_FULL_SW ? true : (g_TileClass == TILE_CLASS_HALF_SW ? checkerboard : false)));
        needs_hw_ray            = needs_ray && !needs_sw_ray;
    }
#endif  // FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED

    FfxUInt32 local_ray_index_in_wave_sw = ffxWavePrefixCountBits(needs_sw_ray);
    FfxUInt32 wave_ray_offset_in_group_sw;
    FfxUInt32 wave_ray_count_sw = ffxWaveActiveCountBits(needs_sw_ray);

#ifdef FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED
    FfxUInt32 local_ray_index_in_wave_hw = ffxWavePrefixCountBits(needs_hw_ray);
    FfxUInt32 wave_ray_count_hw          = ffxWaveActiveCountBits(needs_hw_ray);
    FfxUInt32 base_ray_index_hw          = 0;
#endif  // FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED

    if (is_first_lane_of_wave)
    {
        if (wave_ray_count_sw > 0)
        {
#ifdef FFX_GLSL
            wave_ray_offset_in_group_sw = FFX_ATOMIC_ADD(g_SWCount, FfxInt32(wave_ray_count_sw));
#else
            InterlockedAdd(g_SWCount, wave_ray_count_sw, wave_ray_offset_in_group_sw);
#endif
        }

#ifdef FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED
        if (wave_ray_count_hw > 0)
            IncrementRayCounterHW(wave_ray_count_hw, base_ray_index_hw);
#endif  // FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED
    }
    
    base_ray_index_hw           = ffxWaveReadLaneFirstU1(base_ray_index_hw);
    wave_ray_offset_in_group_sw = ffxWaveReadLaneFirstU1(wave_ray_offset_in_group_sw);

    FFX_GROUP_MEMORY_BARRIER;
    if (flat_group_thread_id == 0 && g_SWCount > 0)
    {
        // [IMPORTANT] We need to round up to the multiple of 32 for software rays, because of the atomic increment coalescing optimization
        g_SWCountTotal = g_SWCount < 32 ? 32 : (g_SWCount > 32 ? 64 : 32);
        IncrementRayCounterSW(g_SWCountTotal, g_base_ray_index_sw);
    }
    FFX_GROUP_MEMORY_BARRIER;

    if (needs_sw_ray)
    {
        FfxUInt32 ray_index_sw = g_base_ray_index_sw + wave_ray_offset_in_group_sw + local_ray_index_in_wave_sw;
        StoreRay(ray_index_sw, dispatch_thread_id, copy_horizontal, copy_vertical, copy_diagonal);
    }

#ifdef FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED
    else if (needs_hw_ray)
    {
        FfxUInt32 ray_index_hw = base_ray_index_hw + local_ray_index_in_wave_hw;
        StoreRayHW(ray_index_hw, dispatch_thread_id, copy_horizontal, copy_vertical, copy_diagonal);
    }
#endif  // FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED

    if (flat_group_thread_id < g_SWCountTotal - g_SWCount)
    {
        // [IMPORTANT] We need to round up to the multiple of 32 for software rays, because of the atomic increment coalescing optimization
        // Emit helper(dead) lanes to fill up 32 lanes per 8x8 tile
        FfxUInt32 ray_index_sw = g_base_ray_index_sw + g_SWCount + flat_group_thread_id;
        StoreRaySWHelper(ray_index_sw);
    }

    // We only need denoiser if we trace any rays in the tile
    if (is_first_lane_of_wave && (wave_ray_count_sw > 0
#ifdef FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED
                                  || wave_ray_count_hw > 0
#endif  // FFX_CLASSIFIER_CLASSIFICATION_HW_RAYTRACING_ENABLED
                                  ))
    {
        FFX_ATOMIC_ADD(g_TileCount, 1);
    }

    FFX_GROUP_MEMORY_BARRIER;  // Wait until all waves wrote into g_TileCount

    if (g_TileCount > 0)
    {
        if (group_thread_id.x == 0 && group_thread_id.y == 0)
        {
            FfxUInt32 tile_index;
            IncrementDenoiserTileCounter(tile_index);
            StoreDenoiserTile(tile_index, dispatch_thread_id);
        }
    }

    if ((!needs_ray && !require_copy)                     // Discarded for some reason
        || (needs_ray && !needs_hw_ray && !needs_sw_ray)  // Or needs a ray but was discarded for some other reason
    )
    {
        if (is_surface)
        {
            FillEnvironment(dispatch_thread_id, IBLFactor());
        }
        else
            ZeroBuffers(dispatch_thread_id);
    }
}
