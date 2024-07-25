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

#ifndef FFX_BRIXELIZER_COMMON_H
#define FFX_BRIXELIZER_COMMON_H

#include "../ffx_core.h"
#include "ffx_brixelizer_host_gpu_shared.h"

#define ffxassert(x)

FfxBoolean FfxBrixelizerIsValidID(FfxUInt32 id)
{
    return (id & FFX_BRIXELIZER_INVALID_ID) != FFX_BRIXELIZER_INVALID_ID;
}

FfxBoolean FfxBrixelizerIntersectAABB(FfxFloat32x3 ray_origin, FfxFloat32x3 ray_invdir, FfxFloat32x3 box_min, FfxFloat32x3 box_max, FFX_PARAMETER_OUT FfxFloat32 hit_min, FFX_PARAMETER_OUT FfxFloat32 hit_max)
{
    FfxFloat32x3 tbot = ray_invdir * (box_min - ray_origin);
    FfxFloat32x3 ttop = ray_invdir * (box_max - ray_origin);
    FfxFloat32x3 tmin = min(ttop, tbot);
    FfxFloat32x3 tmax = max(ttop, tbot);
    FfxFloat32x2 t    = max(tmin.xx, tmin.yz);
    FfxFloat32  t0    = max(t.x, t.y);
    t                 = min(tmax.xx, tmax.yz);
    FfxFloat32 t1     = min(t.x, t.y);
    hit_min           = max(t0, FfxFloat32(0.0));
    hit_max           = max(t1, FfxFloat32(0.0));
    return hit_max > hit_min;
}

FfxUInt32  FfxBrixelizerFlattenPOT(FfxUInt32x3 voxel_coord, FfxUInt32 degree)
{
	return voxel_coord.x | (voxel_coord.y << degree) | (voxel_coord.z << (2 * degree));
}

FfxUInt32x3 FfxBrixelizerUnflattenPOT(FfxUInt32 flat_bx_coord, FfxUInt32 degree)
{
    return FfxUInt32x3(flat_bx_coord & ((FfxUInt32(1) << degree) - 1), (flat_bx_coord >> degree) & ((FfxUInt32(1) << degree) - 1), flat_bx_coord >> (2 * degree));
}

FfxUInt32 FfxBrixelizerPackUnsigned8Bits(FfxFloat32 a)
{
	return FfxUInt32(ffxSaturate(a) * FfxFloat32(255.0)) & 0xffu;
}

FfxFloat32 FfxBrixelizerUnpackUnsigned8Bits(FfxUInt32 uval)
{
    return FfxFloat32(uval & 0xff) / FfxFloat32(255.0);
}

FfxUInt32 PackUVWC(FfxFloat32x4 uvwc)
{
    FfxUInt32 pack =                                     //
        (FfxBrixelizerPackUnsigned8Bits(uvwc.x) << 0)     //
        | (FfxBrixelizerPackUnsigned8Bits(uvwc.y) << 8)   //
        | (FfxBrixelizerPackUnsigned8Bits(uvwc.z) << 16)  //
        | (FfxBrixelizerPackUnsigned8Bits(uvwc.w) << 24); //
    return pack;
}

FfxFloat32x2 FfxBrixelizerOctahedronToUV(FfxFloat32x3 N)
{
    N.xy = FfxFloat32x2(N.xy) / dot(FFX_BROADCAST_FLOAT32X3(1.0), abs(N));
    FfxFloat32x2 k;
    k.x = N.x >= float(0.0) ? float(1.0) : -float(1.0);
    k.y = N.y >= float(0.0) ? float(1.0) : -float(1.0);
    if (N.z <= float(0.0)) N.xy = (float(1.0) - abs(FfxFloat32x2(N.yx))) * k;
    return N.xy * float(0.5) + float(0.5);
}

FfxFloat32x3 FfxBrixelizerUVToOctahedron(FfxFloat32x2 UV)
{
    UV       = float(2.0) * (UV - float(0.5));
    FfxFloat32x3 N = FfxFloat32x3(UV, float(1.0) - abs(UV.x) - abs(UV.y));
    float  t = max(-N.z, float(0.0));
    FfxFloat32x2 k;
    k.x = N.x >= float(0.0) ? -t : t;
    k.y = N.y >= float(0.0) ? -t : t;
    N.xy += k;
    return normalize(N);
}

#endif // FFX_BRIXELIZER_COMMON_H