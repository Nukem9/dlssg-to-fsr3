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

#ifndef FFX_DNSR_REFLECTIONS_PREFILTER
#define FFX_DNSR_REFLECTIONS_PREFILTER

#include "ffx_denoiser_reflections_common.h"

FFX_GROUPSHARED FfxUInt32  g_ffx_dnsr_shared_0[16][16];
FFX_GROUPSHARED FfxUInt32  g_ffx_dnsr_shared_1[16][16];
FFX_GROUPSHARED FfxUInt32  g_ffx_dnsr_shared_2[16][16];
FFX_GROUPSHARED FfxUInt32  g_ffx_dnsr_shared_3[16][16];
FFX_GROUPSHARED FfxFloat32 g_ffx_dnsr_shared_depth[16][16];

#if FFX_HALF

struct FFX_DNSR_Reflections_NeighborhoodSample {
    FfxFloat16x3    radiance;
    FfxFloat16      variance;
    FfxFloat16x3    normal;
    FfxFloat32      depth;
};

FFX_DNSR_Reflections_NeighborhoodSample FFX_DNSR_Reflections_LoadFromGroupSharedMemory(FfxInt32x2 idx) {
    FfxUInt32x2     packed_radiance          = FfxUInt32x2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    FfxFloat16x4    unpacked_radiance        = FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance);
    FfxUInt32x2     packed_normal_variance   = FfxUInt32x2(g_ffx_dnsr_shared_2[idx.y][idx.x], g_ffx_dnsr_shared_3[idx.y][idx.x]);
    FfxFloat16x4    unpacked_normal_variance = FFX_DNSR_Reflections_UnpackFloat16_4(packed_normal_variance);

    FFX_DNSR_Reflections_NeighborhoodSample neighborSample;
    neighborSample.radiance = unpacked_radiance.xyz;
    neighborSample.normal   = unpacked_normal_variance.xyz;
    neighborSample.variance = unpacked_normal_variance.w;
    neighborSample.depth    = g_ffx_dnsr_shared_depth[idx.y][idx.x];
    return neighborSample;
}

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(FfxInt32x2 group_thread_id, FfxFloat16x3 radiance, FfxFloat16 variance, FfxFloat16x3 normal, FfxFloat32 depth) {
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.zz);
    g_ffx_dnsr_shared_2[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(normal.xy);
    g_ffx_dnsr_shared_3[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(FfxFloat16x2(normal.z, variance));
    g_ffx_dnsr_shared_depth[group_thread_id.y][group_thread_id.x] = depth;
}

void FFX_DNSR_Reflections_LoadNeighborhood(
    FfxInt32x2 pixel_coordinate,
    FFX_PARAMETER_OUT FfxFloat16x3 radiance,
    FFX_PARAMETER_OUT FfxFloat16 variance,
    FFX_PARAMETER_OUT FfxFloat16x3 normal,
    FFX_PARAMETER_OUT FfxFloat32 depth,
    FfxInt32x2 screen_size) {

    radiance = LoadRadianceH(FfxInt32x3(pixel_coordinate, 0));
    variance = LoadVarianceH(FfxInt32x3(pixel_coordinate, 0));

    normal = FFX_DENOISER_LoadWorldSpaceNormalH(pixel_coordinate);

    FfxFloat32x2 uv = (pixel_coordinate.xy + (0.5f).xx) / FfxFloat32x2(screen_size.xy);
    depth = FFX_DNSR_Reflections_GetLinearDepth(uv, FFX_DENOISER_LoadDepth(pixel_coordinate, 0));
}

void FFX_DNSR_Reflections_InitializeGroupSharedMemory(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxInt32x2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks.
    FfxInt32x2 offset[4] = {FfxInt32x2(0, 0), FfxInt32x2(8, 0), FfxInt32x2(0, 8), FfxInt32x2(8, 8)};

    // Intermediate storage registers to cache the result of all loads
    FfxFloat16x3 radiance[4];
    FfxFloat16  variance[4];
    FfxFloat16x3 normal[4];
    FfxFloat32       depth[4];

    // Start in the upper left corner of the 16x16 region.
    dispatch_thread_id -= 4;

    // First store all loads in registers
    for (FfxInt32 i = 0; i < 4; ++i) {
        FFX_DNSR_Reflections_LoadNeighborhood(dispatch_thread_id + offset[i], radiance[i], variance[i], normal[i], depth[i], screen_size);
    }

    // Then move all registers to groupshared memory
    for (FfxInt32 j = 0; j < 4; ++j) {
        FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id + offset[j], radiance[j], variance[j], normal[j], depth[j]); // X
    }
}

FfxFloat16 FFX_DNSR_Reflections_GetEdgeStoppingNormalWeight(FfxFloat16x3 normal_p, FfxFloat16x3 normal_q) {
    return FfxFloat16(pow(max(dot(normal_p, normal_q), FfxFloat16(0.0f)), FFX_DNSR_REFLECTIONS_PREFILTER_NORMAL_SIGMA));
}

FfxFloat16 FFX_DNSR_Reflections_GetEdgeStoppingDepthWeight(FfxFloat32 center_depth, FfxFloat32 neighbor_depth) {
    return FfxFloat16(exp(-abs(center_depth - neighbor_depth) * center_depth * FFX_DNSR_REFLECTIONS_PREFILTER_DEPTH_SIGMA));
}

FfxFloat16 FFX_DNSR_Reflections_GetRadianceWeight(FfxFloat16x3 center_radiance, FfxFloat16x3 neighbor_radiance, FfxFloat16 variance) {
    return FfxFloat16(max(exp(-(FFX_DNSR_REFLECTIONS_RADIANCE_WEIGHT_BIAS + variance * FFX_DNSR_REFLECTIONS_RADIANCE_WEIGHT_VARIANCE_K)
                    * length(center_radiance - neighbor_radiance.xyz))
            , 1.0e-2));
}

void FFX_DNSR_Reflections_Resolve(FfxInt32x2 group_thread_id, FfxFloat16x3 avg_radiance, FFX_DNSR_Reflections_NeighborhoodSample center,
                                  out FfxFloat16x3 resolved_radiance, out FfxFloat16 resolved_variance) {
    // Initial weight is important to remove fireflies.
    // That removes quite a bit of energy but makes everything much more stable.
    FfxFloat16  accumulated_weight   = FFX_DNSR_Reflections_GetRadianceWeight(avg_radiance, center.radiance.xyz, center.variance);
    FfxFloat16x3 accumulated_radiance = center.radiance.xyz * accumulated_weight;
    FfxFloat16  accumulated_variance = center.variance * accumulated_weight * accumulated_weight;
    // First 15 numbers of Halton(2,3) streteched to [-3,3]. Skipping the center, as we already have that in center_radiance and center_variance.
    const FfxUInt32 sample_count     = 15;
    const FfxInt32x2 sample_offsets[] = {FfxInt32x2(0, 1),  FfxInt32x2(-2, 1),  FfxInt32x2(2, -3), FfxInt32x2(-3, 0),  FfxInt32x2(1, 2), FfxInt32x2(-1, -2), FfxInt32x2(3, 0), FfxInt32x2(-3, 3),
                                   FfxInt32x2(0, -3), FfxInt32x2(-1, -1), FfxInt32x2(2, 1),  FfxInt32x2(-2, -2), FfxInt32x2(1, 0), FfxInt32x2(0, 2),   FfxInt32x2(3, -1)};
    FfxFloat16 variance_weight = FfxFloat16(max(FFX_DNSR_REFLECTIONS_PREFILTER_VARIANCE_BIAS,
                                     1.0 - exp(-(center.variance * FFX_DNSR_REFLECTIONS_PREFILTER_VARIANCE_WEIGHT))
                                    ));
    for (FfxInt32 i = 0; i < sample_count; ++i) {
        FfxInt32x2                                    new_idx  = group_thread_id + sample_offsets[i];
        FFX_DNSR_Reflections_NeighborhoodSample neighbor = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(new_idx);

        FfxFloat16 weight = FfxFloat16(1.0f);
        weight *= FFX_DNSR_Reflections_GetEdgeStoppingNormalWeight(center.normal, neighbor.normal);
        weight *= FFX_DNSR_Reflections_GetEdgeStoppingDepthWeight(center.depth, neighbor.depth);
        weight *= FFX_DNSR_Reflections_GetRadianceWeight(avg_radiance, neighbor.radiance.xyz, center.variance);
        weight *= variance_weight;

        // Accumulate all contributions.
        accumulated_weight += weight;
        accumulated_radiance += weight * neighbor.radiance.xyz;
        accumulated_variance += weight * weight * neighbor.variance;
    }

    accumulated_radiance /= accumulated_weight;
    accumulated_variance /= (accumulated_weight * accumulated_weight);
    resolved_radiance = accumulated_radiance;
    resolved_variance = accumulated_variance;
}

void FFX_DNSR_Reflections_Prefilter(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxUInt32x2 screen_size) {
    FfxFloat16 center_roughness = FFX_DNSR_Reflections_LoadRoughness(dispatch_thread_id);
    FFX_DNSR_Reflections_InitializeGroupSharedMemory(dispatch_thread_id, group_thread_id, FfxInt32x2(screen_size));
    FFX_GROUP_MEMORY_BARRIER;

    group_thread_id += 4; // Center threads in groupshared memory

    FFX_DNSR_Reflections_NeighborhoodSample center = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(group_thread_id);

    FfxFloat16x3 resolved_radiance = center.radiance;
    FfxFloat16  resolved_variance = center.variance;

    // Check if we have to denoise or if a simple copy is enough
    bool needs_denoiser = center.variance > 0.0 && FFX_DNSR_Reflections_IsGlossyReflection(center_roughness) && !FFX_DNSR_Reflections_IsMirrorReflection(center_roughness);
    if (needs_denoiser) {
        FfxFloat32x2      uv8          = (FfxFloat32x2(dispatch_thread_id.xy) + (0.5).xx) / FFX_DNSR_Reflections_RoundUp8(screen_size);
        FfxFloat16x3 avg_radiance = FFX_DNSR_Reflections_SampleAverageRadiance(uv8);
        FFX_DNSR_Reflections_Resolve(group_thread_id, avg_radiance, center, resolved_radiance, resolved_variance);
    }

    FFX_DNSR_Reflections_StorePrefilteredReflections(dispatch_thread_id, resolved_radiance, resolved_variance);
}

#else // FFX_HALF

struct FFX_DNSR_Reflections_NeighborhoodSample {
    FfxFloat32x3    radiance;
    FfxFloat32      variance;
    FfxFloat32x3    normal;
    FfxFloat32      depth;
};

FFX_DNSR_Reflections_NeighborhoodSample FFX_DNSR_Reflections_LoadFromGroupSharedMemory(FfxInt32x2 idx) {
    FfxUInt32x2     packed_radiance          = FfxUInt32x2(g_ffx_dnsr_shared_0[idx.y][idx.x], g_ffx_dnsr_shared_1[idx.y][idx.x]);
    FfxFloat32x4    unpacked_radiance        = FFX_DNSR_Reflections_UnpackFloat16_4(packed_radiance);
    FfxUInt32x2     packed_normal_variance   = FfxUInt32x2(g_ffx_dnsr_shared_2[idx.y][idx.x], g_ffx_dnsr_shared_3[idx.y][idx.x]);
    FfxFloat32x4    unpacked_normal_variance = FFX_DNSR_Reflections_UnpackFloat16_4(packed_normal_variance);

    FFX_DNSR_Reflections_NeighborhoodSample neighborSample;
    neighborSample.radiance = unpacked_radiance.xyz;
    neighborSample.normal   = unpacked_normal_variance.xyz;
    neighborSample.variance = unpacked_normal_variance.w;
    neighborSample.depth    = g_ffx_dnsr_shared_depth[idx.y][idx.x];
    return neighborSample;
}

void FFX_DNSR_Reflections_StoreInGroupSharedMemory(FfxInt32x2 group_thread_id, FfxFloat32x3 radiance, FfxFloat32 variance, FfxFloat32x3 normal, FfxFloat32 depth) {
    g_ffx_dnsr_shared_0[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.xy);
    g_ffx_dnsr_shared_1[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(radiance.zz);
    g_ffx_dnsr_shared_2[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(normal.xy);
    g_ffx_dnsr_shared_3[group_thread_id.y][group_thread_id.x]     = FFX_DNSR_Reflections_PackFloat16(FfxFloat32x2(normal.z, variance));
    g_ffx_dnsr_shared_depth[group_thread_id.y][group_thread_id.x] = depth;
}

void FFX_DNSR_Reflections_LoadNeighborhood(
    FfxInt32x2 pixel_coordinate,
    FFX_PARAMETER_OUT FfxFloat32x3 radiance,
    FFX_PARAMETER_OUT FfxFloat32 variance,
    FFX_PARAMETER_OUT FfxFloat32x3 normal,
    FFX_PARAMETER_OUT FfxFloat32 depth,
    FfxInt32x2 screen_size) {

    radiance = LoadRadiance(FfxInt32x3(pixel_coordinate, 0));
    variance = LoadVariance(FfxInt32x3(pixel_coordinate, 0));

    normal = FFX_DENOISER_LoadWorldSpaceNormal(pixel_coordinate);

    FfxFloat32x2 uv = (pixel_coordinate.xy + (0.5f).xx) / FfxFloat32x2(screen_size.xy);
    depth = FFX_DNSR_Reflections_GetLinearDepth(uv, FFX_DENOISER_LoadDepth(pixel_coordinate, 0));
}

void FFX_DNSR_Reflections_InitializeGroupSharedMemory(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxInt32x2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks.
    FfxInt32x2 offset[4] = {FfxInt32x2(0, 0), FfxInt32x2(8, 0), FfxInt32x2(0, 8), FfxInt32x2(8, 8)};

    // Intermediate storage registers to cache the result of all loads
    FfxFloat32x3 radiance[4];
    FfxFloat32  variance[4];
    FfxFloat32x3 normal[4];
    FfxFloat32       depth[4];

    // Start in the upper left corner of the 16x16 region.
    dispatch_thread_id -= 4;

    // First store all loads in registers
    for (FfxInt32 i = 0; i < 4; ++i) {
        FFX_DNSR_Reflections_LoadNeighborhood(dispatch_thread_id + offset[i], radiance[i], variance[i], normal[i], depth[i], screen_size);
    }

    // Then move all registers to groupshared memory
    for (FfxInt32 j = 0; j < 4; ++j) {
        FFX_DNSR_Reflections_StoreInGroupSharedMemory(group_thread_id + offset[j], radiance[j], variance[j], normal[j], depth[j]); // X
    }
}

FfxFloat32 FFX_DNSR_Reflections_GetEdgeStoppingNormalWeight(FfxFloat32x3 normal_p, FfxFloat32x3 normal_q) {
    return FfxFloat32(pow(max(dot(normal_p, normal_q), FfxFloat32(0.0f)), FFX_DNSR_REFLECTIONS_PREFILTER_NORMAL_SIGMA));
}

FfxFloat32 FFX_DNSR_Reflections_GetEdgeStoppingDepthWeight(FfxFloat32 center_depth, FfxFloat32 neighbor_depth) {
    return FfxFloat32(exp(-abs(center_depth - neighbor_depth) * center_depth * FFX_DNSR_REFLECTIONS_PREFILTER_DEPTH_SIGMA));
}

FfxFloat32 FFX_DNSR_Reflections_GetRadianceWeight(FfxFloat32x3 center_radiance, FfxFloat32x3 neighbor_radiance, FfxFloat32 variance) {
    return FfxFloat32(max(exp(-(FFX_DNSR_REFLECTIONS_RADIANCE_WEIGHT_BIAS + variance * FFX_DNSR_REFLECTIONS_RADIANCE_WEIGHT_VARIANCE_K)
                    * length(center_radiance - neighbor_radiance.xyz))
            , 1.0e-2));
}

void FFX_DNSR_Reflections_Resolve(FfxInt32x2 group_thread_id, FfxFloat32x3 avg_radiance, FFX_DNSR_Reflections_NeighborhoodSample center,
                                  out FfxFloat32x3 resolved_radiance, out FfxFloat32 resolved_variance) {
    // Initial weight is important to remove fireflies.
    // That removes quite a bit of energy but makes everything much more stable.
    FfxFloat32  accumulated_weight   = FFX_DNSR_Reflections_GetRadianceWeight(avg_radiance, center.radiance.xyz, center.variance);
    FfxFloat32x3 accumulated_radiance = center.radiance.xyz * accumulated_weight;
    FfxFloat32  accumulated_variance = center.variance * accumulated_weight * accumulated_weight;
    // First 15 numbers of Halton(2,3) streteched to [-3,3]. Skipping the center, as we already have that in center_radiance and center_variance.
    const FfxUInt32 sample_count     = 15;
    const FfxInt32x2 sample_offsets[] = {FfxInt32x2(0, 1),  FfxInt32x2(-2, 1),  FfxInt32x2(2, -3), FfxInt32x2(-3, 0),  FfxInt32x2(1, 2), FfxInt32x2(-1, -2), FfxInt32x2(3, 0), FfxInt32x2(-3, 3),
                                   FfxInt32x2(0, -3), FfxInt32x2(-1, -1), FfxInt32x2(2, 1),  FfxInt32x2(-2, -2), FfxInt32x2(1, 0), FfxInt32x2(0, 2),   FfxInt32x2(3, -1)};
    FfxFloat32 variance_weight = FfxFloat32(max(FFX_DNSR_REFLECTIONS_PREFILTER_VARIANCE_BIAS,
                                     1.0 - exp(-(center.variance * FFX_DNSR_REFLECTIONS_PREFILTER_VARIANCE_WEIGHT))
                                    ));
    for (FfxInt32 i = 0; i < sample_count; ++i) {
        FfxInt32x2                                    new_idx  = group_thread_id + sample_offsets[i];
        FFX_DNSR_Reflections_NeighborhoodSample neighbor = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(new_idx);

        FfxFloat32 weight = FfxFloat32(1.0f);
        weight *= FFX_DNSR_Reflections_GetEdgeStoppingNormalWeight(center.normal, neighbor.normal);
        weight *= FFX_DNSR_Reflections_GetEdgeStoppingDepthWeight(center.depth, neighbor.depth);
        weight *= FFX_DNSR_Reflections_GetRadianceWeight(avg_radiance, neighbor.radiance.xyz, center.variance);
        weight *= variance_weight;

        // Accumulate all contributions.
        accumulated_weight += weight;
        accumulated_radiance += weight * neighbor.radiance.xyz;
        accumulated_variance += weight * weight * neighbor.variance;
    }

    accumulated_radiance /= accumulated_weight;
    accumulated_variance /= (accumulated_weight * accumulated_weight);
    resolved_radiance = accumulated_radiance;
    resolved_variance = accumulated_variance;
}

void FFX_DNSR_Reflections_Prefilter(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxUInt32x2 screen_size) {
    FfxFloat32 center_roughness = FFX_DNSR_Reflections_LoadRoughness(dispatch_thread_id);
    FFX_DNSR_Reflections_InitializeGroupSharedMemory(dispatch_thread_id, group_thread_id, FfxInt32x2(screen_size));
    FFX_GROUP_MEMORY_BARRIER;

    group_thread_id += 4; // Center threads in groupshared memory

    FFX_DNSR_Reflections_NeighborhoodSample center = FFX_DNSR_Reflections_LoadFromGroupSharedMemory(group_thread_id);

    FfxFloat32x3 resolved_radiance = center.radiance;
    FfxFloat32  resolved_variance = center.variance;

    // Check if we have to denoise or if a simple copy is enough
    bool needs_denoiser = center.variance > 0.0 && FFX_DNSR_Reflections_IsGlossyReflection(center_roughness) && !FFX_DNSR_Reflections_IsMirrorReflection(center_roughness);
    if (needs_denoiser) {
        FfxFloat32x2      uv8          = (FfxFloat32x2(dispatch_thread_id.xy) + (0.5).xx) / FFX_DNSR_Reflections_RoundUp8(screen_size);
        FfxFloat32x3 avg_radiance = FFX_DNSR_Reflections_SampleAverageRadiance(uv8);
        FFX_DNSR_Reflections_Resolve(group_thread_id, avg_radiance, center, resolved_radiance, resolved_variance);
    }

    FFX_DNSR_Reflections_StorePrefilteredReflections(dispatch_thread_id, resolved_radiance, resolved_variance);
}

#endif // FFX_HALF

void Prefilter(FfxUInt32 group_index, FfxUInt32 group_id, FfxInt32x2 group_thread_id) {
    FfxUInt32   packed_coords = GetDenoiserTile(group_id);
    FfxInt32x2  dispatch_thread_id = FfxInt32x2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + group_thread_id;
    FfxInt32x2  dispatch_group_id = dispatch_thread_id / 8;
    FfxInt32x2 remapped_group_thread_id = FfxInt32x2(ffxRemapForWaveReduction(group_index));
    FfxInt32x2 remapped_dispatch_thread_id = FfxInt32x2(dispatch_group_id * 8 + remapped_group_thread_id);

    FFX_DNSR_Reflections_Prefilter(remapped_dispatch_thread_id, remapped_group_thread_id, RenderSize());
}

#else

void Prefilter(FfxUInt32 group_index, FfxUInt32 group_id, FfxInt32x2 group_thread_id) {}

#endif // FFX_DNSR_REFLECTIONS_PREFILTER
