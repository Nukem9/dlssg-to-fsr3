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

// Generic blur CS dispatch dimensions - FFX-Blur uses its own
#define THREADGROUP_DIMENSION_X 8
#define THREADGROUP_DIMENSION_Y 8

#ifndef KERNEL_DIMENSION
#error Please define KERNEL_DIMENSION
#endif
#include "blur_gaussian_blur_kernels.h"


//--------------------------------------------------------------------------------------
// 
// RESOURCE BINDING
// 
//--------------------------------------------------------------------------------------
#if HALF_PRECISION
Texture2D<min16float4> texColorInput : register(t0);
#else
Texture2D texColorInput : register(t0);
#endif
RWTexture2D<float4> texColorOutput : register(u0);

cbuffer BlurParameters : register(b0)
{
    uint2  ImageSize;
}

//--------------------------------------------------------------------------------------
// 
// ENTRY POINTS
// 
//--------------------------------------------------------------------------------------

#if SINGLE_PASS_BOX_FILTER
[numthreads(THREADGROUP_DIMENSION_X, THREADGROUP_DIMENSION_Y, 1)]
void CSMain_SinglePass_BoxFilter(
    uint3 LocalThreadId    : SV_GroupThreadID,
    uint3 WorkGroupId      : SV_GroupID,
    uint3 DispatchThreadID : SV_DispatchThreadID)
{
    int2 pxCoord = DispatchThreadID.xy;

    // early out for out-of-image-bounds
    if (pxCoord.x >= ImageSize.x || pxCoord.y >= ImageSize.y)
        return;

    float4 InRGBA = texColorInput[pxCoord];
    float3 OutRGB = 0.0.xxx;

    
    [unroll]
    for (int kernelIt = 0; kernelIt < KERNEL_DIMENSION; ++kernelIt) // [0, KERNEL_DIMENSION)
    {
        for (int kernelItY = 0; kernelItY < KERNEL_DIMENSION; ++kernelItY) // [0, KERNEL_DIMENSION)
        {
            int2 kernelOffset = int2(kernelIt - KERNEL_RANGE_MINUS1, kernelItY - KERNEL_RANGE_MINUS1); // [-(KERNEL_RANGE-1), KERNEL_RANGE-1]
            int2 kernelIndex = abs(kernelOffset); // [0, KERNEL_RANGE]
            int2 sampleCoord = pxCoord.xy + kernelOffset;

            sampleCoord.y = clamp(sampleCoord.y, 0, ImageSize.y - 1);
            sampleCoord.x = clamp(sampleCoord.x, 0, ImageSize.x - 1);
            
            float3 sampledColor = texColorInput[sampleCoord].rgb;
            OutRGB += Convolve2D(sampledColor, kernelIndex);
        }
    }

    texColorOutput[pxCoord] = float4(OutRGB, 1.0f); 
}
#endif


#if MULTI_PASS_SEPARABLE_FILTER
[numthreads(THREADGROUP_DIMENSION_X, THREADGROUP_DIMENSION_Y, 1)]
void CSMain_SeparableFilter_X(
    uint3 LocalThreadId    : SV_GroupThreadID,
    uint3 WorkGroupId      : SV_GroupID,
    uint3 DispatchThreadID : SV_DispatchThreadID) 
{
    int2 pxCoord = DispatchThreadID.xy;
    
    // early out for out-of-image-bounds
    if(pxCoord.x >= ImageSize.x || pxCoord.y >= ImageSize.y)
        return; 
    
    float4 InRGBA = texColorInput[pxCoord];
    //float3 OutRGB = InRGBA.brg;
    
    float3 OutRGB = 0.0.xxx;

    [unroll]
    for (int kernelIt = 0; kernelIt < KERNEL_DIMENSION; ++kernelIt)
    {
        int kernelOffset = kernelIt - KERNEL_RANGE_MINUS1;
        int kernelIndex = abs(kernelOffset);
        int2 sampleCoord = int2(pxCoord.x + kernelOffset, pxCoord.y);

        sampleCoord.x = clamp(sampleCoord.x, 0, ImageSize.x - 1);
        
        float3 sampledColor = texColorInput[sampleCoord].rgb;

        OutRGB += Convolve1D(sampledColor, kernelIndex);
    }
    
#if TRANSPOSE_OUT
    pxCoord = pxCoord.yx;
#endif

    texColorOutput[pxCoord] = float4(OutRGB, 1.0f);
}


[numthreads(THREADGROUP_DIMENSION_X, THREADGROUP_DIMENSION_Y, 1)]
void CSMain_SeparableFilter_Y(
    uint3 LocalThreadId    : SV_GroupThreadID,
    uint3 WorkGroupId      : SV_GroupID,
    uint3 DispatchThreadID : SV_DispatchThreadID)
{
    int2 pxCoord = DispatchThreadID.xy;
    
    // early out for out-of-image-bounds
    if (pxCoord.x >= ImageSize.x || pxCoord.y >= ImageSize.y)
        return;
    
    float4 InRGBA = texColorInput[pxCoord];
    float3 OutRGB = 0.0.xxx;

    [unroll]
    for (int kernelIt = 0; kernelIt < KERNEL_DIMENSION; ++kernelIt) // [0, KERNEL_DIMENSION)
    {
        int kernelOffset = kernelIt - KERNEL_RANGE_MINUS1; // [-(KERNEL_RANGE-1), KERNEL_RANGE-1]
        int kernelIndex = abs(kernelOffset); // [0, KERNEL_RANGE]
        int2 sampleCoord = int2(pxCoord.x, pxCoord.y + kernelOffset);

        sampleCoord.y = clamp(sampleCoord.y, 0, ImageSize.y - 1);

        float3 sampledColor = texColorInput[sampleCoord].rgb;
        OutRGB += Convolve1D(sampledColor, kernelIndex);
    }

#if TRANSPOSE_OUT
    pxCoord = pxCoord.yx;
#endif

    texColorOutput[pxCoord] = float4(OutRGB, InRGBA.a);
}
#endif // MULTI_PASS_SEPARABLE_FILTER

#if PASSTHROUGH
[numthreads(THREADGROUP_DIMENSION_X, THREADGROUP_DIMENSION_Y, 1)]
void CSMain_PassThrough(
    uint3 LocalThreadId    : SV_GroupThreadID,
    uint3 WorkGroupId      : SV_GroupID,
    uint3 DispatchThreadID : SV_DispatchThreadID)
{
    int2 pxCoord = DispatchThreadID.xy;
    // early out for out-of-image-bounds
    if (pxCoord.x >= ImageSize.x || pxCoord.y >= ImageSize.y)
        return;

    texColorOutput[pxCoord] = texColorInput[pxCoord];
}
#endif // PASSTHROUGH
