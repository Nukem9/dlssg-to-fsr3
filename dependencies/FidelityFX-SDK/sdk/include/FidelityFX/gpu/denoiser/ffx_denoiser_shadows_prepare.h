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

#ifndef FFX_DNSR_SHADOWS_PREPARESHADOWMASK_HLSL
#define FFX_DNSR_SHADOWS_PREPARESHADOWMASK_HLSL

#include "ffx_denoiser_shadows_util.h"

void FFX_DNSR_Shadows_CopyResult(FfxUInt32x2 gtid, FfxUInt32x2 gid)
{
    const FfxUInt32x2 did = gid * FfxUInt32x2(8, 4) + gtid;
    const FfxUInt32 linear_tile_index = FFX_DNSR_Shadows_LinearTileIndex(gid, BufferDimensions().x);
    const FfxBoolean hit_light = HitsLight(did, gtid, gid);
    const FfxUInt32 lane_mask = hit_light ? FFX_DNSR_Shadows_GetBitMaskFromPixelPosition(did) : 0;
    StoreShadowMask(linear_tile_index, ffxWaveOr(lane_mask));
}

void FFX_DNSR_Shadows_PrepareShadowMask(FfxUInt32x2 gtid, FfxUInt32x2 gid)
{
    gid *= 4;
    FfxUInt32x2 tile_dimensions = (BufferDimensions() + FfxUInt32x2(7, 3)) / FfxUInt32x2(8, 4);

    for (FfxInt32 i = 0; i < 4; ++i)
    {
        for (FfxInt32 j = 0; j < 4; ++j)
        {
            FfxUInt32x2 tile_id = FfxUInt32x2(gid.x + i, gid.y + j);
            tile_id = clamp(tile_id, FfxUInt32x2(0, 0), tile_dimensions - FfxUInt32x2(1,1));
            FFX_DNSR_Shadows_CopyResult(gtid, tile_id);
        }
    }
}

#endif
