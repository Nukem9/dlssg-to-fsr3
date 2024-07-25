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

#ifndef FFX_DNSR_REFLECTIONS_RESOLVE_TEMPORAL
#define FFX_DNSR_REFLECTIONS_RESOLVE_TEMPORAL

#define RADIANCE_THRESHOLD 0.0001

#define FFX_DNSR_REFLECTIONS_ESTIMATES_LOCAL_NEIGHBORHOOD
#include "ffx_denoiser_reflections_common.h"

FFX_GROUPSHARED FfxUInt32 g_ffx_dnsr_shared_0[16][16];
FFX_GROUPSHARED FfxUInt32 g_ffx_dnsr_shared_1[16][16];

#if FFX_HALF

struct FFX_DNSR_Reflections_NeighborhoodSample {
    FfxFloat16x3 radiance;
};

FFX_DNSR_Reflections_NeighborhoodSample FFX_DNSR_Reflections_LoadFromGroupSharedMemory(FfxInt32x2 idx) {
    FfxUInt32x2  packed_radiance   = FfxUInt32x2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    FfxFloat16x3 unpacked_radiance = FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance).xyz;

    FFX_DNSR_Reflections_NeighborhoodSample neighborSample;
    neighborSample.radiance = unpacked_radiance;
    return neighborSample;
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
            FfxFloat16   weight   = FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(FfxFloat16(i)) * FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(FfxFloat16(j));
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

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(FfxInt32x2 group_thread_id, FfxFloat16x3 radiance) {
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x] = FFX_DNSR_Reflections_PackFloat16(radiance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x] = FFX_DNSR_Reflections_PackFloat16(radiance.zz);
}

void FFX_DNSR_Reflections_LoadNeighborhood(FfxInt32x2 pixel_coordinate, out FfxFloat16x3 radiance) { radiance = FFX_DNSR_Reflections_LoadRadiance(pixel_coordinate); }

void FFX_DNSR_Reflections_InitializeGroupSharedMemory(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxInt32x2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks.
    FfxInt32x2 offset[4] = {FfxInt32x2(0, 0), FfxInt32x2(8, 0), FfxInt32x2(0, 8), FfxInt32x2(8, 8)};

    // Intermediate storage registers to cache the result of all loads
    FfxFloat16x3 radiance[4];

    // Start in the upper left corner of the 16x16 region.
    dispatch_thread_id -= 4;

    // First store all loads in registers
    for (FfxInt32 i = 0; i < 4; ++i) {
        FFX_DNSR_Reflections_LoadNeighborhood(dispatch_thread_id + offset[i], radiance[i]);
    }

    // Then move all registers to FFX_GROUPSHARED memory
    for (FfxInt32 j = 0; j < 4; ++j) {
        FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id + offset[j], radiance[j]);
    }
}

void FFX_DNSR_Reflections_ResolveTemporal(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxUInt32x2 screen_size, FfxFloat32x2 inv_screen_size, FfxFloat32 history_clip_weight) {
    FFX_DNSR_Reflections_InitializeGroupSharedMemory(dispatch_thread_id, group_thread_id, FfxInt32x2(screen_size));
    FFX_GROUP_MEMORY_BARRIER;

    group_thread_id += 4; // Center threads in FFX_GROUPSHARED memory

    FfxFloat16x3 radiance = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(group_thread_id).radiance;
    FfxFloat32 radianceSum = radiance.x + radiance.y + radiance.z;
    if (radianceSum < RADIANCE_THRESHOLD)
    {
        FFX_DNSR_Reflections_StoreTemporalAccumulation(dispatch_thread_id, FfxFloat16x3(0.0f, 0.0f, 0.0f), FfxFloat16(0.0f));
        return;
    }

    FFX_DNSR_Reflections_NeighborhoodSample center       = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(group_thread_id);
    FfxFloat16x3                            new_signal   = center.radiance;
    FfxFloat16                              roughness    = FFX_DNSR_Reflections_LoadRoughness(dispatch_thread_id);
    FfxFloat16                              new_variance = FFX_DNSR_Reflections_LoadVariance(dispatch_thread_id);
    
    if (FFX_DNSR_Reflections_IsGlossyReflection(roughness)) {
        FfxFloat16      num_samples  = FFX_DNSR_Reflections_LoadNumSamples(dispatch_thread_id);
        FfxFloat32x2    uv8          = (FfxFloat32x2(dispatch_thread_id.xy) + (0.5).xx) / FFX_DNSR_Reflections_RoundUp8(screen_size);
        FfxFloat16x3    avg_radiance = FFX_DNSR_Reflections_SampleAverageRadiance(uv8);

        FfxFloat16x3                  old_signal         = FFX_DNSR_Reflections_LoadRadianceReprojected(dispatch_thread_id);
        FFX_DNSR_Reflections_Moments local_neighborhood  = FFX_DNSR_Reflections_EstimateLocalNeighborhoodInGroup(group_thread_id);
        // Clip history based on the curren local statistics
        FfxFloat16x3                  color_std          = FfxFloat16x3((sqrt(local_neighborhood.variance.xyz) + length(local_neighborhood.mean.xyz - avg_radiance)) * history_clip_weight * 1.4f);
                             local_neighborhood.mean.xyz = ffxLerp(local_neighborhood.mean.xyz, avg_radiance, FfxFloat16x3(0.2f, 0.2f, 0.2f));
        FfxFloat16x3                  radiance_min       = local_neighborhood.mean.xyz - color_std;
        FfxFloat16x3                  radiance_max       = local_neighborhood.mean.xyz + color_std;
        FfxFloat16x3                  clipped_old_signal = FFX_DNSR_Reflections_ClipAABB(radiance_min, radiance_max, old_signal.xyz);
        FfxFloat16                    accumulation_speed = FfxFloat16(1.0f) / max(num_samples, FfxFloat16(1.0f));
        FfxFloat16                    weight             = (FfxFloat16(1.0f) - accumulation_speed);
        // Blend with average for small sample count
        new_signal.xyz                                   = ffxLerp(new_signal.xyz, avg_radiance, FfxFloat16(1.0f) / max(num_samples + FfxFloat16(1.0f), FfxFloat16(1.0f)));
        // Clip outliers
        {
            FfxFloat16x3                  radiance_min   = avg_radiance.xyz - color_std * FfxFloat16x3(1.0f, 1.0f, 1.0f);
            FfxFloat16x3                  radiance_max   = avg_radiance.xyz + color_std * FfxFloat16x3(1.0f, 1.0f, 1.0f);
            new_signal.xyz                               = FFX_DNSR_Reflections_ClipAABB(radiance_min, radiance_max, new_signal.xyz);
        }
        // Blend with history
#ifdef FFX_GLSL
        new_signal                                       = (FfxFloat16(1.0) - weight) * new_signal + weight * clipped_old_signal;
#else
        new_signal                                       = ffxLerp(new_signal, clipped_old_signal, weight);
#endif
        new_variance                                     = ffxLerp(FFX_DNSR_Reflections_ComputeTemporalVariance(new_signal.xyz, clipped_old_signal.xyz), new_variance, weight);
        if (any(isinf(new_signal)) || any(isnan(new_signal)) || isinf(new_variance) || isnan(new_variance)) {
            new_signal = FfxFloat16x3(0.0f, 0.0f, 0.0f);
            new_variance = FfxFloat16(0.0f);
        }

    }
    FFX_DNSR_Reflections_StoreTemporalAccumulation(dispatch_thread_id, new_signal, new_variance);
}

#else // #if FFX_HALF

struct FFX_DNSR_Reflections_NeighborhoodSample {
    FfxFloat32x3 radiance;
};

FFX_DNSR_Reflections_NeighborhoodSample FFX_DNSR_Reflections_LoadFromGroupSharedMemory(FfxInt32x2 idx) {
    FfxUInt32x2  packed_radiance   = FfxUInt32x2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    FfxFloat32x3 unpacked_radiance = FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance).xyz;

    FFX_DNSR_Reflections_NeighborhoodSample neighborSample;
    neighborSample.radiance = unpacked_radiance;
    return neighborSample;
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
            FfxFloat32   weight   = FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(FfxFloat32(i)) * FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(FfxFloat32(j));
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

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(FfxInt32x2 group_thread_id, FfxFloat32x3 radiance) {
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x] = FFX_DNSR_Reflections_PackFloat16(radiance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x] = FFX_DNSR_Reflections_PackFloat16(radiance.zz);
}

void FFX_DNSR_Reflections_LoadNeighborhood(FfxInt32x2 pixel_coordinate, out FfxFloat32x3 radiance) { radiance = FFX_DNSR_Reflections_LoadRadiance(pixel_coordinate); }

void FFX_DNSR_Reflections_InitializeGroupSharedMemory(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxInt32x2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks.
    FfxInt32x2 offset[4] = {FfxInt32x2(0, 0), FfxInt32x2(8, 0), FfxInt32x2(0, 8), FfxInt32x2(8, 8)};

    // Intermediate storage registers to cache the result of all loads
    FfxFloat32x3 radiance[4];

    // Start in the upper left corner of the 16x16 region.
    dispatch_thread_id -= 4;

    // First store all loads in registers
    for (FfxInt32 i = 0; i < 4; ++i) {
        FFX_DNSR_Reflections_LoadNeighborhood(dispatch_thread_id + offset[i], radiance[i]);
    }

    // Then move all registers to FFX_GROUPSHARED memory
    for (FfxInt32 j = 0; j < 4; ++j) {
        FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id + offset[j], radiance[j]);
    }
}

void FFX_DNSR_Reflections_ResolveTemporal(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxUInt32x2 screen_size, FfxFloat32x2 inv_screen_size, FfxFloat32 history_clip_weight) {
    FFX_DNSR_Reflections_InitializeGroupSharedMemory(dispatch_thread_id, group_thread_id, FfxInt32x2(screen_size));
    FFX_GROUP_MEMORY_BARRIER;

    group_thread_id += 4; // Center threads in FFX_GROUPSHARED memory

    FfxFloat32x3 radiance = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(group_thread_id).radiance;
    FfxFloat32 radianceSum = radiance.x + radiance.y + radiance.z;
    if (radianceSum < RADIANCE_THRESHOLD)
    {
        FFX_DNSR_Reflections_StoreTemporalAccumulation(dispatch_thread_id, FfxFloat32x3(0.0f, 0.0f, 0.0f), FfxFloat32(0.0f));
        return;
    }

    FFX_DNSR_Reflections_NeighborhoodSample center       = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(group_thread_id);
    FfxFloat32x3                            new_signal   = center.radiance;
    FfxFloat32                              roughness    = FFX_DNSR_Reflections_LoadRoughness(dispatch_thread_id);
    FfxFloat32                              new_variance = FFX_DNSR_Reflections_LoadVariance(dispatch_thread_id);
    
    if (FFX_DNSR_Reflections_IsGlossyReflection(roughness)) {
        FfxFloat32      num_samples  = FFX_DNSR_Reflections_LoadNumSamples(dispatch_thread_id);
        FfxFloat32x2    uv8          = (FfxFloat32x2(dispatch_thread_id.xy) + (0.5).xx) / FFX_DNSR_Reflections_RoundUp8(screen_size);
        FfxFloat32x3    avg_radiance = FFX_DNSR_Reflections_SampleAverageRadiance(uv8);

        FfxFloat32x3                  old_signal         = FFX_DNSR_Reflections_LoadRadianceReprojected(dispatch_thread_id);
        FFX_DNSR_Reflections_Moments local_neighborhood  = FFX_DNSR_Reflections_EstimateLocalNeighborhoodInGroup(group_thread_id);
        // Clip history based on the curren local statistics
        FfxFloat32x3                  color_std          = FfxFloat32x3((sqrt(local_neighborhood.variance.xyz) + length(local_neighborhood.mean.xyz - avg_radiance)) * history_clip_weight * 1.4f);
                             local_neighborhood.mean.xyz = ffxLerp(local_neighborhood.mean.xyz, avg_radiance, FfxFloat32x3(0.2f, 0.2f, 0.2f));
        FfxFloat32x3                  radiance_min       = local_neighborhood.mean.xyz - color_std;
        FfxFloat32x3                  radiance_max       = local_neighborhood.mean.xyz + color_std;
        FfxFloat32x3                  clipped_old_signal = FFX_DNSR_Reflections_ClipAABB(radiance_min, radiance_max, old_signal.xyz);
        FfxFloat32                    accumulation_speed = FfxFloat32(1.0f) / max(num_samples, FfxFloat32(1.0f));
        FfxFloat32                    weight             = (FfxFloat32(1.0f) - accumulation_speed);
        // Blend with average for small sample count
        new_signal.xyz                                   = ffxLerp(new_signal.xyz, avg_radiance, FfxFloat32(1.0f) / max(num_samples + FfxFloat32(1.0f), FfxFloat32(1.0f)));
        // Clip outliers
        {
            FfxFloat32x3                  radiance_min   = avg_radiance.xyz - color_std * FfxFloat32x3(1.0f, 1.0f, 1.0f);
            FfxFloat32x3                  radiance_max   = avg_radiance.xyz + color_std * FfxFloat32x3(1.0f, 1.0f, 1.0f);
            new_signal.xyz                               = FFX_DNSR_Reflections_ClipAABB(radiance_min, radiance_max, new_signal.xyz);
        }
        // Blend with history
        new_signal                                       = ffxLerp(new_signal, clipped_old_signal, weight);
        new_variance                                     = ffxLerp(FFX_DNSR_Reflections_ComputeTemporalVariance(new_signal.xyz, clipped_old_signal.xyz), new_variance, weight);
        if (any(isinf(new_signal)) || any(isnan(new_signal)) || isinf(new_variance) || isnan(new_variance)) {
            new_signal   = FfxFloat32x3(0.0f, 0.0f, 0.0f);
            new_variance = FfxFloat32(0.0f);
        }

    }
    FFX_DNSR_Reflections_StoreTemporalAccumulation(dispatch_thread_id, new_signal, new_variance);
}

#endif // #if FFX_HALF

void ResolveTemporal(FfxUInt32 group_index, FfxUInt32 group_id, FfxUInt32x2 group_thread_id) {
    FfxUInt32  packed_coords = GetDenoiserTile(group_id);
    FfxInt32x2  dispatch_thread_id = FfxInt32x2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + FfxInt32x2(group_thread_id);
    FfxInt32x2  dispatch_group_id = dispatch_thread_id / 8;
    FfxInt32x2 remapped_group_thread_id = FfxInt32x2(ffxRemapForWaveReduction(group_index));
    FfxInt32x2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    FFX_DNSR_Reflections_ResolveTemporal(remapped_dispatch_thread_id, remapped_group_thread_id, RenderSize(), InverseRenderSize(), TemporalStabilityFactor());
}

#endif // FFX_DNSR_REFLECTIONS_RESOLVE_TEMPORAL
