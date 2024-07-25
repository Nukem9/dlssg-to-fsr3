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

#include "ffx_core.h"

#define LPM_NO_SETUP 1
#include "lpm/ffx_lpm.h"

#if FFX_HALF
    void CurrFilter(FfxUInt32x4 twoPixelPosInQuad)
    {
        FfxFloat16x4 pixelOneColor = LoadInput(twoPixelPosInQuad.xy);
        FfxFloat16x4 pixelTwoColor = LoadInput(twoPixelPosInQuad.zw);

        FfxFloat16x2 redPair = FfxFloat16x2(pixelOneColor.r, pixelTwoColor.r);
        FfxFloat16x2 greenPair = FfxFloat16x2(pixelOneColor.g, pixelTwoColor.g);
        FfxFloat16x2 bluePair = FfxFloat16x2(pixelOneColor.b, pixelTwoColor.b);

        LpmFilterH(redPair, greenPair, bluePair, GetShoulder(), GetCon(), GetSoft(), GetCon2(), GetClip(), GetScaleOnly());

        pixelOneColor.rgb = FfxFloat16x3(redPair.x, greenPair.x, bluePair.x);
        pixelTwoColor.rgb = FfxFloat16x3(redPair.y, greenPair.y, bluePair.y);

        switch (GetMonitorDisplayMode())
        {
        case 0: // Corresponds to FFX_LPM_DISPLAYMODE_LDR
            pixelOneColor.xyz = ApplyGamma(pixelOneColor.xyz);
            pixelTwoColor.xyz = ApplyGamma(pixelTwoColor.xyz);
            break;

        case 1: // Corresponds to FFX_LPM_DISPLAYMODE_HDR10_2084
        case 3: // Corresponds to FFX_LPM_DISPLAYMODE_FSHDR_2084
            pixelOneColor.xyz = ApplyPQ(pixelOneColor.xyz);
            pixelTwoColor.xyz = ApplyPQ(pixelTwoColor.xyz);
            break;
        }

        StoreOutput(twoPixelPosInQuad.xy, pixelOneColor);
        StoreOutput(twoPixelPosInQuad.zw, pixelTwoColor);
    }
#else
    void CurrFilter(FfxUInt32x2 onePixelPosInQuad)
    {
        FfxFloat32x4 color = LoadInput(onePixelPosInQuad);
        LpmFilter(color.r, color.g, color.b, GetShoulder(), GetCon(), GetSoft(), GetCon2(), GetClip(), GetScaleOnly());

        switch (GetMonitorDisplayMode())
        {
        case 0: // Corresponds to FFX_LPM_DISPLAYMODE_LDR
            color.xyz = ApplyGamma(color.xyz);
            break;

        case 1: // Corresponds to FFX_LPM_DISPLAYMODE_HDR10_2084
        case 3: // Corresponds to FFX_LPM_DISPLAYMODE_FSHDR_2084
            color.xyz = ApplyPQ(color.xyz);
            break;
        }

        StoreOutput(onePixelPosInQuad, color);
    }
#endif

void LPMFilter(FfxUInt32x3 LocalThreadId, FfxUInt32x3 WorkGroupId, FfxUInt32x3 Dtid)
{
    #if FFX_HALF
        // Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
        // But this time for fp16 since LPM can output two colors at a time, we will store coordinates of two pixels at a time
        FfxUInt32x4 twoPixelPosInQuad;

        // Get top left pixel in quad and store in xy component
        twoPixelPosInQuad.xy = ffxRemapForQuad(LocalThreadId.x) + FfxUInt32x2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);

        // Get top right pixel in quad and store in zw component
        twoPixelPosInQuad.zw = twoPixelPosInQuad.xy;
        twoPixelPosInQuad.z += 8u;

        // now call lpm fp16 on both together
        CurrFilter(twoPixelPosInQuad);

        // Get bottom right pixel in quad and store in xy component
        twoPixelPosInQuad.xy = twoPixelPosInQuad.zw;
        twoPixelPosInQuad.y += 8u;

        // Get bottom left pixel in quad and store in zw component
        twoPixelPosInQuad.zw = twoPixelPosInQuad.xy;
        twoPixelPosInQuad.z -= 8u;

        // now call lpm fp16 on both together
        CurrFilter(twoPixelPosInQuad);
    #else
        // Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
        FfxUInt32x2 onePixelPosInQuad;

        // Get top left pixel in quad
        onePixelPosInQuad = ffxRemapForQuad(LocalThreadId.x) + FfxUInt32x2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);
        CurrFilter(onePixelPosInQuad);

        // Get top right
        onePixelPosInQuad.x += 8u;
        CurrFilter(onePixelPosInQuad);

        // Get bottom right
        onePixelPosInQuad.y += 8u;
        CurrFilter(onePixelPosInQuad);

        // Get bottom left
        onePixelPosInQuad.x -= 8u;
        CurrFilter(onePixelPosInQuad);
    #endif
}
