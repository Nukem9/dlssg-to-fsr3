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

#if __cplusplus
    #pragma once
    #include "misc/math.h"
    #include <cstdint>
#else
    #include "shadercommon.h"
#endif // __cplusplus

#define TARGET(v)    CONCAT(SV_Target, v)

#define MAX_TEXTURES_COUNT            1000
#define MAX_SAMPLERS_COUNT            20
#define MAX_SHADOW_MAP_TEXTURES_COUNT 15

// Vertex Skinning
#ifndef MAX_NUM_BONES
    #define MAX_NUM_BONES 200
#endif

struct VertexStrides
{
#if __cplusplus
    uint32_t positionStride, normalStride, weights0Stride, joints0Stride;
    uint32_t numVertices; ///< number of vertices 
#else
    uint positionStride, normalStride, weights0Stride, joints0Stride;
    uint numVertices;
#endif
};

struct MatrixPair
{
#if __cplusplus
    Mat4 m_Current, m_Previous;
    void Set(const Mat4& matrix)
    {
        m_Previous = m_Current;
        m_Current  = matrix;
    }
#else
    matrix Current, Previous;
#endif
};

struct MaterialInformation
{
#if __cplusplus
    Vec4   EmissiveFactor;
    Vec4 AlbedoFactor{1.0f}; // Initializing factor value to 1.0f makes more scense than .0f and also less error-prone.

    // Metal-Rough / Spec-Gloss share the same info space for convenience
    Vec4 PBRParams;    // (Metallic, Roughness, x, x) - Metal-Rough
                       // (Specular.xyz, Glossiness) - Spec-Gloss
    float AlphaCutoff;
    float Padding[3];
#else
    float4 EmissiveFactor;
    float4 AlbedoFactor;
    float4 PBRParams;
    float  AlphaCutoff;
    float3 Padding;
#endif // __cplusplus
};

struct InstanceInformation
{
#if __cplusplus
    Mat4 WorldTransform;
    Mat4 PrevWorldTransform;
#else
    matrix WorldTransform;
    matrix PrevWorldTransform;
#endif // __cplusplus

    MaterialInformation MaterialInfo;
};

struct TextureIndices
{
#if __cplusplus
    int32_t     AlbedoTextureIndex = -1;
    int32_t     AlbedoSamplerIndex = -1;
    int32_t     MetalRoughSpecGlossTextureIndex = -1;
    int32_t     MetalRoughSpecGlossSamplerIndex = -1;

    int32_t     NormalTextureIndex = -1;
    int32_t     NormalSamplerIndex = -1;
    int32_t     EmissiveTextureIndex = -1;
    int32_t     EmissiveSamplerIndex = -1;

    int32_t     OcclusionTextureIndex = -1;
    int32_t     OcclusionSamplerIndex = -1;
    int32_t     Padding[2];
#else
    int         AlbedoTextureIndex;
    int         AlbedoSamplerIndex;
    int         MetalRoughSpecGlossTextureIndex;
    int         MetalRoughSpecGlossSamplerIndex;
    int         NormalTextureIndex;
    int         NormalSamplerIndex;
    int         EmissiveTextureIndex;
    int         EmissiveSamplerIndex;
    int         OcclusionTextureIndex;
    int         OcclusionSamplerIndex;
    int2        Padding;
#endif // __cplusplus
};

#ifndef __cplusplus
#include "rasterlightfunctions.hlsl"

//--------------------------------------------------------------------------------------
//  Remove texture references if the material doesn't have texture coordinates
//--------------------------------------------------------------------------------------

#ifndef HAS_TEXCOORD_0
    #if ID_normalTexCoord == 0
        #undef ID_normalTexCoord
    #endif

    #if ID_emissiveTexCoord == 0
        #undef ID_emissiveTexCoord
    #endif

    #if ID_occlusionTexCoord == 0
        #undef ID_occlusionTexCoord
    #endif

    #if ID_albedoTexCoord == 0
        #undef ID_albedoTexCoord
    #endif

    #if ID_metallicRoughnessTexCoord == 0
        #undef ID_metallicRoughnessTexCoord
    #endif
#endif // !HAS_TEXCOORD_0

#ifndef HAS_TEXCOORD_1
    #if ID_normalTexCoord == 1
        #undef ID_normalTexCoord
    #endif

    #if ID_emissiveTexCoord == 1
        #undef ID_emissiveTexCoord
    #endif

    #if ID_occlusionTexCoord == 1
        #undef ID_occlusionTexCoord
    #endif

    #if ID_albedoTexCoord == 1
        #undef ID_albedoTexCoord
    #endif

    #if ID_metallicRoughnessTexCoord == 1
        #undef ID_metallicRoughnessTexCoord
    #endif
#endif // !HAS_TEXCOORD_1

#if !defined(NO_WORLDPOS)
    #if !defined(HAS_TANGENT) || defined(ID_doublesided)
        #define HAS_WORLDPOS
    #endif // !HAS_TANGENT || ID_doublesided
#endif // !NO_WORLDPOS

// Helper for texture UV mapping
#define CONCAT(a,b) a ## b
#define TEXCOORD(id) CONCAT(Input.UV, id)

//////////////////////////////////////////////////////////////////////////
// Vertex-related helpers
//////////////////////////////////////////////////////////////////////////
struct VS_SURFACE_OUTPUT
{
    float4 Position : SV_POSITION;   // projected position

#ifdef HAS_WORLDPOS
    float3 WorldPos : WORLDPOS;      // world position
#endif // HAS_WORLDPOS

#ifdef HAS_NORMAL
    float3 Normal : NORMAL;
#endif // HAS_NORMAL

#ifdef HAS_TANGENT
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
#endif // HAS_TANGENT

#ifdef HAS_COLOR_0
    float4 Color0 : COLOR0;
#endif // HAS_COLOR_0

#ifdef HAS_TEXCOORD_0
    float2 UV0 : TEXCOORD0;    // vertex texture coords
#endif // HAS_TEXCOORD_0

#ifdef HAS_TEXCOORD_1
    float2 UV1 : TEXCOORD1;    // vertex texture coords
#endif // HAS_TEXCOORD_1

#ifdef HAS_MOTION_VECTORS
    float4 CurPosition  : TEXCOORD2; // current's frame vertex position
    float4 PrevPosition : TEXCOORD3; // previous' frame vertex position
#endif // HAS_MOTION_VECTORS

};

//////////////////////////////////////////////////////////////////////////
// Fetch helpers
//////////////////////////////////////////////////////////////////////////

float4 GetAlbedoTexture(VS_SURFACE_OUTPUT Input, TextureIndices Textures, Texture2D AllTextures[], SamplerState AllSamplers[], float MipLODBias)
{
    float2 uv = float2(0.0, 0.0);
#ifdef ID_albedoTexCoord
    uv = TEXCOORD(ID_albedoTexCoord);
#endif

#ifdef ID_albedoTexture
    return AllTextures[Textures.AlbedoTextureIndex].SampleBias(AllSamplers[Textures.AlbedoSamplerIndex], uv, MipLODBias);
#else
    return float4(1, 1, 1, 1);  // Opaque
#endif
}


float4 GetBaseColorAlpha(VS_SURFACE_OUTPUT Input, MaterialInformation MaterialInfo, TextureIndices Textures, Texture2D AllTextures[], SamplerState AllSamplers[], float MipLODBias)
{
    // Initialize the color using the texture if present
    float4 BaseColorAlpha = float4(1.0, 1.0, 1.0, 1.0);

#ifdef ID_albedoTexture
    BaseColorAlpha = GetAlbedoTexture(Input, Textures, AllTextures, AllSamplers, MipLODBias);
#endif  // ID_albedoTexture

    BaseColorAlpha *= MaterialInfo.AlbedoFactor;

    // Compute final albedo using passed in pixel color
    float4 PixelColor = float4(1.0, 1.0, 1.0, 1.0);
#ifdef HAS_COLOR_0
    PixelColor = Input.Color0;
#endif
    BaseColorAlpha *= PixelColor;

    return BaseColorAlpha;
}


float4 GetMetallicRoughnessTexture(VS_SURFACE_OUTPUT Input, TextureIndices Textures, Texture2D AllTextures[], SamplerState AllSamplers[], float MipLODBias)
{
    float2 uv = float2(0.0, 0.0);
#ifdef ID_metallicRoughnessTexCoord
    uv = TEXCOORD(ID_metallicRoughnessTexCoord);
#endif

#ifdef ID_metallicRoughnessTexture
    return AllTextures[Textures.MetalRoughSpecGlossTextureIndex].SampleBias(AllSamplers[Textures.MetalRoughSpecGlossSamplerIndex], uv, MipLODBias);
#else
    return float4(1, 1, 1, 1);
#endif
}

float4 GetSpecularGlossinessTexture(VS_SURFACE_OUTPUT Input, TextureIndices Textures, Texture2D AllTextures[], SamplerState AllSamplers[], float MipLODBias)
{
    float2 uv = float2(0.0, 0.0);
#ifdef ID_specularGlossinessTexCoord
    uv = TEXCOORD(ID_specularGlossinessTexCoord);
#endif

#ifdef ID_specularGlossinessTexture
    return AllTextures[Textures.MetalRoughSpecGlossTextureIndex].SampleBias(AllSamplers[Textures.MetalRoughSpecGlossSamplerIndex], uv, MipLODBias);
#else
    return float4(1, 1, 1, 1);
#endif
}

float3 GetAoRoughnessMetallic(VS_SURFACE_OUTPUT Input, MaterialInformation MaterialInfo, TextureIndices Textures, Texture2D AllTextures[], SamplerState AllSamplers[], float MipLODBias, float3 BaseColor)
{
    float4 AoRoughnessMetallic = float4(1.0, 1.0, 1.0, 1.0);

#if defined(MATERIAL_METALLICROUGHNESS)

    // AO is stored in the 'r' channel, roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
    AoRoughnessMetallic = GetMetallicRoughnessTexture(Input, Textures, AllTextures, AllSamplers, MipLODBias);
    AoRoughnessMetallic.g *= MaterialInfo.PBRParams.y;  // Material Roughness
    AoRoughnessMetallic.b *= MaterialInfo.PBRParams.x;  // Material Metallic

#elif defined(MATERIAL_SPECULARGLOSSINESS) // ! MATERIAL_METALLICROUGHNESS

    float4 SpecGloss = GetSpecularGlossinessTexture(Input, Textures, AllTextures, AllSamplers, MipLODBias);
    AoRoughnessMetallic.g = (1.0 - SpecGloss.a * MaterialInfo.PBRParams.w);  // Material Glossiness to roughness

    float3 F0 = SpecGloss.rgb * MaterialInfo.PBRParams.xyz;      // Material Specular
    float OneMinusSpecularStrength = 1.0 - max(max(F0.r, F0.g), F0.b);
    AoRoughnessMetallic.b = SolveMetallic(BaseColor, F0, OneMinusSpecularStrength);
#endif  // ! MATERIAL_SPECULARGLOSSINESS

    return AoRoughnessMetallic.rgb;
}

void GetPBRParams(VS_SURFACE_OUTPUT   Input,
                  MaterialInformation MaterialInfo,
                  out float4          AlbedoAlphaOut,
                  out float3          AoRoughnessMetallicOut,
                  TextureIndices      Textures,
                  Texture2D           AllTextures[],
                  SamplerState        AllSamplers[],
                  float MipLODBias)
{
    AlbedoAlphaOut = GetBaseColorAlpha(Input, MaterialInfo, Textures, AllTextures, AllSamplers, MipLODBias);
    AoRoughnessMetallicOut = GetAoRoughnessMetallic(Input, MaterialInfo, Textures, AllTextures, AllSamplers, MipLODBias, AlbedoAlphaOut.rgb);
}

float3 GetNormalTexture(VS_SURFACE_OUTPUT Input, TextureIndices Textures, SceneInformation SceneInfo, Texture2D AllTextures[], SamplerState AllSamplers[], float MipLODBias)
{
    float2 uv = float2(0.0, 0.0);
#ifdef ID_normalTexCoord
    uv = TEXCOORD(ID_normalTexCoord);
#endif

#ifdef ID_normalTexture
    const float2 xy = 2.0 * AllTextures[Textures.NormalTextureIndex].SampleBias(AllSamplers[Textures.NormalSamplerIndex], uv, MipLODBias).rg - 1.0;
    const float  z  = sqrt(1.0f - dot(xy, xy));
    return float3(xy, z);
#else
    return float3(0, 0, 0);
#endif
}

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
float3 GetPixelNormal(VS_SURFACE_OUTPUT Input, TextureIndices Textures, SceneInformation SceneInfo, Texture2D AllTextures[], SamplerState AllSamplers[], float MipLODBias, bool isFrontFace)
{
    // Retrieve the tangent space matrix
#ifndef HAS_TANGENT
    float2 uv = float2(0.0, 0.0);
#ifdef ID_normalTexCoord
    uv = TEXCOORD(ID_normalTexCoord);
#endif

#ifdef HAS_WORLDPOS
    float3 pos_dx = ddx(Input.WorldPos);
    float3 pos_dy = ddy(Input.WorldPos);
#else
    // This function shouldn't be used if HAS_WORLDPOS isn't defined.
    // added default values to enable compilation
    float3 pos_dx = float3(1.0, 0.0, 0.0);
    float3 pos_dy = float3(0.0, 1.0, 0.0);
#endif

    float3 tex_dx = ddx(float3(uv, 0.0));
    float3 tex_dy = ddy(float3(uv, 0.0));
    float3 t      = (tex_dy.y * pos_dx - tex_dx.y * pos_dy) / (tex_dx.x * tex_dy.y - tex_dy.x * tex_dx.y);

#ifdef HAS_NORMAL
    float3 ng = normalize(Input.Normal);
#else
    float3 ng = cross(pos_dx, pos_dy);
#endif

    t            = normalize(t - ng * dot(ng, t));
    float3   b   = normalize(cross(ng, t));
    float3x3 tbn = float3x3(t, b, ng);

#else
    float3x3 tbn = float3x3(Input.Tangent, Input.Binormal, Input.Normal);
#endif  // !HAS_TANGENT

#ifdef ID_normalTexture
    float3 n = GetNormalTexture(Input, Textures, SceneInfo, AllTextures, AllSamplers, MipLODBias);
    n        = normalize(mul(transpose(tbn), n));
#else
    // The tbn matrix is linearly interpolated, so we need to re-normalize
    float3 n = normalize(tbn[2].xyz);
#endif

    // If this material has doubleside property and is facing away from the camera, invert the normal
#ifdef ID_doublesided
    float3 view = SceneInfo.CameraInfo.CameraPos.xyz - Input.WorldPos;
    if (isFrontFace) //Flip based on facing.
        n = -n;
#endif  // ID_doublesided

    return n;
}

float3 CompressNormals(const float3 normal)
{
    return normal * 0.5 + 0.5;
}

float3 DeCompressNormals(const float3 normal)
{
    return 2 * normal - 1;
}

#endif // !__cplusplus
