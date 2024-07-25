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

#define TILE_SIZE_X 8
#define TILE_SIZE_Y 4
#define k_pushOff 4e-2f
#define k_tileSize FfxUInt32x2(TILE_SIZE_X, TILE_SIZE_Y)

//--------------------------------------------------------------------------------------
// I/O Structures
//--------------------------------------------------------------------------------------

struct Tile
{
#if FFX_HALF
    FfxUInt16x2 location;
#else
    FfxUInt32x2 location; // Dead code, f32 not supported
#endif
    FfxUInt32 mask;

    FfxFloat32 minT;
    FfxFloat32 maxT;
};

Tile TileCreate(const FfxUInt32x2 id)
{
#if FFX_HALF
    FfxUInt16x2 f16ID = FfxUInt16x2(FfxUInt16(id.x), FfxUInt16(id.y));
    Tile t = { f16ID, 0, k_pushOff, FFX_POSITIVE_INFINITY_FLOAT };  // skyHeight
#else
    Tile t = { id, 0, k_pushOff, FFX_POSITIVE_INFINITY_FLOAT }; //skyHeight
#endif
    return t;
}

FfxUInt32x4 TileToUint(const Tile t)
{
    const FfxUInt32x4 ui = FfxUInt32x4((FfxUInt32(t.location.y) << 16) | FfxUInt32(t.location.x), t.mask, ffxAsUInt32(t.minT), ffxAsUInt32(t.maxT));
    return ui;
}

Tile TileFromUint(const FfxUInt32x4 ui)
{
#if FFX_HALF
    FfxUInt16x2 id = FfxUInt16x2(ui.x & 0xffff, (ui.x >> 16) & 0xffff);
#else
    FfxUInt32x2 id;
#endif
    Tile t = { id, ui.y, ffxAsFloat(ui.z), ffxAsFloat(ui.w) };
    return t;
}

//--------------------------------------------------------------------------------------
// helper functions
//--------------------------------------------------------------------------------------

FfxUInt32 LaneIdToBitShift(FfxUInt32x2 localID)
{
    return localID.y * TILE_SIZE_X + localID.x;
}

FfxUInt32 BoolToWaveMask(FfxBoolean b, FfxUInt32x2 localID)
{
    const FfxUInt32 value = FfxUInt32(b) << LaneIdToBitShift(localID);
    return ffxWaveOr(value);
}

FfxBoolean WaveMaskToBool(FfxUInt32 mask, FfxUInt32x2 localID)
{
    return FfxBoolean((FfxUInt32(1) << LaneIdToBitShift(localID)) & mask);
}


#define k_pi 3.1415926535897932384f
#define k_2pi 2.0f * k_pi
#define k_pi_over_2 0.5f * k_pi

// made using a modified version of https://www.asawicki.info/news_952_poisson_disc_generator
#define k_poissonDiscSampleCountLow   8
#define k_poissonDiscSampleCountMid   16
#define k_poissonDiscSampleCountHigh  24
#define k_poissonDiscSampleCountUltra 32
