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

FfxUInt32x2 FfxDofCocRadInTiles(FfxFloat32x2 zMinMax)
{
	FfxFloat32x2 rPx = abs(FfxDofGetCoc2(zMinMax));
	return FfxDofPxRadToTiles(rPx);
}

FfxUInt32 FfxDofMaxCocInTiles(FfxFloat32x2 zMinMax)
{
	FfxUInt32x2 rTiles = FfxDofCocRadInTiles(zMinMax);
	return max(rTiles.x, rTiles.y);
}

/// Entry point for depth downsample function. SPD is not used for this,
/// since we only need one specific downsampled resolution.
/// 
/// @param tile      coordinate of the tile to run on (SV_DispatchThreadID)
/// @param imageSize Size of the depth image (full resolution)
/// @ingroup FfxGPUDof
void DownsampleDepth(FfxUInt32x2 tile, FfxUInt32x2 imageSize)
{
	FfxFloat32 minD = 1.0;
	FfxFloat32 maxD = 0.0;

	const FfxUInt32x2 coordBase = tile * FFX_DOF_DEPTH_TILE_SIZE;
	const FfxFloat32x2 rcpImageSize = ffxReciprocal(FfxFloat32x2(imageSize));

	for (FfxUInt32 yy = 0; yy < FFX_DOF_DEPTH_TILE_SIZE; yy += 2)
	{
		for (FfxUInt32 xx = 0; xx < FFX_DOF_DEPTH_TILE_SIZE; xx += 2)
		{
			FfxUInt32x2 coordInt = coordBase + FfxUInt32x2(xx, yy);
			FfxFloat32x2 coord = ffxSaturate(FfxFloat32x2(coordInt) * rcpImageSize);
			FfxFloat32x4 d = FfxDofGatherDepth(coord);

			FfxFloat32 lo = min(min(min(d.x, d.y), d.z), d.w);
			FfxFloat32 hi = max(max(max(d.x, d.y), d.z), d.w);
			minD = min(minD, lo);
			maxD = max(maxD, hi);
		}
	}

#if FFX_DOF_OPTION_REVERSE_DEPTH
	// if Z-buffer is reversed, the nearest Z is the max.
	FfxFloat32x2 nearFarDepth = FfxFloat32x2(maxD, minD);
#else
	FfxFloat32x2 nearFarDepth = FfxFloat32x2(minD, maxD);
#endif
	FfxFloat32x2 coc = FfxDofGetCoc2(nearFarDepth);
	FfxUInt32 rTiles = FfxDofMaxCocInTiles(nearFarDepth);
	FfxDofAccumMaxTileRadius(rTiles);
	FfxDofStoreTileRadius(tile, coc);
}
