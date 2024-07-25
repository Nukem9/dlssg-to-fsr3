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

#ifndef FFX_DNSR_REFLECTIONS_REPROJECT
#define FFX_DNSR_REFLECTIONS_REPROJECT

#define FFX_DNSR_REFLECTIONS_ESTIMATES_LOCAL_NEIGHBORHOOD
#include "ffx_denoiser_reflections_common.h"

FFX_GROUPSHARED FfxUInt32  g_ffx_dnsr_shared_0[16][16];
FFX_GROUPSHARED FfxUInt32  g_ffx_dnsr_shared_1[16][16];

#if FFX_HALF

struct FFX_DNSR_Reflections_NeighborhoodSample {
    FfxFloat16x3 radiance;
};

FFX_DNSR_Reflections_NeighborhoodSample FFX_DNSR_Reflections_LoadFromGroupSharedMemory(FfxInt32x2 idx) {
    FfxUInt32x2       packed_radiance          = FfxUInt32x2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    FfxFloat16x4 unpacked_radiance        = FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance);

    FFX_DNSR_Reflections_NeighborhoodSample neighborSample;
    neighborSample.radiance = unpacked_radiance.xyz;
    return neighborSample;
}

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(FfxInt32x2 group_thread_id, FfxFloat16x3 radiance) {
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.zz);
}

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(FfxInt32x2 group_thread_id, FfxFloat16x4 radiance_variance) {
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance_variance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance_variance.zw);
}

void FFX_DNSR_Reflections_InitializeGroupSharedMemory(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxInt32x2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks.
    FfxInt32x2 offset[4] = {FfxInt32x2(0, 0), FfxInt32x2(8, 0), FfxInt32x2(0, 8), FfxInt32x2(8, 8)};

    // Intermediate storage registers to cache the result of all loads
    FfxFloat16x3 radiance[4];

    // Start in the upper left corner of the 16x16 region.
    dispatch_thread_id -= 4;

    // First store all loads in registers
    for (FfxInt32 i = 0; i < 4; ++i) {
        radiance[i] = FFX_DNSR_Reflections_LoadRadiance(dispatch_thread_id + offset[i]);
    }

    // Then move all registers to FFX_GROUPSHARED memory
    for (FfxInt32 j = 0; j < 4; ++j) {
        FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id + offset[j], radiance[j]); // X
    }
}

FfxFloat16x4 FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2 idx) {
    FfxUInt32x2 packed_radiance = FfxUInt32x2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    return FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance);
}

FfxFloat16 FFX_DNSR_Reflections_GetLuminanceWeight(FfxFloat16x3 val) {
    FfxFloat16 luma   = FFX_DNSR_Reflections_Luminance(val.xyz);
    FfxFloat16 weight = FfxFloat16(max(exp(-luma * FFX_DNSR_REFLECTIONS_AVG_RADIANCE_LUMINANCE_WEIGHT), 1.0e-2));
    return weight;
}

FfxFloat32x2 FFX_DNSR_Reflections_GetSurfaceReprojection(FfxInt32x2 dispatch_thread_id, FfxFloat32x2 uv, FfxFloat32x2 motion_vector) {
    // Reflector position reprojection
    FfxFloat32x2 history_uv = uv + motion_vector;
    return history_uv;
}

FfxFloat32x2 FFX_DNSR_Reflections_GetHitPositionReprojection(FfxInt32x2 dispatch_thread_id, FfxFloat32x2 uv, FfxFloat32 reflected_ray_length) {
    FfxFloat32  z              = FFX_DNSR_Reflections_LoadDepth(dispatch_thread_id);
    FfxFloat32x3 view_space_ray = FFX_DNSR_Reflections_ScreenSpaceToViewSpace(FfxFloat32x3(uv, z));

    // We start out with reconstructing the ray length in view space.
    // This includes the portion from the camera to the reflecting surface as well as the portion from the surface to the hit position.
    FfxFloat32 surface_depth = length(view_space_ray);
    FfxFloat32 ray_length    = surface_depth + reflected_ray_length;

    // We then perform a parallax correction by shooting a ray
    // of the same length "straight through" the reflecting surface
    // and reprojecting the tip of that ray to the previous frame.
    view_space_ray /= surface_depth; // == normalize(view_space_ray)
    view_space_ray *= ray_length;
    FfxFloat32x3 world_hit_position =
        FFX_DNSR_Reflections_ViewSpaceToWorldSpace(FfxFloat32x4(view_space_ray, 1)); // This is the "fake" hit position if we would follow the ray straight through the surface.
    FfxFloat32x3 prev_hit_position = FFX_DNSR_Reflections_WorldSpaceToScreenSpacePrevious(world_hit_position);
    FfxFloat32x2 history_uv        = prev_hit_position.xy;
    return history_uv;
}

FfxFloat16 FFX_DNSR_Reflections_GetDisocclusionFactor(FfxFloat16x3 normal, FfxFloat16x3 history_normal, FfxFloat32 linear_depth, FfxFloat32 history_linear_depth) {
    FfxFloat16 factor = FfxFloat16(1.0f                                                            //
                        * exp(-abs(1.0f - max(FfxFloat16(0.0f), dot(normal, history_normal))) * FFX_DNSR_REFLECTIONS_DISOCCLUSION_NORMAL_WEIGHT) //
                        * exp(-abs(history_linear_depth - linear_depth) / linear_depth * FFX_DNSR_REFLECTIONS_DISOCCLUSION_DEPTH_WEIGHT));
    return factor;
}

struct FFX_DNSR_Reflections_Moments {
    FfxFloat16x3 mean;
    FfxFloat16x3 variance;
};

FFX_DNSR_Reflections_Moments FFX_DNSR_Reflections_EstimateLocalNeighborhoodInGroup(FfxInt32x2 group_thread_id) {
    FFX_DNSR_Reflections_Moments estimate;
    estimate.mean                 = FfxFloat16x3(0.0f, 0.0f, 0.0f);
    estimate.variance             = FfxFloat16x3(0.0f, 0.0f, 0.0f);
    FfxFloat16 accumulated_weight = FfxFloat16(0.0f);
    for (FfxInt32 j = -FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; j <= FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; ++j) {
        for (FfxInt32 i = -FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; i <= FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; ++i) {
            FfxInt32x2        new_idx  = group_thread_id + FfxInt32x2(i, j);
            FfxFloat16x3 radiance = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(new_idx).radiance;
            FfxFloat16  weight   = FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(FfxFloat16(i)) * FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(FfxFloat16(j));
            accumulated_weight  += weight;
            estimate.mean       += radiance * weight;
            estimate.variance   += radiance * radiance * weight;
        }
    }
    estimate.mean     /= accumulated_weight;
    estimate.variance /= accumulated_weight;

    estimate.variance = abs(estimate.variance - estimate.mean * estimate.mean);
    return estimate;
}

FfxFloat32 dot2(FfxFloat32x3 a) { return dot(a, a); }

void FFX_DNSR_Reflections_PickReprojection(FfxInt32x2       dispatch_thread_id,  //
                                           FfxInt32x2       group_thread_id,     //
                                           FfxUInt32x2      screen_size,         //
                                           FfxFloat16       roughness,           //
                                           FfxFloat16       ray_length,          //
                                           out FfxFloat16   disocclusion_factor, //
                                           out FfxFloat32x2 reprojection_uv,     //
                                           out FfxFloat16x3 reprojection) {

    FFX_DNSR_Reflections_Moments local_neighborhood = FFX_DNSR_Reflections_EstimateLocalNeighborhoodInGroup(group_thread_id);

    FfxFloat32x2 uv     = FfxFloat32x2(dispatch_thread_id.x + 0.5, dispatch_thread_id.y + 0.5) / screen_size;
    FfxFloat16x3 normal = FFX_DNSR_Reflections_LoadWorldSpaceNormal(dispatch_thread_id);
    FfxFloat16x3 history_normal;
    FfxFloat32   history_linear_depth;

    {
        const FfxFloat32x2  motion_vector             = FFX_DNSR_Reflections_LoadMotionVector(dispatch_thread_id);
        const FfxFloat32x2  surface_reprojection_uv   = FFX_DNSR_Reflections_GetSurfaceReprojection(dispatch_thread_id, uv, motion_vector);
        const FfxFloat32x2  hit_reprojection_uv       = FFX_DNSR_Reflections_GetHitPositionReprojection(dispatch_thread_id, uv, ray_length);
        const FfxFloat16x3  surface_normal            = FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(surface_reprojection_uv);
        const FfxFloat16x3  hit_normal                = FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(hit_reprojection_uv);
        const FfxFloat16x3  surface_history           = FFX_DNSR_Reflections_SampleRadianceHistory(surface_reprojection_uv);
        const FfxFloat16x3  hit_history               = FFX_DNSR_Reflections_SampleRadianceHistory(hit_reprojection_uv);
        const FfxFloat32    hit_normal_similarity     = dot(normalize(FfxFloat32x3(hit_normal)), normalize(FfxFloat32x3(normal)));
        const FfxFloat32    surface_normal_similarity = dot(normalize(FfxFloat32x3(surface_normal)), normalize(FfxFloat32x3(normal)));
        const FfxFloat16    hit_roughness             = FFX_DNSR_Reflections_SampleRoughnessHistory(hit_reprojection_uv);
        const FfxFloat16    surface_roughness         = FFX_DNSR_Reflections_SampleRoughnessHistory(surface_reprojection_uv);

        // Choose reprojection uv based on similarity to the local neighborhood.
        if (hit_normal_similarity > FFX_DNSR_REFLECTIONS_REPROJECTION_NORMAL_SIMILARITY_THRESHOLD  // Candidate for mirror reflection parallax
            && hit_normal_similarity + 1.0e-3 > surface_normal_similarity                          //
            && abs(hit_roughness - roughness) < abs(surface_roughness - roughness) + 1.0e-3        //
        ) {
            history_normal                      = hit_normal;
            FfxFloat32 hit_history_depth        = FFX_DNSR_Reflections_SampleDepthHistory(hit_reprojection_uv);
            FfxFloat32 hit_history_linear_depth = FFX_DNSR_Reflections_GetLinearDepth(hit_reprojection_uv, hit_history_depth);
            history_linear_depth                = hit_history_linear_depth;
            reprojection_uv                     = hit_reprojection_uv;
            reprojection                        = hit_history;
        } else {
            // Reject surface reprojection based on simple distance
            if (dot2(surface_history - local_neighborhood.mean) <
                FFX_DNSR_REFLECTIONS_REPROJECT_SURFACE_DISCARD_VARIANCE_WEIGHT * length(local_neighborhood.variance)) {
                history_normal                          = surface_normal;
                FfxFloat32 surface_history_depth        = FFX_DNSR_Reflections_SampleDepthHistory(surface_reprojection_uv);
                FfxFloat32 surface_history_linear_depth = FFX_DNSR_Reflections_GetLinearDepth(surface_reprojection_uv, surface_history_depth);
                history_linear_depth                    = surface_history_linear_depth;
                reprojection_uv                         = surface_reprojection_uv;
                reprojection                            = surface_history;
            } else {
                disocclusion_factor = FfxFloat16(0.0f);
                return;
            }
        }
    }
    FfxFloat32 depth        = FFX_DNSR_Reflections_LoadDepth(dispatch_thread_id);
    FfxFloat32 linear_depth = FFX_DNSR_Reflections_GetLinearDepth(uv, depth);
    // Determine disocclusion factor based on history
    disocclusion_factor = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);

    if (disocclusion_factor > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) // Early out, good enough
        return;

    // Try to find the closest sample in the vicinity if we are not convinced of a disocclusion
    if (disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) {
        FfxFloat32x2    closest_uv    = reprojection_uv;
        FfxFloat32x2    dudv          = 1.0 / FfxFloat32x2(screen_size);
        const FfxInt32 search_radius = 1;
        for (FfxInt32 y = -search_radius; y <= search_radius; y++) {
            for (FfxInt32 x = -search_radius; x <= search_radius; x++) {
                FfxFloat32x2 uv                   = reprojection_uv + FfxFloat32x2(x, y) * dudv;
                FfxFloat16x3 history_normal       = FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(uv);
                FfxFloat32   history_depth        = FFX_DNSR_Reflections_SampleDepthHistory(uv);
                FfxFloat32   history_linear_depth = FFX_DNSR_Reflections_GetLinearDepth(uv, history_depth);
                FfxFloat16   weight               = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
                if (weight > disocclusion_factor) {
                    disocclusion_factor = weight;
                    closest_uv          = uv;
                    reprojection_uv     = closest_uv;
                }
            }
        }
        reprojection = FFX_DNSR_Reflections_SampleRadianceHistory(reprojection_uv);
    }

    // Rare slow path - triggered only on the edges.
    // Try to get rid of potential leaks at bilinear interpolation level.
    if (disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) {
        // If we've got a discarded history, try to construct a better sample out of 2x2 interpolation neighborhood
        // Helps quite a bit on the edges in movement
        FfxFloat32   uvx                    = ffxFract(FfxFloat32(screen_size.x) * reprojection_uv.x + 0.5);
        FfxFloat32   uvy                    = ffxFract(FfxFloat32(screen_size.y) * reprojection_uv.y + 0.5);
        FfxInt32x2   reproject_texel_coords = FfxInt32x2(screen_size * reprojection_uv - 0.5);
        FfxFloat16x3 reprojection00         = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + FfxInt32x2(0, 0));
        FfxFloat16x3 reprojection10         = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + FfxInt32x2(1, 0));
        FfxFloat16x3 reprojection01         = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + FfxInt32x2(0, 1));
        FfxFloat16x3 reprojection11         = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + FfxInt32x2(1, 1));
        FfxFloat16x3 normal00               = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + FfxInt32x2(0, 0));
        FfxFloat16x3 normal10               = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + FfxInt32x2(1, 0));
        FfxFloat16x3 normal01               = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + FfxInt32x2(0, 1));
        FfxFloat16x3 normal11               = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + FfxInt32x2(1, 1));
        FfxFloat32   depth00                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + FfxInt32x2(0, 0)));
        FfxFloat32   depth10                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + FfxInt32x2(1, 0)));
        FfxFloat32   depth01                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + FfxInt32x2(0, 1)));
        FfxFloat32   depth11                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + FfxInt32x2(1, 1)));
        FfxFloat16x4 w                      = FfxFloat16x4(1.0f, 1.0f, 1.0f, 1.0f);
        // Initialize with occlusion weights
        w.x = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal00, linear_depth, depth00) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0f ? FfxFloat16(1.0f) : FfxFloat16(0.0f);
        w.y = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal10, linear_depth, depth10) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0f ? FfxFloat16(1.0f) : FfxFloat16(0.0f);
        w.z = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal01, linear_depth, depth01) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0f ? FfxFloat16(1.0f) : FfxFloat16(0.0f);
        w.w = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal11, linear_depth, depth11) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0f ? FfxFloat16(1.0f) : FfxFloat16(0.0f);
        // And then mix in bilinear weights
        w.x           = FfxFloat16(w.x * (1.0 - uvx) * (1.0 - uvy));
        w.y           = FfxFloat16(w.y * (uvx) * (1.0 - uvy));
        w.z           = FfxFloat16(w.z * (1.0 - uvx) * (uvy));
        w.w           = FfxFloat16(w.w * (uvx) * (uvy));
        FfxFloat16 ws = max(w.x + w.y + w.z + w.w, FfxFloat16(1.0e-3));
        // normalize
        w /= ws;

        FfxFloat16x3 history_normal;
        FfxFloat32       history_linear_depth;
        reprojection         = reprojection00 * w.x + reprojection10 * w.y + reprojection01 * w.z + reprojection11 * w.w;
        history_linear_depth = depth00 * w.x + depth10 * w.y + depth01 * w.z + depth11 * w.w;
        history_normal       = normal00 * w.x + normal10 * w.y + normal01 * w.z + normal11 * w.w;
        disocclusion_factor  = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
    }
    disocclusion_factor = disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD ? FfxFloat16(0.0f) : disocclusion_factor;
}

void FFX_DNSR_Reflections_Reproject(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxUInt32x2 screen_size, FfxFloat32 temporal_stability_factor, FfxInt32 max_samples) {
    FFX_DNSR_Reflections_InitializeGroupSharedMemory(dispatch_thread_id, group_thread_id, FfxInt32x2(screen_size));
    FFX_GROUP_MEMORY_BARRIER;

    group_thread_id += 4; // Center threads in FFX_GROUPSHARED memory

    FfxFloat16       variance    = FfxFloat16(1.0f);
    FfxFloat16       num_samples = FfxFloat16(0.0f);
    FfxFloat16       roughness   = FFX_DNSR_Reflections_LoadRoughness(dispatch_thread_id);
    FfxFloat32x3     normal      = FFX_DNSR_Reflections_LoadWorldSpaceNormal(dispatch_thread_id);
    FfxFloat16x3     radiance    = FFX_DNSR_Reflections_LoadRadiance(dispatch_thread_id);
    const FfxFloat16 ray_length  = FFX_DNSR_Reflections_LoadRayLength(dispatch_thread_id);

    if (FFX_DNSR_Reflections_IsGlossyReflection(roughness)) {
        FfxFloat16  disocclusion_factor;
        FfxFloat32x2      reprojection_uv;
        FfxFloat16x3 reprojection;
        FFX_DNSR_Reflections_PickReprojection(/*in*/ dispatch_thread_id,
                                              /* in */ group_thread_id,
                                              /* in */ screen_size,
                                              /* in */ roughness,
                                              /* in */ ray_length,
                                              /* out */ disocclusion_factor,
                                              /* out */ reprojection_uv,
                                              /* out */ reprojection);
        if ( (reprojection_uv.x > 0.0f) && (reprojection_uv.y > 0.0f) && (reprojection_uv.x < 1.0f) && (reprojection_uv.y< 1.0f)) {
            FfxFloat16 prev_variance = FFX_DNSR_Reflections_SampleVarianceHistory(reprojection_uv);
            num_samples              = FFX_DNSR_Reflections_SampleNumSamplesHistory(reprojection_uv) * disocclusion_factor;
            FfxFloat16 s_max_samples = FfxFloat16(max(8.0f, FfxFloat16(max_samples) * FFX_DNSR_REFLECTIONS_SAMPLES_FOR_ROUGHNESS(roughness)));
            num_samples              = min(s_max_samples, num_samples + FfxFloat16(1.0f));
            FfxFloat16 new_variance  = FFX_DNSR_Reflections_ComputeTemporalVariance(radiance.xyz, reprojection.xyz);
            if (disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) {
                FFX_DNSR_Reflections_StoreRadianceReprojected(dispatch_thread_id, FfxFloat16x3(0.0f, 0.0f, 0.0f));
                FFX_DNSR_Reflections_StoreVariance(dispatch_thread_id, FfxFloat16(1.0f));
                FFX_DNSR_Reflections_StoreNumSamples(dispatch_thread_id, FfxFloat16(1.0f));
            } else {
                FfxFloat16 variance_mix = ffxLerp(new_variance, prev_variance, FfxFloat16(1.0f / num_samples));
                FFX_DNSR_Reflections_StoreRadianceReprojected(dispatch_thread_id, reprojection);
                FFX_DNSR_Reflections_StoreVariance(dispatch_thread_id, variance_mix);
                FFX_DNSR_Reflections_StoreNumSamples(dispatch_thread_id, num_samples);
                // Mix in reprojection for radiance mip computation 
                radiance = ffxLerp(radiance, reprojection, FfxFloat16(0.3f));
            }
        } else {
            FFX_DNSR_Reflections_StoreRadianceReprojected(dispatch_thread_id, FfxFloat16x3(0.0f, 0.0f, 0.0f));
            FFX_DNSR_Reflections_StoreVariance(dispatch_thread_id, FfxFloat16(1.0f));
            FFX_DNSR_Reflections_StoreNumSamples(dispatch_thread_id, FfxFloat16(1.0f));
        }
    }
    
    // Downsample 8x8 -> 1 radiance using FFX_GROUPSHARED memory
    // Initialize FFX_GROUPSHARED array for downsampling
    FfxFloat16 weight = FFX_DNSR_Reflections_GetLuminanceWeight(radiance.xyz);
    radiance.xyz *= weight;
    if ( (dispatch_thread_id.x >= screen_size.x) || (dispatch_thread_id.y >= screen_size.y) || any(isinf(radiance)) || any(isnan(radiance)) || (weight > FfxFloat16(1.0e3))) {
        radiance = FfxFloat16x3(0.0f, 0.0f, 0.0f);
        weight   = FfxFloat16(0.0f);
    }

    group_thread_id -= 4; // Center threads in FFX_GROUPSHARED memory

    FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id, FfxFloat16x4(radiance.xyz, weight));
    FFX_GROUP_MEMORY_BARRIER;

    for (FfxInt32 i = 2; i <= 8; i = i * 2) {
        FfxInt32 ox = group_thread_id.x * i;
        FfxInt32 oy = group_thread_id.y * i;
        FfxInt32 ix = group_thread_id.x * i + i / 2;
        FfxInt32 iy = group_thread_id.y * i + i / 2;
        if (ix < 8 && iy < 8) {
            FfxFloat16x4 rad_weight00 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2(ox, oy));
            FfxFloat16x4 rad_weight10 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2(ox, iy));
            FfxFloat16x4 rad_weight01 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2(ix, oy));
            FfxFloat16x4 rad_weight11 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2(ix, iy));
            FfxFloat16x4 sum          = rad_weight00 + rad_weight01 + rad_weight10 + rad_weight11;
            FFX_DNSR_Reflections_StoreInGroupSharedMemory(FfxInt32x2(ox, oy), sum);
        }
        FFX_GROUP_MEMORY_BARRIER;
    }

    if ((group_thread_id.x == 0) && (group_thread_id.y == 0)) {
        FfxFloat16x4    sum          = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2(0, 0));
        FfxFloat16      weight_acc   = max(sum.w, FfxFloat16(1.0e-3));
        FfxFloat32x3    radiance_avg = sum.xyz / weight_acc;
        FFX_DNSR_Reflections_StoreAverageRadiance(dispatch_thread_id.xy / 8, FfxFloat16x3(radiance_avg));
    }
}

#else // #if FFX_HALF

struct FFX_DNSR_Reflections_NeighborhoodSample {
    FfxFloat32x3 radiance;
};

FFX_DNSR_Reflections_NeighborhoodSample FFX_DNSR_Reflections_LoadFromGroupSharedMemory(FfxInt32x2 idx) {
    FfxUInt32x2  packed_radiance          = FfxUInt32x2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    FfxFloat32x4 unpacked_radiance        = FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance);

    FFX_DNSR_Reflections_NeighborhoodSample neighborSample;
    neighborSample.radiance = unpacked_radiance.xyz;
    return neighborSample;
}

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(FfxInt32x2 group_thread_id, FfxFloat32x3 radiance) {
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.zz);
}

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(FfxInt32x2 group_thread_id, FfxFloat32x4 radiance_variance) {
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance_variance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance_variance.zw);
}

void FFX_DNSR_Reflections_InitializeGroupSharedMemory(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxInt32x2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks.
    FfxInt32x2 offset[4] = {FfxInt32x2(0, 0), FfxInt32x2(8, 0), FfxInt32x2(0, 8), FfxInt32x2(8, 8)};

    // Intermediate storage registers to cache the result of all loads
    FfxFloat32x3 radiance[4];

    // Start in the upper left corner of the 16x16 region.
    dispatch_thread_id -= 4;

    // First store all loads in registers
    for (FfxInt32 i = 0; i < 4; ++i) {
        radiance[i] = FFX_DNSR_Reflections_LoadRadiance(dispatch_thread_id + offset[i]);
    }

    // Then move all registers to FFX_GROUPSHARED memory
    for (FfxInt32 j = 0; j < 4; ++j) {
        FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id + offset[j], radiance[j]); // X
    }
}

FfxFloat32x4 FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2 idx) {
    FfxUInt32x2 packed_radiance = FfxUInt32x2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    return FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance);
}

FfxFloat32 FFX_DNSR_Reflections_GetLuminanceWeight(FfxFloat32x3 val) {
    FfxFloat32 luma   = FFX_DNSR_Reflections_Luminance(val.xyz);
    FfxFloat32 weight = FfxFloat32(max(exp(-luma * FFX_DNSR_REFLECTIONS_AVG_RADIANCE_LUMINANCE_WEIGHT), 1.0e-2));
    return weight;
}

FfxFloat32x2 FFX_DNSR_Reflections_GetSurfaceReprojection(FfxInt32x2 dispatch_thread_id, FfxFloat32x2 uv, FfxFloat32x2 motion_vector) {
    // Reflector position reprojection
    FfxFloat32x2 history_uv = uv + motion_vector;
    return history_uv;
}

FfxFloat32x2 FFX_DNSR_Reflections_GetHitPositionReprojection(FfxInt32x2 dispatch_thread_id, FfxFloat32x2 uv, FfxFloat32 reflected_ray_length) {
    FfxFloat32  z               = FFX_DNSR_Reflections_LoadDepth(dispatch_thread_id);
    FfxFloat32x3 view_space_ray = FFX_DNSR_Reflections_ScreenSpaceToViewSpace(FfxFloat32x3(uv, z));

    // We start out with reconstructing the ray length in view space.
    // This includes the portion from the camera to the reflecting surface as well as the portion from the surface to the hit position.
    FfxFloat32 surface_depth = length(view_space_ray);
    FfxFloat32 ray_length    = surface_depth + reflected_ray_length;

    // We then perform a parallax correction by shooting a ray
    // of the same length "straight through" the reflecting surface
    // and reprojecting the tip of that ray to the previous frame.
    view_space_ray /= surface_depth; // == normalize(view_space_ray)
    view_space_ray *= ray_length;
    FfxFloat32x3 world_hit_position =
        FFX_DNSR_Reflections_ViewSpaceToWorldSpace(FfxFloat32x4(view_space_ray, 1)); // This is the "fake" hit position if we would follow the ray straight through the surface.
    FfxFloat32x3 prev_hit_position = FFX_DNSR_Reflections_WorldSpaceToScreenSpacePrevious(world_hit_position);
    FfxFloat32x2 history_uv        = prev_hit_position.xy;
    return history_uv;
}

FfxFloat32 FFX_DNSR_Reflections_GetDisocclusionFactor(FfxFloat32x3 normal, FfxFloat32x3 history_normal, FfxFloat32 linear_depth, FfxFloat32 history_linear_depth) {
    FfxFloat32 factor = FfxFloat32(1.0f                                                            //
                        * exp(-abs(1.0f - max(FfxFloat32(0.0f), dot(normal, history_normal))) * FFX_DNSR_REFLECTIONS_DISOCCLUSION_NORMAL_WEIGHT) //
                        * exp(-abs(history_linear_depth - linear_depth) / linear_depth * FFX_DNSR_REFLECTIONS_DISOCCLUSION_DEPTH_WEIGHT));
    return factor;
}

struct FFX_DNSR_Reflections_Moments {
    FfxFloat32x3 mean;
    FfxFloat32x3 variance;
};

FFX_DNSR_Reflections_Moments FFX_DNSR_Reflections_EstimateLocalNeighborhoodInGroup(FfxInt32x2 group_thread_id) {
    FFX_DNSR_Reflections_Moments estimate;
    estimate.mean                 = FfxFloat32x3(0.0f, 0.0f, 0.0f);
    estimate.variance             = FfxFloat32x3(0.0f, 0.0f, 0.0f);
    FfxFloat32 accumulated_weight = FfxFloat32(0.0f);
    for (FfxInt32 j = -FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; j <= FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; ++j) {
        for (FfxInt32 i = -FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; i <= FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS; ++i) {
            FfxInt32x2        new_idx  = group_thread_id + FfxInt32x2(i, j);
            FfxFloat32x3 radiance = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(new_idx).radiance;
            FfxFloat32  weight   = FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(FfxFloat32(i)) * FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(FfxFloat32(j));
            accumulated_weight  += weight;
            estimate.mean       += radiance * weight;
            estimate.variance   += radiance * radiance * weight;
        }
    }
    estimate.mean     /= accumulated_weight;
    estimate.variance /= accumulated_weight;

    estimate.variance = abs(estimate.variance - estimate.mean * estimate.mean);
    return estimate;
}

FfxFloat32 dot2(FfxFloat32x3 a) { return dot(a, a); }

void FFX_DNSR_Reflections_PickReprojection(FfxInt32x2       dispatch_thread_id,  //
                                           FfxInt32x2       group_thread_id,     //
                                           FfxUInt32x2      screen_size,         //
                                           FfxFloat32       roughness,           //
                                           FfxFloat32       ray_length,          //
                                           out FfxFloat32   disocclusion_factor, //
                                           out FfxFloat32x2 reprojection_uv,     //
                                           out FfxFloat32x3 reprojection) {

    FFX_DNSR_Reflections_Moments local_neighborhood = FFX_DNSR_Reflections_EstimateLocalNeighborhoodInGroup(group_thread_id);

    FfxFloat32x2 uv     = FfxFloat32x2(dispatch_thread_id.x + 0.5, dispatch_thread_id.y + 0.5) / screen_size;
    FfxFloat32x3 normal = FFX_DNSR_Reflections_LoadWorldSpaceNormal(dispatch_thread_id);
    FfxFloat32x3 history_normal;
    FfxFloat32   history_linear_depth;

    {
        const FfxFloat32x2  motion_vector             = FFX_DNSR_Reflections_LoadMotionVector(dispatch_thread_id);
        const FfxFloat32x2  surface_reprojection_uv   = FFX_DNSR_Reflections_GetSurfaceReprojection(dispatch_thread_id, uv, motion_vector);
        const FfxFloat32x2  hit_reprojection_uv       = FFX_DNSR_Reflections_GetHitPositionReprojection(dispatch_thread_id, uv, ray_length);
        const FfxFloat32x3  surface_normal            = FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(surface_reprojection_uv);
        const FfxFloat32x3  hit_normal                = FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(hit_reprojection_uv);
        const FfxFloat32x3  surface_history           = FFX_DNSR_Reflections_SampleRadianceHistory(surface_reprojection_uv);
        const FfxFloat32x3  hit_history               = FFX_DNSR_Reflections_SampleRadianceHistory(hit_reprojection_uv);
        const FfxFloat32    hit_normal_similarity     = dot(normalize(FfxFloat32x3(hit_normal)), normalize(FfxFloat32x3(normal)));
        const FfxFloat32    surface_normal_similarity = dot(normalize(FfxFloat32x3(surface_normal)), normalize(FfxFloat32x3(normal)));
        const FfxFloat32    hit_roughness             = FFX_DNSR_Reflections_SampleRoughnessHistory(hit_reprojection_uv);
        const FfxFloat32    surface_roughness         = FFX_DNSR_Reflections_SampleRoughnessHistory(surface_reprojection_uv);

        // Choose reprojection uv based on similarity to the local neighborhood.
        if (hit_normal_similarity > FFX_DNSR_REFLECTIONS_REPROJECTION_NORMAL_SIMILARITY_THRESHOLD  // Candidate for mirror reflection parallax
            && hit_normal_similarity + 1.0e-3 > surface_normal_similarity                          //
            && abs(hit_roughness - roughness) < abs(surface_roughness - roughness) + 1.0e-3        //
        ) {
            history_normal                      = hit_normal;
            FfxFloat32 hit_history_depth        = FFX_DNSR_Reflections_SampleDepthHistory(hit_reprojection_uv);
            FfxFloat32 hit_history_linear_depth = FFX_DNSR_Reflections_GetLinearDepth(hit_reprojection_uv, hit_history_depth);
            history_linear_depth                = hit_history_linear_depth;
            reprojection_uv                     = hit_reprojection_uv;
            reprojection                        = hit_history;
        } else {
            // Reject surface reprojection based on simple distance
            if (dot2(surface_history - local_neighborhood.mean) <
                FFX_DNSR_REFLECTIONS_REPROJECT_SURFACE_DISCARD_VARIANCE_WEIGHT * length(local_neighborhood.variance)) {
                history_normal                          = surface_normal;
                FfxFloat32 surface_history_depth        = FFX_DNSR_Reflections_SampleDepthHistory(surface_reprojection_uv);
                FfxFloat32 surface_history_linear_depth = FFX_DNSR_Reflections_GetLinearDepth(surface_reprojection_uv, surface_history_depth);
                history_linear_depth                    = surface_history_linear_depth;
                reprojection_uv                         = surface_reprojection_uv;
                reprojection                            = surface_history;
            } else {
                disocclusion_factor = FfxFloat32(0.0f);
                return;
            }
        }
    }
    FfxFloat32 depth        = FFX_DNSR_Reflections_LoadDepth(dispatch_thread_id);
    FfxFloat32 linear_depth = FFX_DNSR_Reflections_GetLinearDepth(uv, depth);
    // Determine disocclusion factor based on history
    disocclusion_factor = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);

    if (disocclusion_factor > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) // Early out, good enough
        return;

    // Try to find the closest sample in the vicinity if we are not convinced of a disocclusion
    if (disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) {
        FfxFloat32x2    closest_uv    = reprojection_uv;
        FfxFloat32x2    dudv          = 1.0 / FfxFloat32x2(screen_size);
        const FfxInt32 search_radius = 1;
        for (FfxInt32 y = -search_radius; y <= search_radius; y++) {
            for (FfxInt32 x = -search_radius; x <= search_radius; x++) {
                FfxFloat32x2 uv                   = reprojection_uv + FfxFloat32x2(x, y) * dudv;
                FfxFloat32x3 history_normal       = FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(uv);
                FfxFloat32   history_depth        = FFX_DNSR_Reflections_SampleDepthHistory(uv);
                FfxFloat32   history_linear_depth = FFX_DNSR_Reflections_GetLinearDepth(uv, history_depth);
                FfxFloat32   weight               = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
                if (weight > disocclusion_factor) {
                    disocclusion_factor = weight;
                    closest_uv          = uv;
                    reprojection_uv     = closest_uv;
                }
            }
        }
        reprojection = FFX_DNSR_Reflections_SampleRadianceHistory(reprojection_uv);
    }

    // Rare slow path - triggered only on the edges.
    // Try to get rid of potential leaks at bilinear interpolation level.
    if (disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) {
        // If we've got a discarded history, try to construct a better sample out of 2x2 interpolation neighborhood
        // Helps quite a bit on the edges in movement
        FfxFloat32   uvx                    = ffxFract(FfxFloat32(screen_size.x) * reprojection_uv.x + 0.5);
        FfxFloat32   uvy                    = ffxFract(FfxFloat32(screen_size.y) * reprojection_uv.y + 0.5);
        FfxInt32x2   reproject_texel_coords = FfxInt32x2(screen_size * reprojection_uv - 0.5);
        FfxFloat32x3 reprojection00         = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + FfxInt32x2(0, 0));
        FfxFloat32x3 reprojection10         = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + FfxInt32x2(1, 0));
        FfxFloat32x3 reprojection01         = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + FfxInt32x2(0, 1));
        FfxFloat32x3 reprojection11         = FFX_DNSR_Reflections_LoadRadianceHistory(reproject_texel_coords + FfxInt32x2(1, 1));
        FfxFloat32x3 normal00               = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + FfxInt32x2(0, 0));
        FfxFloat32x3 normal10               = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + FfxInt32x2(1, 0));
        FfxFloat32x3 normal01               = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + FfxInt32x2(0, 1));
        FfxFloat32x3 normal11               = FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(reproject_texel_coords + FfxInt32x2(1, 1));
        FfxFloat32   depth00                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + FfxInt32x2(0, 0)));
        FfxFloat32   depth10                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + FfxInt32x2(1, 0)));
        FfxFloat32   depth01                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + FfxInt32x2(0, 1)));
        FfxFloat32   depth11                = FFX_DNSR_Reflections_GetLinearDepth(reprojection_uv, FFX_DNSR_Reflections_LoadDepthHistory(reproject_texel_coords + FfxInt32x2(1, 1)));
        FfxFloat32x4 w                      = FfxFloat32x4(1.0f, 1.0f, 1.0f, 1.0f);
        // Initialize with occlusion weights
        w.x = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal00, linear_depth, depth00) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0f ? FfxFloat32(1.0f) : FfxFloat32(0.0f);
        w.y = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal10, linear_depth, depth10) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0f ? FfxFloat32(1.0f) : FfxFloat32(0.0f);
        w.z = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal01, linear_depth, depth01) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0f ? FfxFloat32(1.0f) : FfxFloat32(0.0f);
        w.w = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, normal11, linear_depth, depth11) > FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD / 2.0f ? FfxFloat32(1.0f) : FfxFloat32(0.0f);
        // And then mix in bilinear weights
        w.x           = FfxFloat32(w.x * (1.0 - uvx) * (1.0 - uvy));
        w.y           = FfxFloat32(w.y * (uvx) * (1.0 - uvy));
        w.z           = FfxFloat32(w.z * (1.0 - uvx) * (uvy));
        w.w           = FfxFloat32(w.w * (uvx) * (uvy));
        FfxFloat32 ws = max(w.x + w.y + w.z + w.w, FfxFloat32(1.0e-3));
        // normalize
        w /= ws;

        FfxFloat32x3 history_normal;
        FfxFloat32       history_linear_depth;
        reprojection         = reprojection00 * w.x + reprojection10 * w.y + reprojection01 * w.z + reprojection11 * w.w;
        history_linear_depth = depth00 * w.x + depth10 * w.y + depth01 * w.z + depth11 * w.w;
        history_normal       = normal00 * w.x + normal10 * w.y + normal01 * w.z + normal11 * w.w;
        disocclusion_factor  = FFX_DNSR_Reflections_GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
    }
    disocclusion_factor = disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD ? FfxFloat32(0.0f) : disocclusion_factor;
}

void FFX_DNSR_Reflections_Reproject(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxUInt32x2 screen_size, FfxFloat32 temporal_stability_factor, FfxInt32 max_samples) {
    FFX_DNSR_Reflections_InitializeGroupSharedMemory(dispatch_thread_id, group_thread_id, FfxInt32x2(screen_size));
    FFX_GROUP_MEMORY_BARRIER;

    group_thread_id += 4; // Center threads in FFX_GROUPSHARED memory

    FfxFloat32       variance    = FfxFloat32(1.0f);
    FfxFloat32       num_samples = FfxFloat32(0.0f);
    FfxFloat32       roughness   = FFX_DNSR_Reflections_LoadRoughness(dispatch_thread_id);
    FfxFloat32x3     normal      = FFX_DNSR_Reflections_LoadWorldSpaceNormal(dispatch_thread_id);
    FfxFloat32x3     radiance    = FFX_DNSR_Reflections_LoadRadiance(dispatch_thread_id);
    const FfxFloat32 ray_length  = FFX_DNSR_Reflections_LoadRayLength(dispatch_thread_id);

    if (FFX_DNSR_Reflections_IsGlossyReflection(roughness)) {
        FfxFloat32  disocclusion_factor;
        FfxFloat32x2      reprojection_uv;
        FfxFloat32x3 reprojection;
        FFX_DNSR_Reflections_PickReprojection(/*in*/ dispatch_thread_id,
                                              /* in */ group_thread_id,
                                              /* in */ screen_size,
                                              /* in */ roughness,
                                              /* in */ ray_length,
                                              /* out */ disocclusion_factor,
                                              /* out */ reprojection_uv,
                                              /* out */ reprojection);
        if ( (reprojection_uv.x > 0.0f) && (reprojection_uv.y > 0.0f) && (reprojection_uv.x < 1.0f) && (reprojection_uv.y< 1.0f)) {
            FfxFloat32 prev_variance = FFX_DNSR_Reflections_SampleVarianceHistory(reprojection_uv);
            num_samples              = FFX_DNSR_Reflections_SampleNumSamplesHistory(reprojection_uv) * disocclusion_factor;
            FfxFloat32 s_max_samples = FfxFloat32(max(8.0f, max_samples * FFX_DNSR_REFLECTIONS_SAMPLES_FOR_ROUGHNESS(roughness)));
            num_samples              = min(s_max_samples, num_samples + FfxFloat32(1.0f));
            FfxFloat32 new_variance  = FFX_DNSR_Reflections_ComputeTemporalVariance(radiance.xyz, reprojection.xyz);
            if (disocclusion_factor < FFX_DNSR_REFLECTIONS_DISOCCLUSION_THRESHOLD) {
                FFX_DNSR_Reflections_StoreRadianceReprojected(dispatch_thread_id, FfxFloat32x3(0.0f, 0.0f, 0.0f));
                FFX_DNSR_Reflections_StoreVariance(dispatch_thread_id, FfxFloat32(1.0f));
                FFX_DNSR_Reflections_StoreNumSamples(dispatch_thread_id, FfxFloat32(1.0f));
            } else {
                FfxFloat32 variance_mix = ffxLerp(new_variance, prev_variance, FfxFloat32(1.0f / num_samples));
                FFX_DNSR_Reflections_StoreRadianceReprojected(dispatch_thread_id, reprojection);
                FFX_DNSR_Reflections_StoreVariance(dispatch_thread_id, variance_mix);
                FFX_DNSR_Reflections_StoreNumSamples(dispatch_thread_id, num_samples);
                // Mix in reprojection for radiance mip computation 
                radiance = ffxLerp(radiance, reprojection, FfxFloat32(0.3f));
            }
        } else {
            FFX_DNSR_Reflections_StoreRadianceReprojected(dispatch_thread_id, FfxFloat32x3(0.0f, 0.0f, 0.0f));
            FFX_DNSR_Reflections_StoreVariance(dispatch_thread_id, FfxFloat32(1.0f));
            FFX_DNSR_Reflections_StoreNumSamples(dispatch_thread_id, FfxFloat32(1.0f));
        }
    }
    
    // Downsample 8x8 -> 1 radiance using FFX_GROUPSHARED memory
    // Initialize FFX_GROUPSHARED array for downsampling
    FfxFloat32 weight = FFX_DNSR_Reflections_GetLuminanceWeight(radiance.xyz);
    radiance.xyz *= weight;
    if ( (dispatch_thread_id.x >= screen_size.x) || (dispatch_thread_id.y >= screen_size.y) || any(isinf(radiance)) || any(isnan(radiance)) || (weight > FfxFloat32(1.0e3))) {
        radiance = FfxFloat32x3(0.0f, 0.0f, 0.0f);
        weight   = FfxFloat32(0.0f);
    }

    group_thread_id -= 4; // Center threads in FFX_GROUPSHARED memory

    FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id, FfxFloat32x4(radiance.xyz, weight));
    FFX_GROUP_MEMORY_BARRIER;

    for (FfxInt32 i = 2; i <= 8; i = i * 2) {
        FfxInt32 ox = group_thread_id.x * i;
        FfxInt32 oy = group_thread_id.y * i;
        FfxInt32 ix = group_thread_id.x * i + i / 2;
        FfxInt32 iy = group_thread_id.y * i + i / 2;
        if (ix < 8 && iy < 8) {
            FfxFloat32x4 rad_weight00 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2(ox, oy));
            FfxFloat32x4 rad_weight10 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2(ox, iy));
            FfxFloat32x4 rad_weight01 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2(ix, oy));
            FfxFloat32x4 rad_weight11 = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2(ix, iy));
            FfxFloat32x4 sum          = rad_weight00 + rad_weight01 + rad_weight10 + rad_weight11;
            FFX_DNSR_Reflections_StoreInGroupSharedMemory(FfxInt32x2(ox, oy), sum);
        }
        FFX_GROUP_MEMORY_BARRIER;
    }

    if ((group_thread_id.x == 0) && (group_thread_id.y == 0)) {
        FfxFloat32x4    sum          = FFX_DNSR_Reflections_LoadFromGroupSharedMemoryRaw(FfxInt32x2(0, 0));
        FfxFloat32      weight_acc   = max(sum.w, FfxFloat32(1.0e-3));
        FfxFloat32x3    radiance_avg = sum.xyz / weight_acc;
        FFX_DNSR_Reflections_StoreAverageRadiance(dispatch_thread_id.xy / 8, FfxFloat32x3(radiance_avg));
    }
}

#endif // #if FFX_HALF

void Reproject(FfxUInt32 group_index, FfxUInt32 group_id, FfxUInt32x2 group_thread_id) {
    FfxUInt32   packed_coords = GetDenoiserTile(group_id);
    FfxInt32x2  dispatch_thread_id = FfxInt32x2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + FfxInt32x2(group_thread_id);
    FfxInt32x2  dispatch_group_id = dispatch_thread_id / 8;
    FfxInt32x2  remapped_group_thread_id = FfxInt32x2(ffxRemapForWaveReduction(group_index));
    FfxInt32x2  remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    FFX_DNSR_Reflections_Reproject(remapped_dispatch_thread_id, remapped_group_thread_id, RenderSize(), TemporalStabilityFactor(), 32);
}

#endif // FFX_DNSR_REFLECTIONS_REPROJECT
