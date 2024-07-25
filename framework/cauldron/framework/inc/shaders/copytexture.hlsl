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

#include "shadercommon.h"
#include "transferFunction.h"
#include "fullscreen.hlsl"

cbuffer CBSwapchain : register(b0)
{
    SwapchainCBData swapchainCBData;
}

Texture2D<float4> TextureImg : register(t0);

float4 CopyTextureToSwapChainPS(VertexOut VertexIn) : SV_Target
{
    float4 color = TextureImg[VertexIn.PosOut.xy];

	// Needed now?
    /*switch (swapchainCBData.displayMode)
    {
        case DisplayMode::DISPLAYMODE_LDR:
        case DisplayMode::DISPLAYMODE_HDR10_SCRGB:
        case DisplayMode::DISPLAYMODE_FSHDR_SCRGB:
            break;

        case DisplayMode::DISPLAYMODE_HDR10_2084:
        case DisplayMode::DISPLAYMODE_FSHDR_2084:
            // Apply ST2084 curve
            color.xyz = ApplyPQ(color.xyz);
            break;
    }*/

    return color;
}

Texture2D<float4> inputTexture  : register(t0);
SamplerState linearSampler      : register(s0);
float4 CopyTexturePS(VertexOut input) : SV_Target
{
    float2 texCoord = input.UVOut;
    return inputTexture.Sample(linearSampler, texCoord);
}

Texture2D<float4>   copyTextureCSInput  : register(t0);
RWTexture2D<float4> copyTextureCSOutput : register(u0);

[numthreads(8, 8, 1)]
void CopyTextureCS(uint2 pixelCoordinate : SV_DispatchThreadID) {
    copyTextureCSOutput[pixelCoordinate.xy] = copyTextureCSInput[pixelCoordinate.xy];
}