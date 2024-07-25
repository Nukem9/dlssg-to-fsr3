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

#ifndef FFX_DNSR_SHADOWS_UTILS_HLSL
#define FFX_DNSR_SHADOWS_UTILS_HLSL

FfxUInt32 FFX_DNSR_Shadows_RoundedDivide(FfxUInt32 value, FfxUInt32 divisor)
{
    return (value + divisor - 1) / divisor;
}

FfxUInt32x2 FFX_DNSR_Shadows_GetTileIndexFromPixelPosition(FfxUInt32x2 pixel_pos)
{
    return FfxUInt32x2(pixel_pos.x / 8, pixel_pos.y / 4);
}

FfxUInt32 FFX_DNSR_Shadows_LinearTileIndex(FfxUInt32x2 tile_index, FfxUInt32 screen_width)
{
    return tile_index.y * FFX_DNSR_Shadows_RoundedDivide(screen_width, 8) + tile_index.x;
}

FfxUInt32 FFX_DNSR_Shadows_GetBitMaskFromPixelPosition(FfxUInt32x2 pixel_pos)
{
    FfxUInt32 lane_index = (pixel_pos.y % 4) * 8 + (pixel_pos.x % 8);
    return (1u << lane_index);
}

#define TILE_META_DATA_CLEAR_MASK 1
#define TILE_META_DATA_LIGHT_MASK 2

#endif
