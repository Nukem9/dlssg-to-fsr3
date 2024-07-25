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

#include "sssr_apply_reflections_common.h"

Texture2D<float4> reflectionTarget              : register(t0);
Texture2D<float4> normalsTexture                : register(t1);
Texture2D<float4> baseColorAlphaTexture         : register(t2);
Texture2D<float4> aoRoughnessMetallicTexture    : register(t3);
Texture2D<float4> brdfTexture                   : register(t4);

SamplerState linearSampler : register(s0);

struct VertexOut
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

// Important bits from the PBR shader
float3 getIBLContribution(float perceptualRoughness, float3 f0, float3 specularLight, float3 n, float3 v)
{
    float NdotV = clamp(dot(n, v), 0.0, 1.0);
    float2 brdfSamplePoint = saturate(float2(NdotV, 1.0 - perceptualRoughness));
    float2 brdf = brdfTexture.Sample(linearSampler, brdfSamplePoint).rg;

    return specularLight * (f0 * brdf.x + brdf.y);;
}

float4 ps_main(VertexOut input) : SV_Target0
{
    float3 radiance = reflectionTarget.Sample(linearSampler, input.texcoord).xyz;
    float4 aoRoughnessMetallic = aoRoughnessMetallicTexture.Sample(linearSampler, input.texcoord);
    float perceptualRoughness = sqrt(aoRoughnessMetallic.g); // aoRoughnessMetallic.g contains alphaRoughness
    float3 baseColor = baseColorAlphaTexture.Sample(linearSampler, input.texcoord).rgb;
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), baseColor, float3(aoRoughnessMetallic.b, aoRoughnessMetallic.b, aoRoughnessMetallic.b));
    float3 normal = 2.0 * normalsTexture.Sample(linearSampler, input.texcoord).xyz - 1.0;
    float3 view = viewDirection.xyz;

    if (showReflectionTarget == 1)
    {
        // Show just the reflection view
        return float4(radiance, 0);
    }
    else if (drawReflections == 1)
    {
        // reflectionsIntensity should always be 1 for PBR correctness. We expose it in the UI to help visualize reflections better.
        radiance = getIBLContribution(perceptualRoughness, f0, radiance, normal, view) * reflectionsIntensity;
        return float4(radiance, 1); // Show the reflections applied to the scene
    }
    else
    {
        // Show just the scene
        return float4(0, 0, 0, 1);
    }
}
