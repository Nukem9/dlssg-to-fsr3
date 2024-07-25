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

#include "parallelsort_common.h"

Texture2D<float4>       ValidationTexture   : register(t0);
StructuredBuffer<uint>  SortBuffer          : register(t1);

struct VertexOut
{
    float4 PosOut   : SV_POSITION;
    float2 UVOut    : TEXCOORD;
};

static const float4 FullScreenVertsPos[3] = { float4(-1, 1, FAR_DEPTH, 1), float4(3, 1, FAR_DEPTH, 1), float4(-1, -3, FAR_DEPTH, 1) };
static const float2 FullScreenVertsUVs[3] = { float2(0, 0), float2(2, 0), float2(0, 2) };

VertexOut FullscreenVS(uint vertexId : SV_VertexID)
{
    VertexOut outVert;
    outVert.PosOut  = FullScreenVertsPos[vertexId];
    outVert.UVOut   = FullScreenVertsUVs[vertexId];
    return outVert;
}

float4 RenderSortValidationPS(VertexOut vertexIn) : SV_Target
{
    float4 outColor = float4(0, 0, 0, 0);
    
    // When calculating the coordinates to use to lookup sort data, 
    // always aim to keep the results centered on screen (to account for users
    // resizing the window, or dealing with sort size bigger than our current window)
    int2 uvCoord = vertexIn.UVOut * int2(Width, Height);

    // xRes > sort width
    int xStart, yStart;
    xStart = (Width - SortWidth) / 2;     // Will be positive when screen width is larger than our key source, and negative when smaller
    yStart = (Height - SortHeight) / 2;   // Will be positive when screen height is larger than our key source, and negative when smaller

    int2 lookupCoord = uvCoord.xy - int2(xStart, yStart);

    if (lookupCoord.x >= 0 && lookupCoord.y >= 0 && lookupCoord.x < SortWidth && lookupCoord.y < SortHeight)
    {
        int value   = SortBuffer[lookupCoord.y* SortWidth + lookupCoord.x];
        int height  = value / SortWidth;
        int2 uv     = int2(value - (height * SortWidth), height);
        outColor    = ValidationTexture[uv];
    }

    return pow(outColor, 2.2);
}
