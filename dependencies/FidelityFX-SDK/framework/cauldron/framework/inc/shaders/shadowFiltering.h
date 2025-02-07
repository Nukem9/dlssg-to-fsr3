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

#ifndef SHADOWFILTERING_HLSL
#define SHADOWFILTERING_HLSL

float FilterShadow(Texture2D shadowMap, SamplerComparisonState samShadow, float3 uv)
{
    float shadow = 0.0;

    const int kernelLevel = 2;
    const int kernelWidth = 2 * kernelLevel + 1;
    [unroll] for (int i = -kernelLevel; i <= kernelLevel; i++)
    {
        [unroll] for (int j = -kernelLevel; j <= kernelLevel; j++)
        {
            shadow += shadowMap.SampleCmpLevelZero(samShadow, uv.xy, uv.z, int2(i, j)).r;
        }
    }

    shadow /= (kernelWidth*kernelWidth);
    return shadow;
}

float DoShadow(Texture2D shadowMap[MAX_SHADOW_MAP_TEXTURES_COUNT], SamplerComparisonState samShadow, in float3 vPosition, in float3 worldNormal, in LightInformation lightInfo)
{
    float slopeOffset = (1.0 - dot(normalize(worldNormal), normalize(lightInfo.DirectionRange.xyz)));

    if (lightInfo.NumCascades == 0)
    {
        float4 shadowTexCoord = mul(lightInfo.LightViewProj[0], float4(vPosition, 1.0));
        shadowTexCoord.xyz    = shadowTexCoord.xyz / shadowTexCoord.w;

        shadowTexCoord.xy = lightInfo.ShadowMapTransformation[0].xy * shadowTexCoord.xy + lightInfo.ShadowMapTransformation[0].zw;  // remap to texture coords
        shadowTexCoord.z -= slopeOffset * lightInfo.PosDepthBias.w;

        return FilterShadow(shadowMap[lightInfo.ShadowMapIndex0], samShadow, shadowTexCoord.xyz);
    }

    if (lightInfo.NumCascades > 0)
    {
        float4 shadowTexCoord = mul(lightInfo.LightViewProj[0], float4(vPosition, 1.0));
        shadowTexCoord.xyz    = shadowTexCoord.xyz / shadowTexCoord.w;

        if (all(shadowTexCoord.xy > -1) && all(shadowTexCoord.xy < 1))
        {
            shadowTexCoord.xy = lightInfo.ShadowMapTransformation[0].xy * shadowTexCoord.xy + lightInfo.ShadowMapTransformation[0].zw;  // remap to texture coords
            shadowTexCoord.z -= slopeOffset * lightInfo.PosDepthBias.w;

            return FilterShadow(shadowMap[lightInfo.ShadowMapIndex0], samShadow, shadowTexCoord.xyz);   
        }   
    }

    if (lightInfo.NumCascades > 1)
    {
        float4 shadowTexCoord = mul(lightInfo.LightViewProj[1], float4(vPosition, 1.0));
        shadowTexCoord.xyz    = shadowTexCoord.xyz / shadowTexCoord.w;

        if (all(shadowTexCoord.xy > -1) && all(shadowTexCoord.xy < 1))
        {
            shadowTexCoord.xy = lightInfo.ShadowMapTransformation[1].xy * shadowTexCoord.xy + lightInfo.ShadowMapTransformation[1].zw;  // remap to texture coords
            shadowTexCoord.z -= slopeOffset * lightInfo.PosDepthBias.w * 2;

            return FilterShadow(shadowMap[lightInfo.ShadowMapIndex1], samShadow, shadowTexCoord.xyz);   
        }   
    }

    if (lightInfo.NumCascades > 2)
    {
        float4 shadowTexCoord = mul(lightInfo.LightViewProj[2], float4(vPosition, 1.0));
        shadowTexCoord.xyz    = shadowTexCoord.xyz / shadowTexCoord.w;

        if (all(shadowTexCoord.xy > -1) && all(shadowTexCoord.xy < 1))
        {
            shadowTexCoord.xy = lightInfo.ShadowMapTransformation[2].xy * shadowTexCoord.xy + lightInfo.ShadowMapTransformation[2].zw;  // remap to texture coords
            shadowTexCoord.z -= slopeOffset * lightInfo.PosDepthBias.w * 3;

            return FilterShadow(shadowMap[lightInfo.ShadowMapIndex2], samShadow, shadowTexCoord.xyz);   
        }   
    }

    if (lightInfo.NumCascades > 3)
    {
        float4 shadowTexCoord = mul(lightInfo.LightViewProj[3], float4(vPosition, 1.0));
        shadowTexCoord.xyz    = shadowTexCoord.xyz / shadowTexCoord.w;

        if (all(shadowTexCoord.xy > -1) && all(shadowTexCoord.xy < 1))
        {
            shadowTexCoord.xy = lightInfo.ShadowMapTransformation[3].xy * shadowTexCoord.xy + lightInfo.ShadowMapTransformation[3].zw;  // remap to texture coords
            shadowTexCoord.z -= slopeOffset * lightInfo.PosDepthBias.w * 4;

            return FilterShadow(shadowMap[lightInfo.ShadowMapIndex3], samShadow, shadowTexCoord.xyz);   
        }   
    }

    return 1.0f;
}

#endif // SHADOWFILTERING_HLSL
