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

#ifndef SHADERCOMMON_H
#define SHADERCOMMON_H

#ifdef __cplusplus
    #include "misc/math.h"
#endif // __cplusplus

// NOTE: Make sure this is always packed to 16 bytes
struct CameraInformation
{
#ifdef __cplusplus
    Mat4   ViewMatrix;
    Mat4   ProjectionMatrix;
    Mat4   ViewProjectionMatrix;
    Mat4   InvViewMatrix;
    Mat4   InvProjectionMatrix;
    Mat4   InvViewProjectionMatrix;
    Mat4   PrevViewMatrix;
    Mat4   PrevViewProjectionMatrix;
    Vec4   CameraPos;
    Vec2   CurrJitter;
    Vec2   PrevJitter;
#else
    // HLSL
    matrix   ViewMatrix;
    matrix   ProjectionMatrix;
    matrix   ViewProjectionMatrix;
    matrix   InvViewMatrix;
    matrix   InvProjectionMatrix;
    matrix   InvViewProjectionMatrix;
    matrix   PrevViewMatrix;
    matrix   PrevViewProjectionMatrix;
    float4   CameraPos;
    float2   CurrJitter;
    float2   PrevJitter;
#endif // _cplusplus
};

#define MAX_CASCADES_COUNT 4  // Max of 4 cascades
struct LightInformation
{
#ifdef __cplusplus
    Mat4   LightViewProj[MAX_CASCADES_COUNT];
    Vec4   DirectionRange;     // (Direction + Range)
    Vec4   ColorIntensity;     // (Color + Intensity)
    Vec4   PosDepthBias;       // (Position + InnerConeCosine)
    float  InnerConeCos;
    float  OuterConeCos;
    int    Type;
    int    NumCascades;
    int    ShadowMapIndex[MAX_CASCADES_COUNT];
    Vec4   ShadowMapTransformation[MAX_CASCADES_COUNT];  // scale.xy, offset.zw to get the position of the sample
#else
    // HLSL
    matrix          LightViewProj[MAX_CASCADES_COUNT];
    float4          DirectionRange;     // (Direction + Range)
    float4          ColorIntensity;     // (Color + Intensity)
    float4          PosDepthBias;       // (Position + Depth Bias)
    float           InnerConeCos;
    float           OuterConeCos;
    int             Type;
    int             NumCascades;
    int             ShadowMapIndex0;
    int             ShadowMapIndex1;
    int             ShadowMapIndex2;
    int             ShadowMapIndex3;
    float4          ShadowMapTransformation[MAX_CASCADES_COUNT];
#endif // _cplusplus
};


#define MAX_LIGHT_COUNT         128 // Increase as needed
struct SceneLightingInformation
{
    LightInformation    LightInfo[MAX_LIGHT_COUNT];
    int                 LightCount;
    int                 bUseScreenSpaceShadowMap;
    int                 Padding[2];
};

struct UpscalerInformation
{
#if __cplusplus
    Vec4 FullScreenScaleRatio;
#else
    float4          FullScreenScaleRatio;
#endif  // __cplusplus
};

struct SceneInformation
{
    // Current camera info
    CameraInformation   CameraInfo;
    UpscalerInformation UpscalerInfo;
    float               MipLODBias;
    float               Padding[3];
};

enum class DisplayMode
{
    DISPLAYMODE_LDR,
    DISPLAYMODE_HDR10_2084,
    DISPLAYMODE_HDR10_SCRGB,
    DISPLAYMODE_FSHDR_2084,
    DISPLAYMODE_FSHDR_SCRGB
};

struct SwapchainCBData
{
    DisplayMode displayMode;
};


#if !__cplusplus
struct MaterialInfo
{
    float3 baseColor;           // albedo for dielectrics, specular color for metals
    float  perceptualRoughness; // roughness value, as authored by the model creator (input to shader)

    float3 reflectance0;        // specular reflectance at normal incidence
    float  alphaRoughness;      // roughness mapped to a more linear change in the roughness (proposed by [2])

    float3 reflectance90;       // specular reflectance at grazing incidence
    float  metallic;            // whether a material is a metal or a dielectric
};
#endif

#endif // SHADERCOMMON_H
