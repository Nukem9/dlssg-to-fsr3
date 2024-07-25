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

#ifndef COMMONTYPES_H
#define COMMONTYPES_H

struct Material_Info
{
    float albedo_factor_x;
    float albedo_factor_y;
    float albedo_factor_z;
    float albedo_factor_w;

    // A.R.M. Packed texture - Ambient occlusion | Roughness | Metalness
    float arm_factor_x;
    float arm_factor_y;
    float arm_factor_z;
    int   arm_tex_id;
    int   arm_tex_sampler_id;

    float emission_factor_x;
    float emission_factor_y;
    float emission_factor_z;
    int   emission_tex_id;
    int   emission_tex_sampler_id;

    int   normal_tex_id;
    int   normal_tex_sampler_id;
    int   albedo_tex_id;
    int   albedo_tex_sampler_id;
    float alpha_cutoff;
    int   is_opaque;
};

struct Instance_Info
{
    int surface_id_table_offset;
    int num_opaque_surfaces;
    int node_id;
    int num_surfaces;
};

struct Surface_Info
{
    int material_id;
    int index_offset;  // Offset for the first index
    int index_type;    // 0 - u32, 1 - u16
    int position_attribute_offset;

    int texcoord0_attribute_offset;
    int texcoord1_attribute_offset;
    int normal_attribute_offset;
    int tangent_attribute_offset;

    int num_indices;
    int num_vertices;
    int weight_attribute_offset;
    int joints_attribute_offset;
};

struct FrameInfo
{
#if __cplusplus
    float inv_view_proj[16];
    float proj[16];
    float inv_proj[16];
    float view[16];
    float inv_view[16];
    float prev_view_proj[16];
    float prev_view[16];

    uint32_t frame_index;
    uint32_t max_traversal_intersections;
    uint32_t min_traversal_occupancy;
    uint32_t most_detailed_mip;

    uint32_t samples_per_quad;
    uint32_t temporal_variance_guided_tracing_enabled;
    uint32_t hsr_mask;
    uint32_t random_samples_per_pixel;

    uint32_t base_width;
    uint32_t base_height;
    uint32_t reflection_width;
    uint32_t reflection_height;

    uint32_t reset;
    uint32_t pad0;
    uint32_t pad1;
    uint32_t pad2;

#else
    float4x4 inv_view_proj;
    float4x4 proj;
    float4x4 inv_proj;
    float4x4 view;
    float4x4 inv_view;
    float4x4 prev_view_proj;
    float4x4 prev_view;

    uint frame_index;
    uint max_traversal_intersections;
    uint min_traversal_occupancy;
    uint most_detailed_mip;

    uint samples_per_quad;
    uint temporal_variance_guided_tracing_enabled;
    uint hsr_mask;
    uint random_samples_per_pixel;

    uint base_width;
    uint base_height;
    uint reflection_width;
    uint reflection_height;

    uint reset;
    uint pad0;
    uint pad1;
    uint pad2;
#endif

    float temporal_stability_factor;
    float ssr_confidence_threshold;
    float depth_buffer_thickness;
    float roughness_threshold;

    float hybrid_miss_weight;
    float max_raytraced_distance;
    float hybrid_spawn_rate;
    float reflections_backfacing_threshold;

    float vrt_variance_threshold;
    float ssr_thickness_length_factor;
    float fsr_roughness_threshold;
    float ray_length_exp_factor;

    float rt_roughness_threshold;
    float reflection_factor;
    float ibl_factor;
    float emissive_factor;
};

#ifdef _HLSL

// Convenience definitions
#    define g_frame_info g_frame_info_cb[0]
#    define g_inv_view_proj g_frame_info.inv_view_proj
#    define g_proj g_frame_info.proj
#    define g_inv_proj g_frame_info.inv_proj
#    define g_view g_frame_info.view
#    define g_inv_view g_frame_info.inv_view
#    define g_prev_view_proj g_frame_info.prev_view_proj
#    define g_prev_view g_frame_info.prev_view
#    define g_frame_index g_frame_info.frame_index
#    define g_max_traversal_intersections g_frame_info.max_traversal_intersections
#    define g_min_traversal_occupancy g_frame_info.min_traversal_occupancy
#    define g_most_detailed_mip g_frame_info.most_detailed_mip
#    define g_temporal_stability_factor g_frame_info.temporal_stability_factor
#    define g_depth_buffer_thickness g_frame_info.depth_buffer_thickness
#    define g_roughness_threshold g_frame_info.roughness_threshold
#    define g_samples_per_quad g_frame_info.samples_per_quad
#    define g_temporal_variance_guided_tracing_enabled g_frame_info.temporal_variance_guided_tracing_enabled
#    define g_hsr_mask g_frame_info.hsr_mask
#    define g_hsr_variance_factor g_frame_info.hsr_variance_factor
#    define g_hsr_variance_power g_frame_info.hsr_variance_power
#    define g_hsr_variance_bias g_frame_info.hsr_variance_bias
#    define g_iblFactor g_frame_info.ibl_factor
#    define g_emissiveFactor g_frame_info.emissive_factor

#endif

#endif  // !COMMONTYPES_H
