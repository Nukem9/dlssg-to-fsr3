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

Texture2D<float4> r_currBB : register(t0);
Texture2D<float4> r_uiTexture : register(t1);

float4 mainVS(uint vertexId : SV_VertexID) : SV_POSITION
{
    return float4((int) (vertexId & 1) * 4 - 1, (int) (vertexId & 2) * (-2) + 1, 0.5, 1);
}

float4 mainPS(float4 vPosition : SV_POSITION) : SV_Target
{
    float3 color = r_currBB[vPosition.xy].rgb;
    float4 guiColor = r_uiTexture[vPosition.xy];

#if FFX_UI_PREMUL
    return float4((1.0f - guiColor.a) * color + guiColor.rgb, 1); // premul
#else
    return float4(lerp(color, guiColor.rgb, guiColor.a), 1); // blend (original)
#endif
}
