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

#ifndef FFX_BRIXELIZER_BUILD_COMMON_H
#define FFX_BRIXELIZER_BUILD_COMMON_H

#include "ffx_brixelizer_common_private.h"

void FfxBrixelizerMarkBrickFree(FfxUInt32 brick_id)
{
	StoreBricksVoxelMap(FfxBrixelizerBrickGetIndex(brick_id), FFX_BRIXELIZER_INVALID_ID);
}

FfxUInt32 FfxBrixelizerLoadBrickVoxelID(FfxUInt32 brick_id)
{
	return LoadBricksVoxelMap(FfxBrixelizerBrickGetIndex(brick_id));
}
// FfxUInt32 FfxBrixelizerLoadBrickCascadeID(FfxUInt32 brick_id) { return LoadBricksVoxelMap(FfxBrixelizerBrickGetIndex(brick_id)) >> FFX_BRIXELIZER_CASCADE_ID_SHIFT; }

#endif // FFX_BRIXELIZER_BUILD_COMMON_H