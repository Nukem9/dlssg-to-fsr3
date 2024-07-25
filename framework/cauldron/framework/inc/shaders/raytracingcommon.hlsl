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

// RayTracing Tiles
#define TILE_SIZE_X 8
#define TILE_SIZE_Y 4
static const float k_pushOff = 4e-2f;
static const uint2 k_tileSize = uint2(TILE_SIZE_X, TILE_SIZE_Y);

static const float k_pi = 3.1415926535897932384f;
static const float k_2pi = 2.0f * k_pi;
static const float k_pi_over_2 = 0.5f * k_pi;

struct Tile
{
    Tile Create(const uint2 id)
    {
        uint16_t2 f16ID = uint16_t2(uint16_t(id.x), uint16_t(id.y));
        Tile t = { f16ID, 0, k_pushOff, 1e+37 }; // skyHeight
        return t;
    }

    static uint4 ToUint(const Tile t)
    {
        const uint4 ui = uint4(
            (uint(t.location.y) << 16) | uint(t.location.x),
            t.mask,
            asuint(t.minT),
            asuint(t.maxT));
        return ui;
    }

    static Tile FromUint(const uint4 ui)
    {
        uint16_t2 id = uint16_t2(ui.x & 0xffff, (ui.x >> 16) & 0xffff);

        Tile t = { id, ui.y, asfloat(ui.z), asfloat(ui.w) };
        return t;
    }


    uint16_t2 location;

    uint mask;

    float minT;
    float maxT;
};



uint LaneIdToBitShift(uint2 localID)
{
    return localID.y * k_tileSize.x + localID.x;
}

uint BoolToWaveMask(bool b, uint2 localID)
{
    const uint value = b << LaneIdToBitShift(localID);
    return WaveActiveBitOr(value);
}

bool WaveMaskToBool(uint mask, uint2 localID)
{
    return (1u << LaneIdToBitShift(localID)) & mask;
}

uint FFX_BitfieldExtract(uint src, uint off, uint bits) { uint mask = (1u << bits) - 1; return (src >> off) & mask; } // ABfe
uint FFX_BitfieldInsert(uint src, uint ins, uint bits) { uint mask = (1u << bits) - 1; return (ins & mask) | (src & (~mask)); } // ABfiM

 //  543210
 //  ======
 //  ..xxx.
 //  yy...y
//uint2 FXX_Rmp8x8(uint a)
//{
//	return uint2(
//		FFX_BitfieldExtract(a, 1u, 3u),
//		FFX_BitfieldInsert(FFX_BitfieldExtract(a, 3u, 3u), a, 1u)
//		);
//}

 // More complex remap 64x1 to 8x8 which is necessary for 2D wave reductions.
 //  543210
 //  ======
 //  .xx..x
 //  y..yy.
 // Details,
 //  LANE TO 8x8 MAPPING
 //  ===================
 //  00 01 08 09 10 11 18 19
 //  02 03 0a 0b 12 13 1a 1b
 //  04 05 0c 0d 14 15 1c 1d
 //  06 07 0e 0f 16 17 1e 1f
 //  20 21 28 29 30 31 38 39
 //  22 23 2a 2b 32 33 3a 3b
 //  24 25 2c 2d 34 35 3c 3d
 //  26 27 2e 2f 36 37 3e 3f
uint2 FXX_Rmp8x8(uint a)
{
    return uint2(
        FFX_BitfieldInsert(FFX_BitfieldExtract(a, 2u, 3u), a, 1u),
        FFX_BitfieldInsert(FFX_BitfieldExtract(a, 3u, 3u), FFX_BitfieldExtract(a, 1u, 2u), 2u));
}

//--------------------------------------------------------------------------------------
// helper functions
//--------------------------------------------------------------------------------------


float2x3 CreateTangentVectors(float3 normal)
{
    float3 up = abs(normal.z) < 0.99999f ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);

    float2x3 tangents;

    tangents[0] = normalize(cross(up, normal));
    tangents[1] = cross(normal, tangents[0]);

    return tangents;
}

float3 MapToCone(float2 s, float3 n, float radius)
{

    const float2 offset = 2.0f * s - float2(1.0f, 1.0f);

    if (offset.x == 0.0f && offset.y == 0.0f)
    {
        return n;
    }

    float theta, r;

    if (abs(offset.x) > abs(offset.y))
    {
        r = offset.x;
        theta = k_pi / 4.0f * (offset.y / offset.x);
    }
    else
    {
        r = offset.y;
        theta = k_pi_over_2 * (1.0f - 0.5f * (offset.x / offset.y));
    }

    const float2 uv = float2(radius * r * cos(theta), radius * r * sin(theta));

    const float2x3 tangents = CreateTangentVectors(n);

    return n + uv.x * tangents[0] + uv.y * tangents[1];
}
