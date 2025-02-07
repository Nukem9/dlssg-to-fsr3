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

#ifndef LENSDISTORTION_HLSL
#define LENSDISTORTION_HLSL

#include "tonemappercommon.h"

float2 BarrelDistortion(in float2 Uv)
{    
    float2 remappedUv = (Uv * 2.0f) - 1.0f;
    float r2 = remappedUv.x * remappedUv.x + remappedUv.y * remappedUv.y;
    float2 outUv = remappedUv / (1.0f + LensDistortionStrength * r2);
    return (outUv + 1.0f) / 2.0f;
}

float2 InverseBarrelDistortion(in float2 Uv)
{  
    float2 remappedUv = (Uv * 2.0f) - 1.0f;
    float ru2 = remappedUv.x * remappedUv.x +  remappedUv.y * remappedUv.y;
    float num = sqrt(1.0f - 4.0f * LensDistortionStrength * ru2) - 1.0f;
    float denom = 2.0f * LensDistortionStrength * sqrt(ru2);
    float rd = -num / denom;
    float2 outUV = remappedUv * (rd / sqrt(ru2));
    return (outUV + 1.0f) / 2.0f;
}

float2 Zoom(in float2 Uv)
{
	float2 translatedCoord = (Uv - 0.5f) * 2.0f;
	translatedCoord *= (1.0f - saturate(LensDistortionZoom));
	return (translatedCoord + 1.0f) / 2.0f;
}

float2 InverseZoom(in float2 Uv)
{
	float2 translatedCoord = (Uv - 0.5f) * 2.0f;
    translatedCoord /= (1.0f - saturate(LensDistortionZoom));
	return (translatedCoord + 1.0f) / 2.0f;
}

float2 GenerateDistortionField(in float2 Uv)
{
    float2 xy = Zoom(BarrelDistortion(Uv)) - Uv;
    return xy;
}

float2 ApplyLensDistortion(in float2 Uv)
{
    return Zoom(BarrelDistortion(Uv));
}

#endif