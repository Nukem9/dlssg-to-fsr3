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

// Portions Copyright 2024 Advanced Micro Devices, Inc.All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This shader code was ported from https://github.com/KhronosGroup/glTF-WebGL-PBR
// All credits should go to his original author.

//
// This fragment shader defines a reference implementation for Physically Based Shading of
// a microfacet surface material defined by a glTF model.
//
// References:
// [1] Real Shading in Unreal Engine 4
//     http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] Physically Based Shading at Disney
//     http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] README.md - Environment Maps
//     https://github.com/KhronosGroup/glTF-WebGL-PBR/#environment-maps
// [4] "An Inexpensive BRDF Model for Physically based Rendering" by Christophe Schlick
//     https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf
//

#include "surfacerendercommon.h"

//////////////////////////////////////////////////////////////////////////
// Resources
//////////////////////////////////////////////////////////////////////////

cbuffer CBLightInformation : register(b4)
{
    SceneLightingInformation LightInfo;
}

SamplerComparisonState SamShadow : register(s3);

cbuffer CBSceneInformation : register(b0)
{
    SceneInformation SceneInfo;
};
cbuffer CBInstanceInformation : register(b1)
{
    InstanceInformation InstanceInfo;
};
cbuffer CBTextureIndices : register(b2)
{
    TextureIndices Textures;
}

Texture2D    brdfTexture                                      : register(t0);
TextureCube  irradianceCube                                   : register(t1);
TextureCube  prefilteredCube                                  : register(t2);

SamplerState samBRDF                                          : register(s0);
SamplerState samIrradianceCube                                : register(s1);
SamplerState samPrefilteredCube                               : register(s2);

Texture2D AllTextures[]    : register(TRANS_ALL_TEXTURES_INDEX);
SamplerState AllSamplers[] : register(s4);

// ------------------------------------------------------
// IBL --------------------------------------------------
// ------------------------------------------------------
float4 SampleBRDFTexture(float2 uv)
{
    return brdfTexture.Sample(samBRDF, uv);
}

float4 SampleIrradianceCube(float3 n)
{
    return irradianceCube.Sample(samIrradianceCube, n);
}

float4 SamplePrefilteredCube(float3 reflection, float lod)
{
    return prefilteredCube.SampleLevel(samPrefilteredCube, reflection, lod);
}

#define IBL_INDEX b3
#include "lightingcommon.h"
Texture2D ShadowMapTextures[MAX_SHADOW_MAP_TEXTURES_COUNT] : register(t3);

struct PSOutputs
{
    // Always write out this target
    float4 ColorTarget : SV_TARGET0;

#if defined(ADDITIONAL_TRANSLUCENT_OUTPUTS)
    // This will be a string that represents the outputs and bindings for them
    ADDITIONAL_TRANSLUCENT_OUTPUTS
#endif // ADDITIONAL_TRANSLUCENT_OUTPUTS
};

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------
PSOutputs MainPS(VS_SURFACE_OUTPUT SurfaceInput
#ifdef ID_doublesided
    , bool isFrontFace : SV_IsFrontFace
#endif
)
{
#ifndef ID_doublesided
    bool isFrontFace = false;
#endif

    // Gather PBR info
    PBRPixelInfo pixelInfo;

#ifdef HAS_COLOR_0
    if (SurfaceInput.Color0.a <= 0)
        discard;
#endif
    float4 BaseColorAlpha;
    float3 AoRoughnessMetallic;
    GetPBRParams(SurfaceInput, InstanceInfo.MaterialInfo, BaseColorAlpha, AoRoughnessMetallic, Textures, AllTextures, AllSamplers, SceneInfo.MipLODBias);

    DiscardPixelIfAlphaCutOff(BaseColorAlpha.a, InstanceInfo);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    AoRoughnessMetallic.g *= AoRoughnessMetallic.g;

    pixelInfo.pixelBaseColorAlpha       = BaseColorAlpha;
    pixelInfo.pixelNormal               = float4(GetPixelNormal(SurfaceInput, Textures, SceneInfo, AllTextures, AllSamplers, SceneInfo.MipLODBias, isFrontFace), 1.f);
    pixelInfo.pixelAoRoughnessMetallic  = AoRoughnessMetallic;
    pixelInfo.pixelWorldPos             = float4(SurfaceInput.WorldPos, 1.f);

    // Coordinates
    // Used by the PBRLighting function to determine when to use the screenspace shadow texture to shade the current pixel
    pixelInfo.pixelCoordinates = uint4(SurfaceInput.Position.xy, uint(LightInfo.bUseScreenSpaceShadowMap), 0);

    PSOutputs output;

    output.ColorTarget = float4(PBRLighting(pixelInfo, ShadowMapTextures), BaseColorAlpha.a);

    float ReactiveMask = 0.f;
    float Alpha = BaseColorAlpha.a; // For ADDITIONAL_TRANSLUCENT_EXPORTS

#if defined(ADDITIONAL_TRANSLUCENT_EXPORTS)
    // This will be a string that represents what information to store if the optional outputs
    ADDITIONAL_TRANSLUCENT_EXPORTS
#endif // ADDITIONAL_TRANSLUCENT_EXPORTS

    return output;
}
