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

//
// This fragment shader defines a reference implementation for Physically Based Shading of
// a microfacet surface material defined by a glTF model.
//
// References:
// [1] Real Shading in Unreal Engine 4
//     http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] Physically Based Shading at Disney
//     http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] "An Inexpensive BRDF Model for Physically based Rendering" by Christophe Schlick
//     https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf (link appears to be gone)
//

#ifndef LIGHTFUNCTIONS_HLSL
#define LIGHTFUNCTIONS_HLSL

// Maximum light range in meters. The smaller this is, the less pixels we shade.
#define LIGHT_MAX_RANGE 10

#include "shadercommon.h"
#include "shadowFiltering.h"

// functions and constants related to lighting
static const float M_PI = 3.141592653589793;
static const float MinReflectance = 0.04;

// angular terms
struct AngularInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector

    float VdotH;                  // cos angle between view direction and half vector

    float3 padding;
};

float GetPerceivedBrightness(float3 vec)
{
    return sqrt(0.299 * vec.r * vec.r + 0.587 * vec.g * vec.g + 0.114 * vec.b * vec.b);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness/examples/convert-between-workflows/js/three.pbrUtilities.js#L34
float SolveMetallic(float3 diffuse, float3 specular, float oneMinusSpecularStrength) {
    float specularBrightness = GetPerceivedBrightness(specular);

    if (specularBrightness < MinReflectance) {
        return 0.0;
    }

    float diffuseBrightness = GetPerceivedBrightness(diffuse);

    float a = MinReflectance;
    float b = diffuseBrightness * oneMinusSpecularStrength / (1.0 - MinReflectance) + specularBrightness - 2.0 * MinReflectance;
    float c = MinReflectance - specularBrightness;
    float D = b * b - 4.0 * a * c;

    return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
}

AngularInfo GetAngularInfo(float3 pointToLight, float3 normal, float3 view)
{
    // Standard one-letter names
    float3 n = normalize(normal);           // Outward direction of surface point
    float3 v = normalize(view);             // Direction from surface point to view
    float3 l = normalize(pointToLight);     // Direction from surface point to light
    float3 h = normalize(l + v);            // Direction of the vector between l and v

    float NdotL = clamp(dot(n, l), 0.0, 1.0);
    float NdotV = clamp(dot(n, v), 0.0, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    AngularInfo angularInfo = {
        NdotL,
        NdotV,
        NdotH,
        LdotH,
        VdotH,
        float3(0, 0, 0)
    };

    return angularInfo;
}

// Lambert lighting
// see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
float3 Diffuse_Lambert(MaterialInfo materialInfo)
{
    return materialInfo.baseColor / M_PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [3], Equation 15
float3 SpecularReflection_Schlick(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    return materialInfo.reflectance0 + (materialInfo.reflectance90 - materialInfo.reflectance0) * pow(clamp(1.0 - angularInfo.VdotH, 0.0, 1.0), 5.0);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float VisibilityOcclusion_SmithJointGGX(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    float NdotL = angularInfo.NdotL;
    float NdotV = angularInfo.NdotV;
    float alphaRoughnessSq = materialInfo.alphaRoughness * materialInfo.alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float MicrofacetDistribution_Trowbridge(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    float alphaRoughnessSq = materialInfo.alphaRoughness * materialInfo.alphaRoughness;
    float f = (angularInfo.NdotH * alphaRoughnessSq - angularInfo.NdotH) * angularInfo.NdotH + 1.0;
    return alphaRoughnessSq / (M_PI * f * f + 0.000001f);
}

float3 GetPointShade(float3 pointToLight, MaterialInfo materialInfo, float3 normal, float3 view)
{
    AngularInfo angularInfo = GetAngularInfo(pointToLight, normal, view);

    if (angularInfo.NdotL > 0.0 || angularInfo.NdotV > 0.0)
    {
        // Calculate the shading terms for the microfacet specular shading model
        float3 F = SpecularReflection_Schlick(materialInfo, angularInfo);
        float Vis = VisibilityOcclusion_SmithJointGGX(materialInfo, angularInfo);
        float D = MicrofacetDistribution_Trowbridge(materialInfo, angularInfo);

        // Calculation of analytical lighting contribution
        float3 diffuseContrib = (1.0 - F) * Diffuse_Lambert(materialInfo);
        float3 specContrib = F * Vis * D;

        // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
        return angularInfo.NdotL * (diffuseContrib + specContrib);
    }

    return float3(0.0, 0.0, 0.0);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#range-property
float GetRangeAttenuation(float range, float distance)
{
    // Attenuate according to the inverse square law and smooth transition near the max light range to prevent hard cutoff.
    float distanceOverRange = ( distance / range );
    float distanceOverRange2 = distanceOverRange * distanceOverRange;
    float distanceOverRange4 = distanceOverRange2 * distanceOverRange2;
    return max( min( 1.0f - distanceOverRange4, 1.0f ), 0.0f ) / ( distance * distance );
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
float GetSpotAttenuation(float3 pointToLight, float3 spotDirection, float outerConeCos, float innerConeCos)
{
    float actualCos = dot(normalize(spotDirection), normalize(-pointToLight));
    if (actualCos > outerConeCos)
    {
        if (actualCos < innerConeCos)
        {
            return smoothstep(outerConeCos, innerConeCos, actualCos);
        }
        return 1.0;
    }
    return 0.0;
}

float3 ApplyDirectionalLight(LightInformation light, MaterialInfo materialInfo, float3 normal, float3 view)
{
    float3 pointToLight = light.DirectionRange.xyz;

#if (DEF_doubleSided == 1)
    if (dot(normal, pointToLight) < 0)
    {
        normal = -normal;
    }
#endif

    float3 shade = GetPointShade(pointToLight, materialInfo, normal, view);
    return light.ColorIntensity.rgb * light.ColorIntensity.a * shade;
}

float3 ApplyPointLight(LightInformation light, MaterialInfo materialInfo, float3 normal, float3 worldPos, float3 view)
{
    float3 pointToLight = light.PosDepthBias.xyz - worldPos;
    float distance = length(pointToLight);

    // The GLTF attenuation function expects a range, make this happen CPU side?
    light.DirectionRange.w = light.DirectionRange.w > 0 ? light.DirectionRange.w : LIGHT_MAX_RANGE;

    // Early out if we're out of range of the light.
    if( distance > light.DirectionRange.w )
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

#if (DEF_doubleSided == 1)
    if (dot(normal, pointToLight) < 0)
    {
        normal = -normal;
    }
#endif

    float attenuation = GetRangeAttenuation(light.DirectionRange.w, distance);
    float3 shade = GetPointShade(pointToLight, materialInfo, normal, view);
    return attenuation * light.ColorIntensity.rgb * light.ColorIntensity.a * shade;
}

float3 ApplySpotLight(LightInformation light, MaterialInfo materialInfo, float3 normal, float3 worldPos, float3 view)
{
    float3 pointToLight = light.PosDepthBias.xyz - worldPos;
    float distance = length(pointToLight);

    // The GLTF attenuation function expects a range, make this happen CPU side?
    light.DirectionRange.w = light.DirectionRange.w > 0 ? light.DirectionRange.w : LIGHT_MAX_RANGE;

#if (DEF_doubleSided == 1)
    if (dot(normal, pointToLight) < 0)
    {
        normal = -normal;
    }
#endif

    float rangeAttenuation = GetRangeAttenuation(light.DirectionRange.w, distance);
    float spotAttenuation = GetSpotAttenuation(pointToLight, -light.DirectionRange.xyz, light.OuterConeCos, light.InnerConeCos);
    float3 shade = GetPointShade(pointToLight, materialInfo, normal, view);
    return rangeAttenuation * spotAttenuation * light.ColorIntensity.rgb * light.ColorIntensity.a * shade;
}

float3 ApplyPunctualLight(in float3 worldPos, in float3 normal, in float3 view, in MaterialInfo materialInfo, in LightInformation lightInfo)
{
    float3 color = float3(0.0, 0.0, 0.0);

    if (lightInfo.Type == 0 /* LightType::Directional */)
    {
        color = ApplyDirectionalLight(lightInfo, materialInfo, normal, view);
    }
    else if (lightInfo.Type == 2 /* LightType::Point */)
    {
        color = ApplyPointLight(lightInfo, materialInfo, normal, worldPos, view);
    }
    else if (lightInfo.Type == 1 /* LightType::Spot */)
    {
        color = ApplySpotLight(lightInfo, materialInfo, normal, worldPos, view);
    }

    return color;
}

// Lighting
struct PBRPixelInfo
{
    float4 pixelBaseColorAlpha;
    float4 pixelNormal;
    float3 pixelAoRoughnessMetallic;
    float4 pixelWorldPos;
    // x,y pixel coordinates, z [0/1] use ssstexture
    uint4  pixelCoordinates;
};

float CalcShadows(in float3              worldPos,
                  in float3              worldNormal,
                  in LightInformation    lightInfo,
                  SamplerComparisonState samplerCompShadow,
                  Texture2D              ShadowMapTextures[MAX_SHADOW_MAP_TEXTURES_COUNT])
{
    if (lightInfo.ShadowMapIndex0 < 0)
        return 1.0f;

    return DoShadow(ShadowMapTextures, samplerCompShadow, worldPos, worldNormal, lightInfo);
}

float CalcShadows(in uint2 pixelCoord,
                  Texture2D screenSpaceShadowTexture)
{
    return screenSpaceShadowTexture[pixelCoord].r;
}


#endif // LIGHTFUNCTIONS_HLSL
