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
// I/O Structures
//--------------------------------------------------------------------------------------
struct VertexOut
{
    float4 PosOut       : SV_Position;
    float4 SampleOut    : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Constant Buffer
//--------------------------------------------------------------------------------------
cbuffer constants : register(b0)
{
    SPDVerifyConstants VerifyInfo;
}

//--------------------------------------------------------------------------------------
// Main vertex function
//--------------------------------------------------------------------------------------
static const uint QuadIndices[6] = { 0, 1, 2, 1, 3, 2 };

// Define if a quad vert has a scaled component
static const float2 QuadScales[4] =
{
    float2(0.f, 0.f),
    float2(1.f, 0.f),
    float2(0.f, 1.f),
    float2(1.f, 1.f),
};

// Mip representation sizes (in screen space percentage)
#define SPD_MAX_MIP_LEVELS 12   // If this ever needs to be changed, change must also
                                // be reflected in ffx_spd_resources.h and ffx_spd.h
static const float MipRepSize[SPD_MAX_MIP_LEVELS] =
{
    0.9f,
    0.45f,
    0.225f,
    0.1125f,
    0.05625f,
    0.028125f,
    0.0140625f,
    0.00703125f,
    0.003515625f,
    0.0017578125f,
    0.00087890625f,
    0.000439453125f,
};

static const float2 QuadUVs[4] = 
{ 
    float2(0.0, 0.0), 
    float2(1.0, 0.0), 
    float2(0.0, 1.0), 
    float2(1.0, 1.0),
};

#define HorizontalOffsetStart   0.1f    // Allow 20% of screen on the for UI
#define VerticalOffsetStart     0.05    // Allow 15% of screen on top/bottom of largest quad

VertexOut MainVS(uint vertexId : SV_VertexID, uint quadId : SV_InstanceID)
{
    // Figure out which vert in the quad this is
    uint quadVertId = vertexId % 6;

    // Calculate the size of the quad
    float2 quadSize = float2(MipRepSize[quadId] * VerifyInfo.InvAspectRatio, MipRepSize[quadId]);

    // Calculate the offset for this quad
    float2 quadOffset = float2(0.f, 0.f);
    for (uint i = 0; i < quadId; ++i)
    {
        if (i & 0x01)
            quadOffset.y += MipRepSize[i];
        else
            quadOffset.x += MipRepSize[i];
    }

    // Figure out location of vert (offset and scale)
    float2 vertPos = float2(HorizontalOffsetStart + quadOffset.x * VerifyInfo.InvAspectRatio, VerticalOffsetStart + quadOffset.y) + QuadScales[QuadIndices[vertexId]] * quadSize;

    // Scale to NDC
    float2 vertPosNDC = vertPos * 2.f - 1.f;
    vertPosNDC.y = -vertPosNDC.y; // Flip the Y

    VertexOut outVert;
    outVert.PosOut = float4(vertPosNDC, FAR_DEPTH, 1.f);
    outVert.SampleOut.xyz = float3(QuadUVs[QuadIndices[vertexId]], quadId);
    return outVert;
}

//--------------------------------------------------------------------------------------
// Texture definitions
//--------------------------------------------------------------------------------------
Texture2DArray                   inputTex        : register(t0);
SamplerState                     linearSampler   : register(s0);

//--------------------------------------------------------------------------------------
// Main pixel function
//--------------------------------------------------------------------------------------
float4 MainPS(VertexOut input) : SV_Target
{
    // Sample the correct mip from the right slice
    float4 result = inputTex.SampleLevel(linearSampler, float3(input.SampleOut.xy, VerifyInfo.SliceId), input.SampleOut.z);
    return result;
}
