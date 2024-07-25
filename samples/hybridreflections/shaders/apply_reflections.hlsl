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
#include "commonintersect.hlsl"

SamplerState g_linear_sampler : register(s0);

Texture2D<float4> reflectionTarget : register(t0);
Texture2D<float4> normalsTexture : register(t1);
Texture2D<float4> aoRoughnessMetallicTexture : register(t2);
Texture2D<float4> brdfTexture : register(t3);
Texture2D<float4> depthTexture : register(t4);
Texture2D<float4> g_debug : register(t5);
Texture2D<float4> baseColorAlphaTexture         : register(t6);

ConstantBuffer<FrameInfo> g_frame_info_cb[1] : register(b0);

struct VertexOut
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

// Important bits from the PBR shader
float3 getIBLContribution(float perceptualRoughness, float3 f0, float3 specularLight, float3 n, float3 v)
{
    float NdotV = clamp(dot(n, v), 0.0, 1.0);
    float2 brdfSamplePoint = clamp(float2(NdotV, 1.0 - perceptualRoughness), float2(0.0, 0.0), float2(1.0, 1.0));
    // retrieve a scale and bias to F0. See [1], Figure 3
    float2 brdf = brdfTexture.Sample(g_linear_sampler, brdfSamplePoint).rg;

    return specularLight * (f0 * brdf.x + brdf.y);;
}

float4 ps_main(VertexOut input)
    : SV_Target0
{
    float3 radiance            = reflectionTarget.Sample(g_linear_sampler, input.texcoord).xyz;
    float4 aoRoughnessMetallic   = aoRoughnessMetallicTexture.Sample(g_linear_sampler, input.texcoord);
    float  perceptualRoughness = sqrt(aoRoughnessMetallic.g);
    float3 baseColor = baseColorAlphaTexture.Sample(g_linear_sampler, input.texcoord).rgb;
    float  central_depth       = depthTexture.Sample(g_linear_sampler, input.texcoord).x;
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), baseColor, float3(aoRoughnessMetallic.b, aoRoughnessMetallic.b, aoRoughnessMetallic.b));
    float3 normal              = normalsTexture.Sample(g_linear_sampler, input.texcoord).xyz;

#ifdef HSR_DEBUG
    if (g_hsr_mask & HSR_FLAGS_SHOW_INTERSECTION)
    {
        float4 radiance_weight = g_debug.SampleLevel(g_linear_sampler, input.texcoord, 0.0f);
        return (radiance_weight.xyz / radiance_weight.w).xyzz;
    }

    if (g_hsr_mask & HSR_FLAGS_SHOW_REFLECTION_TARGET)
    {
        // Show just the reflection view
        return float4(radiance, 0);
    }

    if (g_hsr_mask & HSR_FLAGS_SHOW_DEBUG_TARGET)
    {
        float4 debug_value = float4(g_debug.SampleLevel(g_linear_sampler, input.texcoord, 0.0f).xyz, 1.0f);
        if (any(isinf(debug_value)) || any(isnan(debug_value)))
            return float4(1.0f, 0.0f, 1.0f, 1.0f);
        else
            return debug_value;
    }
#endif

    if (g_hsr_mask & HSR_FLAGS_APPLY_REFLECTIONS)
    {
        float3 screen_uv_space_ray_origin = float3(input.texcoord, central_depth);
        float3 view_space_ray             = ScreenSpaceToViewSpace(screen_uv_space_ray_origin, g_inv_proj);
        float3 view_space_ray_direction   = normalize(view_space_ray);
        float3 world_space_ray_direction  = mul(g_inv_view, float4(view_space_ray_direction, 0)).xyz;
        radiance                          = getIBLContribution(perceptualRoughness, f0, radiance, normal, -world_space_ray_direction);
        return float4(radiance * g_frame_info.reflection_factor, 1);  // Show the reflections applied to the scene
    }
    else
    {
        // Show just the scene
        return float4(0, 0, 0, 1);
    }
}
