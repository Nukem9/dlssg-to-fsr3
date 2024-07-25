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

#define FFX_GPU  1
#define FFX_HLSL 1
#include "fidelityfx/ffx_core.h"

//--------------------------------------------------------------------------------------
// Texture definitions
//--------------------------------------------------------------------------------------
RWTexture2D<uint> AutomaticExposureSpdAtomicCounter : register(u0);
RWTexture2D<float> AutomaticExposureMipsShadingChange : register(u1);
RWTexture2D<float> AutomaticExposureMips5 : register(u2);
RWTexture2D<float2> AutomaticExposureValue : register(u3);

Texture2D<float4>   InputTexture  : register(t0);

SamplerState s_LinearClamp : register(s0);

float ComputeAutoExposureFromLavg(float Lavg)
{
    Lavg = exp(Lavg);

    const float S = 100.0f; //ISO arithmetic speed
    const float K = 12.5f;
    float ExposureISO100 = log2((Lavg * S) / K);

    const float q = 0.65f;
    float Lmax = (78.0f / (q * S)) * pow(2.0f, ExposureISO100);

    return 1 / Lmax;
}

groupshared uint spdCounter;

void SpdIncreaseAtomicCounter(uint slice)
{
    InterlockedAdd(AutomaticExposureSpdAtomicCounter[int2(0, 0)], 1, spdCounter);
}

uint SpdGetAtomicCounter()
{
    return spdCounter;
}

void SpdResetAtomicCounter(uint slice)
{
    AutomaticExposureSpdAtomicCounter[int2(0, 0)] = 0;
}

groupshared float spdIntermediateR[16][16];
groupshared float spdIntermediateG[16][16];
groupshared float spdIntermediateB[16][16];
groupshared float spdIntermediateA[16][16];

float3 SampleInputColor(float2 fUV)
{
    return InputTexture.SampleLevel(s_LinearClamp, fUV, 0).rgb;
}

float RGBToLuma(float3 fLinearRgb)
{
    return dot(fLinearRgb, float3(0.2126f, 0.7152f, 0.0722f));
}

float2 ClampUv(float2 fUv, int2 iTextureSize, int2 iResourceSize)
{
    const float2 fSampleLocation = fUv * iTextureSize;
    const float2 fClampedLocation = max(float2(0.5f, 0.5f), min(fSampleLocation, float2(iTextureSize) - float2(0.5f, 0.5f)));
    const float2 fClampedUv = fClampedLocation / float2(iResourceSize);

    return fClampedUv;
}

float4 SpdLoadSourceImage(float2 tex, uint slice)
{
    float2 fUv = (tex + 0.5f) / renderSize;
    fUv = ClampUv(fUv, renderSize, renderSize);
    float3 fRgb = SampleInputColor(fUv);

    //compute log luma
    const float fLogLuma = log(max(1e-03f, RGBToLuma(fRgb)));

    // Make sure out of screen pixels contribute no value to the end result
    const float result = all(tex < renderSize) ? fLogLuma : 0.0f;

    return float4(result, 0, 0, 0);
}

float4 SpdLoad(int2 tex, uint slice)
{
    return float4(AutomaticExposureMips5[tex], 0, 0, 0);
}

void SpdStore(int2 pix, float4 outValue, uint index, uint slice)
{
    if (index == 4)
    {
        AutomaticExposureMipsShadingChange[pix] = outValue.r;

    }
    else if (index == 5)
    {
        AutomaticExposureMips5[pix] = outValue.r;
    }

    if (index == mips - 1)
    { //accumulate on 1x1 level

        if (all(pix == int2(0, 0)))
        {
            float prev = AutomaticExposureValue[int2(0, 0)].y;
            float result = outValue.r;

            if (prev < 1e8f) // Compare Lavg, so small or negative values
            {
                float rate = 1.0f;
                result = prev + (result - prev);
            }
            float2 spdOutput = float2(ComputeAutoExposureFromLavg(result), result);
            AutomaticExposureValue[int2(0, 0)] = spdOutput;
        }
    }
}

float4 SpdLoadIntermediate(uint x, uint y)
{
    return float4(
        spdIntermediateR[x][y],
        spdIntermediateG[x][y],
        spdIntermediateB[x][y],
        spdIntermediateA[x][y]);
}

void SpdStoreIntermediate(uint x, uint y, float4 value)
{
    spdIntermediateR[x][y] = value.x;
    spdIntermediateG[x][y] = value.y;
    spdIntermediateB[x][y] = value.z;
    spdIntermediateA[x][y] = value.w;
}

float4 SpdReduce4(float4 v0, float4 v1, float4 v2, float4 v3)
{
    return (v0 + v1 + v2 + v3) * 0.25f;
}

#include "fidelityfx/spd/ffx_spd.h"

void ComputeAutoExposure(uint3 WorkGroupId, uint LocalThreadIndex)
{
    SpdDownsample(WorkGroupId.xy, LocalThreadIndex, mips, numWorkGroups, (uint) WorkGroupId.z, workGroupOffset);
}

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------
[numthreads(NUM_THREAD_X, 1, 1)]
void MainCS(uint3 WorkGroupId : SV_GroupID, uint LocalThreadIndex : SV_GroupIndex)
{
    ComputeAutoExposure(WorkGroupId, LocalThreadIndex);
}
