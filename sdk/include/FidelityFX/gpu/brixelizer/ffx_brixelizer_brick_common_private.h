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

#ifndef FFX_BRIXELIZER_BRICK_COMMON_PRIVATE_H
#define FFX_BRIXELIZER_BRICK_COMMON_PRIVATE_H

#include "ffx_brixelizer_common_private.h"
#include "ffx_brixelizer_brick_common.h"

FfxUInt32 FfxBrixelizerVoxelGetIndex(FfxUInt32 voxel_id)
{
    return voxel_id & FFX_BRIXELIZER_VOXEL_ID_MASK;
}

FfxUInt32 FfxBrixelizerGetVoxelCascade(FfxUInt32 voxel_id)
{
    return voxel_id >> FFX_BRIXELIZER_CASCADE_ID_SHIFT;
}

FfxUInt32 WrapFlatCoords(FfxBrixelizerCascadeInfo CINFO, FfxUInt32 voxel_idx)
{
    return FfxBrixelizerFlattenPOT((FfxBrixelizerUnflattenPOT(voxel_idx, FFX_BRIXELIZER_CASCADE_DEGREE) + CINFO.clipmap_offset) & FFX_BROADCAST_UINT32X3(FFX_BRIXELIZER_CASCADE_WRAP_MASK), FFX_BRIXELIZER_CASCADE_DEGREE);
}

FfxUInt32 FfxBrixelizerMakeBrickID(FfxUInt32 offset)
{
    return offset;
}

FfxFloat32x3 FfxBrixelizerGetBrixelGrad(FfxUInt32 brick_id, FfxFloat32x3 uvw)
{
    FfxFloat32  EPS          = FfxFloat32(0.1) / (FfxFloat32(8.0) - FfxFloat32(1.0));
    FfxUInt32x3 brick_offset = FfxBrixelizerGetSDFAtlasOffset(brick_id);
    FfxFloat32x3 uvw_min      = (FfxFloat32x3(brick_offset) + FFX_BROADCAST_FLOAT32X3(0.5)) / FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
    FfxFloat32x3 uvw_max      = (FfxFloat32x3(brick_offset) + FFX_BROADCAST_FLOAT32X3(float(8.0 - float(0.5)))) / FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
    return FfxBrixelizerGetBrixelGrad(uvw_min, uvw_max, uvw);
}

#endif // FFX_BRIXELIZER_BRICK_COMMON_PRIVATE_H
