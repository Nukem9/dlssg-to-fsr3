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

#include "fullscreen.hlsl"

#include "surfacerendercommon.h"

//////////////////////////////////////////////////////////////////////////
// Texture definitions
RWTexture2D<float4> ColorTarget : register(u0);

Texture2D    BaseColorAlphaTexture                            : register(t0);
Texture2D    NormalTexture                                    : register(t1);
Texture2D    AoRoughnessMetallicTexture                       : register(t2);
Texture2D    DepthTexture                                     : register(t3);


SamplerState SamPointClamp : register(s0);
SamplerComparisonState SamShadow : register(s1);

//////////////////////////////////////////////////////////////////////////
// SceneInformation, contains scene data for current frame
// Fullscreen.hlsl binds to b0, so contants need to start at b1 if using fullscreen.hlsl
cbuffer CBSceneInformation : register(b0)
{
    SceneInformation SceneInfo;
};

cbuffer CBLightInformation : register(b1)
{
    SceneLightingInformation LightInfo;
}


Texture2D    brdfTexture                                      : register(t4);
TextureCube  irradianceCube                                   : register(t5);
TextureCube  prefilteredCube                                  : register(t6);

SamplerState samBRDF                                          : register(s2);
SamplerState samIrradianceCube                                : register(s3);
SamplerState samPrefilteredCube                               : register(s4);

// ------------------------------------------------------
// IBL --------------------------------------------------
// ------------------------------------------------------
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

#define IBL_INDEX b2
#include "lightingcommon.h"
Texture2D ShadowMapTextures[MAX_SHADOW_MAP_TEXTURES_COUNT] : register(t7);


//--------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------

// Rescales the UV of a fullscreen quad
// Use this in the pixel shader to correctly sample the fullscreen texture
float2 RescaleUVForUpscaler(float2 uv)
{
    return uv / SceneInfo.UpscalerInfo.FullScreenScaleRatio.zw;
}

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------
[numthreads(NUM_THREAD_X, NUM_THREAD_Y, 1)]
void MainCS(uint3 dtID : SV_DispatchThreadID)
{
    // Will return 0-1 based on pixel pos and current resolution setting
    float2 uv = GetUV(dtID.xy, 1.f / SceneInfo.UpscalerInfo.FullScreenScaleRatio.xy);

    // Account for upscaler
    uv *= SceneInfo.UpscalerInfo.FullScreenScaleRatio.zw;

    // Gather PBR info
    PBRPixelInfo pixelInfo;

    // BaseColor + Alpha
    pixelInfo.pixelBaseColorAlpha = BaseColorAlphaTexture[dtID.xy];

    // early out if nothing was written here
    if (!pixelInfo.pixelBaseColorAlpha.a)
        return;

    // Normal + --
    // In the Gbuffer, normals are stored in the compresed [0, 1] range. We're expanding that range back to [-1, 1]
    pixelInfo.pixelNormal = float4(DeCompressNormals(NormalTexture[dtID.xy].xyz), 0.0f);

    // Object space Ambient Occlusion, roughness and metallic
    pixelInfo.pixelAoRoughnessMetallic = AoRoughnessMetallicTexture[dtID.xy].rgb;

    // Depth
    float pixelDepth = DepthTexture.SampleLevel(SamPointClamp, uv, 0).x;

    // Coordinates
    // Used by the PBRLighting function to determine when to use the screenspace shadow texture to shade the current pixel
    pixelInfo.pixelCoordinates = uint4(dtID.xy, uint(LightInfo.bUseScreenSpaceShadowMap), 0);

    // TODO: Check this and put it in a common libary if good
    // Reverse project depth back to world space
    float4 projPosition = float4(RescaleUVForUpscaler(uv) * float2(2.f, -2.0f) + float2(-1.f, 1.f),
                                 pixelDepth,
                                 1.f); // Construct a projection position

    pixelInfo.pixelWorldPos = mul(SceneInfo.CameraInfo.InvViewProjectionMatrix, projPosition);
    pixelInfo.pixelWorldPos /= pixelInfo.pixelWorldPos.w;

    // TODO: Check this and put it in a common libary if good

    float3 lightingColor = PBRLighting(pixelInfo, ShadowMapTextures); // Do the lighting

    ColorTarget[dtID.xy] = float4(lightingColor, 1.f);
}
