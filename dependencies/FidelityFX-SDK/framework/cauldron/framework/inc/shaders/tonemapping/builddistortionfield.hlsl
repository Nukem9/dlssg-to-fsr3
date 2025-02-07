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

#include "tonemappercommon.h"
#include "lensdistortion.h"

//--------------------------------------------------------------------------------------
// Texture definitions
//--------------------------------------------------------------------------------------
RWTexture2D<float2> OutputTexture : register(u0);

bool IsInsideLetterbox(int2 pixel)
{
    if (pixel.x > LetterboxRectBase.x && pixel.y > LetterboxRectBase.y &&
        pixel.x < LetterboxRectBase.x + LetterboxRectSize.x &&
        pixel.y <= LetterboxRectBase.y + LetterboxRectSize.y)
        return true;

    return false;
}

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------
[numthreads(NUM_THREAD_X, NUM_THREAD_Y, 1)]
void MainCS(uint3 dtID : SV_DispatchThreadID)
{
    const uint2 pixel = dtID.xy;

    float2 distortionField = float2(0.0f, 0.0f);
    if (IsInsideLetterbox(pixel))
    {
        float2 uv = (pixel + 0.5f) / LetterboxRectSize;
        distortionField = GenerateDistortionField(uv);
    }
    OutputTexture[pixel] = distortionField;
}