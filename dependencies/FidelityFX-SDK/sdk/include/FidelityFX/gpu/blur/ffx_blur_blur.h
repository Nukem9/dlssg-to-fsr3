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

FFX_BLUR_KERNEL_TYPE BlurLoadKernelWeight(FfxInt32 iKernelIndex)
{
    return FfxBlurLoadKernelWeight(iKernelIndex);
}

#if FFX_HALF

void BlurStoreOutput(FfxInt32x2 outPxCoord, FfxFloat16x3 color)
{
    FfxBlurStoreOutput(outPxCoord, color);
}

FfxFloat16x3 BlurLoadInput(FfxInt16x2 inPxCoord)
{
    return FfxBlurLoadInput(inPxCoord);
}

#else  // FFX_HALF

void BlurStoreOutput(FfxInt32x2 outPxCoord, FfxFloat32x3 color)
{
    FfxBlurStoreOutput(outPxCoord, color);
}

// DXIL generates load/sync/store blocks for each channel, ticket open: https://ontrack-internal.amd.com/browse/SWDEV-303837
// this is 10x times slower!!!
//void Blur_StoreOutput(FfxInt32x2 outPxCoord, FfxFloat32x3 color) { texColorOutput[outPxCoord].rgb = color;  }
FfxFloat32x3 BlurLoadInput(FfxInt32x2 inPxCoord)
{
    return FfxBlurLoadInput(inPxCoord);
}

#endif  // !FFX_HALF

#include "blur/ffx_blur.h"


void ffxBlurPass(FfxInt32x2 DispatchThreadID, FfxInt32x2 LocalThreadId, FfxInt32x2 WorkGroupId)
{
    ffxBlur(DispatchThreadID, LocalThreadId, WorkGroupId, ImageSize());
}
