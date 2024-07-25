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

#include "spd_common.h"

//--------------------------------------------------------------------------------------
// Constant Buffer
//--------------------------------------------------------------------------------------
cbuffer constants : register(b0)
{
    SPDDownsampleInfo DownsampleInfo;
}

//--------------------------------------------------------------------------------------
// Texture definitions
//--------------------------------------------------------------------------------------
Texture2DArray                   inputTex            : register(t0);
SamplerState                     linearSampler       : register(s0);
RWTexture2DArray<float4>         outputTex           : register(u0);

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------
[numthreads(8, 8, 1)]
void mainCS(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= DownsampleInfo.OutSize.x || dispatchId.y >= DownsampleInfo.OutSize.y)
        return;

    float2 samplerTexCoord = DownsampleInfo.InvSize * float2(dispatchId.xy) * 2.0 + DownsampleInfo.InvSize;
    float4 result = inputTex.SampleLevel(linearSampler, float3(samplerTexCoord, DownsampleInfo.Slice), 0);

    result.r = max(min(result.r * (12.92), (0.04045)), (1.055) * pow(result.r, (0.41666)) - (0.055));
    result.g = max(min(result.g * (12.92), (0.04045)), (1.055) * pow(result.g, (0.41666)) - (0.055));
    result.b = max(min(result.b * (12.92), (0.04045)), (1.055) * pow(result.b, (0.41666)) - (0.055));

    outputTex[int3(dispatchId.xy, DownsampleInfo.Slice)] = result;
}
