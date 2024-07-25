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

// Helpers
float2 GetUV(uint2 coord, float2 texelSize)
{
    return (coord + 0.5f) * texelSize;
}

int2 GetScreenCoordinates(float2 uv, int2 textureDims)
{
    return floor(uv * textureDims);
}

//--------------------------------------------------------------------------------------
// I/O Structures
//--------------------------------------------------------------------------------------
struct VertexOut
{
    float4 PosOut   : SV_Position;
    float2 UVOut    : TEXCOORD;
};

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------

static const float4 FullScreenVertsPos[3] = { float4(-1.0, 1.0, FAR_DEPTH, 1.0), float4(3.0, 1.0, FAR_DEPTH, 1.0), float4(-1.0, -3.0, FAR_DEPTH, 1.0) };
static const float2 FullScreenVertsUVs[3] = { float2(0.0, 0.0), float2(2.0, 0.0), float2(0.0, 2.0) };

VertexOut FullscreenVS(uint vertexId : SV_VertexID)
{
    VertexOut outVert;
    outVert.PosOut = FullScreenVertsPos[vertexId];
    outVert.UVOut = FullScreenVertsUVs[vertexId];
    return outVert;
}
