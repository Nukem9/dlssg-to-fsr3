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

#include "../shaders/raytracingcommon.hlsl"

cbuffer cbTilesDebug : register(b0)
{
    int debugMode;
}

StructuredBuffer<uint4> tilesBuffer : register(t0);
RWTexture2D<float4> output : register(u0);

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------

[numthreads(TILE_SIZE_X, TILE_SIZE_Y, 1)]
void MainCS(uint3 globalID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint localIndex : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
    const Tile currentTile = Tile::FromUint(tilesBuffer[groupID.x]);

    const uint2 pixelCoord = currentTile.location * k_tileSize + localID.xy;

    const float zScale = 0.01;
    float4 color = float4(0, 0, 0, 0);
    if (debugMode == 1)
    {
        const bool tracePixel = WaveMaskToBool(currentTile.mask, localID.xy);
        color = (tracePixel == false) ? float4(1, 0, 0, 1) : float4(0, 1, 0, 1);
    }
    else if (debugMode == 2)
    {
        const float t = saturate(currentTile.minT * zScale);
        color = float4(t.xxx, 1);
    }
    else if (debugMode == 3)
    {
        const float t = saturate(currentTile.maxT * zScale);
        color = float4(t.xxx, 1);
    }
    else if (debugMode == 4)
    {
        const float diff = (currentTile.maxT - currentTile.minT);
        const float t = saturate(diff * zScale * 10);
        color = float4(t.xxx, 1);
    }

    output[pixelCoord] = color;
}
