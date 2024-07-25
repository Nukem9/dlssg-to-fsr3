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

#include "commonintersect.hlsl"


#define GOLDEN_RATIO 1.61803398875f
#define FFX_REFLECTIONS_SKY_DISTANCE 100.0f

// Helper defines for hitcouter and classification
#define FFX_HITCOUNTER_SW_HIT_FLAG (1u << 0u)
#define FFX_HITCOUNTER_SW_HIT_SHIFT 0u
#define FFX_HITCOUNTER_SW_OLD_HIT_SHIFT 8u
#define FFX_HITCOUNTER_MASK 0xffu
#define FFX_HITCOUNTER_SW_MISS_FLAG (1u << 16u)
#define FFX_HITCOUNTER_SW_MISS_SHIFT 16u
#define FFX_HITCOUNTER_SW_OLD_MISS_SHIFT 24u

#define FFX_Hitcounter_GetSWHits(counter) ((counter >> FFX_HITCOUNTER_SW_HIT_SHIFT) & FFX_HITCOUNTER_MASK)
#define FFX_Hitcounter_GetSWMisses(counter) ((counter >> FFX_HITCOUNTER_SW_MISS_SHIFT) & FFX_HITCOUNTER_MASK)
#define FFX_Hitcounter_GetOldSWHits(counter) ((counter >> FFX_HITCOUNTER_SW_OLD_HIT_SHIFT) & FFX_HITCOUNTER_MASK)
#define FFX_Hitcounter_GetOldSWMisses(counter) ((counter >> FFX_HITCOUNTER_SW_OLD_MISS_SHIFT) & FFX_HITCOUNTER_MASK)

//=== Common functions of the HSRSample ===

uint PackFloat16(min16float2 v) {
    uint2 p = f32tof16(float2(v));
    return p.x | (p.y << 16);
}

min16float2 UnpackFloat16(uint a) {
    float2 tmp = f16tof32(uint2(a & 0xFFFF, a >> 16));
    return min16float2(tmp);
}

uint PackRayCoords(uint2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal) {
    uint ray_x_15bit          = ray_coord.x & 0b111111111111111;
    uint ray_y_14bit          = ray_coord.y & 0b11111111111111;
    uint copy_horizontal_1bit = copy_horizontal ? 1 : 0;
    uint copy_vertical_1bit   = copy_vertical ? 1 : 0;
    uint copy_diagonal_1bit   = copy_diagonal ? 1 : 0;

    uint packed = (copy_diagonal_1bit << 31) | (copy_vertical_1bit << 30) | (copy_horizontal_1bit << 29) | (ray_y_14bit << 15) | (ray_x_15bit << 0);
    return packed;
}

void UnpackRayCoords(uint packed, out uint2 ray_coord, out bool copy_horizontal, out bool copy_vertical, out bool copy_diagonal) {
    ray_coord.x     = (packed >> 0) & 0b111111111111111;
    ray_coord.y     = (packed >> 15) & 0b11111111111111;
    copy_horizontal = (packed >> 29) & 0b1;
    copy_vertical   = (packed >> 30) & 0b1;
    copy_diagonal   = (packed >> 31) & 0b1;
}


//=== FFX_DNSR_Reflections_ override functions ===


bool FFX_DNSR_Reflections_IsGlossyReflection(float roughness) { return roughness < g_roughness_threshold; }

bool FFX_DNSR_Reflections_IsMirrorReflection(float roughness) { return roughness < 0.0001; }

float2 OctahedronUV(float3 N) {
    N.xy /= dot((1.0f).xxx, abs(N));
    if (N.z <= 0.0f) N.xy = (1.0f - abs(N.yx)) * (N.xy >= 0.0f ? (1.0f).xx : (-1.0f).xx);
    return N.xy * 0.5f + 0.5f;
}

float3 UVtoOctahedron(float2 UV) {
    UV       = 2.0f * (UV - 0.5f);
    float3 N = float3(UV, 1.0f - dot((1.0f).xx, abs(UV)));
    float  t = max(-N.z, 0.0f);
    N.xy += N.xy >= 0.0f ? -t : t;
    return normalize(N);
}

#define GOLDEN_RATIO 1.61803398875f

float2 SampleRandomVector2DBaked(uint2 pixel) {
    int2   coord = int2(pixel.x & 127u, pixel.y & 127u);
    float2 xi    = g_random_number_image[coord].xy;
    float2 u     = float2(fmod(xi.x + (((int)(pixel.x / 128)) & 0xFFu) * GOLDEN_RATIO, 1.0f), fmod(xi.y + (((int)(pixel.y / 128)) & 0xFFu) * GOLDEN_RATIO, 1.0f));
    return u;
}

float FFX_DNSR_Reflections_GetLinearDepth(float2 uv, float depth) {
    const float3 view_space_pos = InvProjectPosition(float3(uv, depth), g_inv_proj);
    return abs(view_space_pos.z);
}

bool FFX_DNSR_Reflections_IsBackground(float depth) 
{
#if FFX_SSSR_OPTION_INVERTED_DEPTH
    return depth < 1.e-6f;
#else
    return depth >= (1.0f - 1.e-6f);
#endif
}
float3 SampleEnvironmentMap(float3 direction, float preceptualRoughness)
{
    float Width; float Height;
    g_atmosphere_lut.GetDimensions(Width, Height);
    int maxMipLevel = int(log2(float(Width > 0 ? Width : 1)));
    float mip = clamp(preceptualRoughness * float(maxMipLevel), 0.0, float(maxMipLevel));
    return g_atmosphere_lut.SampleLevel(g_linear_sampler, direction, mip).xyz * g_iblFactor;
}
float3 FFX_GetEnvironmentSample(float3 world_space_position, float3 world_space_reflected_direction, float roughness) {
    return SampleEnvironmentMap(world_space_reflected_direction, sqrt(roughness));
}

#include "sample_ggx_vndf.h"

float3 Sample_GGX_VNDF_Ellipsoid(float3 Ve, float alpha_x, float alpha_y, float U1, float U2) { return SampleGGXVNDF(Ve, alpha_x, alpha_y, U1, U2); }

float3 Sample_GGX_VNDF_Hemisphere(float3 Ve, float alpha, float U1, float U2) { return Sample_GGX_VNDF_Ellipsoid(Ve, alpha, alpha, U1, U2); }

float3x3 CreateTBN(float3 N) {
    float3 U;
    if (abs(N.z) > 0.0) {
        float k = sqrt(N.y * N.y + N.z * N.z);
        U.x     = 0.0;
        U.y     = -N.z / k;
        U.z     = N.y / k;
    } else {
        float k = sqrt(N.x * N.x + N.y * N.y);
        U.x     = N.y / k;
        U.y     = -N.x / k;
        U.z     = 0.0;
    }

    float3x3 TBN;
    TBN[0] = U;
    TBN[1] = cross(N, U);
    TBN[2] = N;
    return transpose(TBN);
}


float3 SampleReflectionVector(float3 view_direction, float3 normal, float roughness, int2 dispatch_thread_id) {
    if (roughness < 0.001f) {
        return reflect(view_direction, normal);
    }
    float3x3 tbn_transform = CreateTBN(normal);
    float3   view_direction_tbn = mul(-view_direction, tbn_transform);
    float2   u = SampleRandomVector2DBaked(dispatch_thread_id);
    float3   sampled_normal_tbn = Sample_GGX_VNDF_Hemisphere(view_direction_tbn, roughness, u.x, u.y);
#ifdef PERFECT_REFLECTIONS
    sampled_normal_tbn = float3(0, 0, 1); // Overwrite normal sample to produce perfect reflection.
#endif
    float3 reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);
    // Transform reflected_direction back to the initial space.
    float3x3 inv_tbn_transform = transpose(tbn_transform);
    return mul(reflected_direction_tbn, inv_tbn_transform);
}

void FFX_Fetch_Face_Indices_U32(out uint3 face3, in uint offset, in uint triangle_id)
{
    face3[0] = g_index_buffer[NonUniformResourceIndex(offset)].Load(3 * triangle_id);
    face3[1] = g_index_buffer[NonUniformResourceIndex(offset)].Load(3 * triangle_id + 1);
    face3[2] = g_index_buffer[NonUniformResourceIndex(offset)].Load(3 * triangle_id + 2);
}

void FFX_Fetch_Face_Indices_U16(out uint3 face3, in uint offset, in uint triangle_id)
{
    uint word_id_0 = triangle_id * 3 + 0;
    uint dword_id_0 = word_id_0 / 2;
    uint shift_0 = 16 * (word_id_0 & 1);

    uint word_id_1 = triangle_id * 3 + 1;
    uint dword_id_1 = word_id_1 / 2;
    uint shift_1 = 16 * (word_id_1 & 1);

    uint word_id_2 = triangle_id * 3 + 2;
    uint dword_id_2 = word_id_2 / 2;
    uint shift_2 = 16 * (word_id_2 & 1);

    uint u0 = g_index_buffer[NonUniformResourceIndex(offset)].Load(dword_id_0);
    u0 = (u0 >> shift_0) & 0xffffu;
    uint u1 = g_index_buffer[NonUniformResourceIndex(offset)].Load(dword_id_1);
    u1 = (u1 >> shift_1) & 0xffffu;
    uint u2 = g_index_buffer[NonUniformResourceIndex(offset)].Load(dword_id_2);
    u2 = (u2 >> shift_0) & 0xffffu;
    face3 = uint3(u0, u1, u2);

}

float2 FFX_Fetch_float2(in int offset, in int vertex_id)
{
    float2 data;
    data[0] = g_vertex_buffer[NonUniformResourceIndex(offset)].Load(2 * vertex_id);
    data[1] = g_vertex_buffer[NonUniformResourceIndex(offset)].Load(2 * vertex_id + 1);

    return data;
}
float3 FFX_Fetch_float3(in int offset, in int vertex_id)
{
    float3 data;
    data[0] = g_vertex_buffer[NonUniformResourceIndex(offset)].Load(3 * vertex_id);
    data[1] = g_vertex_buffer[NonUniformResourceIndex(offset)].Load(3 * vertex_id + 1);
    data[2] = g_vertex_buffer[NonUniformResourceIndex(offset)].Load(3 * vertex_id + 2);

    return data;
}
float4 FFX_Fetch_float4(in int offset, in int vertex_id)
{
    float4 data;
    data[0] = g_vertex_buffer[NonUniformResourceIndex(offset)].Load(4 * vertex_id);
    data[1] = g_vertex_buffer[NonUniformResourceIndex(offset)].Load(4 * vertex_id + 1);
    data[2] = g_vertex_buffer[NonUniformResourceIndex(offset)].Load(4 * vertex_id + 2);
    data[3] = g_vertex_buffer[NonUniformResourceIndex(offset)].Load(4 * vertex_id + 3);

    return data;
}

void   FFX_Fetch_Local_Basis(in Surface_Info sinfo, in uint3 face3, in float2 bary, out float2 uv, out float3 normal, out float4 tangent) {
    float3 normal0 = FFX_Fetch_float3(sinfo.normal_attribute_offset, face3.x);
    float3 normal1 = FFX_Fetch_float3(sinfo.normal_attribute_offset, face3.y);
    float3 normal2 = FFX_Fetch_float3(sinfo.normal_attribute_offset, face3.z);
    normal = normal1 * bary.x + normal2 * bary.y + normal0 * (1.0 - bary.x - bary.y);

    if (sinfo.tangent_attribute_offset >= 0) {
        float4 tangent0 = FFX_Fetch_float4(sinfo.tangent_attribute_offset, face3.x);
        float4 tangent1 = FFX_Fetch_float4(sinfo.tangent_attribute_offset, face3.y);
        float4 tangent2 = FFX_Fetch_float4(sinfo.tangent_attribute_offset, face3.z);
        tangent = tangent1 * bary.x + tangent2 * bary.y + tangent0 * (1.0 - bary.x - bary.y);
        // tangent.xyz     = normalize(tangent.xyz);
    }
    if (sinfo.texcoord0_attribute_offset >= 0) {
        float2 uv0 = FFX_Fetch_float2(sinfo.texcoord0_attribute_offset, face3.x);
        float2 uv1 = FFX_Fetch_float2(sinfo.texcoord0_attribute_offset, face3.y);
        float2 uv2 = FFX_Fetch_float2(sinfo.texcoord0_attribute_offset, face3.z);
        uv = uv1 * bary.x + uv2 * bary.y + uv0 * (1.0 - bary.x - bary.y);
    }
}
void FFX_Fetch_Local_Basis(in Surface_Info sinfo, in uint3 face3, in float2 bary, out float2 uv, out float3 normal) {
    float3 normal0 = FFX_Fetch_float3(sinfo.normal_attribute_offset, face3.x);
    float3 normal1 = FFX_Fetch_float3(sinfo.normal_attribute_offset, face3.y);
    float3 normal2 = FFX_Fetch_float3(sinfo.normal_attribute_offset, face3.z);
    normal = normal1 * bary.x + normal2 * bary.y + normal0 * (1.0 - bary.x - bary.y);

    if (sinfo.texcoord0_attribute_offset >= 0) {
        float2 uv0 = FFX_Fetch_float2(sinfo.texcoord0_attribute_offset, face3.x);
        float2 uv1 = FFX_Fetch_float2(sinfo.texcoord0_attribute_offset, face3.y);
        float2 uv2 = FFX_Fetch_float2(sinfo.texcoord0_attribute_offset, face3.z);
        uv = uv1 * bary.x + uv2 * bary.y + uv0 * (1.0 - bary.x - bary.y);
    }
}

float3 rotate(float4x4 mat, float3 v) { return mul(float3x3(mat[0].xyz, mat[1].xyz, mat[2].xyz), v); }
float3 rotate(float3x4 mat, float3 v) { return mul(float3x3(mat[0].xyz, mat[1].xyz, mat[2].xyz), v); }
float3 rotate(float3x3 mat, float3 v) { return mul(float3x3(mat[0].xyz, mat[1].xyz, mat[2].xyz), v); }
