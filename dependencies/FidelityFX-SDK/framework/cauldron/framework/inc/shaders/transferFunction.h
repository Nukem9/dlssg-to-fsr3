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

float3 ApplyGamma(float3 color)
{
    color = pow(color, 1.0f / 2.2f);
    return color;
}

float3 ApplyPQ(float3 color)
{
    // Apply ST2084 curve
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;
    float3 cp = pow(abs(color.xyz), m1);
    color.xyz = pow((c1 + c2 * cp) / (1 + c3 * cp), m2);
    return color;
}

float3 ApplyscRGBScale(float3 color, float minLuminancePerNits, float maxLuminancePerNits)
{
    color.xyz = (color.xyz * (maxLuminancePerNits - minLuminancePerNits)) + float3(minLuminancePerNits, minLuminancePerNits, minLuminancePerNits);
    return color;
}

float3 RGBToYCoCg(in float3 rgb)
{
    return float3(0.25f * rgb.r + 0.5f * rgb.g + 0.25f * rgb.b, 0.5f * rgb.r - 0.5f * rgb.b, -0.25f * rgb.r + 0.5f * rgb.g - 0.25f * rgb.b);
}

float3 YCoCgToRGB(in float3 yCoCg)
{
    return float3(yCoCg.x + yCoCg.y - yCoCg.z, yCoCg.x + yCoCg.z, yCoCg.x - yCoCg.y - yCoCg.z);
}
