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

#include "taacommon.h"

#define RADIUS      1
#define GROUP_SIZE  16
#define TILE_DIM    (2 * RADIUS + GROUP_SIZE)

//--------------------------------------------------------------------------------------
// Texture definitions
//--------------------------------------------------------------------------------------
Texture2D ColorBuffer :    register(t0);
Texture2D DepthBuffer :    register(t1);
Texture2D HistoryBuffer :  register(t2);
Texture2D VelocityBuffer : register(t3);

RWTexture2D<float4> OutputBuffer : register(u0);


SamplerState PointSampler :    register(s0);
SamplerState LinearSampler :  register(s1);


groupshared float3 Tile[TILE_DIM * TILE_DIM];

float2 GetClosestVelocity(in float2 uv, in float2 texelSize, out bool isSkyPixel)
{
    // Scale uv for lower resolution motion vector texture
#ifdef INVERTED_DEPTH
    float2 velocity;
    float closestDepth = 0.0f;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            const float2 st = uv + float2(x, y) * texelSize;
            const float depth = DepthBuffer.SampleLevel(PointSampler, st, 0.0f).x;
            if (depth > closestDepth)
            {
                velocity = VelocityBuffer.SampleLevel(PointSampler, st, 0.0f).xy;
                closestDepth = depth;
            }
        }
    isSkyPixel = (closestDepth == 0.0f);
    return velocity;
#else
    float2 velocity;
    float closestDepth = 1.0f;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            const float2 st = uv + float2(x, y) * texelSize;
            const float depth = DepthBuffer.SampleLevel(PointSampler, st, 0.0f).x;
            if (depth < closestDepth)
            {
                velocity = VelocityBuffer.SampleLevel(PointSampler, st, 0.0f).xy;
                closestDepth = depth;
            }
        }
    isSkyPixel = (closestDepth == 1.0f);
    return velocity;
#endif

}

/**********************************************************************
MIT License

Copyright(c) 2019 MJP

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
********************************************************************/
float3 SampleHistoryCatmullRom(in float2 uv, in float2 texelSize)
{
    // Source: https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
    // License: https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae

    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    float2 samplePos = uv / texelSize;
    float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    float2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    float2 w12 = w1 + w2;
    float2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    float2 texPos0 = texPos1 - 1.0f;
    float2 texPos3 = texPos1 + 2.0f;
    float2 texPos12 = texPos1 + offset12;

    texPos0 *= texelSize;
    texPos3 *= texelSize;
    texPos12 *= texelSize;

    float3 result = float3(0.0f, 0.0f, 0.0f);

    result += HistoryBuffer.SampleLevel(LinearSampler, float2(texPos0.x, texPos0.y), 0.0f).xyz * w0.x * w0.y;
    result += HistoryBuffer.SampleLevel(LinearSampler, float2(texPos12.x, texPos0.y), 0.0f).xyz * w12.x * w0.y;
    result += HistoryBuffer.SampleLevel(LinearSampler, float2(texPos3.x, texPos0.y), 0.0f).xyz * w3.x * w0.y;

    result += HistoryBuffer.SampleLevel(LinearSampler, float2(texPos0.x, texPos12.y), 0.0f).xyz * w0.x * w12.y;
    result += HistoryBuffer.SampleLevel(LinearSampler, float2(texPos12.x, texPos12.y), 0.0f).xyz * w12.x * w12.y;
    result += HistoryBuffer.SampleLevel(LinearSampler, float2(texPos3.x, texPos12.y), 0.0f).xyz * w3.x * w12.y;

    result += HistoryBuffer.SampleLevel(LinearSampler, float2(texPos0.x, texPos3.y), 0.0f).xyz * w0.x * w3.y;
    result += HistoryBuffer.SampleLevel(LinearSampler, float2(texPos12.x, texPos3.y), 0.0f).xyz * w12.x * w3.y;
    result += HistoryBuffer.SampleLevel(LinearSampler, float2(texPos3.x, texPos3.y), 0.0f).xyz * w3.x * w3.y;

    return max(result, 0.0f);
}

float3 Reinhard(in float3 hdr)
{
    return hdr / (hdr + 1.0f);
}

float3 Tap(in float2 pos)
{
    return Tile[int(pos.x) + TILE_DIM * int(pos.y)];
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void FirstCS(uint3 globalID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint localIndex : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
    bool isSkyPixel;

    // Populate private memory
    const float2 texelSize = 1.0f / float2(DisplayWidth, DisplayHeight);
    const float2 uv = (globalID.xy + 0.5f) * texelSize;
    const float2 tilePos = localID.xy + RADIUS + 0.5f;

    // Populate local memory
    if (localIndex < TILE_DIM * TILE_DIM / 4)
    {
        const int2 anchor = groupID.xy * GROUP_SIZE - RADIUS;

        const int2 coord1 = anchor + int2(localIndex % TILE_DIM, localIndex / TILE_DIM);
        const int2 coord2 = anchor + int2((localIndex + TILE_DIM * TILE_DIM / 4) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM / 4) / TILE_DIM);
        const int2 coord3 = anchor + int2((localIndex + TILE_DIM * TILE_DIM / 2) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM / 2) / TILE_DIM);
        const int2 coord4 = anchor + int2((localIndex + TILE_DIM * TILE_DIM * 3 / 4) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM * 3 / 4) / TILE_DIM);

        const float3 color0 = ColorBuffer[coord1].xyz;
        const float3 color1 = ColorBuffer[coord2].xyz;
        const float3 color2 = ColorBuffer[coord3].xyz;
        const float3 color3 = ColorBuffer[coord4].xyz;

        Tile[localIndex] = Reinhard(color0);
        Tile[localIndex + TILE_DIM * TILE_DIM / 4] = Reinhard(color1);
        Tile[localIndex + TILE_DIM * TILE_DIM / 2] = Reinhard(color2);
        Tile[localIndex + TILE_DIM * TILE_DIM * 3 / 4] = Reinhard(color3);
    }

    GroupMemoryBarrierWithGroupSync();
    const float3 center = Tap(tilePos);
    OutputBuffer[globalID.xy] = float4(center, 1.0);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void MainCS(uint3 globalID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint localIndex : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
    bool isSkyPixel;

    // Populate private memory
    const float2 texelSize = 1.0f / float2(DisplayWidth, DisplayHeight);
    const float2 velocityScale = float2(RenderWidth, RenderHeight) / float2(DisplayWidth, DisplayHeight);
    const float2 uv = (globalID.xy + 0.5f) * texelSize;
    const float2 tilePos = localID.xy + RADIUS + 0.5f;

    // Populate local memory
    if (localIndex < TILE_DIM * TILE_DIM / 4)
    {
        const int2 anchor = groupID.xy * GROUP_SIZE - RADIUS;

        const int2 coord1 = anchor + int2(localIndex % TILE_DIM, localIndex / TILE_DIM);
        const int2 coord2 = anchor + int2((localIndex + TILE_DIM * TILE_DIM / 4) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM / 4) / TILE_DIM);
        const int2 coord3 = anchor + int2((localIndex + TILE_DIM * TILE_DIM / 2) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM / 2) / TILE_DIM);
        const int2 coord4 = anchor + int2((localIndex + TILE_DIM * TILE_DIM * 3 / 4) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM * 3 / 4) / TILE_DIM);

        const float3 color0 = ColorBuffer[coord1].xyz;
        const float3 color1 = ColorBuffer[coord2].xyz;
        const float3 color2 = ColorBuffer[coord3].xyz;
        const float3 color3 = ColorBuffer[coord4].xyz;

        Tile[localIndex] = Reinhard(color0);
        Tile[localIndex + TILE_DIM * TILE_DIM / 4] = Reinhard(color1);
        Tile[localIndex + TILE_DIM * TILE_DIM / 2] = Reinhard(color2);
        Tile[localIndex + TILE_DIM * TILE_DIM * 3 / 4] = Reinhard(color3);
    }
    GroupMemoryBarrierWithGroupSync();

    // Iterate the neighboring samples
    if (any(int2(globalID.xy) >= float2(RenderWidth, RenderHeight)))
        return; // out of bounds

    float wsum = 0.0f;
    float3 vsum = float3(0.0f, 0.0f, 0.0f);
    float3 vsum2 = float3(0.0f, 0.0f, 0.0f);

    for (float y = -RADIUS; y <= RADIUS; ++y)
        for (float x = -RADIUS; x <= RADIUS; ++x)
        {
            const float3 neigh = Tap(tilePos + float2(x, y));
            const float w = exp(-3.0f * (x * x + y * y) / ((RADIUS + 1.0f) * (RADIUS + 1.0f)));
            vsum2 += neigh * neigh * w;
            vsum += neigh * w;
            wsum += w;
        }

    // Calculate mean and standard deviation
    const float3 ex = vsum / wsum;
    const float3 ex2 = vsum2 / wsum;
    const float3 dev = sqrt(max(ex2 - ex * ex, 0.0f));

    const float2 velocity = GetClosestVelocity(uv, texelSize, isSkyPixel) * velocityScale;
    const float boxSize = lerp(0.5f, 2.5f, isSkyPixel ? 0.0f : smoothstep(0.02f, 0.0f, length(velocity)));

    // Reproject and clamp to bounding box
    const float3 nmin = ex - dev * boxSize;
    const float3 nmax = ex + dev * boxSize;

    const float3 history = SampleHistoryCatmullRom(uv + velocity, texelSize);
    const float3 clampedHistory = clamp(history, nmin, nmax);
    const float3 center = Tap(tilePos); // retrieve center value
    const float3 result = lerp(clampedHistory, center, 1.0f / 16.0f);

    // Write antialised sample to memory
    OutputBuffer[globalID.xy] = float4(result, 1.0f);
}
