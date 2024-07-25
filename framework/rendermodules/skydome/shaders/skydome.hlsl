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

#include "fullscreen.hlsl"
#include "skydomecommon.h"

//--------------------------------------------------------------------------------------
// Texture definitions
//--------------------------------------------------------------------------------------
TextureCube  SkyTexture    : register(t0);
SamplerState SamLinearWrap : register(s0);

//--------------------------------------------------------------------------------------
// I/O Structures
//--------------------------------------------------------------------------------------
struct SkydomeVertexOut
{
    float4 PosOut : SV_Position;
    float4 pixelDir : TEXCOORD0;
};

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------


SkydomeVertexOut MainVS(uint vertexId : SV_VertexID)
{
    SkydomeVertexOut outVert;
    outVert.PosOut = FullScreenVertsPos[vertexId];
    const float2 uv = FullScreenVertsUVs[vertexId];
    float4 clip = float4(2 * uv.x - 1, 1 - 2 * uv.y, FAR_DEPTH, 1);
    outVert.pixelDir  = mul(ClipToWorld, clip);
    return outVert;
}

float4 MainPS(SkydomeVertexOut vertexIn) : SV_Target
{
    return SkyTexture.SampleLevel(SamLinearWrap, vertexIn.pixelDir.xyz, 0);
}
