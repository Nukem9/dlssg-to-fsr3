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

// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.

void GetPointShadeSeparate(float3 pointToLight, MaterialInfo materialInfo, float3 normal, float3 view, inout float3 diffuse, inout float3 specular)
{
    AngularInfo angularInfo = GetAngularInfo(pointToLight, normal, view);

    if (angularInfo.NdotL > 0.0 || angularInfo.NdotV > 0.0)
    {
        // Calculate the shading terms for the microfacet specular shading model
        float3 F   = SpecularReflection_Schlick(materialInfo, angularInfo);
        float  Vis = VisibilityOcclusion_SmithJointGGX(materialInfo, angularInfo);
        float  D   = MicrofacetDistribution_Trowbridge(materialInfo, angularInfo);

        // Calculation of analytical lighting contribution
        float3 diffuseContrib = (1.0 - F) * Diffuse_Lambert(materialInfo);
        float3 specContrib    = F * Vis * D;

        // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
        diffuse = angularInfo.NdotL * diffuseContrib;
        specular = angularInfo.NdotL * specContrib;
    }
}

void ApplyDirectionalLightSeparate(LightInformation light, MaterialInfo materialInfo, float3 normal, float3 view, inout float3 diffuse, inout float3 specular)
{
    float3 pointToLight = light.DirectionRange.xyz;

#if (DEF_doubleSided == 1)
    if (dot(normal, pointToLight) < 0)
    {
        normal = -normal;
    }
#endif
    float3 shadeDiffuse = 0.0f;
    float3 shaderSpecular = 0.0f;

    GetPointShadeSeparate(pointToLight, materialInfo, normal, view, shadeDiffuse, shaderSpecular);

    diffuse = light.ColorIntensity.rgb * light.ColorIntensity.a * shadeDiffuse;
    specular = light.ColorIntensity.rgb * light.ColorIntensity.a * shaderSpecular;
}

void ApplyPointLightSeparate(LightInformation light, MaterialInfo materialInfo, float3 normal, float3 worldPos, float3 view, inout float3 diffuse, inout float3 specular)
{
    float3 pointToLight = light.PosDepthBias.xyz - worldPos;
    float  distance     = length(pointToLight);

    // The GLTF attenuation function expects a range, make this happen CPU side?
    light.DirectionRange.w = light.DirectionRange.w > 0 ? light.DirectionRange.w : LIGHT_MAX_RANGE;

    // Early out if we're out of range of the light.
    if (distance > light.DirectionRange.w)
    {
        return;
    }

#if (DEF_doubleSided == 1)
    if (dot(normal, pointToLight) < 0)
    {
        normal = -normal;
    }
#endif

    float  attenuation = GetRangeAttenuation(light.DirectionRange.w, distance);

    float3 shadeDiffuse   = 0.0f;
    float3 shaderSpecular = 0.0f;

    GetPointShadeSeparate(pointToLight, materialInfo, normal, view, shadeDiffuse, shaderSpecular);

    diffuse  = attenuation * light.ColorIntensity.rgb * light.ColorIntensity.a * shadeDiffuse;
    specular = attenuation * light.ColorIntensity.rgb * light.ColorIntensity.a * shaderSpecular;
}

void ApplySpotLightSeparate(LightInformation light, MaterialInfo materialInfo, float3 normal, float3 worldPos, float3 view, inout float3 diffuse, inout float3 specular)
{
    float3 pointToLight = light.PosDepthBias.xyz - worldPos;
    float  distance     = length(pointToLight);

    // The GLTF attenuation function expects a range, make this happen CPU side?
    light.DirectionRange.w = light.DirectionRange.w > 0 ? light.DirectionRange.w : LIGHT_MAX_RANGE;

#if (DEF_doubleSided == 1)
    if (dot(normal, pointToLight) < 0)
    {
        normal = -normal;
    }
#endif

    float  rangeAttenuation = GetRangeAttenuation(light.DirectionRange.w, distance);
    float  spotAttenuation  = GetSpotAttenuation(pointToLight, -light.DirectionRange.xyz, light.OuterConeCos, light.InnerConeCos);

    float3 shadeDiffuse     = 0.0f;
    float3 shaderSpecular   = 0.0f;

    GetPointShadeSeparate(pointToLight, materialInfo, normal, view, shadeDiffuse, shaderSpecular);

    diffuse  = rangeAttenuation * spotAttenuation * light.ColorIntensity.rgb * light.ColorIntensity.a * shadeDiffuse;
    specular = rangeAttenuation * spotAttenuation * light.ColorIntensity.rgb * light.ColorIntensity.a * shaderSpecular;
}

void ApplyPunctualLightSeparate(in float3           worldPos,
                                in float3           normal,
                                in float3           view,
                                in MaterialInfo     materialInfo,
                                in LightInformation lightInfo,
                                inout float3        diffuse,
                                inout float3        specular)
{
    if (lightInfo.Type == 0 /* LightType::Directional */)
    {
        ApplyDirectionalLightSeparate(lightInfo, materialInfo, normal, view, diffuse, specular);
    }
    else if (lightInfo.Type == 2 /* LightType::Point */)
    {
        ApplyPointLightSeparate(lightInfo, materialInfo, normal, worldPos, view, diffuse, specular);
    }
    else if (lightInfo.Type == 1 /* LightType::Spot */)
    {
        ApplySpotLightSeparate(lightInfo, materialInfo, normal, worldPos, view, diffuse, specular);
    }
}


float3 getIBLContribution(MaterialInfo materialInfo, float3 n, float3 v, float3 diffuseGI, float3 specularGI, float diffuseGIFactor, float specularGIFactor)
{
    float NdotV = clamp(dot(n, v), 0.0, 1.0);

    // In Vulkan and DirectX (0, 0) is on the top left corner. We need to flip the y-axis as the brdf lut has roughness y-up.
    float2 brdfSamplePoint = clamp(float2(NdotV, 1.0 - materialInfo.perceptualRoughness), float2(0.0, 0.0), float2(1.0, 1.0));
    // retrieve a scale and bias to F0. See [1], Figure 3
    float2 brdf = SampleBRDFTexture(brdfSamplePoint).rg;

    float3 diffuse = diffuseGI * materialInfo.baseColor * diffuseGIFactor;
    float3 specular = specularGI * (materialInfo.reflectance0 * brdf.x + materialInfo.reflectance90 * brdf.y) * specularGIFactor;

    return diffuse + specular;
}

void PBRLighting(in PBRPixelInfo pixelInfo, Texture2D ShadowMapTextures[MAX_SHADOW_MAP_TEXTURES_COUNT], float3 diffuseGI, float3 specularGI, float diffuseGIFactor, float specularGIFactor, bool multiBounce, inout float3 color, inout float3 colorDiffuse)
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

    // Accumulate contribution from punctual lights
    for (int i = 0; i < LightInfo.LightCount; ++i)
    {
        float shadowFactor;
        if (pixelInfo.pixelCoordinates.z)
            // Use screenSpaceShadowTexture, stored at index 0
            shadowFactor = CalcShadows(pixelInfo.pixelCoordinates.xy, ShadowMapTextures[0]);
        else
            shadowFactor = CalcShadows(pixelInfo.pixelWorldPos.xyz, pixelInfo.pixelNormal.xyz, LightInfo.LightInfo[i], SamShadow, ShadowMapTextures);
        
        float3 diffuse = 0.0f;
        float3 specular = 0.0f;
            
        ApplyPunctualLightSeparate(pixelInfo.pixelWorldPos.xyz, pixelInfo.pixelNormal.xyz, viewVec, materialInfo, LightInfo.LightInfo[i], diffuse, specular);

        diffuse *= shadowFactor;
        specular *= shadowFactor;

        color += (diffuse + specular);
        colorDiffuse += diffuse;
    }

    // Calculate lighting contribution from image based lighting source (IBL)
    color += getIBLContribution(materialInfo, pixelInfo.pixelNormal.xyz, viewVec, diffuseGI, specularGI, diffuseGIFactor, specularGIFactor);
    
    if (multiBounce)
        colorDiffuse += getIBLContribution(materialInfo, pixelInfo.pixelNormal.xyz, viewVec, diffuseGI, specularGI, diffuseGIFactor, 0.0f);
}
#endif // LIGHTINGCOMMON_HLSL
