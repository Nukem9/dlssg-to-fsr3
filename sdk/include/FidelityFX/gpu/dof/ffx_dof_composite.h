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
#include "ffx_dof_common.h"

#if FFX_HALF
FfxFloat16 FfxMed9(FfxFloat16 a, FfxFloat16 b, FfxFloat16 c,
	FfxFloat16 d, FfxFloat16 e, FfxFloat16 f,
	FfxFloat16 g, FfxFloat16 h, FfxFloat16 i)
{
	// TODO (perf): compiler does not use med3 at all for these
	FfxFloat16 hi_lo = ffxMax3Half(ffxMin3Half(a, b, c), ffxMin3Half(d, e, f), ffxMin3Half(g, h, i));
	FfxFloat16 mi_mi = ffxMed3Half(ffxMed3Half(a, b, c), ffxMed3Half(d, e, f), ffxMed3Half(g, h, i));
	FfxFloat16 lo_hi = ffxMin3Half(ffxMax3Half(a, b, c), ffxMax3Half(d, e, f), ffxMax3Half(g, h, i));
	return ffxMed3Half(hi_lo, mi_mi, lo_hi);
}
#endif

FfxFloat32 FfxMed9(FfxFloat32 a, FfxFloat32 b, FfxFloat32 c,
	FfxFloat32 d, FfxFloat32 e, FfxFloat32 f,
	FfxFloat32 g, FfxFloat32 h, FfxFloat32 i)
{
	// TODO (perf): compiler does not use med3 at all for these
	FfxFloat32 hi_lo = ffxMax3(ffxMin3(a, b, c), ffxMin3(d, e, f), ffxMin3(g, h, i));
	FfxFloat32 mi_mi = ffxMed3(ffxMed3(a, b, c), ffxMed3(d, e, f), ffxMed3(g, h, i));
	FfxFloat32 lo_hi = ffxMin3(ffxMax3(a, b, c), ffxMax3(d, e, f), ffxMax3(g, h, i));
	return ffxMed3(hi_lo, mi_mi, lo_hi);
}

FfxFloat32 FfxCubicSpline(FfxFloat32 x)
{
	// evaluate cubic spline -2x^3 + 3x^2
	return x * x * (3 - 2 * x);
}

FFX_STATIC const FfxUInt32 FFX_DOF_COMBINE_TILE_SIZE = 8;
FFX_STATIC const FfxUInt32 FFX_DOF_COMBINE_ROW_PITCH = FFX_DOF_COMBINE_TILE_SIZE + 3; // Add +2 for 3x3 filter margin, +1 on one side for bilinear filter.
FFX_STATIC const FfxUInt32 FFX_DOF_COMBINE_AREA = FFX_DOF_COMBINE_ROW_PITCH * FFX_DOF_COMBINE_ROW_PITCH;

#if FFX_HALF
FFX_GROUPSHARED FfxUInt32  FfxDofLDSLuma[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxUInt32  FfxDofLDSNearRG[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxUInt32  FfxDofLDSNearBA[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxUInt32  FfxDofLDSFarRG[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxUInt32  FfxDofLDSFarBA[FFX_DOF_COMBINE_AREA];

FFX_GROUPSHARED FfxUInt32  FfxDofLDSFullColorRG[18*18];
FFX_GROUPSHARED FfxFloat16 FfxDofLDSFullColorB[18*18];
#else // #if FFX_HALF
FFX_GROUPSHARED FfxFloat32  FfxDofLDSNearLuma[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxFloat32  FfxDofLDSFarLuma[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxFloat32  FfxDofLDSNearR[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxFloat32  FfxDofLDSNearG[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxFloat32  FfxDofLDSNearB[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxFloat32  FfxDofLDSNearA[FFX_DOF_COMBINE_AREA];

FFX_GROUPSHARED FfxFloat32  FfxDofLDSFarR[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxFloat32  FfxDofLDSFarG[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxFloat32  FfxDofLDSFarB[FFX_DOF_COMBINE_AREA];
FFX_GROUPSHARED FfxFloat32  FfxDofLDSFarA[FFX_DOF_COMBINE_AREA];

FFX_GROUPSHARED FfxFloat32  FfxDofLDSFullColorR[18*18];
FFX_GROUPSHARED FfxFloat32  FfxDofLDSFullColorG[18*18];
FFX_GROUPSHARED FfxFloat32  FfxDofLDSFullColorB[18*18];
#endif // #if FFX_HALF #else

#if FFX_HALF
FfxFloat16x4 FfxDofGetIntermediateNearColor(FfxUInt32 idx)
{
	FfxFloat16x2 rg = FFX_UINT32_TO_FLOAT16X2(FfxDofLDSNearRG[idx]);
	FfxFloat16x2 ba = FFX_UINT32_TO_FLOAT16X2(FfxDofLDSNearBA[idx]);
	return FfxFloat16x4(rg, ba);
}
FfxFloat16x4 FfxDofGetIntermediateFarColor(FfxUInt32 idx)
{
	FfxFloat16x2 rg = FFX_UINT32_TO_FLOAT16X2(FfxDofLDSFarRG[idx]);
	FfxFloat16x2 ba = FFX_UINT32_TO_FLOAT16X2(FfxDofLDSFarBA[idx]);
	return FfxFloat16x4(rg, ba);
}
FfxFloat16x3 FfxDofGetIntFullColor(FfxUInt32 idx)
{
	FfxFloat16x2 rg = FFX_UINT32_TO_FLOAT16X2(FfxDofLDSFullColorRG[idx]);
	FfxFloat16 b = FfxDofLDSFullColorB[idx];
	return FfxFloat16x3(rg, b);
}
void FfxDofSetIntNearLuma(FfxUInt32 idx, FfxFloat16 luma)
{
	FfxFloat16x2 unpacked = FFX_UINT32_TO_FLOAT16X2(FfxDofLDSLuma[idx]);
	unpacked.x = luma;
	FfxDofLDSLuma[idx] = FFX_FLOAT16X2_TO_UINT32(unpacked);
}
void FfxDofSetIntFarLuma(FfxUInt32 idx, FfxFloat16 luma)
{
	FfxFloat16x2 unpacked = FFX_UINT32_TO_FLOAT16X2(FfxDofLDSLuma[idx]);
	unpacked.y = luma;
	FfxDofLDSLuma[idx] = FFX_FLOAT16X2_TO_UINT32(unpacked);
}

void FfxDofSetIntermediateNearColor(FfxUInt32 idx, FfxFloat16x4 col)
{
	FfxDofLDSNearRG[idx] = FFX_FLOAT16X2_TO_UINT32(col.rg);
	FfxDofLDSNearBA[idx] = FFX_FLOAT16X2_TO_UINT32(col.ba);
}
void FfxDofSetIntermediateFarColor(FfxUInt32 idx, FfxFloat16x4 col)
{
	FfxDofLDSFarRG[idx] = FFX_FLOAT16X2_TO_UINT32(col.rg);
	FfxDofLDSFarBA[idx] = FFX_FLOAT16X2_TO_UINT32(col.ba);
}
void FfxDofSetIntFullColor(FfxUInt32 idx, FfxFloat16x3 col)
{
	FfxDofLDSFullColorRG[idx] = FFX_FLOAT16X2_TO_UINT32(col.rg);
	FfxDofLDSFullColorB[idx] = col.b;
}
FfxFloat16 FfxDofGetIntermediateNearAlpha(FfxUInt32 idx, FfxUInt32 offX, FfxUInt32 offY)
{
	return FfxDofGetIntermediateNearColor(idx + offX + FFX_DOF_COMBINE_ROW_PITCH * offY).a;
}
FfxFloat16 FfxDofGetIntermediateFarAlpha(FfxUInt32 idx)
{
	return FfxDofGetIntermediateFarColor(idx).a;
}
#else // #if FFX_HALF
FfxFloat32x4 FfxDofGetIntermediateNearColor(FfxUInt32 idx)
{
	return FfxFloat32x4(FfxDofLDSNearR[idx], FfxDofLDSNearG[idx], FfxDofLDSNearB[idx], FfxDofLDSNearA[idx]);
}
FfxFloat32x4 FfxDofGetIntermediateFarColor(FfxUInt32 idx)
{
	return FfxFloat32x4(FfxDofLDSFarR[idx], FfxDofLDSFarG[idx], FfxDofLDSFarB[idx], FfxDofLDSFarA[idx]);
}
FfxFloat32x3 FfxDofGetIntFullColor(FfxUInt32 idx)
{
	return FfxFloat32x3(FfxDofLDSFullColorR[idx], FfxDofLDSFullColorG[idx], FfxDofLDSFullColorB[idx]);
}
void FfxDofSetIntNearLuma(FfxUInt32 idx, FfxFloat32 luma)
{
	FfxDofLDSNearLuma[idx] = luma;
}
void FfxDofSetIntFarLuma(FfxUInt32 idx, FfxFloat32 luma)
{
	FfxDofLDSFarLuma[idx] = luma;
}

void FfxDofSetIntermediateNearColor(FfxUInt32 idx, FfxFloat32x4 col)
{
	FfxDofLDSNearR[idx] = col.r;
	FfxDofLDSNearG[idx] = col.g;
	FfxDofLDSNearB[idx] = col.b;
	FfxDofLDSNearA[idx] = col.a;
}
void FfxDofSetIntermediateFarColor(FfxUInt32 idx, FfxFloat32x4 col)
{
	FfxDofLDSFarR[idx] = col.r;
	FfxDofLDSFarG[idx] = col.g;
	FfxDofLDSFarB[idx] = col.b;
	FfxDofLDSFarA[idx] = col.a;
}
void FfxDofSetIntFullColor(FfxUInt32 idx, FfxFloat32x3 col)
{
	FfxDofLDSFullColorR[idx] = col.r;
	FfxDofLDSFullColorG[idx] = col.g;
	FfxDofLDSFullColorB[idx] = col.b;
}
FfxFloat32 FfxDofGetIntermediateNearAlpha(FfxUInt32 idx, FfxUInt32 offX, FfxUInt32 offY)
{
	return FfxDofGetIntermediateNearColor(idx + offX + FFX_DOF_COMBINE_ROW_PITCH * offY).a;
}
FfxFloat32 FfxDofGetIntermediateFarAlpha(FfxUInt32 idx)
{
	return FfxDofGetIntermediateFarColor(idx).a;
}
#endif // #if FFX_HALF #else

FfxHalfOpt3 FfxDofBlur3x3(FfxUInt32 baseIdx)
{
	// kernel coefficients based on coverage of a circle in a 3x3 grid
	const FfxHalfOpt CORNER = FfxHalfOpt(0.5453);
	const FfxHalfOpt SIDE = FfxHalfOpt(0.9717);

	// accumulate convolution
	const FfxHalfOpt weights_sum = FfxHalfOpt(1) + FfxHalfOpt(4) * CORNER + FfxHalfOpt(4) * SIDE;
	FfxHalfOpt3 sum = FfxDofGetIntFullColor(baseIdx + 19)
		+ CORNER * (FfxDofGetIntFullColor(baseIdx) + FfxDofGetIntFullColor(baseIdx + 2) + FfxDofGetIntFullColor(baseIdx + 36) + FfxDofGetIntFullColor(baseIdx + 38))
		+ SIDE * (FfxDofGetIntFullColor(baseIdx + 1) + FfxDofGetIntFullColor(baseIdx + 18) + FfxDofGetIntFullColor(baseIdx + 20) + FfxDofGetIntFullColor(baseIdx + 37));

	return sum / weights_sum;
}

FfxHalfOpt4 FfxDofFinalCombineColors(FfxUInt32x2 coord, FfxUInt32x2 relCoord, FfxHalfOpt4 bg, FfxHalfOpt4 fg, FfxHalfOpt minFgW)
{
	FfxFloat32 d = FfxDofLoadFullDepth(coord);
	FfxUInt32 baseIdx = relCoord.x + 18 * relCoord.y;
	FfxHalfOpt3 full = FfxHalfOpt3(FfxDofGetIntFullColor(baseIdx + 19));
	FfxHalfOpt3 fixBlurred = FfxHalfOpt3(FfxDofBlur3x3(baseIdx));

	// expand background around edges
	if (bg.a > FfxHalfOpt(0)) bg.rgb /= bg.a;
	// if any FG sample has zero weight, the interpolation is invalid.
	if (minFgW == FfxHalfOpt(0)) fg.a = FfxHalfOpt(0);
	FfxHalfOpt c = FfxHalfOpt(2) * FfxHalfOpt(abs(FfxDofGetCoc(d))); // double it for full-res pixels
	FfxHalfOpt c1 = ffxSaturate(c - FfxHalfOpt(0.5)); // lerp factor for full vs. fixed 1.5px blur
	FfxHalfOpt c2 = ffxSaturate(c - FfxHalfOpt(1.5)); // lerp factor for prev vs. quarter res
	if (bg.a == FfxHalfOpt(0)) c2 = FfxHalfOpt(0);
	FfxHalfOpt3 combinedColor = ffxLerp(full, fixBlurred, c1);
	combinedColor = ffxLerp(combinedColor, bg.rgb, c2);

	combinedColor = ffxLerp(combinedColor, fg.rgb, FfxHalfOpt(FfxCubicSpline(fg.a)));
	return FfxHalfOpt4(combinedColor, 1);
}

#if FFX_HALF
FfxFloat16 FfxDofGetLDSNearLuma(FfxUInt32 idx, FfxUInt32 offX, FfxUInt32 offY)
{
	return FFX_UINT32_TO_FLOAT16X2(FfxDofLDSLuma[idx + offX + FFX_DOF_COMBINE_ROW_PITCH * offY]).x;
}
FfxFloat16 FfxDofGetLDSFarLuma(FfxUInt32 idx, FfxUInt32 offX, FfxUInt32 offY)
{
	return FFX_UINT32_TO_FLOAT16X2(FfxDofLDSLuma[idx + offX + FFX_DOF_COMBINE_ROW_PITCH * offY]).y;
}
#else // #if FFX_HALF
FfxFloat32 FfxDofGetLDSNearLuma(FfxUInt32 idx, FfxUInt32 offX, FfxUInt32 offY)
{
	return FfxDofLDSNearLuma[idx + offX + FFX_DOF_COMBINE_ROW_PITCH * offY];
}
FfxFloat32 FfxDofGetLDSFarLuma(FfxUInt32 idx, FfxUInt32 offX, FfxUInt32 offY)
{
	return FfxDofLDSFarLuma[idx + offX + FFX_DOF_COMBINE_ROW_PITCH * offY];
}
#endif // #if FFX_HALF #else

FfxHalfOpt4 FfxDofFilterFF(FfxUInt32 baseIdx)
{
	// get the median of the surrounding 3x3 area of luma values
	FfxHalfOpt med_luma = FfxMed9(
		FfxDofGetLDSFarLuma(baseIdx, 0, 0), FfxDofGetLDSFarLuma(baseIdx, 1, 0), FfxDofGetLDSFarLuma(baseIdx, 2, 0),
		FfxDofGetLDSFarLuma(baseIdx, 0, 1), FfxDofGetLDSFarLuma(baseIdx, 1, 1), FfxDofGetLDSFarLuma(baseIdx, 2, 1),
		FfxDofGetLDSFarLuma(baseIdx, 0, 2), FfxDofGetLDSFarLuma(baseIdx, 1, 2), FfxDofGetLDSFarLuma(baseIdx, 2, 2));

	FfxUInt32 idx = baseIdx + FFX_DOF_COMBINE_ROW_PITCH + 1;
	FfxHalfOpt3 col = FfxDofGetIntermediateFarColor(idx).rgb;
	FfxHalfOpt lumaFactor = clamp(med_luma / FfxDofGetLDSFarLuma(idx, 0, 0), FfxHalfOpt(0), FfxHalfOpt(2));
	// corner fix: if color pixel is on a corner (has 5 black pixels as neighbor), don't reduce color.
	if (med_luma == FfxHalfOpt(0)) lumaFactor = FfxHalfOpt(1);
	return FfxHalfOpt4(col * lumaFactor, FfxDofGetIntermediateFarAlpha(idx));
}

FfxHalfOpt4 FfxDofFilterNF(FfxUInt32 baseIdx)
{
	// Get 3x3 median luma
	FfxHalfOpt med_luma = FfxMed9(
		FfxDofGetLDSNearLuma(baseIdx, 0, 0), FfxDofGetLDSNearLuma(baseIdx, 1, 0), FfxDofGetLDSNearLuma(baseIdx, 2, 0),
		FfxDofGetLDSNearLuma(baseIdx, 0, 1), FfxDofGetLDSNearLuma(baseIdx, 1, 1), FfxDofGetLDSNearLuma(baseIdx, 2, 1),
		FfxDofGetLDSNearLuma(baseIdx, 0, 2), FfxDofGetLDSNearLuma(baseIdx, 1, 2), FfxDofGetLDSNearLuma(baseIdx, 2, 2));
	FfxHalfOpt avg_alpha = FfxHalfOpt(ffxReciprocal(9.0)) * (
		FfxDofGetIntermediateNearAlpha(baseIdx, 0, 0) + FfxDofGetIntermediateNearAlpha(baseIdx, 1, 0) + FfxDofGetIntermediateNearAlpha(baseIdx, 2, 0) +
		FfxDofGetIntermediateNearAlpha(baseIdx, 0, 1) + FfxDofGetIntermediateNearAlpha(baseIdx, 1, 1) + FfxDofGetIntermediateNearAlpha(baseIdx, 2, 1) +
		FfxDofGetIntermediateNearAlpha(baseIdx, 0, 2) + FfxDofGetIntermediateNearAlpha(baseIdx, 1, 2) + FfxDofGetIntermediateNearAlpha(baseIdx, 2, 2));

	FfxUInt32 idx = baseIdx + FFX_DOF_COMBINE_ROW_PITCH + 1;
	if (FfxDofGetIntermediateNearAlpha(idx, 0, 0) < 0.01)
	{
		// center has zero weight, grab one of the corner colors
		FfxUInt32 maxIdx = baseIdx;
		if (FfxDofGetLDSNearLuma(baseIdx, 2, 0) > FfxDofGetLDSNearLuma(maxIdx, 0, 0)) maxIdx = baseIdx + 2;
		if (FfxDofGetLDSNearLuma(baseIdx, 0, 2) > FfxDofGetLDSNearLuma(maxIdx, 0, 0)) maxIdx = baseIdx + 2 * FFX_DOF_COMBINE_ROW_PITCH;
		if (FfxDofGetLDSNearLuma(baseIdx, 2, 2) > FfxDofGetLDSNearLuma(maxIdx, 0, 0)) maxIdx = baseIdx + 2 * FFX_DOF_COMBINE_ROW_PITCH + 2;
		idx = maxIdx;
	}
	FfxHalfOpt3 col = FfxDofGetIntermediateNearColor(idx).rgb;
	FfxHalfOpt lumaFactor = med_luma > FfxHalfOpt(0) ? clamp(med_luma / FfxDofGetLDSNearLuma(idx, 0, 0), FfxHalfOpt(0), FfxHalfOpt(2)) : FfxHalfOpt(1.0);
	return FfxHalfOpt4(col.rgb * lumaFactor, avg_alpha);
}

FfxFloat32x2 FfxDofGetTileRadius(FfxUInt32x2 group)
{
	// need to read 4 values
	FfxUInt32x2 tile = group * 2;
	FfxFloat32x2 a = FfxDofLoadDilatedRadius(tile);
	FfxFloat32x2 b = FfxDofLoadDilatedRadius(tile + FfxUInt32x2(0, 1));
	FfxFloat32x2 c = FfxDofLoadDilatedRadius(tile + FfxUInt32x2(1, 0));
	FfxFloat32x2 d = FfxDofLoadDilatedRadius(tile + FfxUInt32x2(1, 1));
	FfxFloat32 near = max(a.x, ffxMax3(b.x, c.x, d.x));
	FfxFloat32 far = min(a.y, ffxMin3(b.y, c.y, d.y));
	return FfxFloat32x2(near, far);
}

void FfxDofCombineSharpOnly(FfxUInt32x2 group, FfxUInt32x2 thread)
{
#if !defined(FFX_DOF_OPTION_COMBINE_IN_PLACE) || !FFX_DOF_OPTION_COMBINE_IN_PLACE
	FfxUInt32x2 base = 16 * group;
	FfxDofStoreOutput(base + thread + FfxUInt32x2(0, 0), FfxDofLoadFullInput(base + thread + FfxUInt32x2(0, 0)));
	FfxDofStoreOutput(base + thread + FfxUInt32x2(8, 0), FfxDofLoadFullInput(base + thread + FfxUInt32x2(8, 0)));
	FfxDofStoreOutput(base + thread + FfxUInt32x2(0, 8), FfxDofLoadFullInput(base + thread + FfxUInt32x2(0, 8)));
	FfxDofStoreOutput(base + thread + FfxUInt32x2(8, 8), FfxDofLoadFullInput(base + thread + FfxUInt32x2(8, 8)));
#endif
}

void FfxDofFetchFullColor(FfxUInt32x2 gid, FfxUInt32 gix, FfxUInt32x2 imageSize)
{
	FFX_DOF_UNROLL
	for (FfxUInt32 iter = 0; iter < 6; iter++)
	{
		FfxUInt32 iFetch = (gix + iter * 64) % (18 * 18);
		FfxInt32x2 coord = FfxInt32x2(gid * 16) + FfxInt32x2(iFetch % 18 - 1, iFetch / 18 - 1);
		coord = clamp(coord, FfxInt32x2(0, 0), FfxInt32x2(imageSize) - FfxInt32x2(1, 1));
		FfxHalfOpt3 color = FfxHalfOpt3(FfxDofLoadFullInput(coord).rgb);
		FfxDofSetIntFullColor(iFetch, color);
	}
}

void FfxDofSwizQuad(inout FfxUInt32x2 a, inout FfxUInt32x2 b, inout FfxUInt32x2 c, inout FfxUInt32x2 d)
{
	// Input: four color values in a quad.
	// Re-orders the output to a swizzled format for better store throughput.
	// This maps from one quad per lane, stored in four separate registers to
	// four 16x2 regions (one per register).
	// This is done in two steps. First, permute the values among the lanes
	// using WaveReadLaneAt. Second, swap values between registers.

	// This only works for lane counts >= 32, do nothing otherwise for compatibility
#if FFX_HLSL
	if (WaveGetLaneCount() < 32) return;

	FfxUInt32 lane = WaveGetLaneIndex();
	// index for A, switch bits around 43210 -> 10432.
	FfxUInt32 idxA = ((lane & 3) << 3) + (lane >> 2);
	// Adding 8/16/24 for B/C/D makes each variable offset from the previous by one slot.
	a = WaveReadLaneAt(a, (lane & ~31) + (idxA + 0) % 32);
	b = WaveReadLaneAt(b, (lane & ~31) + (idxA + 8) % 32);
	c = WaveReadLaneAt(c, (lane & ~31) + (idxA + 16) % 32);
	d = WaveReadLaneAt(d, (lane & ~31) + (idxA + 24) % 32);
#elif FFX_GLSL
	if (gl_SubgroupSize < 32) return;

	FfxUInt32 lane = gl_SubgroupInvocationID;
	FfxUInt32 idxA = ((lane & 3) << 3) + (lane >> 2);
	a = subgroupShuffle(a, (lane & ~31) + (idxA + 0) % 32);
	b = subgroupShuffle(b, (lane & ~31) + (idxA + 8) % 32);
	c = subgroupShuffle(c, (lane & ~31) + (idxA + 16) % 32);
	d = subgroupShuffle(d, (lane & ~31) + (idxA + 24) % 32);
#endif

	// Now, for each lane, a/b/c/d contain one value from each of the four 16x2 lines.
	// And each group of 4 lanes have values from the same quads.
	// We just need to shuffle between abcd, so that each set of 4 lanes contains one quad per variable.
	// General idea: rotate by (lane % 4) variables.
	if ((lane & 1) != 0)
	{
		// rotate A->B->C->D->A
		FfxUInt32x2 tmp = d;
		d = c;
		c = b;
		b = a;
		a = tmp;
	}
	if ((lane & 2) != 0)
	{
		// swap A<->C and B<->D
		FfxUInt32x2 tmp = a;
		a = c;
		c = tmp;
		tmp = b;
		b = d;
		d = tmp;
	}
}

#if FFX_HALF
void FfxDofSwizQuad(inout FfxFloat16x4 a, inout FfxFloat16x4 b, inout FfxFloat16x4 c, inout FfxFloat16x4 d)
{
	// Same as above.
	FfxUInt32x2 packed_a = FFX_FLOAT16X4_TO_UINT32X2(a);
	FfxUInt32x2 packed_b = FFX_FLOAT16X4_TO_UINT32X2(b);
	FfxUInt32x2 packed_c = FFX_FLOAT16X4_TO_UINT32X2(c);
	FfxUInt32x2 packed_d = FFX_FLOAT16X4_TO_UINT32X2(d);
	FfxDofSwizQuad(packed_a, packed_b, packed_c, packed_d);
	a = FFX_UINT32X2_TO_FLOAT16X4(packed_a);
	b = FFX_UINT32X2_TO_FLOAT16X4(packed_b);
	c = FFX_UINT32X2_TO_FLOAT16X4(packed_c);
	d = FFX_UINT32X2_TO_FLOAT16X4(packed_d);
}
#else // #if FFX_HALF
void FfxDofSwizQuad(inout FfxFloat32x4 a, inout FfxFloat32x4 b, inout FfxFloat32x4 c, inout FfxFloat32x4 d)
{
	// Same as above.
	FfxUInt32x2 a0 = ffxAsUInt32(a.xy);
	FfxUInt32x2 a1 = ffxAsUInt32(a.zw);
	FfxUInt32x2 b0 = ffxAsUInt32(b.xy);
	FfxUInt32x2 b1 = ffxAsUInt32(b.zw);
	FfxUInt32x2 c0 = ffxAsUInt32(c.xy);
	FfxUInt32x2 c1 = ffxAsUInt32(c.zw);
	FfxUInt32x2 d0 = ffxAsUInt32(d.xy);
	FfxUInt32x2 d1 = ffxAsUInt32(d.zw);
	FfxDofSwizQuad(a0, b0, c0, d0);
	FfxDofSwizQuad(a1, b1, c1, d1);
	a = FfxFloat32x4(ffxAsFloat(a0), ffxAsFloat(a1));
	b = FfxFloat32x4(ffxAsFloat(b0), ffxAsFloat(b1));
	c = FfxFloat32x4(ffxAsFloat(c0), ffxAsFloat(c1));
	d = FfxFloat32x4(ffxAsFloat(d0), ffxAsFloat(d1));
}
#endif // #if FFX_HALF #else

void FfxDofCombineFarOnly(FfxUInt32x2 id, FfxUInt32x2 gtID, FfxUInt32x2 gid, FfxUInt32 gix, FfxUInt32x2 imageSize)
{
	// TODO: Is this the best configuration for fetching?
	FFX_DOF_UNROLL_N(2)
	for (FfxUInt32 iter = 0; iter < 2; iter++)
	{
		// HACK: with the modulo, we will re-fetch some pixels, but might be better than waiting twice (latency-wise)
		// which is what the compiler would do if it had to possibly branch
		FfxUInt32 iFetch = (gix + iter * 64) % FFX_DOF_COMBINE_AREA;
		FfxInt32x2 coord = FfxInt32x2(gid * FFX_DOF_COMBINE_TILE_SIZE) + FfxInt32x2(iFetch % FFX_DOF_COMBINE_ROW_PITCH - 1, iFetch / FFX_DOF_COMBINE_ROW_PITCH - 1);
		coord = clamp(coord, FfxInt32x2(0, 0), FfxInt32x2(imageSize - 1));
		FfxHalfOpt4 ffColor = FfxHalfOpt4(FfxDofLoadFar(coord));

		// calculate and store luma for later median calculation
		FfxHalfOpt ffLuma = FfxHalfOpt(0.2126) * ffColor.r + FfxHalfOpt(0.7152) * ffColor.g + FfxHalfOpt(0.0722) * ffColor.b;
		FfxDofSetIntFarLuma(iFetch, ffLuma);
		FfxDofSetIntermediateFarColor(iFetch, ffColor);
	}

	FFX_GROUP_MEMORY_BARRIER;

	const FfxUInt32 baseIdx = gtID.x + gtID.y * FFX_DOF_COMBINE_ROW_PITCH;
	// one extra round of filtering needs to be done around the edge, this index maps to that.
	// TODO: This is ugly and possibly slow
	const FfxUInt32 baseIdx2 = (FFX_DOF_COMBINE_TILE_SIZE + FFX_DOF_COMBINE_ROW_PITCH * gix + (gix / (FFX_DOF_COMBINE_TILE_SIZE + 1)) * ((gix - FFX_DOF_COMBINE_TILE_SIZE) * (-FFX_DOF_COMBINE_ROW_PITCH + 1) - (FFX_DOF_COMBINE_TILE_SIZE + 1))) % FFX_DOF_COMBINE_AREA;

	FfxHalfOpt4 ffColor = FfxHalfOpt4(0, 0, 0, 0), ffColor2 = FfxHalfOpt4(0, 0, 0, 0);
	// far-field post-filter
	ffColor = FfxDofFilterFF(baseIdx);
	ffColor2 = gix < (2 * FFX_DOF_COMBINE_TILE_SIZE + 1) ? FfxDofFilterFF(baseIdx2) : FfxHalfOpt4(0, 0, 0, 0);

	FFX_GROUP_MEMORY_BARRIER;

	// write out colors for interpolation
	FfxDofSetIntermediateFarColor(baseIdx, ffColor);
	if (gix < (2 * FFX_DOF_COMBINE_TILE_SIZE + 1))
	{
		FfxDofSetIntermediateFarColor(baseIdx2, ffColor2);
	}

	FFX_GROUP_MEMORY_BARRIER;

	// upscaling
	FfxHalfOpt4 ffTR = FfxHalfOpt4(0, 0, 0, 0), ffBL = FfxHalfOpt4(0, 0, 0, 0), ffBR = FfxHalfOpt4(0, 0, 0, 0);
	ffTR = FfxHalfOpt(0.5) * ffColor + FfxHalfOpt(0.5) * FfxDofGetIntermediateFarColor(baseIdx + 1);
	ffBL = FfxHalfOpt(0.5) * ffColor + FfxHalfOpt(0.5) * FfxDofGetIntermediateFarColor(baseIdx + FFX_DOF_COMBINE_ROW_PITCH);
	ffBR = FfxHalfOpt(0.5) * ffTR + FfxHalfOpt(0.25) * FfxDofGetIntermediateFarColor(baseIdx + FFX_DOF_COMBINE_ROW_PITCH) + FfxHalfOpt(0.25) * FfxDofGetIntermediateFarColor(baseIdx + FFX_DOF_COMBINE_ROW_PITCH + 1);

	// top-left pixel
	FfxUInt32x2 coord = 2 * id;
	FfxUInt32x2 relCoord = 2 * gtID;
	FfxUInt32x2 coordA = coord;
	FfxHalfOpt4 colA = FfxDofFinalCombineColors(coord, relCoord, ffColor, FfxHalfOpt4(0, 0, 0, 0), FfxHalfOpt(0));
	// top-right
	coord.x++;
	relCoord.x++;
	FfxUInt32x2 coordB = coord;
	FfxHalfOpt4 colB = FfxDofFinalCombineColors(coord, relCoord, ffTR, FfxHalfOpt4(0, 0, 0, 0), FfxHalfOpt(0));
	// bottom-right
	coord.y++;
	relCoord.y++;
	FfxUInt32x2 coordC = coord;
	FfxHalfOpt4 colC = FfxDofFinalCombineColors(coord, relCoord, ffBR, FfxHalfOpt4(0, 0, 0, 0), FfxHalfOpt(0));
	// bottom-left
	coord.x--;
	relCoord.x--;
	FfxUInt32x2 coordD = coord;
	FfxHalfOpt4 colD = FfxDofFinalCombineColors(coord, relCoord, ffBL, FfxHalfOpt4(0, 0, 0, 0), FfxHalfOpt(0));

    // TODO: Navi3 should make swizzling unnecessary because it supports write-combining clauses
	FfxDofSwizQuad(colA, colB, colC, colD);
	FfxDofSwizQuad(coordA, coordB, coordC, coordD);

	FfxDofStoreOutput(coordA, colA);
	FfxDofStoreOutput(coordB, colB);
	FfxDofStoreOutput(coordC, colC);
	FfxDofStoreOutput(coordD, colD);
}

void FfxDofCombineAll(FfxUInt32x2 id, FfxUInt32x2 gtID, FfxUInt32x2 gid, FfxUInt32 gix, FfxUInt32x2 imageSize)
{
	// TODO: Is this the best configuration for fetching?
	FFX_DOF_UNROLL_N(2)
	for (FfxUInt32 iter = 0; iter < 2; iter++)
	{
		// HACK: with the modulo, we will re-fetch some pixels, but might be better than waiting twice (latency-wise)
		// which is what the compiler would do if it had to possibly branch
		FfxUInt32 iFetch = (gix + iter * 64) % FFX_DOF_COMBINE_AREA;
		FfxInt32x2 coord = FfxInt32x2(gid * FFX_DOF_COMBINE_TILE_SIZE) + FfxInt32x2(iFetch % FFX_DOF_COMBINE_ROW_PITCH - 1, iFetch / FFX_DOF_COMBINE_ROW_PITCH - 1);
		coord = clamp(coord, FfxInt32x2(0, 0), FfxInt32x2(imageSize - 1));
		FfxHalfOpt4 ffColor = FfxHalfOpt4(FfxDofLoadFar(coord));
		FfxHalfOpt4 nfColor = FfxHalfOpt4(FfxDofLoadNear(coord));

		// calculate and store luma for later median calculation
		FfxHalfOpt ffLuma = FfxHalfOpt(0.2126) * ffColor.r + FfxHalfOpt(0.7152) * ffColor.g + FfxHalfOpt(0.0722) * ffColor.b;
		FfxHalfOpt nfLuma = FfxHalfOpt(0.2126) * nfColor.r + FfxHalfOpt(0.7152) * nfColor.g + FfxHalfOpt(0.0722) * nfColor.b;
		FfxDofSetIntFarLuma(iFetch, ffLuma);
		FfxDofSetIntNearLuma(iFetch, nfLuma);
		FfxDofSetIntermediateFarColor(iFetch, ffColor);
		FfxDofSetIntermediateNearColor(iFetch, nfColor);
	}

	FFX_GROUP_MEMORY_BARRIER;

	const FfxUInt32 baseIdx = gtID.x + gtID.y * FFX_DOF_COMBINE_ROW_PITCH;
	// one extra round of filtering needs to be done around the edge, this index maps to that.
	// TODO: same as above, ugly and slow.
	const FfxUInt32 baseIdx2 = (FFX_DOF_COMBINE_TILE_SIZE + FFX_DOF_COMBINE_ROW_PITCH * gix + (gix / (FFX_DOF_COMBINE_TILE_SIZE + 1)) * ((gix - FFX_DOF_COMBINE_TILE_SIZE) * (-FFX_DOF_COMBINE_ROW_PITCH + 1) - (FFX_DOF_COMBINE_TILE_SIZE + 1))) % FFX_DOF_COMBINE_AREA;

	FfxHalfOpt4 ffColor = FfxHalfOpt4(0, 0, 0, 0), ffColor2 = FfxHalfOpt4(0, 0, 0, 0), nfColor = FfxHalfOpt4(0, 0, 0, 0), nfColor2 = FfxHalfOpt4(0, 0, 0, 0);
	// far-field post-filter
	ffColor = FfxDofFilterFF(baseIdx);
	ffColor2 = gix < (2 * FFX_DOF_COMBINE_TILE_SIZE + 1) ? FfxDofFilterFF(baseIdx2) : FfxHalfOpt4(0, 0, 0, 0);

	// near-field post-filter
	nfColor = FfxDofFilterNF(baseIdx);
	nfColor2 = gix < (2 * FFX_DOF_COMBINE_TILE_SIZE + 1) ? FfxDofFilterNF(baseIdx2) : FfxHalfOpt4(0, 0, 0, 0);

	FFX_GROUP_MEMORY_BARRIER;

	// write out colors for interpolation
	FfxDofSetIntermediateNearColor(baseIdx, nfColor);
	FfxDofSetIntermediateFarColor(baseIdx, ffColor);
	if (gix < (2 * FFX_DOF_COMBINE_TILE_SIZE + 1))
	{
		FfxDofSetIntermediateNearColor(baseIdx2, nfColor2);
		FfxDofSetIntermediateFarColor(baseIdx2, ffColor2);
	}

	FFX_GROUP_MEMORY_BARRIER;

	// if any FG sample has zero weight, the interpolation is invalid.
	// take the min and invalidate if zero (see CombineColors)
	FfxHalfOpt fgMinW = min(nfColor.a, FfxHalfOpt(ffxMin3(FfxDofGetIntermediateNearAlpha(baseIdx, 1, 0), FfxDofGetIntermediateNearAlpha(baseIdx, 0, 1), FfxDofGetIntermediateNearAlpha(baseIdx, 1, 1))));

	// upscaling
	FfxHalfOpt4 nfTR = FfxHalfOpt4(0, 0, 0, 0), nfBL = FfxHalfOpt4(0, 0, 0, 0), nfBR = FfxHalfOpt4(0, 0, 0, 0);
	nfTR = FfxHalfOpt(0.5) * nfColor + FfxHalfOpt(0.5) * FfxDofGetIntermediateNearColor(baseIdx + 1);
	nfBL = FfxHalfOpt(0.5) * nfColor + FfxHalfOpt(0.5) * FfxDofGetIntermediateNearColor(baseIdx + FFX_DOF_COMBINE_ROW_PITCH);
	nfBR = FfxHalfOpt(0.5) * nfTR + FfxHalfOpt(0.25) * FfxDofGetIntermediateNearColor(baseIdx + FFX_DOF_COMBINE_ROW_PITCH) + FfxHalfOpt(0.25) * FfxDofGetIntermediateNearColor(baseIdx + FFX_DOF_COMBINE_ROW_PITCH + 1);

	FfxHalfOpt4 ffTR = FfxHalfOpt4(0, 0, 0, 0), ffBL = FfxHalfOpt4(0, 0, 0, 0), ffBR = FfxHalfOpt4(0, 0, 0, 0);
	ffTR = FfxHalfOpt(0.5) * ffColor + FfxHalfOpt(0.5) * FfxDofGetIntermediateFarColor(baseIdx + 1);
	ffBL = FfxHalfOpt(0.5) * ffColor + FfxHalfOpt(0.5) * FfxDofGetIntermediateFarColor(baseIdx + FFX_DOF_COMBINE_ROW_PITCH);
	ffBR = FfxHalfOpt(0.5) * ffTR + FfxHalfOpt(0.25) * FfxDofGetIntermediateFarColor(baseIdx + FFX_DOF_COMBINE_ROW_PITCH) + FfxHalfOpt(0.25) * FfxDofGetIntermediateFarColor(baseIdx + FFX_DOF_COMBINE_ROW_PITCH + 1);

	// top-left pixel
	FfxUInt32x2 coord = 2 * id;
	FfxUInt32x2 relCoord = 2 * gtID;
	FfxUInt32x2 coordA = coord;
	FfxHalfOpt4 colA = FfxDofFinalCombineColors(coord, relCoord, ffColor, nfColor, fgMinW);
	// top-right
	coord.x += 1;
	relCoord.x += 1;
	FfxUInt32x2 coordB = coord;
	FfxHalfOpt4 colB = FfxDofFinalCombineColors(coord, relCoord, ffTR, nfTR, fgMinW);
	// bottom-right
	coord.y++;
	relCoord.y++;
	FfxUInt32x2 coordC = coord;
	FfxHalfOpt4 colC = FfxDofFinalCombineColors(coord, relCoord, ffBR, nfBR, fgMinW);
	// bottom-left
	coord.x--;
	relCoord.x--;
	FfxUInt32x2 coordD = coord;
	FfxHalfOpt4 colD = FfxDofFinalCombineColors(coord, relCoord, ffBL, nfBL, fgMinW);

	FfxDofSwizQuad(colA, colB, colC, colD);
	FfxDofSwizQuad(coordA, coordB, coordC, coordD);

	FfxDofStoreOutput(coordA, colA);
	FfxDofStoreOutput(coordB, colB);
	FfxDofStoreOutput(coordC, colC);
	FfxDofStoreOutput(coordD, colD);
}

/// Entry point. Meant to run in 8x8 threads and writes 16x16 output pixels.
///
/// @param threadID SV_DispatchThreadID.xy
/// @param groupThreadID SV_GroupThreadID.xy
/// @param group SV_GroupID.xy
/// @param index SV_GroupIndex
/// @param halfImageSize Pixel size of the input (half resolution)
/// @param fullImageSize Pixel size of the output (full resolution)
/// @ingroup FfxGPUDof
void FfxDofCombineHalfRes(FfxUInt32x2 threadID, FfxUInt32x2 groupThreadID, FfxUInt32x2 group, FfxUInt32 index, FfxUInt32x2 halfImageSize, FfxUInt32x2 fullImageSize)
{
	// classify tile
	FfxFloat32x2 tileCoc = FfxDofGetTileRadius(group);
	FfxBoolean nearNeeded = tileCoc.x > -1.025; // halved due to resolution change, then: 2px = threshold in main pass + small inaccuracy bias
	FfxBoolean allSharp = max(abs(tileCoc.x), abs(tileCoc.y)) < 0.25;

	if (allSharp)
	{
		FfxDofCombineSharpOnly(group, groupThreadID);
	}
	else if (!nearNeeded)
	{
		FfxDofFetchFullColor(group, index, fullImageSize);
		FfxDofCombineFarOnly(threadID, groupThreadID, group, index, halfImageSize);
	}
	else
	{
		FfxDofFetchFullColor(group, index, fullImageSize);
		FfxDofCombineAll(threadID, groupThreadID, group, index, halfImageSize);
	}
}
