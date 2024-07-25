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

#define FFX_SSSR_FLOAT_MAX                 3.402823466e+38
#define FFX_SSSR_DEPTH_HIERARCHY_MAX_MIP   6

float3 FFX_SSSR_ScreenSpaceToViewSpace(float3 screen_space_position)
{
    return InvProjectPosition(screen_space_position, g_inv_proj);
}

void FFX_SSSR_InitialAdvanceRay(float3     origin,
                                float3     direction,
                                float3     inv_direction,
                                float2     current_mip_resolution,
                                float2     current_mip_resolution_inv,
                                float2     floor_offset,
                                float2     uv_offset,
                                out float3 position,
                                out float  current_t)
{
    float2 current_mip_position = current_mip_resolution * origin.xy;

    // Intersect ray with the half box that is pointing away from the ray origin.
    float2 xy_plane = floor(current_mip_position) + floor_offset;
    xy_plane        = xy_plane * current_mip_resolution_inv + uv_offset;

    // o + d * t = p' => t = (p' - o) / d
    float2 t  = xy_plane * inv_direction.xy - origin.xy * inv_direction.xy;
    current_t = min(t.x, t.y);
    position  = origin + current_t * direction;
}

bool FFX_SSSR_AdvanceRay(float3       origin,
                         float3       direction,
                         float3       inv_direction,
                         float2       current_mip_position,
                         float2       current_mip_resolution_inv,
                         float2       floor_offset,
                         float2       uv_offset,
                         float        surface_z,
                         inout float3 position,
                         inout float  current_t)
{
    // Create boundary planes
    float2 xy_plane        = floor(current_mip_position) + floor_offset;
    xy_plane               = xy_plane * current_mip_resolution_inv + uv_offset;
    float3 boundary_planes = float3(xy_plane, surface_z);

    // Intersect ray with the half box that is pointing away from the ray origin.
    // o + d * t = p' => t = (p' - o) / d
    float3 t = boundary_planes * inv_direction - origin * inv_direction;

    // Prevent using z plane when shooting out of the depth buffer.
#if FFX_SSSR_OPTION_INVERTED_DEPTH
    t.z = direction.z < 0 ? t.z : FFX_SSSR_FLOAT_MAX;
#else
    t.z = direction.z > 0 ? t.z : FFX_SSSR_FLOAT_MAX;
#endif

    // Choose nearest intersection with a boundary.
    float t_min = min(min(t.x, t.y), t.z);

#if FFX_SSSR_OPTION_INVERTED_DEPTH
    // Larger z means closer to the camera.
    bool above_surface = surface_z < position.z;
#else
    // Smaller z means closer to the camera.
    bool above_surface = surface_z > position.z;
#endif

    // Decide whether we are able to advance the ray until we hit the xy boundaries or if we had to clamp it at the surface.
    // We use the asuint comparison to avoid NaN / Inf logic, also we actually care about bitwise equality here to see if t_min is the t.z we fed into the min3 above.
    bool skipped_tile = asuint(t_min) != asuint(t.z) && above_surface;

    // Make sure to only advance the ray if we're still above the surface.
    current_t = above_surface ? t_min : current_t;

    // Advance ray
    position = origin + current_t * direction;

    return skipped_tile;
}

float2 FFX_SSSR_GetMipResolution(float2 screen_dimensions, int mip_level)
{
    return screen_dimensions * pow(0.5, mip_level);
}

// Requires origin and direction of the ray to be in screen space [0, 1] x [0, 1]
float3 FFX_SSSR_HierarchicalRaymarch(float3 origin, float3 direction, bool is_mirror, float2 screen_size, int most_detailed_mip, uint min_traversal_occupancy,
                                     uint max_traversal_intersections, out bool valid_hit, out uint _num_iters) {
    const float3 inv_direction = abs(direction) > float(1.0e-12) ? float(1.0) / direction : FFX_SSSR_FLOAT_MAX;

    // Start on mip with highest detail.
    int current_mip = most_detailed_mip;

    // Could recompute these every iteration, but it's faster to hoist them out and update them.
    float2 current_mip_resolution     = FFX_SSSR_GetMipResolution(screen_size, current_mip);
    float2 current_mip_resolution_inv = rcp(current_mip_resolution);

    // Offset to the bounding boxes uv space to intersect the ray with the center of the next pixel.
    // This means we ever so slightly over shoot into the next region.
    float2 uv_offset = float(0.001) * exp2(most_detailed_mip) / screen_size;
    uv_offset        = direction.xy < 0 ? -uv_offset : uv_offset;

    // Offset applied depending on current mip resolution to move the boundary to the left/right upper/lower border depending on ray direction.
    float2 floor_offset = direction.xy < 0 ? 0 : 1;

    // valid_hit = false;
    // if (direction.z < f32(1.0e-6)) return f32x3(0.0, 0.0, 0.0);

    // Initially advance ray to avoid immediate self intersections.
    float  current_t;
    float3 position;
    FFX_SSSR_InitialAdvanceRay(origin, direction, inv_direction, current_mip_resolution, current_mip_resolution_inv, floor_offset, uv_offset, position, current_t);

    bool exit_due_to_low_occupancy = false;
    _num_iters                     = uint(0);
    while (_num_iters < max_traversal_intersections && current_mip >= most_detailed_mip && !exit_due_to_low_occupancy) {
        if (any(position.xy > float2(1.0, 1.0)) || any(position.xy < float2(0.0, 0.0))) break;
#ifdef FFX_SSSR_INVERTED_DEPTH_RANGE
        if (position.z < f32(1.0e-6)) break;
#else
        if (position.z > float(1.0) - float(1.0e-6)) break;
#endif

        float2 current_mip_position = current_mip_resolution * position.xy;
        float  surface_z            = FFX_SSSR_LoadDepth(current_mip_position, current_mip);
        exit_due_to_low_occupancy   = !is_mirror && WaveActiveCountBits(true) <= min_traversal_occupancy;
        bool skipped_tile =
            FFX_SSSR_AdvanceRay(origin, direction, inv_direction, current_mip_position, current_mip_resolution_inv, floor_offset, uv_offset, surface_z, position, current_t);
        current_mip += skipped_tile ? 1 : -1;
        current_mip_resolution *= skipped_tile ? 0.5 : 2;
        current_mip_resolution_inv *= skipped_tile ? 2 : 0.5;
        ++_num_iters;
    }

    valid_hit = (_num_iters <= max_traversal_intersections);

    return position;
}

float FFX_SSSR_ValidateHit(float3 hit, float2 uv, float3 world_space_ray_direction, float2 screen_size, float depth_buffer_thickness)
{
    // Reject hits outside the view frustum
    if ((hit.x < 0.0f) || (hit.y < 0.0f) || (hit.x > 1.0f) || (hit.y > 1.0f))
    {
        return 0.0f;
    }

    // Reject the hit if we didnt advance the ray significantly to avoid immediate self reflection
    float2 manhattan_dist = abs(hit.xy - uv);
    if ((manhattan_dist.x < (2.0f / screen_size.x)) && (manhattan_dist.y < (2.0f / screen_size.y)))
    {
        return 0.0;
    }

    // Don't lookup radiance from the background.
    int2  texel_coords = int2(screen_size * hit.xy);
    float surface_z    = FFX_SSSR_LoadDepth(texel_coords / 2, 1);
#if FFX_SSSR_OPTION_INVERTED_DEPTH
    if (surface_z == 0.0)
    {
#else
    if (surface_z == 1.0)
    {
#endif
        return 0;
    }

    // We check if we hit the surface from the back, these should be rejected.
    float3 hit_normal = FFX_SSSR_LoadWorldSpaceNormal(texel_coords);
    if (dot(hit_normal, world_space_ray_direction) > 0)
    {
        return 0;
    }

    float3 view_space_surface = FFX_SSSR_ScreenSpaceToViewSpace(float3(hit.xy, surface_z));
    float3 view_space_hit     = FFX_SSSR_ScreenSpaceToViewSpace(hit);
    float  distance           = length(view_space_surface - view_space_hit);

    // Fade out hits near the screen borders
    float2 fov      = 0.05 * float2(screen_size.y / screen_size.x, 1);
    float2 border   = smoothstep(float2(0.0f, 0.0f), fov, hit.xy) * (1 - smoothstep(float2(1.0f, 1.0f) - fov, float2(1.0f, 1.0f), hit.xy));
    float  vignette = border.x * border.y;

    // We accept all hits that are within a reasonable minimum distance below the surface.
    // Add constant in linear space to avoid growing of the reflections toward the reflected objects.
    float confidence = 1.0f - smoothstep(0.0f, depth_buffer_thickness, distance);
    confidence *= confidence;

    return vignette * confidence;
}
