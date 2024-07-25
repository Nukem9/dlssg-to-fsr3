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

#ifndef FFX_BRIXELIZER_BRICK_COMMON_H
#define FFX_BRIXELIZER_BRICK_COMMON_H

#include "ffx_brixelizer_common.h"

FfxUInt32 FfxBrixelizerBrickGetIndex(FfxUInt32 brick_id)
{
	return brick_id & FFX_BRIXELIZER_BRICK_ID_MASK;
}

FfxUInt32x3 FfxBrixelizerWrapCoords(FfxInt32x3 clipmap_offset, FfxUInt32 wrap_mask, FfxUInt32x3 voxel_coord)
{
    return (voxel_coord + FfxUInt32x3(clipmap_offset)) & FfxUInt32x3(wrap_mask, wrap_mask, wrap_mask);
}

FfxUInt32x3 FfxBrixelizerWrapCoords(FfxBrixelizerCascadeInfo cinfo, FfxUInt32x3 voxel_coord)
{
    return (voxel_coord + cinfo.clipmap_offset) & FfxUInt32x3(FFX_BRIXELIZER_CASCADE_WRAP_MASK, FFX_BRIXELIZER_CASCADE_WRAP_MASK, FFX_BRIXELIZER_CASCADE_WRAP_MASK);
}

FfxUInt32 FfxBrixelizerLoadBrickIDUniform(FfxUInt32 voxel_flat_id, FfxUInt32 cascade_id)
{
    return LoadCascadeBrickMapArrayUniform(cascade_id, voxel_flat_id);
}

FfxUInt32x3 FfxBrixelizerGetSDFAtlasOffset(FfxUInt32 brick_id)
{
    FfxUInt32 dim     = 8;
    FfxUInt32 offset  = FfxBrixelizerBrickGetIndex(brick_id);
    FfxUInt32 bperdim = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE / dim;
    FfxUInt32 xoffset = offset % bperdim;
    FfxUInt32 yoffset = (offset / bperdim) % bperdim;
    FfxUInt32 zoffset = (offset / bperdim / bperdim);
    return FfxUInt32x3(xoffset, yoffset, zoffset) * dim;
}

struct FfxBxAtlasBounds 
{
    FfxUInt32    brick_dim;
    FfxFloat32x3 uvw_min;
    FfxFloat32x3 uvw_max;
};

FfxBxAtlasBounds FfxBrixelizerGetAtlasBounds(FfxUInt32 brick_id) 
{
    FfxUInt32x3      brick_offset = FfxBrixelizerGetSDFAtlasOffset(brick_id);
    FfxBxAtlasBounds bounds;
    bounds.brick_dim = FfxUInt32(8);
    bounds.uvw_min   = (FfxFloat32x3(brick_offset) + FFX_BROADCAST_FLOAT32X3(0.5)) / FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
    bounds.uvw_max   = (FfxFloat32x3(brick_offset) + FFX_BROADCAST_FLOAT32X3(float(8.0 - 0.5))) / FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
    return bounds;
}

FfxFloat32 FfxBrixelizerSampleBrixelDistance(FfxUInt32 brick_id, FfxFloat32x3 uvw)
{
    FfxUInt32  brick_dim      = FfxUInt32(8);
    FfxFloat32 idim           = FfxFloat32(1.0) / FfxFloat32(brick_dim);
    FfxUInt32  texture_offset = FfxBrixelizerBrickGetIndex(brick_id);

    // Offset for 7x7x7 region

    FfxUInt32x3  brick_offset = FfxBrixelizerGetSDFAtlasOffset(brick_id);
    FfxFloat32x3 uvw_min      = (FfxFloat32x3(brick_offset) + FFX_BROADCAST_FLOAT32X3(0.5)) / FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
    FfxFloat32x3 uvw_max      = (FfxFloat32x3(brick_offset) + FFX_BROADCAST_FLOAT32X3(FfxFloat32(brick_dim - FfxFloat32(0.5)))) / FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
    uvw                       = ffxLerp(uvw_min, uvw_max, uvw);

    return SampleSDFAtlas(uvw);
}

FfxFloat32 FfxBrixelizerSampleBrixelDistance(FfxFloat32x3 uvw_min, FfxFloat32x3 uvw_max, FfxFloat32x3 uvw)
{
    uvw = ffxLerp(uvw_min, uvw_max, uvw);
    return SampleSDFAtlas(uvw);
}

FfxFloat32x3 FfxBrixelizerGetBrixelGrad(FfxFloat32x3 uvw_min, FfxFloat32x3 uvw_max, FfxFloat32x3 uvw) {
    FfxFloat32 EPS = FfxFloat32(0.25) / (FfxFloat32(8.0) - FfxFloat32(1.0));

    FfxFloat32x4 k = FfxFloat32x4(1.0, 1.0, 1.0, 0.0);
    if (uvw.x > FfxFloat32(0.5)) k.x = -k.x;
    if (uvw.y > FfxFloat32(0.5)) k.y = -k.y;
    if (uvw.z > FfxFloat32(0.5)) k.z = -k.z;
    FfxFloat32   fcenter = FfxBrixelizerSampleBrixelDistance(uvw_min, uvw_max, uvw);
    FfxFloat32x3 grad    = normalize(
        k.xww * (FfxBrixelizerSampleBrixelDistance(uvw_min, uvw_max, uvw + k.xww * EPS) - fcenter) +
        k.wyw * (FfxBrixelizerSampleBrixelDistance(uvw_min, uvw_max, uvw + k.wyw * EPS) - fcenter) +
        k.wwz * (FfxBrixelizerSampleBrixelDistance(uvw_min, uvw_max, uvw + k.wwz * EPS) - fcenter)
    );
    if (any(isnan(grad))) {
        return normalize(uvw - FFX_BROADCAST_FLOAT32X3(0.5));
    }
    return grad;
}

#endif // FFX_BRIXELIZER_BRICK_COMMON_H