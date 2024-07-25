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

#define FFX_CACAO_BILATERAL_UPSCALE_BUFFER_WIDTH  (FFX_CACAO_BILATERAL_UPSCALE_WIDTH  + 4)
#define FFX_CACAO_BILATERAL_UPSCALE_BUFFER_HEIGHT (FFX_CACAO_BILATERAL_UPSCALE_HEIGHT + 4 + 4)

struct FFX_CACAO_BilateralBufferVal
{
	FfxUInt32 packedDepths;
	FfxUInt32 packedSsaoVals;
};

FFX_GROUPSHARED FFX_CACAO_BilateralBufferVal s_FFX_CACAO_BilateralUpscaleBuffer[FFX_CACAO_BILATERAL_UPSCALE_BUFFER_WIDTH][FFX_CACAO_BILATERAL_UPSCALE_BUFFER_HEIGHT];

void FFX_CACAO_BilateralUpscaleNxN(FfxUInt32x2 tid, FfxUInt32x2 gtid, FfxUInt32x2 gid, const FfxInt32 width, const FfxInt32 height, const bool useEdges)
{
	// fill in group shared buffer
	{
		FfxUInt32 threadNum = (gtid.y * FFX_CACAO_BILATERAL_UPSCALE_WIDTH + gtid.x) * 3;
		FfxUInt32x2 bufferCoord = FfxUInt32x2(threadNum % FFX_CACAO_BILATERAL_UPSCALE_BUFFER_WIDTH, threadNum / FFX_CACAO_BILATERAL_UPSCALE_BUFFER_WIDTH);
		FfxUInt32x2 imageCoord = (gid * FfxUInt32x2(FFX_CACAO_BILATERAL_UPSCALE_WIDTH, FFX_CACAO_BILATERAL_UPSCALE_HEIGHT)) + bufferCoord - 2;

		if (useEdges)
		{
			FfxFloat32x2 inputs[3];
			for (FfxInt32 j = 0; j < 3; ++j)
			{
				FfxInt32x2 p = FfxInt32x2(imageCoord.x + j, imageCoord.y);
				FfxInt32x2 pos = p / 2;
				FfxInt32 index = (p.x % 2) + 2 * (p.y % 2);
				inputs[j] = FFX_CACAO_BilateralUpscale_LoadSSAO(pos, index);
			}

			for (FfxInt32 i = 0; i < 3; ++i)
			{
				FfxInt32 mx = FfxInt32(imageCoord.x % 2);
				FfxInt32 my = FfxInt32(imageCoord.y % 2);

				FfxInt32 ic = mx + my * 2;       // center index
				FfxInt32 ih = (1 - mx) + my * 2;   // neighbouring, horizontal
				FfxInt32 iv = mx + (1 - my) * 2;   // neighbouring, vertical
				FfxInt32 id = (1 - mx) + (1 - my) * 2; // diagonal

				FfxFloat32x2 centerVal = inputs[i];

				FfxFloat32 ao = centerVal.x;

				FfxFloat32x4 edgesLRTB = FFX_CACAO_UnpackEdges(centerVal.y);

				// convert index shifts to sampling offsets
				FfxFloat32 fmx = FfxFloat32(mx);
				FfxFloat32 fmy = FfxFloat32(my);

				// in case of an edge, push sampling offsets away from the edge (towards pixel center)
				FfxFloat32 fmxe = (edgesLRTB.y - edgesLRTB.x);
				FfxFloat32 fmye = (edgesLRTB.w - edgesLRTB.z);

				// calculate final sampling offsets and sample using bilinear filter
				FfxFloat32x2 p = imageCoord;
				FfxFloat32x2  uvH = (p + FfxFloat32x2(fmx + fmxe - 0.5, 0.5 - fmy)) * 0.5 * SSAOBufferInverseDimensions();
				FfxFloat32   aoH = FFX_CACAO_BilateralUpscale_SampleSSAOLinear(uvH, ih);
				FfxFloat32x2  uvV = (p + FfxFloat32x2(0.5 - fmx, fmy - 0.5 + fmye)) * 0.5 * SSAOBufferInverseDimensions();
				FfxFloat32   aoV = FFX_CACAO_BilateralUpscale_SampleSSAOLinear(uvV, iv);
				FfxFloat32x2  uvD = (p + FfxFloat32x2(fmx - 0.5 + fmxe, fmy - 0.5 + fmye)) * 0.5 * SSAOBufferInverseDimensions();
				FfxFloat32   aoD = FFX_CACAO_BilateralUpscale_SampleSSAOLinear(uvD, id);

				// reduce weight for samples near edge - if the edge is on both sides, weight goes to 0
				FfxFloat32x4 blendWeights;
				blendWeights.x = 1.0;
				blendWeights.y = (edgesLRTB.x + edgesLRTB.y) * 0.5;
				blendWeights.z = (edgesLRTB.z + edgesLRTB.w) * 0.5;
				blendWeights.w = (blendWeights.y + blendWeights.z) * 0.5;

				// calculate weighted average
				FfxFloat32 blendWeightsSum = dot(blendWeights, FfxFloat32x4(1.0, 1.0, 1.0, 1.0));
				ao = dot(FfxFloat32x4(ao, aoH, aoV, aoD), blendWeights) / blendWeightsSum;

				++imageCoord.x;

				FFX_CACAO_BilateralBufferVal bufferVal;

				FfxUInt32x2 depthArrayBufferCoord = FfxUInt32x2((imageCoord / 2) + DeinterleavedDepthBufferOffset());
				FfxUInt32 depthArrayBufferIndex = ic;
				FfxFloat32 depth = FFX_CACAO_BilateralUpscale_LoadDownscaledDepth(depthArrayBufferCoord, depthArrayBufferIndex);

				bufferVal.packedDepths = ffxPackF32(FfxFloat32x2(depth, depth));
				bufferVal.packedSsaoVals = ffxPackF32(FfxFloat32x2(ao, ao));

				s_FFX_CACAO_BilateralUpscaleBuffer[bufferCoord.x + i][bufferCoord.y] = bufferVal;
			}
		}
		else
		{
			for (FfxInt32 i = 0; i < 3; ++i)
			{
				FfxFloat32x2 sampleLoc0 = (FfxFloat32x2(imageCoord / 2) + 0.5f) * SSAOBufferInverseDimensions();
				FfxFloat32x2 sampleLoc1 = sampleLoc0;
				FfxFloat32x2 sampleLoc2 = sampleLoc0;
				FfxFloat32x2 sampleLoc3 = sampleLoc0;
				switch ((imageCoord.y % 2) * 2 + (imageCoord.x % 2)) {
				case 0:
					sampleLoc1.x -= 0.5f * SSAOBufferInverseDimensions().x;
					sampleLoc2.y -= 0.5f * SSAOBufferInverseDimensions().y;
					sampleLoc3 -= 0.5f * SSAOBufferInverseDimensions();
					break;
				case 1:
					sampleLoc0.x += 0.5f * SSAOBufferInverseDimensions().x;
					sampleLoc2 += FfxFloat32x2(0.5f, -0.5f) * SSAOBufferInverseDimensions();
					sampleLoc3.y -= 0.5f * SSAOBufferInverseDimensions().y;
					break;
				case 2:
					sampleLoc0.y += 0.5f * SSAOBufferInverseDimensions().y;
					sampleLoc1 += FfxFloat32x2(-0.5f, 0.5f) * SSAOBufferInverseDimensions();
					sampleLoc3.x -= 0.5f * SSAOBufferInverseDimensions().x;
					break;
				case 3:
					sampleLoc0 += 0.5f * SSAOBufferInverseDimensions();
					sampleLoc1.y += 0.5f * SSAOBufferInverseDimensions().y;
					sampleLoc2.x += 0.5f * SSAOBufferInverseDimensions().x;
					break;
				}

				FfxFloat32 ssaoVal0 = FFX_CACAO_BilateralUpscale_SampleSSAOPoint(sampleLoc0, 0);
				FfxFloat32 ssaoVal1 = FFX_CACAO_BilateralUpscale_SampleSSAOPoint(sampleLoc1, 1);
				FfxFloat32 ssaoVal2 = FFX_CACAO_BilateralUpscale_SampleSSAOPoint(sampleLoc2, 2);
				FfxFloat32 ssaoVal3 = FFX_CACAO_BilateralUpscale_SampleSSAOPoint(sampleLoc3, 3);

				FfxUInt32x3 ssaoArrayBufferCoord = FfxUInt32x3(imageCoord / 2, 2 * (imageCoord.y % 2) + imageCoord.x % 2);
				FfxUInt32x2 depthArrayBufferCoord = FfxUInt32x2(ssaoArrayBufferCoord.xy + DeinterleavedDepthBufferOffset());
				FfxUInt32 depthArrayBufferIndex = ssaoArrayBufferCoord.z;
				++imageCoord.x;

				FFX_CACAO_BilateralBufferVal bufferVal;

				FfxFloat32 depth = FFX_CACAO_BilateralUpscale_LoadDownscaledDepth(depthArrayBufferCoord, depthArrayBufferIndex);
				FfxFloat32 ssaoVal = (ssaoVal0 + ssaoVal1 + ssaoVal2 + ssaoVal3) * 0.25f;

				bufferVal.packedDepths = ffxPackF32(FfxFloat32x2(depth, depth));
				bufferVal.packedSsaoVals = ffxPackF32(FfxFloat32x2(ssaoVal, ssaoVal)); //CACAO_TODO half path?

				s_FFX_CACAO_BilateralUpscaleBuffer[bufferCoord.x + i][bufferCoord.y] = bufferVal;
			}
		}
	}

	FFX_GROUP_MEMORY_BARRIER;

	FfxFloat32 depths[4];
	// load depths
	{
		FfxInt32x2 fullBufferCoord = FfxInt32x2(2 * tid);
		FfxInt32x2 fullDepthBufferCoord = fullBufferCoord + DepthBufferOffset();

		FfxFloat32x4 screenSpaceDepths = FFX_CACAO_BilateralUpscale_LoadDepths(fullDepthBufferCoord);

		depths[0] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.x);
		depths[1] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.y);
		depths[2] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.z);
		depths[3] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.w);
	}

#if FFX_HALF
	FfxFloat16x4 packedDepths = FfxFloat16x4(depths[0], depths[1], depths[2], depths[3]);

	FfxInt32x2 baseBufferCoord = FfxInt32x2(gtid) + FfxInt32x2(width, height);

	FfxFloat16 epsilonWeight = FfxFloat16(1e-3f);
	FfxFloat16x2 nearestSsaoVals = ffxUnpackF16(s_FFX_CACAO_BilateralUpscaleBuffer[baseBufferCoord.x][baseBufferCoord.y].packedSsaoVals);
	FfxFloat16x4 packedTotals = epsilonWeight * FfxFloat16x4(1.0f, 1.0f, 1.0f, 1.0f);
	packedTotals.xy *= nearestSsaoVals;
	packedTotals.zw *= nearestSsaoVals;
	FfxFloat16x4 packedTotalWeights = epsilonWeight * FfxFloat16x4(1.0f, 1.0f, 1.0f, 1.0f);

	FfxFloat32 distanceSigma = BilateralSimilarityDistanceSigma();
	FfxFloat16x2 packedDistSigma = FfxFloat16x2(1.0f / distanceSigma, 1.0f / distanceSigma);
	FfxFloat32 sigma = BilateralSigmaSquared();
	FfxFloat16x2 packedSigma = FfxFloat16x2(1.0f / sigma, 1.0f / sigma);

	for (FfxInt32 x = -width; x <= width; ++x)
	{
		for (FfxInt32 y = -height; y <= height; ++y)
		{
			FfxInt32x2 bufferCoord = baseBufferCoord + FfxInt32x2(x, y);

			FFX_CACAO_BilateralBufferVal bufferVal = s_FFX_CACAO_BilateralUpscaleBuffer[bufferCoord.x][bufferCoord.y];

			FfxFloat16x2 u = FfxFloat16x2(x, x) - FfxFloat16x2(0.0f, 0.5f);
			FfxFloat16x2 v1 = FfxFloat16x2(y, y) - FfxFloat16x2(0.0f, 0.0f);
			FfxFloat16x2 v2 = FfxFloat16x2(y, y) - FfxFloat16x2(0.5f, 0.5f);
			u = u * u;
			v1 = v1 * v1;
			v2 = v2 * v2;

			FfxFloat16x2 dist1 = u + v1;
			FfxFloat16x2 dist2 = u + v2;

			FfxFloat16x2 wx1 = exp(-dist1 * packedSigma);
			FfxFloat16x2 wx2 = exp(-dist2 * packedSigma);

			FfxFloat16x2 bufferPackedDepths = ffxUnpackF16(bufferVal.packedDepths);

			FfxFloat16x2 diff1 = packedDepths.xy - bufferPackedDepths;
			FfxFloat16x2 diff2 = packedDepths.zw - bufferPackedDepths;
			diff1 *= diff1;
			diff2 *= diff2;

			FfxFloat16x2 wy1 = exp(-diff1 * packedDistSigma);
			FfxFloat16x2 wy2 = exp(-diff2 * packedDistSigma);

			FfxFloat16x2 weight1 = wx1 * wy1;
			FfxFloat16x2 weight2 = wx2 * wy2;

			FfxFloat16x2 packedSsaoVals = ffxUnpackF16(bufferVal.packedSsaoVals);
			packedTotals.xy += packedSsaoVals * weight1;
			packedTotals.zw += packedSsaoVals * weight2;
			packedTotalWeights.xy += weight1;
			packedTotalWeights.zw += weight2;
		}
	}

	FfxUInt32x2 outputCoord = 2 * tid;
	FfxFloat16x4 outputValues = packedTotals / packedTotalWeights;
#else
	FfxFloat32x4 packedDepths = FfxFloat32x4(depths[0], depths[1], depths[2], depths[3]);

	FfxInt32x2 baseBufferCoord = FfxInt32x2(gtid) + FfxInt32x2(width, height);

	FfxFloat32 epsilonWeight = FfxFloat32(1e-3f);
	FfxFloat32x2 nearestSsaoVals = ffxUnpackF32(s_FFX_CACAO_BilateralUpscaleBuffer[baseBufferCoord.x][baseBufferCoord.y].packedSsaoVals);
	FfxFloat32x4 packedTotals = epsilonWeight * FfxFloat32x4(1.0f, 1.0f, 1.0f, 1.0f);
	packedTotals.xy *= nearestSsaoVals;
	packedTotals.zw *= nearestSsaoVals;
	FfxFloat32x4 packedTotalWeights = epsilonWeight * FfxFloat32x4(1.0f, 1.0f, 1.0f, 1.0f);

	FfxFloat32 distanceSigma = BilateralSimilarityDistanceSigma();
	FfxFloat32x2 packedDistSigma = FfxFloat32x2(1.0f / distanceSigma, 1.0f / distanceSigma);
	FfxFloat32 sigma = BilateralSigmaSquared();
	FfxFloat32x2 packedSigma = FfxFloat32x2(1.0f / sigma, 1.0f / sigma);

	for (FfxInt32 x = -width; x <= width; ++x)
	{
		for (FfxInt32 y = -height; y <= height; ++y)
		{
			FfxInt32x2 bufferCoord = baseBufferCoord + FfxInt32x2(x, y);

			FFX_CACAO_BilateralBufferVal bufferVal = s_FFX_CACAO_BilateralUpscaleBuffer[bufferCoord.x][bufferCoord.y];

			FfxFloat32x2 u = FfxFloat32x2(x, x) - FfxFloat32x2(0.0f, 0.5f);
			FfxFloat32x2 v1 = FfxFloat32x2(y, y) - FfxFloat32x2(0.0f, 0.0f);
			FfxFloat32x2 v2 = FfxFloat32x2(y, y) - FfxFloat32x2(0.5f, 0.5f);
			u = u * u;
			v1 = v1 * v1;
			v2 = v2 * v2;

			FfxFloat32x2 dist1 = u + v1;
			FfxFloat32x2 dist2 = u + v2;

			FfxFloat32x2 wx1 = exp(-dist1 * packedSigma);
			FfxFloat32x2 wx2 = exp(-dist2 * packedSigma);

			FfxFloat32x2 bufferPackedDepths = ffxUnpackF32(bufferVal.packedDepths);

			FfxFloat32x2 diff1 = packedDepths.xy - bufferPackedDepths;
			FfxFloat32x2 diff2 = packedDepths.zw - bufferPackedDepths;
			diff1 *= diff1;
			diff2 *= diff2;

			FfxFloat32x2 wy1 = exp(-diff1 * packedDistSigma);
			FfxFloat32x2 wy2 = exp(-diff2 * packedDistSigma);

			FfxFloat32x2 weight1 = wx1 * wy1;
			FfxFloat32x2 weight2 = wx2 * wy2;

			FfxFloat32x2 packedSsaoVals = ffxUnpackF32(bufferVal.packedSsaoVals);
			packedTotals.xy += packedSsaoVals * weight1;
			packedTotals.zw += packedSsaoVals * weight2;
			packedTotalWeights.xy += weight1;
			packedTotalWeights.zw += weight2;
		}
	}

	FfxUInt32x2 outputCoord = 2 * tid;
	FfxFloat32x4 outputValues = packedTotals / packedTotalWeights;
	outputValues = pow(outputValues, FfxFloat32x4(2.2, 2.2, 2.2, 2.2));
#endif //FFX_HALF
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(0, 0), outputValues.x); // totals[0] / totalWeights[0];
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(1, 0), outputValues.y); // totals[1] / totalWeights[1];
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(0, 1), outputValues.z); // totals[2] / totalWeights[2];
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(1, 1), outputValues.w); // totals[3] / totalWeights[3];
}


void FFX_CACAO_UpscaleBilateral5x5Smart(FfxUInt32x2 tid, FfxUInt32x2 gtid, FfxUInt32x2 gid)
{
	FFX_CACAO_BilateralUpscaleNxN(tid, gtid, gid, 2, 2, true);
}

void FFX_CACAO_UpscaleBilateral5x5NonSmart(FfxUInt32x2 tid, FfxUInt32x2 gtid, FfxUInt32x2 gid)
{
	FFX_CACAO_BilateralUpscaleNxN(tid, gtid, gid, 2, 2, false);
}

void FFX_CACAO_UpscaleBilateral7x7(FfxUInt32x2 tid, FfxUInt32x2 gtid, FfxUInt32x2 gid)
{
	FFX_CACAO_BilateralUpscaleNxN(tid, gtid, gid, 3, 3, true);
}

#if FFX_HALF
void FFX_CACAO_UpscaleBilateral5x5Half(FfxUInt32x2 tid, FfxUInt32x2 gtid, FfxUInt32x2 gid)
{
	const FfxInt32 width = 2, height = 2;

	// fill in group shared buffer
	{
		FfxUInt32 threadNum = (gtid.y * FFX_CACAO_BILATERAL_UPSCALE_WIDTH + gtid.x) * 3;
		FfxUInt32x2 bufferCoord = FfxUInt32x2(threadNum % FFX_CACAO_BILATERAL_UPSCALE_BUFFER_WIDTH, threadNum / FFX_CACAO_BILATERAL_UPSCALE_BUFFER_WIDTH);
		FfxUInt32x2 imageCoord = (gid * FfxUInt32x2(FFX_CACAO_BILATERAL_UPSCALE_WIDTH, FFX_CACAO_BILATERAL_UPSCALE_HEIGHT)) + bufferCoord - 2;

		for (FfxInt32 i = 0; i < 3; ++i)
		{
			FfxFloat32x2 sampleLoc0 = (FfxFloat32x2(imageCoord / 2) + 0.5f) * SSAOBufferInverseDimensions();
			FfxFloat32x2 sampleLoc1 = sampleLoc0;
			switch ((imageCoord.y % 2) * 2 + (imageCoord.x % 2)) {
			case 0:
				sampleLoc1 -= 0.5f * SSAOBufferInverseDimensions();
				break;
			case 1:
				sampleLoc0.x += 0.5f * SSAOBufferInverseDimensions().x;
				sampleLoc1.y -= 0.5f * SSAOBufferInverseDimensions().y;
				break;
			case 2:
				sampleLoc0.y += 0.5f * SSAOBufferInverseDimensions().y;
				sampleLoc1.x -= 0.5f * SSAOBufferInverseDimensions().x;
				break;
			case 3:
				sampleLoc0 += 0.5f * SSAOBufferInverseDimensions();
				break;
			}

			FfxFloat32 ssaoVal0 = FFX_CACAO_BilateralUpscale_SampleSSAOPoint(sampleLoc0, 0);
			FfxFloat32 ssaoVal1 = FFX_CACAO_BilateralUpscale_SampleSSAOPoint(sampleLoc1, 3);

			FfxUInt32x2 depthArrayBufferCoord = FfxUInt32x2((imageCoord / 2) + DeinterleavedDepthBufferOffset());
			FfxUInt32 depthArrayBufferIndex = (imageCoord.y % 2) * 3;
			++imageCoord.x;

			FFX_CACAO_BilateralBufferVal bufferVal;

			FfxFloat32 depth = FFX_CACAO_BilateralUpscale_LoadDownscaledDepth(depthArrayBufferCoord, depthArrayBufferIndex);
			FfxFloat32 ssaoVal = (ssaoVal0 + ssaoVal1) * 0.5f;

			bufferVal.packedDepths = ffxPackF16(FfxFloat16x2(depth, depth));
			bufferVal.packedSsaoVals = ffxPackF16(FfxFloat16x2(ssaoVal, ssaoVal));

			s_FFX_CACAO_BilateralUpscaleBuffer[bufferCoord.x + i][bufferCoord.y] = bufferVal;
		}
	}

	FFX_GROUP_MEMORY_BARRIER;

	FfxFloat32 depths[4];
	// load depths
	{
		FfxInt32x2 fullBufferCoord = FfxInt32x2(2 * tid);
		FfxInt32x2 fullDepthBufferCoord = fullBufferCoord + DepthBufferOffset();

		FfxFloat32x4 screenSpaceDepths = FFX_CACAO_BilateralUpscale_LoadDepths(fullDepthBufferCoord);

		depths[0] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.x);
		depths[1] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.y);
		depths[2] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.z);
		depths[3] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.w);
	}
	FfxFloat16x4 packedDepths = FfxFloat16x4(depths[0], depths[1], depths[2], depths[3]);

	FfxInt32x2 baseBufferCoord = FfxInt32x2(gtid) + FfxInt32x2(width, height);

	FfxFloat16 epsilonWeight = FfxFloat16(1e-3f);
	FfxFloat16x2 nearestSsaoVals = ffxUnpackF16(s_FFX_CACAO_BilateralUpscaleBuffer[baseBufferCoord.x][baseBufferCoord.y].packedSsaoVals);
	FfxFloat16x4 packedTotals = epsilonWeight * FfxFloat16x4(1.0f, 1.0f, 1.0f, 1.0f);
	packedTotals.xy *= nearestSsaoVals;
	packedTotals.zw *= nearestSsaoVals;
	FfxFloat16x4 packedTotalWeights = epsilonWeight * FfxFloat16x4(1.0f, 1.0f, 1.0f, 1.0f);

	FfxFloat32 distanceSigma = BilateralSimilarityDistanceSigma();
	FfxFloat16x2 packedDistSigma = FfxFloat16x2(1.0f / distanceSigma, 1.0f / distanceSigma);
	FfxFloat32 sigma = BilateralSigmaSquared();
	FfxFloat16x2 packedSigma = FfxFloat16x2(1.0f / sigma, 1.0f / sigma);

	for (FfxInt32 x = -width; x <= width; ++x)
	{
		for (FfxInt32 y = -height; y <= height; ++y)
		{
			FfxInt32x2 bufferCoord = baseBufferCoord + FfxInt32x2(x, y);

			FFX_CACAO_BilateralBufferVal bufferVal = s_FFX_CACAO_BilateralUpscaleBuffer[bufferCoord.x][bufferCoord.y];

			FfxFloat16x2 u = FfxFloat16x2(x, x) - FfxFloat16x2(0.0f, 0.5f);
			FfxFloat16x2 v1 = FfxFloat16x2(y, y) - FfxFloat16x2(0.0f, 0.0f);
			FfxFloat16x2 v2 = FfxFloat16x2(y, y) - FfxFloat16x2(0.5f, 0.5f);
			u = u * u;
			v1 = v1 * v1;
			v2 = v2 * v2;

			FfxFloat16x2 dist1 = u + v1;
			FfxFloat16x2 dist2 = u + v2;

			FfxFloat16x2 wx1 = exp(-dist1 * packedSigma);
			FfxFloat16x2 wx2 = exp(-dist2 * packedSigma);

			FfxFloat16x2 bufferPackedDepths = ffxUnpackF16(bufferVal.packedDepths);

			FfxFloat16x2 diff1 = packedDepths.xy - bufferPackedDepths;
			FfxFloat16x2 diff2 = packedDepths.zw - bufferPackedDepths;
			diff1 *= diff1;
			diff2 *= diff2;

			FfxFloat16x2 wy1 = exp(-diff1 * packedDistSigma);
			FfxFloat16x2 wy2 = exp(-diff2 * packedDistSigma);

			FfxFloat16x2 weight1 = wx1 * wy1;
			FfxFloat16x2 weight2 = wx2 * wy2;

			FfxFloat16x2 packedSsaoVals = ffxUnpackF16(bufferVal.packedSsaoVals);
			packedTotals.xy += packedSsaoVals * weight1;
			packedTotals.zw += packedSsaoVals * weight2;
			packedTotalWeights.xy += weight1;
			packedTotalWeights.zw += weight2;
		}
	}

	FfxUInt32x2 outputCoord = 2 * tid;
	FfxFloat16x4 outputValues = packedTotals / packedTotalWeights;
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(0, 0), outputValues.x); // totals[0] / totalWeights[0];
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(1, 0), outputValues.y); // totals[1] / totalWeights[1];
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(0, 1), outputValues.z); // totals[2] / totalWeights[2];
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(1, 1), outputValues.w); // totals[3] / totalWeights[3];
}
#else
void FFX_CACAO_UpscaleBilateral5x5(FfxUInt32x2 tid, FfxUInt32x2 gtid, FfxUInt32x2 gid)
{
	const FfxInt32 width = 2, height = 2;

	// fill in group shared buffer
	{
		FfxUInt32 threadNum = (gtid.y * FFX_CACAO_BILATERAL_UPSCALE_WIDTH + gtid.x) * 3;
		FfxUInt32x2 bufferCoord = FfxUInt32x2(threadNum % FFX_CACAO_BILATERAL_UPSCALE_BUFFER_WIDTH, threadNum / FFX_CACAO_BILATERAL_UPSCALE_BUFFER_WIDTH);
		FfxUInt32x2 imageCoord = (gid * FfxUInt32x2(FFX_CACAO_BILATERAL_UPSCALE_WIDTH, FFX_CACAO_BILATERAL_UPSCALE_HEIGHT)) + bufferCoord - 2;

		for (FfxInt32 i = 0; i < 3; ++i)
		{
			FfxFloat32x2 sampleLoc0 = (FfxFloat32x2(imageCoord / 2) + 0.5f) * SSAOBufferInverseDimensions();
			FfxFloat32x2 sampleLoc1 = sampleLoc0;
			switch ((imageCoord.y % 2) * 2 + (imageCoord.x % 2)) {
			case 0:
				sampleLoc1 -= 0.5f * SSAOBufferInverseDimensions();
				break;
			case 1:
				sampleLoc0.x += 0.5f * SSAOBufferInverseDimensions().x;
				sampleLoc1.y -= 0.5f * SSAOBufferInverseDimensions().y;
				break;
			case 2:
				sampleLoc0.y += 0.5f * SSAOBufferInverseDimensions().y;
				sampleLoc1.x -= 0.5f * SSAOBufferInverseDimensions().x;
				break;
			case 3:
				sampleLoc0 += 0.5f * SSAOBufferInverseDimensions();
				break;
			}

			FfxFloat32 ssaoVal0 = FFX_CACAO_BilateralUpscale_SampleSSAOPoint(sampleLoc0, 0);
			FfxFloat32 ssaoVal1 = FFX_CACAO_BilateralUpscale_SampleSSAOPoint(sampleLoc1, 3);

			FfxUInt32x2 depthArrayBufferCoord = FfxUInt32x2((imageCoord / 2) + DeinterleavedDepthBufferOffset());
			FfxUInt32 depthArrayBufferIndex = (imageCoord.y % 2) * 3;
			++imageCoord.x;

			FFX_CACAO_BilateralBufferVal bufferVal;

			FfxFloat32 depth = FFX_CACAO_BilateralUpscale_LoadDownscaledDepth(depthArrayBufferCoord, depthArrayBufferIndex);
			FfxFloat32 ssaoVal = (ssaoVal0 + ssaoVal1) * 0.5f;

			bufferVal.packedDepths = ffxPackF32(FfxFloat32x2(depth, depth));
			bufferVal.packedSsaoVals = ffxPackF32(FfxFloat32x2(ssaoVal, ssaoVal));

			s_FFX_CACAO_BilateralUpscaleBuffer[bufferCoord.x + i][bufferCoord.y] = bufferVal;
		}
	}

	FFX_GROUP_MEMORY_BARRIER;

	FfxFloat32 depths[4];
	// load depths
	{
		FfxInt32x2 fullBufferCoord = FfxInt32x2(2 * tid);
		FfxInt32x2 fullDepthBufferCoord = fullBufferCoord + DepthBufferOffset();

		FfxFloat32x4 screenSpaceDepths = FFX_CACAO_BilateralUpscale_LoadDepths(fullDepthBufferCoord);

		depths[0] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.x);
		depths[1] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.y);
		depths[2] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.z);
		depths[3] = FFX_CACAO_ScreenSpaceToViewSpaceDepth(screenSpaceDepths.w);
	}
	FfxFloat32x4 packedDepths = FfxFloat32x4(depths[0], depths[1], depths[2], depths[3]);

	FfxInt32x2 baseBufferCoord = FfxInt32x2(gtid) + FfxInt32x2(width, height);

	FfxFloat32 epsilonWeight = FfxFloat32(1e-3f);
	FfxFloat32x2 nearestSsaoVals = ffxUnpackF32(s_FFX_CACAO_BilateralUpscaleBuffer[baseBufferCoord.x][baseBufferCoord.y].packedSsaoVals);
	FfxFloat32x4 packedTotals = epsilonWeight * FfxFloat32x4(1.0f, 1.0f, 1.0f, 1.0f);
	packedTotals.xy *= nearestSsaoVals;
	packedTotals.zw *= nearestSsaoVals;
	FfxFloat32x4 packedTotalWeights = epsilonWeight * FfxFloat32x4(1.0f, 1.0f, 1.0f, 1.0f);

	FfxFloat32 distanceSigma = BilateralSimilarityDistanceSigma();
	FfxFloat32x2 packedDistSigma = FfxFloat32x2(1.0f / distanceSigma, 1.0f / distanceSigma);
	FfxFloat32 sigma = BilateralSigmaSquared();
	FfxFloat32x2 packedSigma = FfxFloat32x2(1.0f / sigma, 1.0f / sigma);

	for (FfxInt32 x = -width; x <= width; ++x)
	{
		for (FfxInt32 y = -height; y <= height; ++y)
		{
			FfxInt32x2 bufferCoord = baseBufferCoord + FfxInt32x2(x, y);

			FFX_CACAO_BilateralBufferVal bufferVal = s_FFX_CACAO_BilateralUpscaleBuffer[bufferCoord.x][bufferCoord.y];

			FfxFloat32x2 u = FfxFloat32x2(x, x) - FfxFloat32x2(0.0f, 0.5f);
			FfxFloat32x2 v1 = FfxFloat32x2(y, y) - FfxFloat32x2(0.0f, 0.0f);
			FfxFloat32x2 v2 = FfxFloat32x2(y, y) - FfxFloat32x2(0.5f, 0.5f);
			u = u * u;
			v1 = v1 * v1;
			v2 = v2 * v2;

			FfxFloat32x2 dist1 = u + v1;
			FfxFloat32x2 dist2 = u + v2;

			FfxFloat32x2 wx1 = exp(-dist1 * packedSigma);
			FfxFloat32x2 wx2 = exp(-dist2 * packedSigma);

			FfxFloat32x2 bufferPackedDepths = ffxUnpackF32(bufferVal.packedDepths);

			FfxFloat32x2 diff1 = packedDepths.xy - bufferPackedDepths;
			FfxFloat32x2 diff2 = packedDepths.zw - bufferPackedDepths;
			diff1 *= diff1;
			diff2 *= diff2;

			FfxFloat32x2 wy1 = exp(-diff1 * packedDistSigma);
			FfxFloat32x2 wy2 = exp(-diff2 * packedDistSigma);

			FfxFloat32x2 weight1 = wx1 * wy1;
			FfxFloat32x2 weight2 = wx2 * wy2;

			FfxFloat32x2 packedSsaoVals = ffxUnpackF32(bufferVal.packedSsaoVals);
			packedTotals.xy += packedSsaoVals * weight1;
			packedTotals.zw += packedSsaoVals * weight2;
			packedTotalWeights.xy += weight1;
			packedTotalWeights.zw += weight2;
		}
	}

	FfxUInt32x2 outputCoord = 2 * tid;
	FfxFloat32x4 outputValues = packedTotals / packedTotalWeights;
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(0, 0), outputValues.x); // totals[0] / totalWeights[0];
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(1, 0), outputValues.y); // totals[1] / totalWeights[1];
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(0, 1), outputValues.z); // totals[2] / totalWeights[2];
	FFX_CACAO_BilateralUpscale_StoreOutput(outputCoord, FfxInt32x2(1, 1), outputValues.w); // totals[3] / totalWeights[3];
}
#endif //FFX_HALF

void FFX_CACAO_UpscaleBilateral5x5Pass(FfxUInt32x2 tid, FfxUInt32x2 gtid, FfxUInt32x2 gid)
{
	#if FFX_CACAO_OPTION_APPLY_SMART
        FFX_CACAO_UpscaleBilateral5x5Smart(tid, gtid, gid);
    #else

        #if FFX_HALF
            FFX_CACAO_UpscaleBilateral5x5Half(tid, gtid, gid);
        #else
            FFX_CACAO_UpscaleBilateral5x5NonSmart(tid, gtid, gid);
        #endif

    #endif
}

#undef FFX_CACAO_BILATERAL_UPSCALE_BUFFER_WIDTH
#undef FFX_CACAO_BILATERAL_UPSCALE_BUFFER_HEIGHT
