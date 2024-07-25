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

#include "declarations.h"
#include "common_types.h"


StructuredBuffer<uint> sobolBuffer : register(t0);
StructuredBuffer<uint> scramblingTileBuffer : register(t1);
StructuredBuffer<uint> rankingTileBuffer : register(t2);

RWTexture2D<float2> blueNoiseTexture : register(u0);

ConstantBuffer<FrameInfo> g_frame_info_cb[1] : register(b0);


// Blue Noise Sampler by Eric Heitz. Returns a value in the range [0, 1].
float SampleRandomNumber(uint pixel_i, uint pixel_j, uint sample_index, uint sample_dimension, uint samples_per_pixel) {
    // Wrap arguments
    pixel_i = pixel_i & 127u;
    pixel_j = pixel_j & 127u;
    sample_index = (sample_index % samples_per_pixel) & 255u;
    sample_dimension = sample_dimension & 255u;

#ifndef SPP
#    define SPP 256
#endif

#if SPP == 1
    const uint ranked_sample_index = sample_index ^ 0;
#else
    // xor index based on optimized ranking
    const uint ranked_sample_index = sample_index ^ rankingTileBuffer.Load(sample_dimension + (pixel_i + pixel_j * 128u) * 8u);
#endif

    // Fetch value in sequence
    uint value = sobolBuffer.Load(sample_dimension + ranked_sample_index * 256u);

    // If the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ scramblingTileBuffer.Load((sample_dimension % 8u) + (pixel_i + pixel_j * 128u) * 8u);

    // Convert to float and return
    return (value + 0.5f) / 256.0f;
}

[numthreads(8, 8, 1)] void main(uint3 dispatch_thread_id
    : SV_DispatchThreadID) {
    if (all(dispatch_thread_id.xy < 128)) {
        float2 xi = float2(
            SampleRandomNumber(dispatch_thread_id.x, dispatch_thread_id.y, g_frame_index, 0u, g_frame_info.random_samples_per_pixel),
            SampleRandomNumber(dispatch_thread_id.x, dispatch_thread_id.y, g_frame_index, 1u, g_frame_info.random_samples_per_pixel));
        blueNoiseTexture[dispatch_thread_id.xy] = xi.xy;
    }
}
