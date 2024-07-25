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

#ifndef FFX_BRIXELIZER_GI_COMMON_H
#define FFX_BRIXELIZER_GI_COMMON_H

#include "../ffx_core.h"

struct FfxRay 
{
    FfxBoolean   valid;
    FfxFloat32   roughness;
    FfxFloat32x3 camera_pos;
    FfxFloat32x3 normal;
    FfxFloat32x3 origin;
    FfxFloat32x3 direction;
    FfxFloat32x3 major_direction;
};

FfxBoolean ffxIsBackground(FfxFloat32 depth) 
{
#if FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED == 1
    return depth < 1.e-12f;
#    else  // !FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED
    return depth >= (1.0f - 1.e-12f);
#    endif // !FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED
}

FfxFloat32x3 ffxViewSpaceToWorldSpace(FfxFloat32x4 view_space_coord)
{
    return FFX_MATRIX_MULTIPLY(GetInverseViewMatrix(), view_space_coord).xyz;
}

FfxFloat32x3 ffxPreviousViewSpaceToWorldSpace(FfxFloat32x4 view_space_coord)
{
    return FFX_MATRIX_MULTIPLY(GetPreviousInverseViewMatrix(), view_space_coord).xyz;
}

// Transforms origin to uv space
// Mat must be able to transform origin from its current space into clip space.
FfxFloat32x3 ffxProjectPosition(FfxFloat32x3 origin, FfxFloat32x4x4 mat)
{
    FfxFloat32x4 projected = FFX_MATRIX_MULTIPLY(mat, FfxFloat32x4(origin, FfxFloat32(1.0)));
    projected.xyz /= projected.w;
    projected.xy = FfxFloat32(0.5) * projected.xy + FfxFloat32(0.5);
    projected.y  = (FfxFloat32(1.0) - projected.y);
    return projected.xyz;
}

FfxFloat32x3 ffxInvProjectPosition(FfxFloat32x3 coord, FfxFloat32x4x4 mat)
{
    coord.y          = (FfxFloat32(1.0) - coord.y);
    coord.xy         = FfxFloat32(2.0) * coord.xy - FfxFloat32(1.0);
    FfxFloat32x4 projected = FFX_MATRIX_MULTIPLY(mat, FfxFloat32x4(coord, FfxFloat32(1.0)));
    projected.xyz /= projected.w;
    return projected.xyz;
}

FfxFloat32 ffxGetLinearDepth(FfxFloat32x2 uv, FfxFloat32 depth)
{
    const FfxFloat32x3 view_space_pos = ffxInvProjectPosition(FfxFloat32x3(uv, depth), GetInverseProjectionMatrix());
    return abs(view_space_pos.z);
}

FfxFloat32x3 ffxScreenSpaceToViewSpace(FfxFloat32x3 screen_uv_coord)
{
    return ffxInvProjectPosition(screen_uv_coord, GetInverseProjectionMatrix());
}

FfxFloat32x3 ffxPreviousScreenSpaceToViewSpace(FfxFloat32x3 screen_uv_coord)
{
    return ffxInvProjectPosition(screen_uv_coord, GetPreviousInverseProjectionMatrix());
}

FfxFloat32x3 ffxGetWorldPosition(FfxFloat32x2 uv, FfxFloat32 depth)
{
    FfxFloat32x3 screen_uv_space_ray_origin = FfxFloat32x3(uv, depth);
    FfxFloat32x3 view_space_position        = ffxScreenSpaceToViewSpace(screen_uv_space_ray_origin);
    FfxFloat32x3 world_space_origin         = ffxViewSpaceToWorldSpace(FfxFloat32x4(view_space_position, 1.0)).xyz;
    return world_space_origin;
}

FfxFloat32x3 ffxGetPreviousWorldPosition(FfxFloat32x2 uv, FfxFloat32 depth)
{
    FfxFloat32x3 screen_uv_space_ray_origin = FfxFloat32x3(uv, depth);
    FfxFloat32x3 view_space_position        = ffxPreviousScreenSpaceToViewSpace(screen_uv_space_ray_origin);
    FfxFloat32x3 world_space_origin         = ffxPreviousViewSpaceToWorldSpace(FfxFloat32x4(view_space_position, 1.0)).xyz;
    return world_space_origin;
}

#if FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED == 1
FFX_STATIC const FfxFloat32 FfxBrixelizerGIFrontendConstants_BackgroundDepth = FfxFloat32(0.0);
#else  // !FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED
FFX_STATIC const FfxFloat32 FfxBrixelizerGIFrontendConstants_BackgroundDepth   = FfxFloat32(1.0);
#endif // !FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED

FfxBoolean FfxBrixelizerGIIsDepthACloserThanB(FfxFloat32 a, FfxFloat32 b)
{
#if FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED == 1
    return a > b;
#else  // !FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED
    return a < b;
#endif // !FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED
}

FfxFloat32 FfxBrixelizerGIDepthCloserOp(FfxFloat32 a, FfxFloat32 b)
{
#ifdef FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED
    return max(a, b);
#else  // !FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED
    return min(a, b);
#endif // !FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED
}

FfxFloat32 FfxBrixelizerGIDepthFarthestOp(FfxFloat32 a, FfxFloat32 b)
{
#if FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED == 1
    return min(a, b);
#else  // !FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED
    return max(a, b);
#endif // !FFX_BRIXELIZER_GI_OPTION_DEPTH_INVERTED
}

#endif // FFX_BRIXELIZER_GI_COMMON_H