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

#include "tonemappers.hlsl"
#include "tonemappercommon.h"
#include "transferfunction.h"
#include "lensdistortion.h"

//--------------------------------------------------------------------------------------
// Texture definitions
//--------------------------------------------------------------------------------------
Texture2D<float2> AutomaticExposureValue : register(t0);
Texture2D<float4> InputTexture : register(t1);

RWTexture2D<float4> OutputTexture : register(u0);

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------
[numthreads(NUM_THREAD_X, NUM_THREAD_Y, 1)]
void MainCS(uint3 dtID : SV_DispatchThreadID)
{
    const int2 coord = dtID.xy;
    float4     color;

    if (coord.x < LetterboxRectBase.x || coord.y < LetterboxRectBase.y || coord.x >= LetterboxRectBase.x + LetterboxRectSize.x ||
        coord.y >= LetterboxRectBase.y + LetterboxRectSize.y)
    {
        color = float4(0.0f.xxx, 1.0f);
    }
    else
    {
        int2   coordInLetterbox = dtID.xy - LetterboxRectBase;

        if (LensDistortionEnabled)
        {
            const float2 uvInLetterbox = (coordInLetterbox + 0.5f) / LetterboxRectSize;
            const float2 distortedUvInLetterbox = ApplyLensDistortion(uvInLetterbox);
            coordInLetterbox = distortedUvInLetterbox * LetterboxRectSize;
        }
        const float4 texColor         = InputTexture[coordInLetterbox];

        const float2 autoExposure = AutomaticExposureValue[int2(0, 0)];

        
        color.xyz = Tonemap(texColor.rgb, UseAutoExposure ? autoExposure.x : Exposure, ToneMapper);
        color.a   = texColor.a;

        switch (MonitorDisplayMode)
        {
        case DisplayMode::DISPLAYMODE_LDR:
            color.xyz = ApplyGamma(color.xyz);
            break;

        case DisplayMode::DISPLAYMODE_HDR10_SCRGB:
        case DisplayMode::DISPLAYMODE_FSHDR_SCRGB:
            color.xyz = ApplyscRGBScale(color.xyz, 0.0f, DisplayMaxLuminance / 80.0f);
            break;

        case DisplayMode::DISPLAYMODE_HDR10_2084:
        case DisplayMode::DISPLAYMODE_FSHDR_2084:
            // Convert to rec2020 colour space
            color.xyz = mul(ContentToMonitorRecMatrix, color).xyz;

            // DisplayMaxLuminancePerNits = max nits set / 80
            // ST2084 convention when they define PQ formula is 1 = 10,000 nits.
            // MS/driver convention is 1 = 80 nits, so when we apply the formula, we need to scale by 80/10000 to get us on the same page as ST2084 spec
            // So, scaling should be color *= (nits_when_color_is_1 / 10,000).
            // Since we want our color which is normalized so that 1.0 = max nits in above cbuffer to be mapped on the display, we need to do the below calc
            // Lets look at whole calculation with example: max nits = 1000, color = 1
            // 1 * ((1000 / 80) * (80 / 10000)) = 1 / 10 = 0.1
            // For simplcity we are getting rid of conversion to per 80 nit division factor and directly dividing max luminance set by 10000 nits
            color.xyz *= (DisplayMaxLuminance / 10000.0f);
            color.xyz = ApplyPQ(color.xyz);
            break;
        }
    }

    OutputTexture[coord] = color;
}
