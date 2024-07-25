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

#include "ffx_sssr_common.h"

#define GOLDEN_RATIO                       1.61803398875f

// Blue Noise Sampler by Eric Heitz. Returns a value in the range [0, 1].
FfxFloat32 SampleRandomNumber(FfxUInt32 pixel_i, FfxUInt32 pixel_j, FfxUInt32 sample_index, FfxUInt32 sample_dimension) {
    // Wrap arguments
    pixel_i = pixel_i & 127u;
    pixel_j = pixel_j & 127u;
    sample_index = sample_index & 255u;
    sample_dimension = sample_dimension & 255u;

    // xor index based on optimized ranking
    const FfxUInt32 ranked_sample_index = sample_index;

    // Fetch value in sequence
    FfxUInt32 value = FFX_SSSR_GetSobolSample(FfxUInt32x3(sample_dimension, ranked_sample_index * 256u, 0));
    

    // If the dimension is optimized, xor sequence value based on optimized scrambling
    FfxUInt32 originalIndex = (sample_dimension % 8u) + (pixel_i + pixel_j * 128u) * 8u;
    value = value ^ FFX_SSSR_GetScramblingTile(FfxUInt32x3(originalIndex % 512u, originalIndex / 512u, 0));

    // Convert to FfxFloat32 and return
    return (value + 0.5f) / 256.0f;
}

FfxFloat32x2 SampleRandomVector2D(FfxUInt32x2 pixel) {
    FfxFloat32x2 u = FfxFloat32x2(
        FFX_MODULO(SampleRandomNumber(pixel.x, pixel.y, 0, 0u) + (FrameIndex() & 0xFFu) * GOLDEN_RATIO, 1.0f),
        FFX_MODULO(SampleRandomNumber(pixel.x, pixel.y, 0, 1u) + (FrameIndex() & 0xFFu) * GOLDEN_RATIO, 1.0f));
    return u;
}

void PrepareBlueNoiseTexture(FfxUInt32x2 dispatch_thread_id) {
    FFX_SSSR_StoreBlueNoiseSample(dispatch_thread_id, SampleRandomVector2D(dispatch_thread_id));
}
