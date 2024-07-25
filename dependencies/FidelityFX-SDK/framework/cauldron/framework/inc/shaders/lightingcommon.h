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

#ifndef LIGHTINGCOMMON_HLSL
#define LIGHTINGCOMMON_HLSL

#if __cplusplus
struct LightingCBData
{
    mutable float IBLFactor = 1.0f;
    mutable float SpecularIBLFactor = 1.0f;
};
#else
cbuffer LightingCBData : register(IBL_INDEX)
{
    float IBLFactor : packoffset(c0.x);
    float SpecularIBLFactor : packoffset(c0.y);
}

// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.
float3 getIBLContribution(MaterialInfo materialInfo, float3 n, float3 v)
{
    float NdotV = clamp(dot(n, v), 0.0, 1.0);
    float3 reflection = normalize(reflect(-v, n));

    float Width; float Height;;
    prefilteredCube.GetDimensions(Width, Height);
    int maxMipLevel = int(log2(float(Width > 0 ? Width : 1)));
    float lod = clamp(materialInfo.perceptualRoughness * float(maxMipLevel), 0.0, float(maxMipLevel));

    // In Vulkan and DirectX (0, 0) is on the top left corner. We need to flip the y-axis as the brdf lut has roughness y-up.
    float2 brdfSamplePoint = clamp(float2(NdotV, 1.0 - materialInfo.perceptualRoughness), float2(0.0, 0.0), float2(1.0, 1.0));
    // retrieve a scale and bias to F0. See [1], Figure 3
    float2 brdf = SampleBRDFTexture(brdfSamplePoint).rg;

    float3 irradianceSample  = SampleIrradianceCube(n).rgb;
    float3 prefilteredSample = SamplePrefilteredCube(reflection, lod).rgb;

    float3 diffuse = irradianceSample * materialInfo.baseColor;
    float3 specular = prefilteredSample * (materialInfo.reflectance0 * brdf.x + materialInfo.reflectance90 * brdf.y) * SpecularIBLFactor;

    return diffuse + specular;
}

float3 PBRLighting(in PBRPixelInfo pixelInfo, Texture2D ShadowMapTextures[MAX_SHADOW_MAP_TEXTURES_COUNT])
{
    MaterialInfo materialInfo;
    materialInfo.perceptualRoughness    = sqrt(pixelInfo.pixelAoRoughnessMetallic.g);
    materialInfo.alphaRoughness         = pixelInfo.pixelAoRoughnessMetallic.g;
    materialInfo.metallic               = pixelInfo.pixelAoRoughnessMetallic.b;
    materialInfo.baseColor              = pixelInfo.pixelBaseColorAlpha.rgb * (1.0 - pixelInfo.pixelAoRoughnessMetallic.b);
    float3 metallic                     = float3(pixelInfo.pixelAoRoughnessMetallic.b, pixelInfo.pixelAoRoughnessMetallic.b, pixelInfo.pixelAoRoughnessMetallic.b);
    materialInfo.reflectance0           = lerp(float3(MinReflectance, MinReflectance, MinReflectance), pixelInfo.pixelBaseColorAlpha.rgb, metallic);
    materialInfo.reflectance90          = float3(1.0, 1.0, 1.0);

    // Resolve the view vector to this pixel
    float3 viewVec = normalize(SceneInfo.CameraInfo.CameraPos.xyz - pixelInfo.pixelWorldPos.xyz);

    float3 color = float3(0.f, 0.f, 0.f);

    // Accumulate contribution from punctual lights
    for (int i = 0; i < LightInfo.LightCount; ++i)
    {
        float shadowFactor;
        if (pixelInfo.pixelCoordinates.z)
            // Use screenSpaceShadowTexture, stored at index 0
            shadowFactor = CalcShadows(pixelInfo.pixelCoordinates.xy, ShadowMapTextures[0]);
        else
            shadowFactor = CalcShadows(pixelInfo.pixelWorldPos.xyz, pixelInfo.pixelNormal.xyz, LightInfo.LightInfo[i], SamShadow, ShadowMapTextures);
        color += ApplyPunctualLight(pixelInfo.pixelWorldPos.xyz, pixelInfo.pixelNormal.xyz, viewVec, materialInfo, LightInfo.LightInfo[i]) * shadowFactor;
    }

    // Calculate lighting contribution from image based lighting source (IBL)
#if (DEF_SSAO == 1)
    color += getIBLContribution(materialInfo, pixelInfo.pixelNormal.xyz, viewVec) * IBLFactor* pixelInfo.pixelAoRoughnessMetallic.r;
#else
    color += getIBLContribution(materialInfo, pixelInfo.pixelNormal.xyz, viewVec) * IBLFactor;
#endif

    // TODO add AO etc.

    return color;
}
#endif // __cplusplus
#endif // LIGHTINGCOMMON_HLSL
