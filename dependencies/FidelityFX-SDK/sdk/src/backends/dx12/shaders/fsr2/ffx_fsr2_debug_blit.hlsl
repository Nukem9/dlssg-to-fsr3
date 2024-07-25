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

Texture2D<float4>   r_in            : register(t0);
RWTexture2D<float4> rw_out          : register(u0);

cbuffer cbBlit0 : register(b0) {

    int         g_outChannelRed;
    int         g_outChannelGreen;
    int         g_outChannelBlue;
    float2      outChannelRedMinMax;
    float2      outChannelGreenMinMax;
    float2      outChannelBlueMinMax;
};

[numthreads(8, 8, 1)]
void CS(uint3 globalID : SV_DispatchThreadID)
{
    float4 srcColor = r_in[globalID.xy];

    // remap channels
    float4 dstColor = float4(srcColor[g_outChannelRed], srcColor[g_outChannelGreen], srcColor[g_outChannelBlue], 1.f);

    // apply offset and scale
    dstColor.rgb -= float3(outChannelRedMinMax.x, outChannelGreenMinMax.x, outChannelBlueMinMax.x);
    dstColor.rgb /= float3(outChannelRedMinMax.y - outChannelRedMinMax.x, outChannelGreenMinMax.y - outChannelGreenMinMax.x, outChannelBlueMinMax.y - outChannelBlueMinMax.x);

    rw_out[globalID.xy] = float4(dstColor);
}
