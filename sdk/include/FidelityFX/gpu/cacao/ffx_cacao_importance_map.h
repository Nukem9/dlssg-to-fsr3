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
#include "ffx_cacao_defines.h"
#include "ffx_cacao_utils.h"

void FFX_CACAO_GenerateImportanceMap(FfxUInt32x2 tid)
{
	FfxUInt32x2 basePos = tid * 2;

	FfxFloat32x2 baseUV = (FfxFloat32x2(basePos) + 1.0f) * SSAOBufferInverseDimensions();

	FfxFloat32 avg = 0.0;
	FfxFloat32 minV = 1.0;
	FfxFloat32 maxV = 0.0;
	FFX_UNROLL
	for (FfxInt32 i = 0; i < 4; i++)
	{
		FfxFloat32x4 vals = FFX_CACAO_Importance_GatherSSAO(baseUV, i);

		// apply the same modifications that would have been applied in the main shader
		vals = EffectShadowStrength() * vals;

		vals = 1 - vals;

		vals = pow(ffxSaturate(vals), FfxFloat32x4(EffectShadowPow(), EffectShadowPow(), EffectShadowPow(), EffectShadowPow()));

		avg += dot(FfxFloat32x4(vals.x, vals.y, vals.z, vals.w), FfxFloat32x4(1.0 / 16.0, 1.0 / 16.0, 1.0 / 16.0, 1.0 / 16.0));

		maxV = max(maxV, max(max(vals.x, vals.y), max(vals.z, vals.w)));
		minV = min(minV, min(min(vals.x, vals.y), min(vals.z, vals.w)));
	}

	FfxFloat32 minMaxDiff = maxV - minV;

	FFX_CACAO_Importance_StoreImportance(tid, pow(ffxSaturate(minMaxDiff * 2.0), 0.8f));
}

FFX_STATIC const FfxFloat32 c_FFX_CACAO_SmoothenImportance = 1.0f;

void FFX_CACAO_PostprocessImportanceMapA(FfxUInt32x2 tid)
{
	FfxFloat32x2 uv = (FfxFloat32x2(tid)+0.5f) * ImportanceMapInverseDimensions();

	FfxFloat32 centre = FFX_CACAO_Importance_SampleImportanceA(uv);

	FfxFloat32x2 halfPixel = 0.5f * ImportanceMapInverseDimensions();

	FfxFloat32x4 vals;
	vals.x = FFX_CACAO_Importance_SampleImportanceA(uv + FfxFloat32x2(-halfPixel.x * 3, -halfPixel.y));
	vals.y = FFX_CACAO_Importance_SampleImportanceA(uv + FfxFloat32x2(+halfPixel.x, -halfPixel.y * 3));
	vals.z = FFX_CACAO_Importance_SampleImportanceA(uv + FfxFloat32x2(+halfPixel.x * 3, +halfPixel.y));
	vals.w = FFX_CACAO_Importance_SampleImportanceA(uv + FfxFloat32x2(-halfPixel.x, +halfPixel.y * 3));

	FfxFloat32 avgVal = dot(vals, FfxFloat32x4(0.25, 0.25, 0.25, 0.25));
	vals.xy = max(vals.xy, vals.zw);
	FfxFloat32 maxVal = max(centre, max(vals.x, vals.y));

	FFX_CACAO_Importance_StoreImportanceA(tid, ffxLerp(maxVal, avgVal, c_FFX_CACAO_SmoothenImportance));
}

void FFX_CACAO_PostprocessImportanceMapB(FfxUInt32x2 tid)
{
	FfxFloat32x2 uv = (FfxFloat32x2(tid)+0.5f) * ImportanceMapInverseDimensions();

	FfxFloat32 centre = FFX_CACAO_Importance_SampleImportanceB(uv);

	FfxFloat32x2 halfPixel = 0.5f * ImportanceMapInverseDimensions();

	FfxFloat32x4 vals;
	vals.x = FFX_CACAO_Importance_SampleImportanceB(uv + FfxFloat32x2(-halfPixel.x, -halfPixel.y * 3));
	vals.y = FFX_CACAO_Importance_SampleImportanceB(uv + FfxFloat32x2(+halfPixel.x * 3, -halfPixel.y));
	vals.z = FFX_CACAO_Importance_SampleImportanceB(uv + FfxFloat32x2(+halfPixel.x, +halfPixel.y * 3));
	vals.w = FFX_CACAO_Importance_SampleImportanceB(uv + FfxFloat32x2(-halfPixel.x * 3, +halfPixel.y));

	FfxFloat32 avgVal = dot(vals, FfxFloat32x4(0.25, 0.25, 0.25, 0.25));
	vals.xy = max(vals.xy, vals.zw);
	FfxFloat32 maxVal = max(centre, max(vals.x, vals.y));

	FfxFloat32 retVal = ffxLerp(maxVal, avgVal, c_FFX_CACAO_SmoothenImportance);
	FFX_CACAO_Importance_StoreImportanceB(tid, retVal);

	// sum the average; to avoid overflowing we assume max AO resolution is not bigger than 16384x16384; so quarter res (used here) will be 4096x4096, which leaves us with 8 bits per pixel
	FfxUInt32 sum = FfxUInt32(ffxSaturate(retVal) * 255.0 + 0.5);

	// save every 9th to avoid InterlockedAdd congestion - since we're blurring, this is good enough; compensated by multiplying LoadCounterAvgDiv by 9
	if (((tid.x % 3) + (tid.y % 3)) == 0)
	{
		FFX_CACAO_Importance_LoadCounterInterlockedAdd(sum);
	}
}
