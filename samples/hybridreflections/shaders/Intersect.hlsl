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

#include "declarations.h"
#include "common_types.h"
#include "raytracinglightfunctions.hlsl"

SamplerState           g_linear_sampler : register(s0);
SamplerState           g_wrap_linear_sampler : register(s1);
SamplerState           samBRDF : register(s2);
SamplerState           samIrradianceCube : register(s3);
SamplerState           samPrefilteredCube : register(s4);
SamplerComparisonState SamShadow : register(s5);
SamplerState           g_samplers[] : DECLARE_SAMPLER(SAMPLER_BEGIN_SLOT);

RaytracingAccelerationStructure g_global : register(t0);

Texture2D<float4>   g_brdf_lut : register(t1);
TextureCube<float4> g_atmosphere_lut : register(t2);
TextureCube<float4> g_atmosphere_dif : register(t3);
Texture2D<float4>   g_motion_vector : register(t4);
Texture2D<float4>   g_gbuffer_normal : register(t5);
Texture2D<float4>   g_gbuffer_depth : register(t6);
Texture2D<float4>   g_gbuffer_roughness : register(t7);
Texture2D<float4>   g_hiz : register(t8);
Texture2D<float>    g_extracted_roughness : register(t9);
Texture2D<float4>   g_lit_scene_history : register(t10);
Texture2D<float4>   g_random_number_image : register(t11);
Texture2D<float4>   g_gbuffer_full_albedo : register(t12);

StructuredBuffer<Material_Info> g_material_info : DECLARE_SRV(RAYTRACING_INFO_MATERIAL);
StructuredBuffer<Instance_Info> g_instance_info : DECLARE_SRV(RAYTRACING_INFO_INSTANCE);
StructuredBuffer<uint>          g_surface_id : DECLARE_SRV(RAYTRACING_INFO_SURFACE_ID);
StructuredBuffer<Surface_Info>  g_surface_info : DECLARE_SRV(RAYTRACING_INFO_SURFACE);

Texture2D g_textures[200] : DECLARE_SRV(TEXTURE_BEGIN_SLOT);
Texture2D ShadowMapTextures[MAX_SHADOW_MAP_TEXTURES_COUNT] : DECLARE_SRV(SHADOW_MAP_BEGIN_SLOT);

StructuredBuffer<uint>  g_index_buffer[MAX_BUFFER_COUNT] : DECLARE_SRV(INDEX_BUFFER_BEGIN_SLOT);
StructuredBuffer<float> g_vertex_buffer[] : DECLARE_SRV(VERTEX_BUFFER_BEGIN_SLOT);

RWTexture2D<uint>   g_rw_hit_counter[2] : register(u0);
RWTexture2D<float4> g_rw_radiance[2] : register(u2);
RWTexture2D<float>  g_rw_radiance_variance[2] : register(u4);
RWTexture2D<float4> g_rw_debug : register(u6);
RWByteAddressBuffer g_rw_hw_ray_list : register(u7);
RWByteAddressBuffer g_rw_ray_list : register(u8);
RWByteAddressBuffer g_rw_ray_counter : register(u9);
RWByteAddressBuffer g_rw_ray_gbuffer_list : register(u10);

ConstantBuffer<FrameInfo> g_frame_info_cb[1] : register(b0);

cbuffer CBSceneInformation : register(b1)
{
    SceneInformation SceneInfo;
};

cbuffer CBLightInformation : register(b2)
{
    SceneLightingInformation LightInfo;
}

#include "common.hlsl"

#define brdfTexture     g_brdf_lut
#define irradianceCube  g_atmosphere_dif
#define prefilteredCube g_atmosphere_lut
#define myPerFrame      g_frame_info.perFrame

float4 SampleBRDFTexture(float2 uv)
{
    return brdfTexture.SampleLevel(samBRDF, uv, 0);
}

float4 SampleIrradianceCube(float3 n)
{
    return irradianceCube.SampleLevel(samIrradianceCube, n, 0);
}

float4 SamplePrefilteredCube(float3 reflection, float lod)
{
    return prefilteredCube.SampleLevel(samPrefilteredCube, reflection, lod);
}

#define IBL_INDEX b3
#include "lightingcommon.h"

float2 SampleMotionVector(float2 uv)
{
    return g_motion_vector.SampleLevel(g_linear_sampler, uv, 0.0).xy * float2(-1, -1);
}

float SampleDepth(float2 uv)
{
    return g_gbuffer_depth.SampleLevel(g_linear_sampler, uv, 0.0).x;
}

float3 FFX_SSSR_LoadWorldSpaceNormal(int2 pixel_coordinate)
{
    return normalize(g_gbuffer_normal.Load(int3(pixel_coordinate, 0)).xyz * 2.0f - (1.0f).xxx);
}

float3 SampleNormal(float2 uv) 
{ 
    return normalize(2 * g_gbuffer_normal.SampleLevel(g_linear_sampler, uv, 0.0).xyz - 1); 
}

float FFX_SSSR_LoadDepth(int2 pixel_coordinate, int mip)
{
    return g_hiz.Load(int3(pixel_coordinate, mip /* + pc.depth_mip_bias*/)).x;
}

bool IsMirrorReflection(float roughness)
{
    return roughness < 0.0001;
}

#ifdef USE_SSR

#include "ffx_sssr_intersect.h"

#else
float2 FFX_SSSR_GetMipResolution(float2 screen_dimensions, int mip_level)
{
    return screen_dimensions * pow(0.5, mip_level);
}
#endif

// By Morgan McGuire @morgan3d, http://graphicscodex.com
// Reuse permitted under the BSD license.
// https://www.shadertoy.com/view/4dsSzr
float3 heatmapGradient(float t)
{
    return clamp((pow(t, 1.5) * 0.8 + 0.2) * float3(smoothstep(0.0, 0.35, t) + t * 0.5, smoothstep(0.5, 1.0, t), max(1.0 - t * 1.7, t * 7.0 - 6.0)), 0.0, 1.0);
}

struct RayGbuffer
{
    float2 uv;
    float3 normal;
    uint   material_id;
    float  world_ray_length;
    // Skip shading and do env probe
    bool skip;
};

struct PackedRayGbuffer
{
    uint pack0;
    uint pack1;
    uint pack2;
};

bool GbufferIsSkip(PackedRayGbuffer gbuffer)
{
    return (gbuffer.pack1 & (1u << 31u)) != 0;
}

float GbufferGetRayLength(PackedRayGbuffer gbuffer)
{
    return asfloat(gbuffer.pack2);
}

PackedRayGbuffer PackGbuffer(RayGbuffer gbuffer)
{
    uint   pack0 = PackFloat16((min16float2)gbuffer.uv);
    uint   pack1;
    float2 nuv  = OctahedronUV(gbuffer.normal);
    uint   iuvx = uint(255.0 * nuv.x);
    uint   iuvy = uint(255.0 * nuv.y);
    pack1       =                                   //
        ((gbuffer.material_id & 0x7fffu) << 16u) |  //
        (iuvx << 0u) |                              //
        (iuvy << 8u);
    if (gbuffer.skip)
        pack1 |= (1u << 31u);
    uint             pack2 = asuint(gbuffer.world_ray_length);  // PackFloat16(float2());
    PackedRayGbuffer pack  = {pack0, pack1, pack2};
    return pack;
}

RayGbuffer UnpackGbuffer(PackedRayGbuffer gbuffer)
{
    RayGbuffer ogbuffer;
    ogbuffer.uv               = UnpackFloat16(gbuffer.pack0);
    uint   iuvx               = (gbuffer.pack1 >> 0u) & 0xffu;
    uint   iuvy               = (gbuffer.pack1 >> 8u) & 0xffu;
    float2 nuv                = float2(float(iuvx) / 255.0, float(iuvy) / 255.0);
    ogbuffer.normal           = UVtoOctahedron(nuv);
    ogbuffer.material_id      = (gbuffer.pack1 >> 16u) & 0x7fffu;
    ogbuffer.skip             = GbufferIsSkip(gbuffer);
    ogbuffer.world_ray_length = GbufferGetRayLength(gbuffer);
    return ogbuffer;
}

void SkipRayGBuffer(uint ray_index)
{
    uint pack1 = (1u << 31u);
    g_rw_ray_gbuffer_list.Store<uint>(ray_index * 12 + 4, pack1);
}

void StoreRayGBuffer(uint ray_index, in PackedRayGbuffer gbuffer)
{
    g_rw_ray_gbuffer_list.Store<uint>(ray_index * 12 + 0, gbuffer.pack0);
    g_rw_ray_gbuffer_list.Store<uint>(ray_index * 12 + 4, gbuffer.pack1);
    g_rw_ray_gbuffer_list.Store<uint>(ray_index * 12 + 8, gbuffer.pack2);
}

PackedRayGbuffer LoadRayGBuffer(uint ray_index)
{
    uint             pack0 = g_rw_ray_gbuffer_list.Load<uint>(ray_index * 12 + 0);
    uint             pack1 = g_rw_ray_gbuffer_list.Load<uint>(ray_index * 12 + 4);
    uint             pack2 = g_rw_ray_gbuffer_list.Load<uint>(ray_index * 12 + 8);
    PackedRayGbuffer pack  = {pack0, pack1, pack2};
    return pack;
}

void WriteRadiance(uint packed_coords, float4 radiance)
{
    if (any(isnan(radiance)))
        return;

    int2 coords;
    bool copy_horizontal;
    bool copy_vertical;
    bool copy_diagonal;
    UnpackRayCoords(packed_coords, coords, copy_horizontal, copy_vertical, copy_diagonal);

    g_rw_radiance[(g_frame_index + 1) % 2][coords]          = radiance;
    g_rw_radiance_variance[(g_frame_index + 1) % 2][coords] = radiance.w;
    uint2 copy_target                = coords ^ 0b1;  // Flip last bit to find the mirrored coords along the x and y axis within a quad.
    {
        if (copy_horizontal)
        {
            uint2 copy_coords                     = uint2(copy_target.x, coords.y);
            g_rw_radiance[(g_frame_index + 1) % 2][copy_coords]          = radiance;
            g_rw_radiance_variance[(g_frame_index + 1) % 2][copy_coords] = radiance.w;
        }
        if (copy_vertical)
        {
            uint2 copy_coords                     = uint2(coords.x, copy_target.y);
            g_rw_radiance[(g_frame_index + 1) % 2][copy_coords]          = radiance;
            g_rw_radiance_variance[(g_frame_index + 1) % 2][copy_coords] = radiance.w;
        }
        if (copy_diagonal)
        {
            uint2 copy_coords                     = copy_target;
            g_rw_radiance[(g_frame_index + 1) % 2][copy_coords]          = radiance;
            g_rw_radiance_variance[(g_frame_index + 1) % 2][copy_coords] = radiance.w;
        }
    }
}

[numthreads(32, 1, 1)] void main(uint group_index
                                 : SV_GroupIndex, uint group_id
                                 : SV_GroupID) {
    //////////////////////////////////////////
    ///////////  INITIALIZATION  /////////////
    //////////////////////////////////////////
    uint ray_index = group_id * 32 + group_index;
#ifdef USE_DEFERRED_RAYTRACING
    uint packed_coords = g_rw_hw_ray_list.Load(sizeof(uint) * ray_index);
#else   // USE_DEFERRED_RAYTRACING
    uint packed_coords = g_rw_ray_list.Load(sizeof(uint) * ray_index);
#endif  // USE_DEFERRED_RAYTRACING
    int2 coords;
    {
        bool copy_horizontal;
        bool copy_vertical;
        bool copy_diagonal;
        UnpackRayCoords(packed_coords, coords, copy_horizontal, copy_vertical, copy_diagonal);
    }
#ifdef HSR_DEBUG
    float4 debug_value = float4(0.0, 0.0, 0.0, 0.0);
#endif  // HSR_DEBUG
    float roughness = g_extracted_roughness.Load(int3(coords, 0));
#ifdef USE_DEFERRED_RAYTRACING
    if (ray_index >= g_rw_ray_counter.Load(RAY_COUNTER_HW_HISTORY_OFFSET) || !FFX_DNSR_Reflections_IsGlossyReflection(roughness))
    {
        SkipRayGBuffer(ray_index);
        return;
    }
#else  // USE_DEFERRED_RAYTRACING

#endif  // USE_DEFERRED_RAYTRACING

    //////////////////////////////////////////
    ///////////  Screen Space  ///////////////
    //////////////////////////////////////////

#ifdef USE_SSR
    uint2  screen_size                     = uint2(g_frame_info.reflection_width, g_frame_info.reflection_height);
    float2 uv                              = float2(coords + 0.5) / float2(screen_size);
    bool   is_mirror                       = IsMirrorReflection(roughness);
    int    most_detailed_mip               = is_mirror ? 0 : g_most_detailed_mip;
    float2 mip_resolution                  = FFX_SSSR_GetMipResolution(screen_size, most_detailed_mip);
    float  z                               = FFX_SSSR_LoadDepth(uv * mip_resolution, most_detailed_mip);
    float3 screen_uv_space_ray_origin      = float3(uv, z);
    float3 view_space_ray                  = ScreenSpaceToViewSpace(screen_uv_space_ray_origin, g_inv_proj);
    float3 world_space_normal              = FFX_SSSR_LoadWorldSpaceNormal(coords);
    float3 view_space_surface_normal       = mul(g_view, float4(world_space_normal, 0)).xyz;
    float3 view_space_ray_direction        = normalize(view_space_ray);
    float3 view_space_reflected_direction  = SampleReflectionVector(view_space_ray_direction, view_space_surface_normal, roughness, coords);
    screen_uv_space_ray_origin             = ProjectPosition(view_space_ray, g_proj);
    float3 screen_space_ray_direction      = ProjectDirection(view_space_ray, view_space_reflected_direction, screen_uv_space_ray_origin, g_proj);
    float3 world_space_reflected_direction = mul(g_inv_view, float4(view_space_reflected_direction, 0)).xyz;
    float3 world_space_origin              = mul(g_inv_view, float4(view_space_ray, 1)).xyz;
    float  world_ray_length                = 0.0;
    bool   valid_ray                       = !FFX_DNSR_Reflections_IsBackground(z) && FFX_DNSR_Reflections_IsGlossyReflection(roughness) &&
                     ray_index < g_rw_ray_counter.Load(RAY_COUNTER_SW_HISTORY_OFFSET) &&
                     all(coords < int2(g_frame_info.reflection_width, g_frame_info.reflection_height));

    bool   do_hw           = false;
    uint   hit_counter     = 0;
    float3 hit             = float3(0.0, 0.0, 0.0);
    float  confidence      = 0.0;
    float3 world_space_hit = float3(0.0, 0.0, 0.0);
    float3 world_space_ray = float3(0.0, 0.0, 0.0);

    if (valid_ray)
    {
        bool valid_hit;
        uint numIterations;
        hit = FFX_SSSR_HierarchicalRaymarch(screen_uv_space_ray_origin,
                                            screen_space_ray_direction,
                                            is_mirror,
                                            screen_size,
                                            most_detailed_mip,
                                            g_min_traversal_occupancy,
                                            g_max_traversal_intersections,
                                            valid_hit, numIterations);

        world_space_hit  = ScreenSpaceToWorldSpace(hit, g_inv_view_proj);
        world_space_ray  = world_space_hit - world_space_origin.xyz;
        world_ray_length = length(world_space_ray);
        confidence       = valid_hit ? FFX_SSSR_ValidateHit(hit,
                                                      uv,
                                                      world_space_ray,
                                                      screen_size,
                                                      g_depth_buffer_thickness
                                                          // Add thickness for rough surfaces
                                                          + roughness * 10.0
                                                          // Add thickness with distance
                                                          + world_ray_length * g_frame_info.ssr_thickness_length_factor)
                                     : 0;

        do_hw = (g_hsr_mask & HSR_FLAGS_USE_RAY_TRACING) &&
                        // Add some bias with distance to push the confidence
                        (confidence < g_frame_info.ssr_confidence_threshold) && !FFX_DNSR_Reflections_IsBackground(hit.z)
                    // Ray tracing roughness threshold
                    ? true
                    : false;
        // Feedback flags
        // See ClassifyTiles.hlsl
        if (!do_hw)
            hit_counter = FFX_HITCOUNTER_SW_HIT_FLAG;
        else
            hit_counter = FFX_HITCOUNTER_SW_MISS_FLAG;

        uint hit_counter_per_pixel = hit_counter;
        // For variable rate shading we also need to copy counters
        {
            bool copy_horizontal;
            bool copy_vertical;
            bool copy_diagonal;
            UnpackRayCoords(packed_coords, coords, copy_horizontal, copy_vertical, copy_diagonal);

            if (copy_horizontal)
                hit_counter += hit_counter_per_pixel;
            if (copy_vertical)
                hit_counter += hit_counter_per_pixel;
            if (copy_diagonal)
                hit_counter += hit_counter_per_pixel;
        }
    }
    // Feedback information for the next frame
    if (g_hsr_mask & HSR_FLAGS_USE_HIT_COUNTER)
    {
        // One atomic increment per wave
        uint hit_counter_sum = WaveActiveSum(hit_counter);
        if (WaveIsFirstLane())
            InterlockedAdd(g_rw_hit_counter[(g_frame_index + 1) % 2][coords / 8], hit_counter_sum);
    }

    if (valid_ray)
    {
        if (do_hw)
        {
#ifdef HSR_DEBUG
            debug_value = float4(1.0, 0.0, 0.0, 1.0);
#endif  // HSR_DEBUG
        }
        else
        {
            if (confidence < 0.9)
            {
                WriteRadiance(
                    packed_coords, float4(FFX_GetEnvironmentSample(world_space_origin, world_space_reflected_direction, 0.0), FFX_REFLECTIONS_SKY_DISTANCE));
            }
            else
            {
                // Found an intersection with the depth buffer -> We can lookup the color from the lit scene history.

                float3 reflection_radiance;
                if (g_hsr_mask & HSR_FLAGS_SHADING_USE_SCREEN) {
                    reflection_radiance = g_lit_scene_history.SampleLevel(g_linear_sampler, hit.xy - SampleMotionVector(hit.xy), 0.0).xyz;
                }
                else {
                    float4 albedo = g_gbuffer_full_albedo.SampleLevel(g_linear_sampler, hit.xy, 0.0).xyzw;

                    PBRPixelInfo pixelInfo;
                    pixelInfo.pixelBaseColorAlpha = float4(albedo.xyz, 1.f);
                    pixelInfo.pixelNormal = float4(SampleNormal(hit.xy), 1.f);
                    pixelInfo.pixelAoRoughnessMetallic = g_gbuffer_roughness.SampleLevel(g_linear_sampler, hit.xy, 0.0).xyz;
                    pixelInfo.pixelWorldPos = float4(world_space_hit, 1.f);
                    pixelInfo.pixelCoordinates = float4(0, 0, 0, 0);

                    reflection_radiance = PBRLighting(pixelInfo, ShadowMapTextures);
                }
                WriteRadiance(packed_coords, float4(reflection_radiance, world_ray_length));
            }
#ifdef HSR_DEBUG
            debug_value = float4(0.0, 1.0, 0.0, 1.0);
#endif  // HSR_DEBUG
        }
    }
#ifdef HSR_DEBUG
    if (g_hsr_mask & HSR_FLAGS_VISUALIZE_TRANSPARENT_QUERY)
        debug_value = float4(0.0, 0.0, 0.0, 0.0);
#endif  // HSR_DEBUG

    // Fall back to env probe in case of rough surfaces
    if (do_hw && roughness > g_frame_info.rt_roughness_threshold)
    {
        do_hw = false;
        WriteRadiance(packed_coords,
                                      float4(FFX_GetEnvironmentSample(world_space_origin, world_space_reflected_direction, 0.0), FFX_REFLECTIONS_SKY_DISTANCE));
    }
    // Store ray coordinate for deferred HW ray tracing pass
    uint deferred_hw_wave_sum  = WaveActiveCountBits(do_hw);
    uint deferred_hw_wave_scan = WavePrefixCountBits(do_hw);
    uint deferred_hw_offset;
    if (WaveIsFirstLane() && deferred_hw_wave_sum)
    {
        g_rw_ray_counter.InterlockedAdd(RAY_COUNTER_HW_OFFSET, deferred_hw_wave_sum, deferred_hw_offset);
    }
    deferred_hw_offset = WaveReadLaneFirst(deferred_hw_offset);
    if (do_hw)
    {
        g_rw_hw_ray_list.Store(4 * (deferred_hw_offset + deferred_hw_wave_scan), packed_coords);
    }
    if (!valid_ray)
        return;
#endif  // USE_SSR

        //////////////////////////////////////////
        ///////////  HW Ray Tracing  /////////////
        //////////////////////////////////////////
#ifdef USE_INLINE_RAYTRACING
    float3 world_space_origin;
    float3 world_space_reflected_direction;

    uint2  screen_size                    = uint2(g_frame_info.reflection_width, g_frame_info.reflection_height);
    float2 uv                             = float2(coords + 0.5) / float2(screen_size);
    float  z                              = FFX_SSSR_LoadDepth(coords, 0);
    float3 screen_uv_space_ray_origin     = float3(uv, z);
    float3 view_space_ray                 = ScreenSpaceToViewSpace(screen_uv_space_ray_origin, g_inv_proj);
    float3 world_space_normal             = FFX_SSSR_LoadWorldSpaceNormal(coords);
    float3 view_space_surface_normal      = mul(g_view, float4(normalize(world_space_normal), 0)).xyz;
    float3 view_space_ray_direction       = normalize(view_space_ray);
    float3 view_space_reflected_direction = SampleReflectionVector(view_space_ray_direction, view_space_surface_normal, roughness, coords);
    screen_uv_space_ray_origin            = ProjectPosition(view_space_ray, g_proj);
    float3 screen_space_ray_direction     = ProjectDirection(view_space_ray, view_space_reflected_direction, screen_uv_space_ray_origin, g_proj);
    world_space_reflected_direction       = normalize(mul(g_inv_view, float4(view_space_reflected_direction, 0)).xyz);
    world_space_origin                    = mul(g_inv_view, float4(view_space_ray, 1)).xyz;

#ifdef HSR_DEBUG
    if (g_hsr_mask & HSR_FLAGS_USE_SCREEN_SPACE)
        debug_value = float4(0.0, 0.0, 1.0, 1.0);
    if (g_hsr_mask & HSR_FLAGS_VISUALIZE_TRANSPARENT_QUERY)
        debug_value = float4(0.0, 0.0, 0.0, 0.0);
#endif  // HSR_DEBUG
    PackedRayGbuffer packed_gbuffer;
    {
        RayGbuffer default_gbuffer;
        default_gbuffer.normal           = float3(0.0, 0.0, 1.0);
        default_gbuffer.uv               = (0.0).xx;
        default_gbuffer.material_id      = 0;
        default_gbuffer.skip             = true;
        default_gbuffer.world_ray_length = FFX_REFLECTIONS_SKY_DISTANCE;
        packed_gbuffer                   = PackGbuffer(default_gbuffer);
    }

    const float max_t = g_frame_info.max_raytraced_distance * exp(-roughness * g_frame_info.ray_length_exp_factor);

    {
        // Just a single ray, no transparency
        RayQuery<RAY_FLAG_NONE | RAY_FLAG_FORCE_OPAQUE> opaque_query;
        RayDesc                                         ray;
        ray.Origin    = world_space_origin + world_space_normal * 3.0e-3 * length(view_space_ray);
        ray.Direction = world_space_reflected_direction;
        ray.TMin      = 3.0e-3;
        ray.TMax      = max_t;
        opaque_query.TraceRayInline(g_global, 0, 0xff, ray);
        opaque_query.Proceed();
        if (opaque_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            uint          instance_id;
            uint          geometry_id;
            uint          surface_id;
            uint          triangle_id;
            Surface_Info  sinfo;
            Material_Info material;
            Instance_Info iinfo;
            float2        bary;
            uint3         face3;
            float3x3      obj_to_world;

            obj_to_world = float3x3(opaque_query.CommittedObjectToWorld3x4()[0].xyz,
                                    opaque_query.CommittedObjectToWorld3x4()[1].xyz,
                                    opaque_query.CommittedObjectToWorld3x4()[2].xyz);
            bary         = opaque_query.CommittedTriangleBarycentrics();
            instance_id  = opaque_query.CommittedInstanceID();
            geometry_id  = opaque_query.CommittedGeometryIndex();
            triangle_id  = opaque_query.CommittedPrimitiveIndex();

            // bool opaque = true;
            iinfo      = g_instance_info.Load(instance_id);
            surface_id = g_surface_id.Load((iinfo.surface_id_table_offset + geometry_id));
            sinfo      = g_surface_info.Load(surface_id);
            material   = g_material_info.Load(sinfo.material_id);

            if (sinfo.index_type == SURFACE_INFO_INDEX_TYPE_U16)
            {
                FFX_Fetch_Face_Indices_U16(/* out */ face3, /* in */ sinfo.index_offset, /* in */ triangle_id);
            }
            else
            {  // SURFACE_INFO_INDEX_TYPE_U32
                FFX_Fetch_Face_Indices_U32(/* out */ face3, /* in */ sinfo.index_offset, /* in */ triangle_id);
            }
            float3 normal;
            float4 tangent;
            float2 uv;
            // Fetch interpolated local geometry basis
            FFX_Fetch_Local_Basis(/* in */ sinfo, /* in */ face3, /* in */ bary, /* out */ uv, /* out */ normal, /* out */ tangent);

            float4 albedo = (1.0).xxxx;
            if (albedo.w > 0.5)
            {
                normal      = normalize(rotate(obj_to_world, normal));
                tangent.xyz = rotate(obj_to_world, tangent.xyz);
                if (material.normal_tex_id >= 0 && any(abs(tangent.xyz) > 0.0f))
                {
                    tangent.xyz       = normalize(tangent.xyz);
                    float3 binormal   = normalize(cross(normal, tangent.xyz) * tangent.w);
                    float3 normal_rgb = float3(0.0f, 0.0f, 1.0f);
                    normal_rgb        = g_textures[NonUniformResourceIndex(material.normal_tex_id)].SampleLevel(g_samplers[material.normal_tex_sampler_id], uv, 2.0).rgb;
                    normal_rgb.z      = sqrt(saturate(1.0 - normal_rgb.r * normal_rgb.r - normal_rgb.g * normal_rgb.g));
                    normal            = normalize(normal_rgb.z * normal + (2.0f * normal_rgb.x - 1.0f) * tangent.xyz + (2.0f * normal_rgb.y - 1.0f) * binormal);
                }
            }

            if (material.albedo_tex_id >= 0)
            {
                albedo = albedo * g_textures[NonUniformResourceIndex(material.albedo_tex_id)].SampleLevel(g_samplers[material.albedo_tex_sampler_id], uv, 0).xyzw;
            }
            RayGbuffer gbuffer;
            gbuffer.material_id      = sinfo.material_id;
            gbuffer.normal           = normal;
            gbuffer.uv               = uv;
            gbuffer.world_ray_length = opaque_query.CommittedRayT();
#ifdef HSR_TRANSPARENT_QUERY
            gbuffer.skip = albedo.w < 0.5;
#else   // HSR_TRANSPARENT_QUERY
            gbuffer.skip = false;
#endif  // HSR_TRANSPARENT_QUERY
            packed_gbuffer = PackGbuffer(gbuffer);
        }
    }
#ifdef HSR_TRANSPARENT_QUERY
    if (GbufferIsSkip(packed_gbuffer))
    {  // Try transparent query
#ifdef HSR_DEBUG
        if (g_hsr_mask & HSR_FLAGS_VISUALIZE_TRANSPARENT_QUERY)
            debug_value = float4(0.0, 1.0, 0.0, 1.0);
#endif  // HSR_DEBUG
        RayQuery<RAY_FLAG_NONE> transparent_query;
        RayDesc                 ray;
        ray.Origin    = world_space_origin + world_space_normal * 3.0e-3 * length(view_space_ray);
        ray.Direction = world_space_reflected_direction;
        ray.TMin      = GbufferGetRayLength(packed_gbuffer) - 1.0e-2;
        ray.TMax      = max_t - ray.TMin;
        transparent_query.TraceRayInline(g_transparent,
                                         0,  // OR'd with flags above
                                         0xff,
                                         ray);
        while (transparent_query.Proceed())
        {
            if (transparent_query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
            {
                uint          instance_id;
                uint          geometry_id;
                uint          surface_id;
                uint          triangle_id;
                Surface_Info  sinfo;
                Material_Info material;
                Instance_Info iinfo;
                float2        bary;
                uint3         face3;
                float3x3      obj_to_world;
                obj_to_world = float3x3(transparent_query.CandidateObjectToWorld3x4()[0].xyz,
                                        transparent_query.CandidateObjectToWorld3x4()[1].xyz,
                                        transparent_query.CandidateObjectToWorld3x4()[2].xyz);
                bary         = transparent_query.CandidateTriangleBarycentrics();
                instance_id  = transparent_query.CandidateInstanceID();
                geometry_id  = transparent_query.CandidateGeometryIndex();
                triangle_id  = transparent_query.CandidatePrimitiveIndex();

                // bool opaque = true;
                iinfo      = g_instance_info.Load(instance_id);
                surface_id = g_surface_id.Load((iinfo.surface_id_table_offset + geometry_id));
                sinfo      = g_surface_info.Load(surface_id);
                material   = g_material_info.Load(sinfo.material_id);

                if (sinfo.index_type == SURFACE_INFO_INDEX_TYPE_U16)
                {
                    FFX_Fetch_Face_Indices_U16(/* out */ face3, /* in */ sinfo.index_offset, /* in */ triangle_id);
                }
                else
                {  // SURFACE_INFO_INDEX_TYPE_U32
                    FFX_Fetch_Face_Indices_U32(/* out */ face3, /* in */ sinfo.index_offset, /* in */ triangle_id);
                }
                float3 normal;
                float2 uv;
                // Fetch interpolated local geometry basis
                FFX_Fetch_Local_Basis(/* in */ sinfo, /* in */ face3, /* in */ bary, /* out */ uv, /* out */ normal);
                float4 albedo = (1.0).xxxx;
                if (material.albedo_tex_id >= 0)
                {
                    albedo = albedo * g_textures[NonUniformResourceIndex(material.albedo_tex_id)].SampleLevel(g_samplers[material.albedo_tex_sampler_id], uv, 0).xyzw;
                }

                normal = normalize(rotate(obj_to_world, normal));

                if (albedo.w > 0.1)
                {
#ifdef HSR_DEBUG
                    if (g_hsr_mask & HSR_FLAGS_VISUALIZE_TRANSPARENT_QUERY)
                        debug_value = float4(1.0, 0.0, 1.0, 1.0);
#endif  // HSR_DEBUG
                    RayGbuffer gbuffer;
                    gbuffer.material_id      = sinfo.material_id;
                    gbuffer.normal           = normal;
                    gbuffer.uv               = uv;
                    gbuffer.world_ray_length = transparent_query.CandidateTriangleRayT();
                    gbuffer.skip             = false;
                    packed_gbuffer           = PackGbuffer(gbuffer);
                    transparent_query.CommitNonOpaqueTriangleHit();
                }
            }
        }
    }
#endif  // HSR_TRANSPARENT_QUERY

    StoreRayGBuffer(ray_index, packed_gbuffer);

#endif  // USE_INLINE_RAYTRACING

#ifdef HSR_DEBUG
#ifdef USE_SSR
    if (g_hsr_mask & HSR_FLAGS_VISUALIZE_WAVES)
        debug_value = heatmapGradient(frac(float(group_id) / 100.0)).xyzz;

#else   // USE_SSR
    if (g_hsr_mask & HSR_FLAGS_VISUALIZE_WAVES)
        return;
#endif  // USE_SSR
    if ((g_hsr_mask & HSR_FLAGS_VISUALIZE_HIT_COUNTER) || (g_hsr_mask & HSR_FLAGS_VISUALIZE_TRANSPARENT_QUERY))
    {
        bool clear_debug = false;
        if (g_hsr_mask & HSR_FLAGS_VISUALIZE_TRANSPARENT_QUERY)
            clear_debug = true;
        g_rw_debug[coords] = (!clear_debug ? g_rw_debug[coords] : (0.0).xxxx) + debug_value;
        uint2 copy_target  = coords ^ 0b1;  // Flip last bit to find the mirrored coords along the x and y axis within a quad.
        {
            bool copy_horizontal;
            bool copy_vertical;
            bool copy_diagonal;
            UnpackRayCoords(packed_coords, coords, copy_horizontal, copy_vertical, copy_diagonal);
            if (copy_horizontal)
            {
                uint2 copy_coords       = uint2(copy_target.x, coords.y);
                g_rw_debug[copy_coords] = (!clear_debug ? g_rw_debug[copy_coords] : (0.0).xxxx) + debug_value;
            }
            if (copy_vertical)
            {
                uint2 copy_coords       = uint2(coords.x, copy_target.y);
                g_rw_debug[copy_coords] = (!clear_debug ? g_rw_debug[copy_coords] : (0.0).xxxx) + debug_value;
            }
            if (copy_diagonal)
            {
                uint2 copy_coords       = copy_target;
                g_rw_debug[copy_coords] = (!clear_debug ? g_rw_debug[copy_coords] : (0.0).xxxx) + debug_value;
            }
        }
    }
#endif  // HSR_DEBUG
}

    // Kernel for deferred shading of the results of ray traced intersection
    // Each thread loads a RayGBuffer and evaluates lights+shadows+IBL - application decides how to shade
    [numthreads(32, 1, 1)] void DeferredShade(uint group_index
                                              : SV_GroupIndex, uint group_id
                                              : SV_GroupID)
{
    uint ray_index     = group_id * 32 + group_index;
    uint packed_coords = g_rw_hw_ray_list.Load(sizeof(uint) * ray_index);
    int2 coords;
    bool copy_horizontal;
    bool copy_vertical;
    bool copy_diagonal;
    UnpackRayCoords(packed_coords, coords, copy_horizontal, copy_vertical, copy_diagonal);

    uint2  screen_size                     = uint2(g_frame_info.reflection_width, g_frame_info.reflection_height);
    float2 uv                              = float2(coords + 0.5) / float2(screen_size);
    float3 world_space_normal              = FFX_SSSR_LoadWorldSpaceNormal(coords);
    float  roughness                       = g_extracted_roughness.Load(int3(coords, 0)).x;
    float  z                               = FFX_SSSR_LoadDepth(coords, 0);
    float3 screen_uv_space_ray_origin      = float3(uv, z);
    float3 view_space_ray                  = ScreenSpaceToViewSpace(screen_uv_space_ray_origin, g_inv_proj);
    float3 view_space_surface_normal       = mul(g_view, float4(normalize(world_space_normal), 0)).xyz;
    float3 view_space_ray_direction        = normalize(view_space_ray);
    float3 view_space_reflected_direction  = SampleReflectionVector(view_space_ray_direction, view_space_surface_normal, roughness, coords);
    screen_uv_space_ray_origin             = ProjectPosition(view_space_ray, g_proj);
    float3 screen_space_ray_direction      = ProjectDirection(view_space_ray, view_space_reflected_direction, screen_uv_space_ray_origin, g_proj);
    float3 world_space_reflected_direction = normalize(mul(g_inv_view, float4(view_space_reflected_direction, 0)).xyz);
    float3 world_space_origin              = mul(g_inv_view, float4(view_space_ray, 1)).xyz;

    RayGbuffer gbuffer = UnpackGbuffer(LoadRayGBuffer(ray_index));

    if (gbuffer.skip)
    {
        // Fall back to the pre-filtered sample or a stochastic one
        float3 reflection_radiance = FFX_GetEnvironmentSample(world_space_origin, world_space_reflected_direction, 0.0);
        WriteRadiance(packed_coords, float4(reflection_radiance, FFX_REFLECTIONS_SKY_DISTANCE));
        return;
    }

    float3 world_space_hit = world_space_origin + world_space_reflected_direction * gbuffer.world_ray_length;
    float4 projected       = mul(g_proj, mul(g_view, float4(world_space_hit, 1))).xyzw;
    float2 hituv           = (projected.xy / projected.w) * float2(0.5, -0.5) + float2(0.5, 0.5);

    if ((g_hsr_mask & HSR_FLAGS_SHADING_USE_SCREEN) &&
        all(abs(projected.xy) < projected.w) && projected.w > 0.0 &&
        abs(FFX_DNSR_Reflections_GetLinearDepth(hituv, projected.z / projected.w) -
            FFX_DNSR_Reflections_GetLinearDepth(hituv, SampleDepth(hituv))) < g_depth_buffer_thickness)
    {
        float3 reflection_radiance = g_lit_scene_history.SampleLevel(g_linear_sampler, hituv - SampleMotionVector(hituv), 0.0).xyz;
        WriteRadiance(packed_coords, float4(reflection_radiance, gbuffer.world_ray_length));
    }
    else
    {
        Material_Info material = g_material_info.Load(gbuffer.material_id);
        float4        albedo = float4(material.albedo_factor_x, material.albedo_factor_y, material.albedo_factor_z, material.albedo_factor_w);
        if (material.albedo_tex_id >= 0)
        {
            albedo = albedo * g_textures[NonUniformResourceIndex(material.albedo_tex_id)].SampleLevel(g_samplers[material.albedo_tex_sampler_id], gbuffer.uv, 0).xyzw;
        }
        float4 arm = float4(1.0, material.arm_factor_y, material.arm_factor_z, 1.0);
        if (material.arm_tex_id >= 0)
        {
            arm = arm * g_textures[NonUniformResourceIndex(material.arm_tex_id)].SampleLevel(g_samplers[material.arm_tex_sampler_id], gbuffer.uv, 0).xyzw;
        }
        float4 emission = float4(material.emission_factor_x, material.emission_factor_y, material.emission_factor_z, 1.0);
        if (material.emission_tex_id >= 0)
        {
            emission = emission * g_textures[NonUniformResourceIndex(material.emission_tex_id)].SampleLevel(g_samplers[material.emission_tex_sampler_id], gbuffer.uv, 0).xyzw;
        }

        PBRPixelInfo pixelInfo;
        pixelInfo.pixelBaseColorAlpha = float4(albedo.xyz, 1.f);
        pixelInfo.pixelNormal = float4(gbuffer.normal, 1.f);
        pixelInfo.pixelAoRoughnessMetallic = arm.xyz;
        pixelInfo.pixelWorldPos = float4(world_space_hit, 1);
        pixelInfo.pixelCoordinates = float4(0, 0, 0, 0);

        float3 reflection_radiance = PBRLighting(pixelInfo, ShadowMapTextures);

        WriteRadiance(packed_coords, float4(reflection_radiance, gbuffer.world_ray_length));
    }
}
