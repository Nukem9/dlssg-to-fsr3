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

#define M_PI                               3.14159265358979f
#define FFX_SSSR_FLOAT_MAX                 3.402823466e+38
#define FFX_SSSR_DEPTH_HIERARCHY_MAX_MIP   6

FfxFloat32x3 FFX_SSSR_ScreenSpaceToViewSpace(FfxFloat32x3 screen_space_position) {
    return InvProjectPosition(screen_space_position, InvProjection());
}

FfxFloat32x3 ScreenSpaceToWorldSpace(FfxFloat32x3 screen_space_position) {
    return InvProjectPosition(screen_space_position, InvViewProjection());
}

// http://jcgt.org/published/0007/04/01/paper.pdf by Eric Heitz
// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
FfxFloat32x3 SampleGGXVNDF(FfxFloat32x3 Ve, FfxFloat32 alpha_x, FfxFloat32 alpha_y, FfxFloat32 U1, FfxFloat32 U2) {
    // Section 3.2: transforming the view direction to the hemisphere configuration
    FfxFloat32x3 Vh = normalize(FfxFloat32x3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    FfxFloat32 lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    FfxFloat32x3 T1 = lensq > 0 ? FfxFloat32x3(-Vh.y, Vh.x, 0) * ffxRsqrt(lensq) : FfxFloat32x3(1, 0, 0);
    FfxFloat32x3 T2 = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    FfxFloat32 r = sqrt(U1);
    FfxFloat32 phi = 2.0 * M_PI * U2;
    FfxFloat32 t1 = r * cos(phi);
    FfxFloat32 t2 = r * sin(phi);
    FfxFloat32 s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    FfxFloat32x3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    FfxFloat32x3 Ne = normalize(FfxFloat32x3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
    return Ne;
}

FfxFloat32x3 Sample_GGX_VNDF_Ellipsoid(FfxFloat32x3 Ve, FfxFloat32 alpha_x, FfxFloat32 alpha_y, FfxFloat32 U1, FfxFloat32 U2) {
    return SampleGGXVNDF(Ve, alpha_x, alpha_y, U1, U2);
}

FfxFloat32x3 Sample_GGX_VNDF_Hemisphere(FfxFloat32x3 Ve, FfxFloat32 alpha, FfxFloat32 U1, FfxFloat32 U2) {
    return Sample_GGX_VNDF_Ellipsoid(Ve, alpha, alpha, U1, U2);
}

FfxFloat32x3 SampleReflectionVector(FfxFloat32x3 view_direction, FfxFloat32x3 normal, FfxFloat32 roughness, FfxInt32x2 dispatch_thread_id) {
    FfxFloat32x3 U;
    FfxFloat32x3 N = normal;
    if (abs(N.z) > 0.0) {
        FfxFloat32 k = sqrt(N.y * N.y + N.z * N.z);
        U.x = 0.0; U.y = -N.z / k; U.z = N.y / k;
    }
    else {
        FfxFloat32 k = sqrt(N.x * N.x + N.y * N.y);
        U.x = N.y / k; U.y = -N.x / k; U.z = 0.0;
    }

    // TBN 3x3 matrix
    FfxFloat32x3 TBN_row0 = U;
    FfxFloat32x3 TBN_row1 = cross(N, U);
    FfxFloat32x3 TBN_row2 = N;

    // TBN * -view_direction
    FfxFloat32x3 view_direction_tbn = FfxFloat32x3(dot(TBN_row0, -view_direction), dot(TBN_row1, -view_direction), dot(TBN_row2, -view_direction));

    FfxFloat32x2 u = FFX_SSSR_SampleRandomVector2D(dispatch_thread_id);

    FfxFloat32x3 sampled_normal_tbn = Sample_GGX_VNDF_Hemisphere(view_direction_tbn, roughness, u.x, u.y);
    #ifdef PERFECT_REFLECTIONS
        sampled_normal_tbn = FfxFloat32x3(0, 0, 1); // Overwrite normal sample to produce perfect reflection.
    #endif

    FfxFloat32x3 reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);

    // Transpose of TBN
    FfxFloat32x3 TBN_col0 = FfxFloat32x3(TBN_row0[0], TBN_row1[0], TBN_row2[0]);
    FfxFloat32x3 TBN_col1 = FfxFloat32x3(TBN_row0[1], TBN_row1[1], TBN_row2[1]);
    FfxFloat32x3 TBN_col2 = FfxFloat32x3(TBN_row0[2], TBN_row1[2], TBN_row2[2]);

    // transpose(TBN) * reflected_direction_tbn
    return FfxFloat32x3(dot(TBN_col0, reflected_direction_tbn), dot(TBN_col1, reflected_direction_tbn), dot(TBN_col2, reflected_direction_tbn));
}

void FFX_SSSR_InitialAdvanceRay(FfxFloat32x3 origin, FfxFloat32x3 direction, FfxFloat32x3 inv_direction, FfxFloat32x2 current_mip_resolution, FfxFloat32x2 current_mip_resolution_inv, FfxFloat32x2 floor_offset, FfxFloat32x2 uv_offset, FFX_PARAMETER_OUT FfxFloat32x3 position, FFX_PARAMETER_OUT FfxFloat32 current_t) {
    FfxFloat32x2 current_mip_position = current_mip_resolution * origin.xy;

    // Intersect ray with the half box that is pointing away from the ray origin.
    FfxFloat32x2 xy_plane = floor(current_mip_position) + floor_offset;
    xy_plane = xy_plane * current_mip_resolution_inv + uv_offset;

    // o + d * t = p' => t = (p' - o) / d
    FfxFloat32x2 t = xy_plane * inv_direction.xy - origin.xy * inv_direction.xy;
    current_t = min(t.x, t.y);
    position = origin + current_t * direction;
}

FfxBoolean FFX_SSSR_AdvanceRay(FfxFloat32x3 origin, FfxFloat32x3 direction, FfxFloat32x3 inv_direction, FfxFloat32x2 current_mip_position, FfxFloat32x2 current_mip_resolution_inv, FfxFloat32x2 floor_offset, FfxFloat32x2 uv_offset, FfxFloat32 surface_z, FFX_PARAMETER_INOUT FfxFloat32x3 position, FFX_PARAMETER_INOUT FfxFloat32 current_t) {
    // Create boundary planes
    FfxFloat32x2 xy_plane = floor(current_mip_position) + floor_offset;
    xy_plane = xy_plane * current_mip_resolution_inv + uv_offset;
    FfxFloat32x3 boundary_planes = FfxFloat32x3(xy_plane, surface_z);

    // Intersect ray with the half box that is pointing away from the ray origin.
    // o + d * t = p' => t = (p' - o) / d
    FfxFloat32x3 t = boundary_planes * inv_direction - origin * inv_direction;

    // Prevent using z plane when shooting out of the depth buffer.
#if FFX_SSSR_OPTION_INVERTED_DEPTH
    t.z = direction.z < 0 ? t.z : FFX_SSSR_FLOAT_MAX;
#else
    t.z = direction.z > 0 ? t.z : FFX_SSSR_FLOAT_MAX;
#endif

    // Choose nearest intersection with a boundary.
    FfxFloat32 t_min = min(min(t.x, t.y), t.z);

#if FFX_SSSR_OPTION_INVERTED_DEPTH
    // Larger z means closer to the camera.
    FfxBoolean above_surface = surface_z < position.z;
#else
    // Smaller z means closer to the camera.
    FfxBoolean above_surface = surface_z > position.z;
#endif

    // Decide whether we are able to advance the ray until we hit the xy boundaries or if we had to clamp it at the surface.
    // We use the asuint comparison to avoid NaN / Inf logic, also we actually care about bitwise equality here to see if t_min is the t.z we fed into the min3 above.
    FfxBoolean skipped_tile = ffxAsUInt32(t_min) != ffxAsUInt32(t.z) && above_surface;

    // Make sure to only advance the ray if we're still above the surface.
    current_t = above_surface ? t_min : current_t;

    // Advance ray
    position = origin + current_t * direction;

    return skipped_tile;
}

FfxFloat32x2 FFX_SSSR_GetMipResolution(FfxFloat32x2 screen_dimensions, FfxInt32 mip_level) {
    return screen_dimensions * pow(0.5, mip_level);
}

// Requires origin and direction of the ray to be in screen space [0, 1] x [0, 1]
FfxFloat32x3 FFX_SSSR_HierarchicalRaymarch(FfxFloat32x3 origin, FfxFloat32x3 direction, FfxBoolean is_mirror, FfxFloat32x2 screen_size, FfxInt32 most_detailed_mip, FfxUInt32 min_traversal_occupancy, FfxUInt32 max_traversal_intersections, FFX_PARAMETER_OUT FfxBoolean valid_hit) 
{
    const FfxFloat32x3 inv_direction = FFX_SELECT(direction != FfxFloat32x3(0.0f, 0.0f, 0.0f), FfxFloat32x3(1.0f, 1.0f, 1.0f) / direction, FfxFloat32x3(FFX_SSSR_FLOAT_MAX, FFX_SSSR_FLOAT_MAX, FFX_SSSR_FLOAT_MAX));

    // Start on mip with highest detail.
    FfxInt32 current_mip = most_detailed_mip;

    // Could recompute these every iteration, but it's faster to hoist them out and update them.
    FfxFloat32x2 current_mip_resolution = FFX_SSSR_GetMipResolution(screen_size, current_mip);
    FfxFloat32x2 current_mip_resolution_inv = ffxReciprocal(current_mip_resolution);

    // Offset to the bounding boxes uv space to intersect the ray with the center of the next pixel.
    // This means we ever so slightly over shoot into the next region.
    FfxFloat32x2 uv_offset = 0.005 * exp2(most_detailed_mip) / screen_size;
    uv_offset.x = direction.x < 0.0f ? -uv_offset.x : uv_offset.x;
    uv_offset.y = direction.y < 0.0f ? -uv_offset.y : uv_offset.y;

    // Offset applied depending on current mip resolution to move the boundary to the left/right upper/lower border depending on ray direction.
    FfxFloat32x2 floor_offset;
    floor_offset.x = direction.x < 0.0f ? 0.0f : 1.0f;
    floor_offset.y = direction.y < 0.0f ? 0.0f : 1.0f;

    // Initially advance ray to avoid immediate self intersections.
    FfxFloat32 current_t;
    FfxFloat32x3 position;
    FFX_SSSR_InitialAdvanceRay(origin, direction, inv_direction, current_mip_resolution, current_mip_resolution_inv, floor_offset, uv_offset, position, current_t);

    FfxBoolean exit_due_to_low_occupancy = false;
    FfxInt32 i = 0;
    while (i < max_traversal_intersections && current_mip >= most_detailed_mip && !exit_due_to_low_occupancy) {
        FfxFloat32x2 current_mip_position = current_mip_resolution * position.xy;
        FfxFloat32 surface_z = FFX_SSSR_LoadDepth(FfxInt32x2(current_mip_position), current_mip);
        exit_due_to_low_occupancy = !is_mirror && ffxWaveActiveCountBits(true) <= min_traversal_occupancy;
        FfxBoolean skipped_tile = FFX_SSSR_AdvanceRay(origin, direction, inv_direction, current_mip_position, current_mip_resolution_inv, floor_offset, uv_offset, surface_z, position, current_t);
        
        // Don't increase mip further than this because we did not generate it
        FfxBoolean nextMipIsOutOfRange = skipped_tile && (current_mip >= FFX_SSSR_DEPTH_HIERARCHY_MAX_MIP);
        if (!nextMipIsOutOfRange)
        {
            current_mip += skipped_tile ? 1 : -1;
            current_mip_resolution *= skipped_tile ? 0.5 : 2;
            current_mip_resolution_inv *= skipped_tile ? 2 : 0.5;;
        }

        ++i;
    }

    valid_hit = (i <= max_traversal_intersections);

    return position;
}

FfxFloat32 FFX_SSSR_ValidateHit(FfxFloat32x3 hit, FfxFloat32x2 uv, FfxFloat32x3 world_space_ray_direction, FfxFloat32x2 screen_size, FfxFloat32 depth_buffer_thickness) {
    // Reject hits outside the view frustum
    if ((hit.x < 0.0f) || (hit.y < 0.0f) || (hit.x > 1.0f) || (hit.y > 1.0f)) {
        return 0.0f;
    }

    // Reject the hit if we didnt advance the ray significantly to avoid immediate self reflection
    FfxFloat32x2 manhattan_dist = abs(hit.xy - uv);
    if((manhattan_dist.x < (2.0f / screen_size.x)) && (manhattan_dist.y < (2.0f / screen_size.y)) ) {
        return 0.0;
    }

    // Don't lookup radiance from the background.
    FfxInt32x2 texel_coords = FfxInt32x2(screen_size * hit.xy);
    FfxFloat32 surface_z = FFX_SSSR_LoadDepth(texel_coords / 2, 1);
#if FFX_SSSR_OPTION_INVERTED_DEPTH
    if (surface_z == 0.0) {
#else
    if (surface_z == 1.0) {
#endif
        return 0;
    }

    // We check if we hit the surface from the back, these should be rejected.
    FfxFloat32x3 hit_normal = FFX_SSSR_LoadWorldSpaceNormal(texel_coords);
    if (dot(hit_normal, world_space_ray_direction) > 0) {
        return 0;
    }

    FfxFloat32x3 view_space_surface = FFX_SSSR_ScreenSpaceToViewSpace(FfxFloat32x3(hit.xy, surface_z));
    FfxFloat32x3 view_space_hit = FFX_SSSR_ScreenSpaceToViewSpace(hit);
    FfxFloat32 distance = length(view_space_surface - view_space_hit);

    // Fade out hits near the screen borders
    FfxFloat32x2 fov = 0.05 * FfxFloat32x2(screen_size.y / screen_size.x, 1);
    FfxFloat32x2 border = smoothstep(FfxFloat32x2(0.0f, 0.0f), fov, hit.xy) * (1 - smoothstep(FfxFloat32x2(1.0f, 1.0f) - fov, FfxFloat32x2(1.0f, 1.0f), hit.xy));
    FfxFloat32 vignette = border.x * border.y;

    // We accept all hits that are within a reasonable minimum distance below the surface.
    // Add constant in linear space to avoid growing of the reflections toward the reflected objects.
    FfxFloat32 confidence = 1.0f - smoothstep(0.0f, depth_buffer_thickness, distance);
    confidence *= confidence;

    return vignette * confidence;
}

void Intersect(FfxUInt32 group_index, FfxUInt32 group_id)
{
    FfxUInt32 ray_index = group_id * 64 + group_index;
    if(!IsRayIndexValid(ray_index))
    {
        return;
    }

    FfxUInt32 packed_coords = GetRaylist(ray_index);

    FfxUInt32x2 coords;
    FfxBoolean copy_horizontal;
    FfxBoolean copy_vertical;
    FfxBoolean copy_diagonal;
    UnpackRayCoords(packed_coords, coords, copy_horizontal, copy_vertical, copy_diagonal);

    const FfxUInt32x2 screen_size = RenderSize();

    FfxFloat32x2 uv = (coords + 0.5) * InverseRenderSize();

    FfxFloat32x3 world_space_normal = FFX_SSSR_LoadWorldSpaceNormal(FfxInt32x2(coords));
    FfxFloat32 roughness = FFX_SSSR_LoadExtractedRoughness(FfxInt32x3(coords, 0));
    FfxBoolean is_mirror = IsMirrorReflection(roughness);

    FfxInt32 most_detailed_mip = is_mirror ? 0 : FfxInt32(MostDetailedMip());
    FfxFloat32x2 mip_resolution = FFX_SSSR_GetMipResolution(screen_size, most_detailed_mip);
    FfxFloat32 z = FFX_SSSR_LoadDepth(FfxInt32x2(uv * mip_resolution), most_detailed_mip);

    FfxFloat32x3 screen_uv_space_ray_origin = FfxFloat32x3(uv, z);
    FfxFloat32x3 view_space_ray = ScreenSpaceToViewSpace(screen_uv_space_ray_origin);
    FfxFloat32x3 view_space_ray_direction = normalize(view_space_ray);

    FfxFloat32x3 view_space_surface_normal = FFX_MATRIX_MULTIPLY(ViewMatrix(), FfxFloat32x4(world_space_normal, 0)).xyz;
    FfxFloat32x3 view_space_reflected_direction = SampleReflectionVector(view_space_ray_direction, view_space_surface_normal, roughness, FfxInt32x2(coords));
    FfxFloat32x3 screen_space_ray_direction = ProjectDirection(view_space_ray, view_space_reflected_direction, screen_uv_space_ray_origin, Projection());

    //====SSSR====
    FfxBoolean valid_hit = false;
    FfxFloat32x3 hit = FFX_SSSR_HierarchicalRaymarch(screen_uv_space_ray_origin, screen_space_ray_direction, is_mirror, screen_size, most_detailed_mip, MinTraversalOccupancy(), MaxTraversalIntersections(), valid_hit);

    FfxFloat32x3 world_space_origin   = ScreenSpaceToWorldSpace(screen_uv_space_ray_origin);
    FfxFloat32x3 world_space_hit      = ScreenSpaceToWorldSpace(hit);
    FfxFloat32x3 world_space_ray      = world_space_hit - world_space_origin.xyz;

    FfxFloat32 confidence = valid_hit ? FFX_SSSR_ValidateHit(hit, uv, world_space_ray, screen_size, DepthBufferThickness()) : 0;
    FfxFloat32 world_ray_length = max(0, length(world_space_ray));

    FfxFloat32x3 reflection_radiance = FfxFloat32x3(0.0f, 0.0f, 0.0f);
    if (confidence > 0.0f) {
        // Found an intersection with the depth buffer -> We can lookup the color from lit scene.
        reflection_radiance = FFX_SSSR_LoadInputColor(FfxInt32x3(screen_size * hit.xy, 0));
    }

    // Sample environment map.
    FfxFloat32x3 world_space_reflected_direction = FFX_MATRIX_MULTIPLY(InvView(), FfxFloat32x4(view_space_reflected_direction, 0)).xyz;
    FfxFloat32x3 environment_lookup = FFX_SSSR_SampleEnvironmentMap(world_space_reflected_direction, 0.0f);
    reflection_radiance = ffxLerp(environment_lookup, reflection_radiance, confidence);

    FfxFloat32x4 new_sample = FfxFloat32x4(reflection_radiance, world_ray_length);

    FFX_SSSR_StoreRadiance(coords, new_sample);

    FfxUInt32x2 copy_target = coords ^ 1; // Flip last bit to find the mirrored coords along the x and y axis within a quad.
    if (copy_horizontal) {
        FfxUInt32x2 copy_coords = FfxUInt32x2(copy_target.x, coords.y);
        FFX_SSSR_StoreRadiance(copy_coords, new_sample);
    }
    if (copy_vertical) {
        FfxUInt32x2 copy_coords = FfxUInt32x2(coords.x, copy_target.y);
        FFX_SSSR_StoreRadiance(copy_coords, new_sample);
    }
    if (copy_diagonal) {
        FfxUInt32x2 copy_coords = copy_target;
        FFX_SSSR_StoreRadiance(copy_coords, new_sample);
    }
}
