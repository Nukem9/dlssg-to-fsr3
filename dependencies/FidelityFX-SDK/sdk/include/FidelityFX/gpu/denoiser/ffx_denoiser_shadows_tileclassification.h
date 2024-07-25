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

#ifndef FFX_DNSR_SHADOWS_TILECLASSIFICATION_HLSL
#define FFX_DNSR_SHADOWS_TILECLASSIFICATION_HLSL

#include "ffx_denoiser_shadows_util.h"

FFX_GROUPSHARED FfxInt32 g_FFX_DNSR_Shadows_false_count;
FfxBoolean FFX_DNSR_Shadows_ThreadGroupAllTrue(FfxBoolean val)
{
    const FfxUInt32 lane_count_in_thread_group = 64;
    if (ffxWaveLaneCount() == lane_count_in_thread_group)
    {
        return ffxWaveAllTrue(val);
    }
    else
    {
        FFX_GROUP_MEMORY_BARRIER;
        g_FFX_DNSR_Shadows_false_count = 0;
        FFX_GROUP_MEMORY_BARRIER;
        if (!val) g_FFX_DNSR_Shadows_false_count = 1;
        FFX_GROUP_MEMORY_BARRIER;
        return g_FFX_DNSR_Shadows_false_count == 0;
    }
}

void FFX_DNSR_Shadows_SearchSpatialRegion(FfxUInt32x2 gid, out FfxBoolean all_in_light, out FfxBoolean all_in_shadow)
{
    // The spatial passes can reach a total region of 1+2+4 = 7x7 around each block.
    // The masks are 8x4, so we need a larger vertical stride

    // Visualization - each x represents a 4x4 block, xx is one entire 8x4 mask as read from the raytracer result
    // Same for yy, these are the ones we are working on right now

    // xx xx xx
    // xx xx xx
    // xx yy xx <-- yy here is the base_tile below
    // xx yy xx
    // xx xx xx
    // xx xx xx

    // All of this should result in scalar ops
    FfxUInt32x2 base_tile = FFX_DNSR_Shadows_GetTileIndexFromPixelPosition(gid * FfxInt32x2(8, 8));

    // Load the entire region of masks in a scalar fashion
    FfxUInt32 combined_or_mask = 0;
    FfxUInt32 combined_and_mask = 0xFFFFFFFF;
    for (FfxInt32 j = -2; j <= 3; ++j)
    {
        for (FfxInt32 i = -1; i <= 1; ++i)
        {
            FfxInt32x2 tile_index = FfxInt32x2(base_tile) + FfxInt32x2(i, j);
            tile_index = clamp(tile_index, FfxInt32x2(0,0), FfxInt32x2(FFX_DNSR_Shadows_RoundedDivide(BufferDimensions().x, 8), FFX_DNSR_Shadows_RoundedDivide(BufferDimensions().y, 4)) - 1);
            const FfxUInt32 linear_tile_index = FFX_DNSR_Shadows_LinearTileIndex(tile_index, BufferDimensions().x);
            const FfxUInt32 shadow_mask = LoadRaytracedShadowMask(linear_tile_index);

            combined_or_mask = combined_or_mask | shadow_mask;
            combined_and_mask = combined_and_mask & shadow_mask;
        }
    }

    all_in_light = combined_and_mask == 0xFFFFFFFFu;
    all_in_shadow = combined_or_mask == 0u;
}

FfxFloat32 FFX_DNSR_Shadows_GetLinearDepth(FfxUInt32x2 did, FfxFloat32 depth)
{
    const FfxFloat32x2 uv = (did + 0.5f) * InvBufferDimensions();
    const FfxFloat32x2 ndc = 2.0f * FfxFloat32x2(uv.x, 1.0f - uv.y) - 1.0f;

    FfxFloat32x4 projected = FFX_MATRIX_MULTIPLY(ProjectionInverse(), FfxFloat32x4(ndc, depth, 1));

    return abs(projected.z / projected.w);
}

FfxBoolean FFX_DNSR_Shadows_IsDisoccluded(FfxUInt32x2 did, FfxFloat32 depth, FfxFloat32x2 velocity)
{
    const FfxInt32x2 dims = BufferDimensions();
    const FfxFloat32x2 texel_size = InvBufferDimensions();
    const FfxFloat32x2 uv = (did + 0.5f) * texel_size;
    const FfxFloat32x2 ndc = (2.0f * uv - 1.0f) * FfxFloat32x2(1.0f, -1.0f);
    const FfxFloat32x2 previous_uv = uv + velocity;

    FfxBoolean is_disoccluded = FFX_TRUE;

    if (all(FFX_GREATER_THAN(previous_uv, FfxFloat32x2(0,0))) && all(FFX_LESS_THAN(previous_uv, FfxFloat32x2(1,1))))
    {
        // Read the center values
        FfxFloat32x3 normal = LoadNormals(did);

        FfxFloat32x4 clip_space = FFX_MATRIX_MULTIPLY(ReprojectionMatrix(), FfxFloat32x4(ndc, depth, 1.0f));

        clip_space.z /= clip_space.w; // perspective divide

        // How aligned with the view vector? (the more Z aligned, the higher the depth errors)
        const FfxFloat32x4 homogeneous = FFX_MATRIX_MULTIPLY(ViewProjectionInverse(), FfxFloat32x4(ndc, depth, 1.0f));
        const FfxFloat32x3 world_position = FfxFloat32x3(homogeneous.xyz / homogeneous.w);  // perspective divide
        const FfxFloat32x3 view_direction = normalize(Eye().xyz - world_position);
        FfxFloat32 z_alignment = 1.0f - dot(view_direction, normal);
        z_alignment = pow(z_alignment, 8);

        // Calculate the depth difference
        FfxFloat32 linear_depth = FFX_DNSR_Shadows_GetLinearDepth(did, clip_space.z);   // get linear depth

        FfxInt32x2 idx = FfxInt32x2(previous_uv * FfxFloat32x2(dims));
        const FfxFloat32 previous_depth = FFX_DNSR_Shadows_GetLinearDepth(idx, LoadPreviousDepth(idx));
        const FfxFloat32 depth_difference = abs(previous_depth - linear_depth) / linear_depth;

        // Resolve into the disocclusion mask
        const FfxFloat32 depth_tolerance = ffxLerp(1e-2f, 1e-1f, z_alignment);
        is_disoccluded = depth_difference >= depth_tolerance;
    }

    return is_disoccluded;
}

FfxFloat32x2 FFX_DNSR_Shadows_GetClosestVelocity(FfxInt32x2 did, FfxFloat32 depth)
{
    FfxFloat32x2 closest_velocity = LoadVelocity(did);
    FfxFloat32 closest_depth = depth;

    FfxFloat32 new_depth = ffxQuadReadX(closest_depth);
    FfxFloat32x2 new_velocity = ffxQuadReadX(closest_velocity);

#if FFX_DENOISER_OPTION_INVERTED_DEPTH
    if (new_depth > closest_depth)
#else
    if (new_depth < closest_depth)
#endif
    {
        closest_depth = new_depth;
        closest_velocity = new_velocity;
    }

    new_depth = ffxQuadReadY(closest_depth);
    new_velocity = ffxQuadReadY(closest_velocity);

#if FFX_DENOISER_OPTION_INVERTED_DEPTH
    if (new_depth > closest_depth)
#else
    if (new_depth < closest_depth)
#endif
    {
        closest_depth = new_depth;
        closest_velocity = new_velocity;
    }

    return closest_velocity;
}

#define KERNEL_RADIUS 8
FfxFloat32 FFX_DNSR_Shadows_KernelWeight(FfxFloat32 i)
{
#define KERNEL_WEIGHT(i) (exp(-3.0 * FfxFloat32(i * i) / ((KERNEL_RADIUS + 1.0) * (KERNEL_RADIUS + 1.0))))

    // Statically initialize kernel_weights_sum
    FfxFloat32 kernel_weights_sum = 0;
    kernel_weights_sum += KERNEL_WEIGHT(0);
    for (FfxInt32 c = 1; c <= KERNEL_RADIUS; ++c)
    {
        kernel_weights_sum += 2 * KERNEL_WEIGHT(c); // Add other half of the kernel to the sum
    }
    FfxFloat32 inv_kernel_weights_sum = ffxReciprocal(kernel_weights_sum);

    // The only runtime code in this function
    return KERNEL_WEIGHT(i) * inv_kernel_weights_sum;
}

void FFX_DNSR_Shadows_AccumulateMoments(FfxFloat32 value, FfxFloat32 weight, inout FfxFloat32 moments)
{
    // We get value from the horizontal neighborhood calculations. Thus, it's both mean and variance due to using one sample per pixel
    moments += value * weight;
}

// The horizontal part of a 17x17 local neighborhood kernel
FfxFloat32 FFX_DNSR_Shadows_HorizontalNeighborhood(FfxInt32x2 did)
{
   const FfxInt32x2 base_did = did;

    // Prevent vertical out of bounds access
    if ((base_did.y < 0) || (base_did.y >= BufferDimensions().y)) return 0;

    const FfxUInt32x2 tile_index = FFX_DNSR_Shadows_GetTileIndexFromPixelPosition(base_did);
    const FfxUInt32 linear_tile_index = FFX_DNSR_Shadows_LinearTileIndex(tile_index, BufferDimensions().x);

    const FfxUInt32 left_tile_index = linear_tile_index - 1;
    const FfxUInt32 center_tile_index = linear_tile_index;
    const FfxUInt32 right_tile_index = linear_tile_index + 1;

    FfxBoolean is_first_tile_in_row = tile_index.x == 0;
    FfxBoolean is_last_tile_in_row = tile_index.x == (FFX_DNSR_Shadows_RoundedDivide(BufferDimensions().x, 8) - 1);

    FfxUInt32 left_tile = 0;
    if (!is_first_tile_in_row) left_tile = LoadRaytracedShadowMask(left_tile_index);
    FfxUInt32 center_tile = LoadRaytracedShadowMask(center_tile_index);
    FfxUInt32 right_tile = 0;
    if (!is_last_tile_in_row) right_tile = LoadRaytracedShadowMask(right_tile_index);

    // Construct a single FfxUInt32 with the lowest 17bits containing the horizontal part of the local neighborhood.

    // First extract the 8 bits of our row in each of the neighboring tiles
    const FfxUInt32 row_base_index = (did.y % 4) * 8;
    const FfxUInt32 left = (left_tile >> row_base_index) & 0xFF;
    const FfxUInt32 center = (center_tile >> row_base_index) & 0xFF;
    const FfxUInt32 right = (right_tile >> row_base_index) & 0xFF;

    // Combine them into a single mask containting [left, center, right] from least significant to most significant bit
    FfxUInt32 neighborhood = left | (center << 8) | (right << 16);

    // Make sure our pixel is at bit position 9 to get the highest contribution from the filter kernel
    const FfxUInt32 bit_index_in_row = (did.x % 8);
    neighborhood = neighborhood >> bit_index_in_row; // Shift out bits to the right, so the center bit ends up at bit 9.

    FfxFloat32 moment = 0.0; // For one sample per pixel this is both, mean and variance

    // First 8 bits up to the center pixel
    FfxUInt32 mask;
    FfxInt32 i;
    for (i = 0; i < 8; ++i)
    {
        mask = 1u << i;
        moment += FfxBoolean(mask & neighborhood) ? FFX_DNSR_Shadows_KernelWeight(8 - i) : 0;
    }

    // Center pixel
    mask = 1u << 8;
    moment += FfxBoolean(mask & neighborhood) ? FFX_DNSR_Shadows_KernelWeight(0) : 0;

    // Last 8 bits
    for (i = 1; i <= 8; ++i)
    {
        mask = 1u << (8 + i);
        moment += FfxBoolean(mask & neighborhood) ? FFX_DNSR_Shadows_KernelWeight(i) : 0;
    }

    return moment;
}

FFX_GROUPSHARED FfxFloat32 g_FFX_DNSR_Shadows_neighborhood[8][24];

FfxFloat32 FFX_DNSR_Shadows_ComputeLocalNeighborhood(FfxInt32x2 did, FfxInt32x2 gtid)
{
    FfxFloat32 local_neighborhood = 0;

    FfxFloat32 upper = FFX_DNSR_Shadows_HorizontalNeighborhood(FfxInt32x2(did.x, did.y - 8));
    FfxFloat32 center = FFX_DNSR_Shadows_HorizontalNeighborhood(FfxInt32x2(did.x, did.y));
    FfxFloat32 lower = FFX_DNSR_Shadows_HorizontalNeighborhood(FfxInt32x2(did.x, did.y + 8));

    g_FFX_DNSR_Shadows_neighborhood[gtid.x][gtid.y] = upper;
    g_FFX_DNSR_Shadows_neighborhood[gtid.x][gtid.y + 8] = center;
    g_FFX_DNSR_Shadows_neighborhood[gtid.x][gtid.y + 16] = lower;

    FFX_GROUP_MEMORY_BARRIER;

    // First combine the own values.
    // KERNEL_RADIUS pixels up is own upper and KERNEL_RADIUS pixels down is own lower value
    FFX_DNSR_Shadows_AccumulateMoments(center, FFX_DNSR_Shadows_KernelWeight(0), local_neighborhood);
    FFX_DNSR_Shadows_AccumulateMoments(upper, FFX_DNSR_Shadows_KernelWeight(KERNEL_RADIUS), local_neighborhood);
    FFX_DNSR_Shadows_AccumulateMoments(lower, FFX_DNSR_Shadows_KernelWeight(KERNEL_RADIUS), local_neighborhood);

    // Then read the neighboring values.
    for (FfxInt32 i = 1; i < KERNEL_RADIUS; ++i)
    {
        FfxFloat32 upper_value = g_FFX_DNSR_Shadows_neighborhood[gtid.x][8 + gtid.y - i];
        FfxFloat32 lower_value = g_FFX_DNSR_Shadows_neighborhood[gtid.x][8 + gtid.y + i];
        FfxFloat32 weight = FFX_DNSR_Shadows_KernelWeight(i);
        FFX_DNSR_Shadows_AccumulateMoments(upper_value, weight, local_neighborhood);
        FFX_DNSR_Shadows_AccumulateMoments(lower_value, weight, local_neighborhood);
    }

    return local_neighborhood;
}

void FFX_DNSR_Shadows_WriteTileMetaData(FfxUInt32x2 gid, FfxUInt32x2 gtid, FfxBoolean is_cleared, FfxBoolean all_in_light)
{
    if (all(FFX_EQUAL(gtid, FfxUInt32x2(0,0))))
    {
        FfxUInt32 light_mask = all_in_light ? TILE_META_DATA_LIGHT_MASK : 0;
        FfxUInt32 clear_mask = is_cleared ? TILE_META_DATA_CLEAR_MASK : 0;
        FfxUInt32 mask = FfxUInt32(light_mask | clear_mask);
        StoreMetadata(gid.y * FFX_DNSR_Shadows_RoundedDivide(BufferDimensions().x, 8) + gid.x, mask);
    }
}

void FFX_DNSR_Shadows_ClearTargets(FfxUInt32x2 did, FfxUInt32x2 gtid, FfxUInt32x2 gid, FfxFloat32 shadow_value, FfxBoolean is_shadow_receiver, FfxBoolean all_in_light)
{
    FFX_DNSR_Shadows_WriteTileMetaData(gid, gtid, FFX_TRUE, all_in_light);
    StoreReprojectionResults(did, FfxFloat32x2(shadow_value, 0)); // mean, variance

    FfxFloat32 temporal_sample_count = is_shadow_receiver ? 1 : 0;
    StoreMoments(did, FfxFloat32x3(shadow_value, 0, temporal_sample_count));// mean, variance, temporal sample count
}

void FFX_DNSR_Shadows_TileClassification(FfxUInt32 group_index, FfxUInt32x2 gid)
{
    FfxUInt32x2 gtid = ffxRemapForWaveReduction(group_index);  // Make sure we can use the QuadReadAcross intrinsics to access a 2x2 region.
    FfxUInt32x2 did = gid * 8 + gtid;

    FfxBoolean is_shadow_receiver = IsShadowReciever(did);

    FfxBoolean skip_sky = FFX_DNSR_Shadows_ThreadGroupAllTrue(!is_shadow_receiver);
    if (skip_sky)
    {
        // We have to set all resources of the tile we skipped to sensible values as neighboring active denoiser tiles might want to read them.
        FFX_DNSR_Shadows_ClearTargets(did, gtid, gid, 0, is_shadow_receiver, FFX_FALSE);
        return;
    }

    FfxBoolean all_in_light = FFX_FALSE;
    FfxBoolean all_in_shadow = FFX_FALSE;
    FFX_DNSR_Shadows_SearchSpatialRegion(gid, all_in_light, all_in_shadow);
    FfxFloat32 shadow_value = all_in_light ? 1 : 0; // Either all_in_light or all_in_shadow must be true, otherwise we would not skip the tile.

    FfxBoolean can_skip = all_in_light || all_in_shadow;
    // We have to append the entire tile if there is a single lane that we can't skip
    FfxBoolean skip_tile = FFX_DNSR_Shadows_ThreadGroupAllTrue(can_skip);
    if (skip_tile)
    {
        // We have to set all resources of the tile we skipped to sensible values as neighboring active denoiser tiles might want to read them.
        FFX_DNSR_Shadows_ClearTargets(did, gtid, gid, shadow_value, is_shadow_receiver, all_in_light);
        return;
    }

    FFX_DNSR_Shadows_WriteTileMetaData(gid, gtid, FFX_FALSE, FFX_FALSE);

    FfxFloat32 depth = LoadDepth(FfxInt32x2(did));
    const FfxFloat32x2 velocity = FFX_DNSR_Shadows_GetClosestVelocity(FfxInt32x2(did), depth); // Must happen before we deactivate lanes
    const FfxFloat32 local_neighborhood = FFX_DNSR_Shadows_ComputeLocalNeighborhood(FfxInt32x2(did), FfxInt32x2(gtid));

    const FfxFloat32x2 texel_size = InvBufferDimensions();
    const FfxFloat32x2 uv = (did.xy + 0.5f) * texel_size;
    const FfxFloat32x2 history_uv = uv + velocity;
    const FfxInt32x2 history_pos = FfxInt32x2(history_uv * BufferDimensions());

    const FfxUInt32x2 tile_index = FFX_DNSR_Shadows_GetTileIndexFromPixelPosition(FfxInt32x2(did));
    const FfxUInt32 linear_tile_index = FFX_DNSR_Shadows_LinearTileIndex(tile_index, BufferDimensions().x);

    const FfxUInt32 shadow_tile = LoadRaytracedShadowMask(linear_tile_index);

    FfxFloat32x3 moments_current = FfxFloat32x3(0,0,0);
    FfxFloat32 variance = 0;
    FfxFloat32 shadow_clamped = 0;
    if (is_shadow_receiver) // do not process sky pixels
    {
        FfxBoolean hit_light = FfxBoolean(shadow_tile & FFX_DNSR_Shadows_GetBitMaskFromPixelPosition(did));
        const FfxFloat32 shadow_current = hit_light ? 1.0 : 0.0;

        // Perform moments and variance calculations
        {
            FfxBoolean is_disoccluded = FFX_DNSR_Shadows_IsDisoccluded(did, depth, velocity);
            const FfxFloat32x3 previous_moments = is_disoccluded ? FfxFloat32x3(0.0f, 0.0f, 0.0f) // Can't trust previous moments on disocclusion
                : LoadPreviousMomentsBuffer(history_pos);

            const FfxFloat32 old_m = previous_moments.x;
            const FfxFloat32 old_s = previous_moments.y;
            const FfxFloat32 sample_count = previous_moments.z + 1.0f;
            const FfxFloat32 new_m = old_m + (shadow_current - old_m) / sample_count;
            const FfxFloat32 new_s = old_s + (shadow_current - old_m) * (shadow_current - new_m);

            variance = (sample_count > 1.0f ? new_s / (sample_count - 1.0f) : 1.0f);
            moments_current = FfxFloat32x3(new_m, new_s, sample_count);
        }

        // Retrieve local neighborhood and reproject
        {
            FfxFloat32 mean = local_neighborhood;
            FfxFloat32 spatial_variance = local_neighborhood;

            spatial_variance = max(spatial_variance - mean * mean, 0.0f);

            // Compute the clamping bounding box
            const FfxFloat32 std_deviation = sqrt(spatial_variance);
            const FfxFloat32 nmin = mean - 0.5f * std_deviation;
            const FfxFloat32 nmax = mean + 0.5f * std_deviation;

            // Clamp reprojected sample to local neighborhood
            FfxFloat32 shadow_previous = shadow_current;
            if (IsFirstFrame() == 0)
            {
                shadow_previous = LoadHistory(history_uv);
            }

            shadow_clamped = clamp(shadow_previous, nmin, nmax);

            // Reduce history weighting
            const FfxFloat32 sigma = 20.0f;
            const FfxFloat32 temporal_discontinuity = (shadow_previous - mean) / max(0.5f * std_deviation, 0.001f);
            const FfxFloat32 sample_counter_damper = exp(-temporal_discontinuity * temporal_discontinuity / sigma);
            moments_current.z *= sample_counter_damper;

            // Boost variance on first frames
            if (moments_current.z < 16.0f)
            {
                const FfxFloat32 variance_boost = max(16.0f - moments_current.z, 1.0f);
                variance = max(variance, spatial_variance);
                variance *= variance_boost;
            }
        }

        // Perform the temporal blend
        const FfxFloat32 history_weight = sqrt(max(8.0f - moments_current.z, 0.0f) / 8.0f);
        shadow_clamped = ffxLerp(shadow_clamped, shadow_current, ffxLerp(0.05f, 1.0f, history_weight));
    }

    // Output the results of the temporal pass
    StoreReprojectionResults(did.xy, FfxFloat32x2(shadow_clamped, variance));
    StoreMoments(did.xy, moments_current);
}

#endif
