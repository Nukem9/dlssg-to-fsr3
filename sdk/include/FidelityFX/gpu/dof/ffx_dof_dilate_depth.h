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

// helper function for dilating depth values circularly
void FfxDofDilateStep(inout FfxFloat32 cMin, inout FfxFloat32 cMax, FfxInt32 x, FfxInt32 y, FfxInt32 r, FfxUInt32x2 size, FfxUInt32x2 dtID)
{
	if (x * x + y * y <= r * r)
	{
		// inside the circle
		FfxInt32x2 tileID = FfxInt32x2(dtID) + FfxInt32x2(x, y);
		if (tileID.x >= 0 && tileID.y >= 0 && tileID.x < size.x && tileID.y < size.y)
		{
			FfxFloat32x2 localCocRange = FfxDofLoadTileRadius(tileID);
			FfxUInt32x2 tileRad = FfxDofPxRadToTiles(abs(localCocRange));

			// seperately for min/max: check radius in range to spread and update
			if (x * x + y * y < tileRad.x * tileRad.x)
			{
				// using max for cMin and min for cMax, since cMin actually refers to the CoC for the minimal view depth
				// NOT the minimum signed CoC value
				cMin = max(cMin, localCocRange.x);
			}
			if (x * x + y * y < tileRad.y * tileRad.y)
			{
				cMax = min(cMax, localCocRange.y);
			}
		}
	}
}

/// Entry point for the dilate pass.
///
/// @param tile Coordinate of the tile to run on (SV_DispatchThreadID)
/// @param imageSize Resolution of the depth image (full resolution)
/// @ingroup FfxGPUDof
void FfxDofDilate(FfxUInt32x2 tile, FfxUInt32x2 imageSize)
{
    // dilate scatter-as-gather using global max radius

    // reject out-of-bounds on border
    FfxUInt32x2 size = FfxUInt32x2(ceil(FfxFloat32x2(imageSize) / FfxFloat32(FFX_DOF_DEPTH_TILE_SIZE)));

    // get CoC and depth
    FfxInt32 rMax = FfxInt32(FfxDofGetMaxTileRadius());
    FfxFloat32x2 cocMinMax = FfxDofLoadTileRadius(tile);
    FfxFloat32x2 absCocMinMax = abs(cocMinMax);

    FfxFloat32 cMin = cocMinMax.x;
    FfxFloat32 cMax = cocMinMax.y;

    // Very extremes of the kernel done explicity. Expanding the square (loop below) to this radius would
    // waste time with a lot of failed radius checks.
    if (rMax > 0)
    {
        FfxDofDilateStep(cMin, cMax, -rMax, 0, rMax, size, tile);
        FfxDofDilateStep(cMin, cMax, rMax, 0, rMax, size, tile);
        FfxDofDilateStep(cMin, cMax, 0, -rMax, rMax, size, tile);
        FfxDofDilateStep(cMin, cMax, 0, rMax, rMax, size, tile);
    }
    // Gather the rest as a square shape. Likely faster than trying to make a circle.
    for (FfxInt32 x = -rMax + 1; x < rMax; x++)
    {
        for (FfxInt32 y = -rMax + 1; y < rMax; y++)
        {
            // Zero offset is the starting point, don't need to handle again.
            if (x == 0 && y == 0) continue;
            FfxDofDilateStep(cMin, cMax, x, y, rMax, size, tile);
        }
    }

    // If center tile is in-focus enough: ignore far-field dilation (it is occluded).
    if (absCocMinMax.x < 0.5 && absCocMinMax.y < 0.5)
    {
        cMax = cocMinMax.y;
    }

    // store min and max
    FfxDofStoreDilatedRadius(tile, FfxFloat32x2(cMin, cMax));
}
