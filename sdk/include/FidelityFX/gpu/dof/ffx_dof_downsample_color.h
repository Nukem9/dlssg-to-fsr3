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

#ifndef FFX_DOF_OPTION_MAX_MIP
#define FFX_DOF_OPTION_MAX_MIP FFX_DOF_INTERNAL_BILAT_MIP_COUNT
#endif

#if FFX_HALF
#define FFX_SPD_PACKED_ONLY

FFX_GROUPSHARED FfxFloat16x2 spdIntermediateRG[16][16];
FFX_GROUPSHARED FfxFloat16x2 spdIntermediateBA[16][16];

FfxFloat16x4 SpdLoadSourceImageH(FfxInt32x2 tex, FfxUInt32 slice)
{
    FfxFloat16 d = FfxFloat16(FfxDofLoadFullDepth(tex));
    FfxFloat16 c = FfxFloat16(FfxDofGetCoc(d));
    return FfxFloat16x4(FfxDofLoadSource(tex).rgb, c);
}

FfxFloat16x4 SpdLoadH(FfxInt32x2 tex, FfxUInt32 slice)
{
    return FfxFloat16x4(0, 0, 0, 0);
}

void SpdStoreH(FfxInt32x2 pix, FfxFloat16x4 value, FfxUInt32 mip, FfxUInt32 slice)
{
    FfxDofStoreBilatMip(mip, pix, value);
}

FfxFloat16x4 SpdLoadIntermediateH(FfxUInt32 x, FfxUInt32 y)
{
    return FfxFloat16x4(
        spdIntermediateRG[x][y].x,
        spdIntermediateRG[x][y].y,
        spdIntermediateBA[x][y].x,
        spdIntermediateBA[x][y].y);
}
void SpdStoreIntermediateH(FfxUInt32 x, FfxUInt32 y, FfxFloat16x4 value)
{
    spdIntermediateRG[x][y] = value.xy;
    spdIntermediateBA[x][y] = value.zw;
}

/// Bilateral downsampling function, half-precision version
/// @ingroup FfxGPUDof
FfxFloat16x4 FfxDofDownsampleQuadH(FfxFloat16x4 v0, FfxFloat16x4 v1, FfxFloat16x4 v2, FfxFloat16x4 v3)
{
	FfxFloat16 c0 = v0.a;
	FfxFloat16 c1 = v1.a;
	FfxFloat16 c2 = v2.a;
	FfxFloat16 c3 = v3.a;
	FfxFloat16 w1 = ffxSaturate(FfxFloat16(1.0) - abs(c0 - c1));
	FfxFloat16 w2 = ffxSaturate(FfxFloat16(1.0) - abs(c0 - c2));
	FfxFloat16 w3 = ffxSaturate(FfxFloat16(1.0) - abs(c0 - c3));
	FfxFloat16x3 color = v0.rgb + w1 * v1.rgb + w2 * v2.rgb + w3 * v3.rgb;
	FfxFloat16 weights = FfxFloat16(1.0) + w1 + w2 + w3;
	return FfxFloat16x4(color / weights, c0);
}

FfxFloat16x4 SpdReduce4H(FfxFloat16x4 v0, FfxFloat16x4 v1, FfxFloat16x4 v2, FfxFloat16x4 v3)
{
    return FfxDofDownsampleQuadH(v0, v1, v2, v3);
}
#else // #if FFX_HALF
#define FFX_SPD_NO_WAVE_OPERATIONS 1
FFX_GROUPSHARED FfxFloat32 spdIntermediateR[16][16];
FFX_GROUPSHARED FfxFloat32 spdIntermediateG[16][16];
FFX_GROUPSHARED FfxFloat32 spdIntermediateB[16][16];
FFX_GROUPSHARED FfxFloat32 spdIntermediateA[16][16];

FfxFloat32x4 SpdLoadSourceImage(FfxInt32x2 tex, FfxUInt32 slice)
{
    FfxFloat32 d = FfxDofLoadFullDepth(tex);
    FfxFloat32 c = FfxDofGetCoc(d);
    return FfxFloat32x4(FfxDofLoadSource(tex).rgb, c);
}

FfxFloat32x4 SpdLoad(FfxInt32x2 tex, FfxUInt32 slice)
{
    return FfxFloat32x4(0, 0, 0, 0);
}

void SpdStore(FfxInt32x2 pix, FfxFloat32x4 value, FfxUInt32 mip, FfxUInt32 slice)
{
    FfxDofStoreBilatMip(mip, pix, value);
}

FfxFloat32x4 SpdLoadIntermediate(FfxUInt32 x, FfxUInt32 y)
{
    return FfxFloat32x4(
        spdIntermediateR[x][y],
        spdIntermediateG[x][y],
        spdIntermediateB[x][y],
        spdIntermediateA[x][y]);
}
void SpdStoreIntermediate(FfxUInt32 x, FfxUInt32 y, FfxFloat32x4 value)
{
    spdIntermediateR[x][y] = value.r;
    spdIntermediateG[x][y] = value.g;
    spdIntermediateB[x][y] = value.b;
    spdIntermediateA[x][y] = value.a;
}

/// Bilateral downsampling function, full-precision version
/// @ingroup FfxGPUDof
FfxFloat32x4 FfxDofDownsampleQuad(FfxFloat32x4 v0, FfxFloat32x4 v1, FfxFloat32x4 v2, FfxFloat32x4 v3)
{
	FfxFloat32 c0 = v0.a;
	FfxFloat32 c1 = v1.a;
	FfxFloat32 c2 = v2.a;
	FfxFloat32 c3 = v3.a;
	FfxFloat32 w1 = ffxSaturate(1.0 - abs(c0 - c1));
	FfxFloat32 w2 = ffxSaturate(1.0 - abs(c0 - c2));
	FfxFloat32 w3 = ffxSaturate(1.0 - abs(c0 - c3));
	FfxFloat32x3 color = v0.rgb + w1 * v1.rgb + w2 * v2.rgb + w3 * v3.rgb;
	FfxFloat32 weights = 1.0 + w1 + w2 + w3;
	return FfxFloat32x4(color / weights, c0);
}

FfxFloat32x4 SpdReduce4(FfxFloat32x4 v0, FfxFloat32x4 v1, FfxFloat32x4 v2, FfxFloat32x4 v3)
{
    return FfxDofDownsampleQuad(v0, v1, v2, v3);
}
#endif // #if FFX_HALF #else

// we only generate 4 mips, so these functions are not needed and never called.
void SpdIncreaseAtomicCounter(FfxUInt32 slice)
{
    /*nothing*/
}
FfxUInt32 SpdGetAtomicCounter()
{
    return 0;
}
void SpdResetAtomicCounter(FfxUInt32 slice)
{
    /*nothing*/
}

#include "spd/ffx_spd.h"

/// Entry point for the downsample color pass. Uses SPD internally.
///
/// @param LocalThreadId Thread index in thread group (SV_GroupIndex)
/// @param WorkGroupId Coordinate of the tile (SV_GroupID.xy)
/// @ingroup FfxGPUDof
void DownsampleColor(FfxUInt32 LocalThreadId, FfxUInt32x2 WorkGroupId)
{
#if FFX_HALF
    SpdDownsampleH(WorkGroupId, LocalThreadId, FfxUInt32(FFX_DOF_OPTION_MAX_MIP), FfxUInt32(0), FfxUInt32(0));
#else
    SpdDownsample(WorkGroupId, LocalThreadId, FfxUInt32(FFX_DOF_OPTION_MAX_MIP), FfxUInt32(0), FfxUInt32(0));
#endif
}
