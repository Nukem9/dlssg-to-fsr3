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

void FFX_CACAO_Apply(FfxUInt32x2 coord)
{
	FfxFloat32 ao;
	FfxFloat32x2 inPos = coord;
	FfxUInt32x2 pixPos = coord;
	FfxUInt32x2 pixPosHalf = pixPos / FfxUInt32x2(2, 2);

	// calculate index in the four deinterleaved source array texture
	FfxInt32 mx = FfxInt32(pixPos.x % 2);
	FfxInt32 my = FfxInt32(pixPos.y % 2);
	FfxInt32 ic = mx + my * 2;       // center index
	FfxInt32 ih = (1 - mx) + my * 2;   // neighbouring, horizontal
	FfxInt32 iv = mx + (1 - my) * 2;   // neighbouring, vertical
	FfxInt32 id = (1 - mx) + (1 - my) * 2; // diagonal

	FfxFloat32x2 centerVal = FFX_CACAO_Apply_LoadSSAOPass(FfxInt32x2(pixPosHalf), ic);

	ao = centerVal.x;

#if 1   // change to 0 if you want to disable last pass high-res blur (for debugging purposes, etc.)
	FfxFloat32x4 edgesLRTB = FFX_CACAO_UnpackEdges(centerVal.y);

	// return 1.0 - FfxFloat32x4( edgesLRTB.x, edgesLRTB.y * 0.5 + edgesLRTB.w * 0.5, edgesLRTB.z, 0.0 ); // debug show edges

	// convert index shifts to sampling offsets
	FfxFloat32 fmx = FfxFloat32(mx);
	FfxFloat32 fmy = FfxFloat32(my);

	// in case of an edge, push sampling offsets away from the edge (towards pixel center)
	FfxFloat32 fmxe = (edgesLRTB.y - edgesLRTB.x);
	FfxFloat32 fmye = (edgesLRTB.w - edgesLRTB.z);

	// calculate final sampling offsets and sample using bilinear filter
	FfxFloat32x2  uvH = (inPos.xy + FfxFloat32x2(fmx + fmxe - 0.5, 0.5 - fmy)) * 0.5 * SSAOBufferInverseDimensions();
	FfxFloat32   aoH = FFX_CACAO_Apply_SampleSSAOUVPass(uvH, ih);
	FfxFloat32x2  uvV = (inPos.xy + FfxFloat32x2(0.5 - fmx, fmy - 0.5 + fmye)) * 0.5 * SSAOBufferInverseDimensions();
	FfxFloat32   aoV = FFX_CACAO_Apply_SampleSSAOUVPass(uvV, iv);
	FfxFloat32x2  uvD = (inPos.xy + FfxFloat32x2(fmx - 0.5 + fmxe, fmy - 0.5 + fmye)) * 0.5 * SSAOBufferInverseDimensions();
	FfxFloat32   aoD = FFX_CACAO_Apply_SampleSSAOUVPass(uvD, id);

	// reduce weight for samples near edge - if the edge is on both sides, weight goes to 0
	FfxFloat32x4 blendWeights;
	blendWeights.x = 1.0;
	blendWeights.y = (edgesLRTB.x + edgesLRTB.y) * 0.5;
	blendWeights.z = (edgesLRTB.z + edgesLRTB.w) * 0.5;
	blendWeights.w = (blendWeights.y + blendWeights.z) * 0.5;

	// calculate weighted average
	FfxFloat32 blendWeightsSum = dot(blendWeights, FfxFloat32x4(1.0, 1.0, 1.0, 1.0));
	ao = dot(FfxFloat32x4(ao, aoH, aoV, aoD), blendWeights) / blendWeightsSum;
#endif
	ao.x = pow(ao.x, 2.2);
	FFX_CACAO_Apply_StoreOutput(FfxInt32x2(coord), ao.x);
}


// edge-ignorant blur & apply (for the lowest quality level 0)
void FFX_CACAO_NonSmartApply(FfxUInt32x2 tid)
{
	FfxFloat32x2 inUV = FfxFloat32x2(tid) * OutputBufferInverseDimensions();
	FfxFloat32 a = FFX_CACAO_Apply_SampleSSAOUVPass(inUV.xy, 0);
	FfxFloat32 b = FFX_CACAO_Apply_SampleSSAOUVPass(inUV.xy, 1);
	FfxFloat32 c = FFX_CACAO_Apply_SampleSSAOUVPass(inUV.xy, 2);
	FfxFloat32 d = FFX_CACAO_Apply_SampleSSAOUVPass(inUV.xy, 3);
	FfxFloat32 avg = (a + b + c + d) * 0.25f;

	FFX_CACAO_Apply_StoreOutput(FfxInt32x2(tid), avg);
}

// edge-ignorant blur & apply, skipping half pixels in checkerboard pattern (for the Lowest quality level 0 and Settings::SkipHalfPixelsOnLowQualityLevel == true )
void FFX_CACAO_NonSmartHalfApply(FfxUInt32x2 tid)
{
	FfxFloat32x2 inUV = FfxFloat32x2(tid) * OutputBufferInverseDimensions();
	FfxFloat32 a = FFX_CACAO_Apply_SampleSSAOUVPass(inUV.xy, 0);
	FfxFloat32 d = FFX_CACAO_Apply_SampleSSAOUVPass(inUV.xy, 3);
	FfxFloat32 avg = (a + d) * 0.5f;

	FFX_CACAO_Apply_StoreOutput(FfxInt32x2(tid), avg);
}
