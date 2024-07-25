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

FFX_GROUPSHARED FfxFloat32 s_FFX_CACAO_PrepareDepthsAndMipsBuffer[4][8][8];

FfxFloat32 FFX_CACAO_MipSmartAverage(FfxFloat32x4 depths)
{
	FfxFloat32 closest = min(min(depths.x, depths.y), min(depths.z, depths.w));
	FfxFloat32 falloffCalcMulSq = -1.0f / EffectRadius() * EffectRadius();
	FfxFloat32x4 dists = depths - closest.xxxx;
	FfxFloat32x4 weights = ffxSaturate(dists * dists * falloffCalcMulSq + 1.0);
	return dot(weights, depths) / dot(weights, FfxFloat32x4(1.0, 1.0, 1.0, 1.0));
}

void FFX_CACAO_PrepareDepthsAndMips(FfxFloat32x4 samples, FfxUInt32x2 outputCoord, FfxUInt32x2 gtid)
{
	samples = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples);

	s_FFX_CACAO_PrepareDepthsAndMipsBuffer[0][gtid.x][gtid.y] = samples.w;
	s_FFX_CACAO_PrepareDepthsAndMipsBuffer[1][gtid.x][gtid.y] = samples.z;
	s_FFX_CACAO_PrepareDepthsAndMipsBuffer[2][gtid.x][gtid.y] = samples.x;
	s_FFX_CACAO_PrepareDepthsAndMipsBuffer[3][gtid.x][gtid.y] = samples.y;

	FFX_CACAO_Prepare_StoreDepthMip0(outputCoord, 0, samples.w);
	FFX_CACAO_Prepare_StoreDepthMip0(outputCoord, 1, samples.z);
	FFX_CACAO_Prepare_StoreDepthMip0(outputCoord, 2, samples.x);
	FFX_CACAO_Prepare_StoreDepthMip0(outputCoord, 3, samples.y);

	FfxUInt32 depthArrayIndex = 2 * (gtid.y % 2) + (gtid.x % 2);
	FfxUInt32x2 depthArrayOffset = FfxInt32x2(gtid.x % 2, gtid.y % 2);
	FfxInt32x2 bufferCoord = FfxInt32x2(gtid) - FfxInt32x2(depthArrayOffset);

	outputCoord /= 2;
	FFX_GROUP_MEMORY_BARRIER;

	// if (stillAlive) <-- all threads alive here
	{
		FfxFloat32 sample_00 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 0];
		FfxFloat32 sample_01 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 1];
		FfxFloat32 sample_10 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 1][bufferCoord.y + 0];
		FfxFloat32 sample_11 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 1][bufferCoord.y + 1];

		FfxFloat32 avg = FFX_CACAO_MipSmartAverage(FfxFloat32x4(sample_00, sample_01, sample_10, sample_11));
		FFX_CACAO_Prepare_StoreDepthMip1(outputCoord, depthArrayIndex, avg);
		s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x][bufferCoord.y] = avg;
	}

	bool stillAlive = gtid.x % 4 == depthArrayOffset.x && gtid.y % 4 == depthArrayOffset.y;

	outputCoord /= 2;
	FFX_GROUP_MEMORY_BARRIER;

	if (stillAlive)
	{
		FfxFloat32 sample_00 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 0];
		FfxFloat32 sample_01 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 2];
		FfxFloat32 sample_10 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 2][bufferCoord.y + 0];
		FfxFloat32 sample_11 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 2][bufferCoord.y + 2];

		FfxFloat32 avg = FFX_CACAO_MipSmartAverage(FfxFloat32x4(sample_00, sample_01, sample_10, sample_11));
		FFX_CACAO_Prepare_StoreDepthMip2(outputCoord, depthArrayIndex, avg);
		s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x][bufferCoord.y] = avg;
	}

	stillAlive = gtid.x % 8 == depthArrayOffset.x && gtid.y % 8 == depthArrayOffset.y;

	outputCoord /= 2;
	FFX_GROUP_MEMORY_BARRIER;

	if (stillAlive)
	{
		FfxFloat32 sample_00 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 0];
		FfxFloat32 sample_01 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 4];
		FfxFloat32 sample_10 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 4][bufferCoord.y + 0];
		FfxFloat32 sample_11 = s_FFX_CACAO_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 4][bufferCoord.y + 4];

		FfxFloat32 avg = FFX_CACAO_MipSmartAverage(FfxFloat32x4(sample_00, sample_01, sample_10, sample_11));
		FFX_CACAO_Prepare_StoreDepthMip3(outputCoord, depthArrayIndex, avg);
	}
}

void FFX_CACAO_PrepareDownsampledDepthsAndMips(FfxUInt32x2 tid, FfxUInt32x2 gtid)
{
	FfxInt32x2 depthBufferCoord = 4 * FfxInt32x2(tid.xy);
	FfxInt32x2 outputCoord = FfxInt32x2(tid);

	FfxFloat32x2 uv = (FfxFloat32x2(depthBufferCoord)+0.5f) * DepthBufferInverseDimensions();
	FfxFloat32x4 samples = FFX_CACAO_Prepare_SampleDepthOffsets(uv);

	FFX_CACAO_PrepareDepthsAndMips(samples, outputCoord, gtid);
}

void FFX_CACAO_PrepareNativeDepthsAndMips(FfxUInt32x2 tid, FfxUInt32x2 gtid)
{
	FfxInt32x2 depthBufferCoord = 2 * FfxInt32x2(tid.xy);
	FfxInt32x2 outputCoord = FfxInt32x2(tid);

	FfxFloat32x2 uv = (FfxFloat32x2(depthBufferCoord)+ 1.0f) * DepthBufferInverseDimensions();
	FfxFloat32x4 samples = FFX_CACAO_Prepare_GatherDepth(uv);

	FFX_CACAO_PrepareDepthsAndMips(samples, outputCoord, gtid);
}

void FFX_CACAO_PrepareDepths(FfxFloat32x4 samples, FfxUInt32x2 tid)
{
	samples = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples);
	FFX_CACAO_Prepare_StoreDepth(tid, 0, samples.w);
	FFX_CACAO_Prepare_StoreDepth(tid, 1, samples.z);
	FFX_CACAO_Prepare_StoreDepth(tid, 2, samples.x);
	FFX_CACAO_Prepare_StoreDepth(tid, 3, samples.y);
}

void FFX_CACAO_PrepareDownsampledDepths(FfxUInt32x2 tid)
{
	FfxInt32x2 depthBufferCoord = 4 * FfxInt32x2(tid.xy);

	FfxFloat32x2 uv = (FfxFloat32x2(depthBufferCoord)+0.5f) * DepthBufferInverseDimensions();
	FfxFloat32x4 samples = FFX_CACAO_Prepare_SampleDepthOffsets(uv);

	FFX_CACAO_PrepareDepths(samples, tid);
}

void FFX_CACAO_PrepareNativeDepths(FfxUInt32x2 tid)
{
	FfxInt32x2 depthBufferCoord = 2 * FfxInt32x2(tid.xy);

	FfxFloat32x2 uv = (FfxFloat32x2(depthBufferCoord) + 1.0f) * DepthBufferInverseDimensions();
	FfxFloat32x4 samples = FFX_CACAO_Prepare_GatherDepth(uv);

	FFX_CACAO_PrepareDepths(samples, tid);
}

void FFX_CACAO_PrepareDownsampledDepthsHalf(FfxUInt32x2 tid)
{
	FfxFloat32 sample_00 = FFX_CACAO_Prepare_LoadDepth(FfxInt32x2(4 * tid.x + 0, 4 * tid.y + 0));
	FfxFloat32 sample_11 = FFX_CACAO_Prepare_LoadDepth(FfxInt32x2(4 * tid.x + 2, 4 * tid.y + 2));
	sample_00 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(sample_00);
	sample_11 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(sample_11);
	FFX_CACAO_Prepare_StoreDepth(tid, 0, sample_00);
	FFX_CACAO_Prepare_StoreDepth(tid, 3, sample_11);
}

void FFX_CACAO_PrepareNativeDepthsHalf(FfxUInt32x2 tid)
{
	FfxFloat32 sample_00 = FFX_CACAO_Prepare_LoadDepth(FfxInt32x2(2 * tid.x + 0, 2 * tid.y + 0));
	FfxFloat32 sample_11 = FFX_CACAO_Prepare_LoadDepth(FfxInt32x2(2 * tid.x + 1, 2 * tid.y + 1));
	sample_00 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(sample_00);
	sample_11 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(sample_11);
	FFX_CACAO_Prepare_StoreDepth(tid, 0, sample_00);
	FFX_CACAO_Prepare_StoreDepth(tid, 3, sample_11);
}

struct FFX_CACAO_PrepareNormalsInputDepths
{
	FfxFloat32 depth_10;
	FfxFloat32 depth_20;

	FfxFloat32 depth_01;
	FfxFloat32 depth_11;
	FfxFloat32 depth_21;
	FfxFloat32 depth_31;

	FfxFloat32 depth_02;
	FfxFloat32 depth_12;
	FfxFloat32 depth_22;
	FfxFloat32 depth_32;

	FfxFloat32 depth_13;
	FfxFloat32 depth_23;
};

void FFX_CACAO_PrepareNormals(FFX_CACAO_PrepareNormalsInputDepths depths, FfxFloat32x2 uv, FfxFloat32x2 pixelSize, FfxUInt32x2 normalCoord)
{
	FfxFloat32x3 p_10 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(+0.0f, -1.0f) * pixelSize, depths.depth_10);
	FfxFloat32x3 p_20 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(+1.0f, -1.0f) * pixelSize, depths.depth_20);

	FfxFloat32x3 p_01 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(-1.0f, +0.0f) * pixelSize, depths.depth_01);
	FfxFloat32x3 p_11 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(+0.0f, +0.0f) * pixelSize, depths.depth_11);
	FfxFloat32x3 p_21 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(+1.0f, +0.0f) * pixelSize, depths.depth_21);
	FfxFloat32x3 p_31 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(+2.0f, +0.0f) * pixelSize, depths.depth_31);

	FfxFloat32x3 p_02 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(-1.0f, +1.0f) * pixelSize, depths.depth_02);
	FfxFloat32x3 p_12 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(+0.0f, +1.0f) * pixelSize, depths.depth_12);
	FfxFloat32x3 p_22 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(+1.0f, +1.0f) * pixelSize, depths.depth_22);
	FfxFloat32x3 p_32 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(+2.0f, +1.0f) * pixelSize, depths.depth_32);

	FfxFloat32x3 p_13 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(+0.0f, +2.0f) * pixelSize, depths.depth_13);
	FfxFloat32x3 p_23 = FFX_CACAO_NDCToViewSpace(uv + FfxFloat32x2(+1.0f, +2.0f) * pixelSize, depths.depth_23);

	FfxFloat32x4 edges_11 = FFX_CACAO_CalculateEdges(p_11.z, p_01.z, p_21.z, p_10.z, p_12.z);
	FfxFloat32x4 edges_21 = FFX_CACAO_CalculateEdges(p_21.z, p_11.z, p_31.z, p_20.z, p_22.z);
	FfxFloat32x4 edges_12 = FFX_CACAO_CalculateEdges(p_12.z, p_02.z, p_22.z, p_11.z, p_13.z);
	FfxFloat32x4 edges_22 = FFX_CACAO_CalculateEdges(p_22.z, p_12.z, p_32.z, p_21.z, p_23.z);

	FfxFloat32x3 norm_11 = FFX_CACAO_CalculateNormal(edges_11, p_11, p_01, p_21, p_10, p_12);
	FfxFloat32x3 norm_21 = FFX_CACAO_CalculateNormal(edges_21, p_21, p_11, p_31, p_20, p_22);
	FfxFloat32x3 norm_12 = FFX_CACAO_CalculateNormal(edges_12, p_12, p_02, p_22, p_11, p_13);
	FfxFloat32x3 norm_22 = FFX_CACAO_CalculateNormal(edges_22, p_22, p_12, p_32, p_21, p_23);

	FFX_CACAO_Prepare_StoreNormal(normalCoord, 0, norm_11);
	FFX_CACAO_Prepare_StoreNormal(normalCoord, 1, norm_21);
	FFX_CACAO_Prepare_StoreNormal(normalCoord, 2, norm_12);
	FFX_CACAO_Prepare_StoreNormal(normalCoord, 3, norm_22);
}

void FFX_CACAO_PrepareDownsampledNormals(FfxUInt32x2 tid)
{
	FfxInt32x2 depthCoord = 4 * FfxInt32x2(tid) + DepthBufferOffset();

	FFX_CACAO_PrepareNormalsInputDepths depths;

	depths.depth_10 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(+0, -2)));
	depths.depth_20 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(+2, -2)));

	depths.depth_01 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(-2, +0)));
	depths.depth_11 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(+0, +0)));
	depths.depth_21 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(+2, +0)));
	depths.depth_31 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(+4, +0)));

	depths.depth_02 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(-2, +2)));
	depths.depth_12 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(+0, +2)));
	depths.depth_22 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(+2, +2)));
	depths.depth_32 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(+4, +2)));

	depths.depth_13 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(+0, +4)));
	depths.depth_23 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(FFX_CACAO_Prepare_LoadDepthOffset(depthCoord, FfxInt32x2(+2, +4)));

	FfxFloat32x2 pixelSize = 2.0f * OutputBufferInverseDimensions();
	FfxFloat32x2 uv = (FfxFloat32x2(4 * tid) + 0.5f) * OutputBufferInverseDimensions();

	FFX_CACAO_PrepareNormals(depths, uv, pixelSize, tid);
}

void FFX_CACAO_PrepareNativeNormals(FfxUInt32x2 tid)
{
	FfxInt32x2 depthCoord = 2 * FfxInt32x2(tid) + DepthBufferOffset();
	FfxFloat32x2 depthBufferUV = FfxFloat32x2(depthCoord) * DepthBufferInverseDimensions();
	FfxFloat32x4 samples_00 = FFX_CACAO_Prepare_GatherDepthOffset(depthBufferUV, FfxInt32x2(0, 0)); //CACAO_TODO fix gather
	FfxFloat32x4 samples_10 = FFX_CACAO_Prepare_GatherDepthOffset(depthBufferUV, FfxInt32x2(2, 0));
	FfxFloat32x4 samples_01 = FFX_CACAO_Prepare_GatherDepthOffset(depthBufferUV, FfxInt32x2(0, 2));
	FfxFloat32x4 samples_11 = FFX_CACAO_Prepare_GatherDepthOffset(depthBufferUV, FfxInt32x2(2, 2));

	FFX_CACAO_PrepareNormalsInputDepths depths;

	depths.depth_10 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_00.z);
	depths.depth_20 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_10.w);

	depths.depth_01 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_00.x);
	depths.depth_11 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_00.y);
	depths.depth_21 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_10.x);
	depths.depth_31 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_10.y);

	depths.depth_02 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_01.w);
	depths.depth_12 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_01.z);
	depths.depth_22 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_11.w);
	depths.depth_32 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_11.z);

	depths.depth_13 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_01.y);
	depths.depth_23 = FFX_CACAO_ScreenSpaceToViewSpaceDepth(samples_11.x);

	// use unused samples to make sure compiler doesn't overlap memory and put a sync
	// between loads
	FfxFloat32 epsilon = (samples_00.w + samples_10.z + samples_01.x + samples_11.y) * 1e-20f;

	FfxFloat32x2 pixelSize = OutputBufferInverseDimensions();
	FfxFloat32x2 uv = (FfxFloat32x2(2 * tid) + 0.5f + epsilon) * OutputBufferInverseDimensions();

	FFX_CACAO_PrepareNormals(depths, uv, pixelSize, tid);
}

void FFX_CACAO_PrepareDownsampledNormalsFromInputNormals(FfxUInt32x2 tid)
{
	FfxUInt32x2 baseCoord = 4 * tid;
	FFX_CACAO_Prepare_StoreNormal(tid, 0, FFX_CACAO_Prepare_LoadNormal(baseCoord + FfxUInt32x2(0, 0)));
	FFX_CACAO_Prepare_StoreNormal(tid, 1, FFX_CACAO_Prepare_LoadNormal(baseCoord + FfxUInt32x2(2, 0)));
	FFX_CACAO_Prepare_StoreNormal(tid, 2, FFX_CACAO_Prepare_LoadNormal(baseCoord + FfxUInt32x2(0, 2)));
	FFX_CACAO_Prepare_StoreNormal(tid, 3, FFX_CACAO_Prepare_LoadNormal(baseCoord + FfxUInt32x2(2, 2)));
}

void FFX_CACAO_PrepareNativeNormalsFromInputNormals(FfxUInt32x2 tid)
{
	FfxUInt32x2 baseCoord = 2 * tid;
	FFX_CACAO_Prepare_StoreNormal(tid, 0, FFX_CACAO_Prepare_LoadNormal(baseCoord + FfxUInt32x2(0, 0)));
	FFX_CACAO_Prepare_StoreNormal(tid, 1, FFX_CACAO_Prepare_LoadNormal(baseCoord + FfxUInt32x2(1, 0)));
	FFX_CACAO_Prepare_StoreNormal(tid, 2, FFX_CACAO_Prepare_LoadNormal(baseCoord + FfxUInt32x2(0, 1)));
	FFX_CACAO_Prepare_StoreNormal(tid, 3, FFX_CACAO_Prepare_LoadNormal(baseCoord + FfxUInt32x2(1, 1)));
}
