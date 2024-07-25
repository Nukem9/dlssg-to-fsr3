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

static const uint FFX_VARIABLESHADING_RATE1D_1X = 0x0;
static const uint FFX_VARIABLESHADING_RATE1D_2X = 0x1;
static const uint FFX_VARIABLESHADING_RATE1D_4X = 0x2;
#define FFX_VARIABLESHADING_MAKE_SHADING_RATE(x,y) ((x << 2) | (y))

static const uint FFX_VARIABLESHADING_RATE_1X1 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_1X, FFX_VARIABLESHADING_RATE1D_1X); // 0;
static const uint FFX_VARIABLESHADING_RATE_1X2 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_1X, FFX_VARIABLESHADING_RATE1D_2X); // 0x1;
static const uint FFX_VARIABLESHADING_RATE_2X1 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_1X); // 0x4;
static const uint FFX_VARIABLESHADING_RATE_2X2 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_2X); // 0x5;
static const uint FFX_VARIABLESHADING_RATE_2X4 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_4X); // 0x6;
static const uint FFX_VARIABLESHADING_RATE_4X2 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_4X, FFX_VARIABLESHADING_RATE1D_2X); // 0x9;
static const uint FFX_VARIABLESHADING_RATE_4X4 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_4X, FFX_VARIABLESHADING_RATE1D_4X); // 0xa;

// Constant Buffer
cbuffer CBOverlayInformation : register(b0)
{
    uint2   g_Resolution;
    uint    g_TileSize;
}

Texture2D<uint> inU8 : register(t0);

struct VERTEX_OUT
{
    float4 vPosition : SV_POSITION;
};

VERTEX_OUT mainVS(uint id : SV_VertexID)
{
    VERTEX_OUT output;
    output.vPosition = float4(float2(id & 1, id >> 1) * float2(4, -4) + float2(-1, 1), 0, 1);
    return output;
}

float4 mainPS(VERTEX_OUT input) : SV_Target
{
    int2 pos = input.vPosition.xy / g_TileSize;
    // encode different shading rates as colors
    float3 color = float3(1, 1, 1);

    switch (inU8[pos].r)
    {
    case FFX_VARIABLESHADING_RATE_1X1:
        color = float3(0.5, 0.0, 0.0);
        break;
    case FFX_VARIABLESHADING_RATE_1X2:
        color = float3(0.5, 0.5, 0.0);
        break;
    case FFX_VARIABLESHADING_RATE_2X1:
        color = float3(0.5, 0.25, 0.0);
        break;
    case FFX_VARIABLESHADING_RATE_2X2:
        color = float3(0.0, 0.5, 0.0);
        break;
    case FFX_VARIABLESHADING_RATE_2X4:
        color = float3(0.25, 0.25, 0.5);
        break;
    case FFX_VARIABLESHADING_RATE_4X2:
        color = float3(0.5, 0.25, 0.5);
        break;
    case FFX_VARIABLESHADING_RATE_4X4:
        color = float3(0.0, 0.5, 0.5);
        break;
    }

    // add grid
    int2 grid = int2(input.vPosition.xy) % g_TileSize;
    bool border = (grid.x == 0) || (grid.y == 0);

    return float4(color, 0.5) * (border ? 0.5f : 1.0f);
}
