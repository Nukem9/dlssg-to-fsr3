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

cbuffer input : register(b0)
{
    uint frameIndex;
};

float4 mainVS(uint vertexId : SV_VertexID) : SV_POSITION
{
    return float4((int) (vertexId & 1) * 4 - 1, (int) (vertexId & 2) * (-2) + 1, 0.5, 1);
}

float4 mainPS(float4 vPosition: SV_POSITION) : SV_Target
{
    float a = frameIndex & 1;
    float b = (~frameIndex) & 1;

    if (vPosition.x < 16.0f)
    {
        // Alternate between magenta and green 
        return float4(b, a, b, 1.0f);
    }
    else if (vPosition.x < 32.0f)
    {
        // Alternate between black and white
        return float4(a, a, a, 1);
    }

    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
