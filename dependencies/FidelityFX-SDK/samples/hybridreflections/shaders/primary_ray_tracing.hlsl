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

SamplerState samBRDF : register(s0);
SamplerState samIrradianceCube : register(s1);
SamplerState samPrefilteredCube : register(s2);
SamplerComparisonState SamShadow : register(s3);
SamplerState g_samplers[] : DECLARE_SAMPLER(SAMPLER_BEGIN_SLOT);

Texture2D<float4>               g_gbuffer_depth : register(t0);
Texture2D<float4>               g_gbuffer_normal : register(t1);
RaytracingAccelerationStructure g_global : register(t2);
Texture2D<float4>               g_brdf_lut : register(t3);
TextureCube                     g_atmosphere_lut : register(t4);
TextureCube                     g_atmosphere_dif : register(t5);
Texture2D<float4>               g_random_number_image : register(t6);
StructuredBuffer<Material_Info> g_material_info : DECLARE_SRV(RAYTRACING_INFO_MATERIAL);
StructuredBuffer<Instance_Info> g_instance_info : DECLARE_SRV(RAYTRACING_INFO_INSTANCE);
StructuredBuffer<uint>          g_surface_id : DECLARE_SRV(RAYTRACING_INFO_SURFACE_ID);
StructuredBuffer<Surface_Info>  g_surface_info : DECLARE_SRV(RAYTRACING_INFO_SURFACE);

Texture2D                       g_textures[200] : DECLARE_SRV(TEXTURE_BEGIN_SLOT);
Texture2D    ShadowMapTextures[MAX_SHADOW_MAP_TEXTURES_COUNT] : DECLARE_SRV(SHADOW_MAP_BEGIN_SLOT);

StructuredBuffer<uint>  g_index_buffer[MAX_BUFFER_COUNT] : DECLARE_SRV(INDEX_BUFFER_BEGIN_SLOT);
StructuredBuffer<float> g_vertex_buffer[] : DECLARE_SRV(VERTEX_BUFFER_BEGIN_SLOT);

RWTexture2D<float4> g_rw_debug : register(u0);


ConstantBuffer<FrameInfo> g_frame_info_cb[1] : register(b0);
cbuffer CBSceneInformation : register(b1)
{
    SceneInformation SceneInfo;
};

cbuffer CBLightInformation : register(b2)
{
    SceneLightingInformation LightInfo;
}

#define brdfTexture g_brdf_lut
#define irradianceCube g_atmosphere_dif
#define prefilteredCube g_atmosphere_lut
#define myPerFrame g_frame_info.perFrame
#define g_linear_sampler samPrefilteredCube

#include "common.hlsl"

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

struct LocalBasis {
    float2 uv;
    float2 uv_ddx;
    float2 uv_ddy;
    float3 normal;
    float4 tangent;
};

void GetPBRPixelInfoFromMaterial(out PBRPixelInfo pixelInfo, in LocalBasis local_basis, in Material_Info material)
{
    float3 normal = normalize(local_basis.normal);
    float3 tangent = local_basis.tangent.xyz;
    if (material.normal_tex_id >= 0 && any(abs(tangent.xyz) > 0.0f))
    {
        tangent = normalize(tangent);
        float3 binormal = normalize(cross(normal, tangent) * local_basis.tangent.w);
        float3 normal_rgb = float3(0.0f, 0.0f, 1.0f);
        normal_rgb = g_textures[NonUniformResourceIndex(material.normal_tex_id)].SampleLevel(g_samplers[material.normal_tex_sampler_id], local_basis.uv, 2.0)
            .rgb;
        normal_rgb.z = sqrt(saturate(1.0 - normal_rgb.r * normal_rgb.r - normal_rgb.g * normal_rgb.g));
        normal = normalize(normal_rgb.z * normal + (2.0f * normal_rgb.x - 1.0f) * tangent + (2.0f * normal_rgb.y - 1.0f) * binormal);
    }
    pixelInfo.pixelBaseColorAlpha = float4(material.albedo_factor_x, material.albedo_factor_y, material.albedo_factor_z, material.albedo_factor_w);
    if (material.albedo_tex_id >= 0)
    {
        pixelInfo.pixelBaseColorAlpha = pixelInfo.pixelBaseColorAlpha * g_textures[NonUniformResourceIndex(material.albedo_tex_id)].SampleLevel(g_samplers[material.albedo_tex_sampler_id], local_basis.uv, 2.0);
    }
    float3 arm = float3(1.0f, material.arm_factor_y, material.arm_factor_z);
    if (material.arm_tex_id >= 0)
    {
        arm = arm * g_textures[NonUniformResourceIndex(material.arm_tex_id)].SampleLevel(g_samplers[material.arm_tex_sampler_id], local_basis.uv, 2.0)
            .xyz;
    }
    float3 emission = (0.0f).xxx;
    if (material.emission_tex_id >= 0)
    {
        emission =  // material.emission_factor *
            g_textures[NonUniformResourceIndex(material.emission_tex_id)].SampleLevel(g_samplers[material.emission_tex_sampler_id], local_basis.uv, 2.0)
            .xyz;
    }

    pixelInfo.pixelNormal = float4(normal, 1.f);
    pixelInfo.pixelAoRoughnessMetallic = float3(arm.r, saturate(arm.g), arm.b);
}

[numthreads(8, 8, 1)] void main(uint3 tid
    : SV_DispatchThreadID) {
    uint width, height;
    g_rw_debug.GetDimensions(width, height);
    if (tid.x >= width || tid.y >= height) return;

    float2 screen_uv = (float2(tid.xy) + float2(0.5f, 0.5f)) / float2(width, height);

    float  z = g_gbuffer_depth.Load(int3(tid.xy, 0)).x;
    float3 screen_uv_space_ray_origin = float3(screen_uv, z);
    float3 view_space_hit = ScreenSpaceToViewSpace(screen_uv_space_ray_origin, g_inv_proj);
    float3 world_space_hit = mul(g_inv_view, float4(view_space_hit, 1)).xyz;
    float3 camera_position = ViewSpaceToWorldSpace(float4(0.0f, 0.0f, 0.0f, 1.0f), g_inv_view);

    float3 wnormal = normalize(g_gbuffer_normal.Load(int3(tid.xy, 0)).xyz * 2.0f - (1.0f).xxx);
    float3 ve = ScreenSpaceToViewSpace(float3(screen_uv, z), g_inv_proj);
    float3 vo = float3(0.0f, 0.0f, 0.0f);
    float3 wo = mul(g_inv_view, float4(vo, 1.0f)).xyz;
    float3 we = mul(g_inv_view, float4(ve, 1.0f)).xyz;
    float3 wdir = normalize(we - wo);
    float3 reflected = -reflect(wdir, wnormal);

    bool   hit = false;
    float2 bary = float2(0.0, 0.0);
    RayQuery<RAY_FLAG_CULL_NON_OPAQUE              //
        | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES //
    >
        q;
    RayDesc ray;
    ray.Origin = wo;
    ray.Direction = wdir;
    ray.TMin = 0.1f;
    ray.TMax = 1000.0f;
    q.TraceRayInline(g_global, 0, 0xff, ray);

    q.Proceed();
    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        bary = q.CommittedTriangleBarycentrics();
        hit = true;

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

        obj_to_world = float3x3(q.CommittedObjectToWorld3x4()[0].xyz, q.CommittedObjectToWorld3x4()[1].xyz, q.CommittedObjectToWorld3x4()[2].xyz);
        bary = q.CommittedTriangleBarycentrics();
        instance_id = q.CommittedInstanceID();
        geometry_id = q.CommittedGeometryIndex();
        triangle_id = q.CommittedPrimitiveIndex();

        // bool opaque = true;
        iinfo = g_instance_info.Load(instance_id);
        surface_id = g_surface_id.Load((iinfo.surface_id_table_offset + geometry_id));
        sinfo = g_surface_info.Load(surface_id);
        material = g_material_info.Load(sinfo.material_id);

        if (sinfo.index_type == SURFACE_INFO_INDEX_TYPE_U16) {
            FFX_Fetch_Face_Indices_U16(/* out */ face3, /* in */ sinfo.index_offset, /* in */ triangle_id);
        }
        else { // SURFACE_INFO_INDEX_TYPE_U32
            FFX_Fetch_Face_Indices_U32(/* out */ face3, /* in */ sinfo.index_offset, /* in */ triangle_id);
        }

        float3 normal;
        float4 tangent;
        float2 uv;
        // Fetch interpolated local geometry basis
        FFX_Fetch_Local_Basis(/* in */ sinfo, /* in */ face3, /* in */ bary, /* out */ uv, /* out */ normal, /* out */ tangent);

        normal = normalize(rotate(obj_to_world, normal));
        tangent.xyz = normalize(rotate(obj_to_world, tangent.xyz));

        PBRPixelInfo pixelInfo;
        LocalBasis lbasis;
        lbasis.uv = uv;
        lbasis.uv_ddx = 1.0e-3f;
        lbasis.uv_ddy = 1.0e-3f;
        lbasis.normal = normal;
        lbasis.tangent = tangent;

        GetPBRPixelInfoFromMaterial(pixelInfo, lbasis, material);
        pixelInfo.pixelWorldPos = float4(ray.Origin + ray.Direction * q.CommittedRayT(), 1);
        pixelInfo.pixelCoordinates = float4(0, 0, 0, 0);

        float3 reflection_radiance = PBRLighting(pixelInfo, ShadowMapTextures);

        float4 emission = float4(material.emission_factor_x, material.emission_factor_y, material.emission_factor_z, 1.0);
        if (material.emission_tex_id >= 0)
        {
            emission = emission * g_textures[NonUniformResourceIndex(material.emission_tex_id)].SampleLevel(g_samplers[material.emission_tex_sampler_id], uv, 0).xyzw;
        }

        reflection_radiance += emission.xyz * g_emissiveFactor;

        g_rw_debug[tid.xy] = float4(reflection_radiance, 1.0f);
        return;
    }

    g_rw_debug[tid.xy] = float4(0.0, 0.0, 0.0, 1.0f);
}
