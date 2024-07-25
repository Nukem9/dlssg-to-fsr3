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

#ifndef FFX_DNSR_REFLECTIONS_COMMON
#define FFX_DNSR_REFLECTIONS_COMMON

#include "ffx_denoiser_reflections_config.h"

FfxBoolean FFX_DNSR_Reflections_IsGlossyReflection(FfxFloat32 roughness) {
    return roughness < RoughnessThreshold();
}

FfxBoolean FFX_DNSR_Reflections_IsMirrorReflection(FfxFloat32 roughness) {
    return roughness < 0.0001;
}

// Transforms origin to uv space
// Mat must be able to transform origin from its current space into clip space.
FfxFloat32x3 ProjectPosition(FfxFloat32x3 origin, FfxFloat32Mat4 mat) {
    FfxFloat32x4 projected = FFX_MATRIX_MULTIPLY(mat, FfxFloat32x4(origin, 1));
    projected.xyz /= projected.w;
    projected.xy = 0.5 * projected.xy + 0.5;
    projected.y = (1 - projected.y);
    return projected.xyz;
}

// Mat must be able to transform origin from texture space to a linear space.
FfxFloat32x3 FFX_DNSR_InvProjectPosition(FfxFloat32x3 coord, FfxFloat32Mat4 mat) {
    coord.y = (1 - coord.y);
    coord.xy = 2 * coord.xy - 1;
    FfxFloat32x4 projected = FFX_MATRIX_MULTIPLY(mat, FfxFloat32x4(coord, 1));
    projected.xyz /= projected.w;
    return projected.xyz;
}

FfxFloat32 FFX_DNSR_Reflections_GetLinearDepth(FfxFloat32x2 uv, FfxFloat32 depth) {
    const FfxFloat32x3 view_space_pos = FFX_DNSR_InvProjectPosition(FfxFloat32x3(uv, depth), InvProjection());
    return abs(view_space_pos.z);
}

FfxFloat32x3 FFX_DNSR_Reflections_ScreenSpaceToViewSpace(FfxFloat32x3 screen_uv_coord) {
    return FFX_DNSR_InvProjectPosition(screen_uv_coord, InvProjection());
}

FfxFloat32x3 FFX_DNSR_Reflections_WorldSpaceToScreenSpacePrevious(FfxFloat32x3 world_space_pos) {
    return ProjectPosition(world_space_pos, PrevViewProjection());
}

FfxFloat32x3 FFX_DNSR_Reflections_ViewSpaceToWorldSpace(FfxFloat32x4 view_space_coord) {
    return FFX_MATRIX_MULTIPLY(InvView(), view_space_coord).xyz;
}

// Rounds value to the nearest multiple of 8
FfxUInt32x2 FFX_DNSR_Reflections_RoundUp8(FfxUInt32x2 value) {
    FfxUInt32x2 round_down = value & ~7;    // 0b111;
    return FFX_SELECT((round_down == value), value, value + 8);
}

#if FFX_HALF

FfxFloat16 FFX_DNSR_Reflections_Luminance(FfxFloat16x3 color)
{ 
    return max(FfxFloat16(dot(color, FfxFloat16x3(0.299f, 0.587f, 0.114f))), FfxFloat16(0.001));
}

FfxFloat16 FFX_DNSR_Reflections_ComputeTemporalVariance(FfxFloat16x3 history_radiance, FfxFloat16x3 radiance) {
    FfxFloat16 history_luminance = FFX_DNSR_Reflections_Luminance(history_radiance);
    FfxFloat16 luminance         = FFX_DNSR_Reflections_Luminance(radiance);
    FfxFloat16 diff              = abs(history_luminance - luminance) / max(max(history_luminance, luminance), FfxFloat16(0.5f));
    return diff * diff;
}

FfxUInt32 FFX_DNSR_Reflections_PackFloat16(FfxFloat16x2 v)
{
#if defined(FFX_GLSL)
    return ffxPackHalf2x16(FfxFloat32x2(v));
#elif defined(FFX_HLSL)
    FfxUInt32x2 p = ffxF32ToF16(FfxFloat32x2(v));
    return p.x | (p.y << 16);
#endif

    return 0;
}

FfxFloat16x2 FFX_DNSR_Reflections_UnpackFloat16(FfxUInt32 a)
{
#if defined(FFX_GLSL)
    return FfxFloat16x2(unpackHalf2x16(a));
#elif defined(FFX_HLSL)
    FfxFloat32x2 tmp = f16tof32(FfxUInt32x2(a & 0xFFFF, a >> 16));
    return FfxFloat16x2(tmp);
#endif

    return FfxFloat16x2(0.0f, 0.0f);
}

FfxUInt32x2 FFX_DNSR_Reflections_PackFloat16_4(FfxFloat16x4 v) { return FfxUInt32x2(FFX_DNSR_Reflections_PackFloat16(v.xy), FFX_DNSR_Reflections_PackFloat16(v.zw)); }

FfxFloat16x4 FFX_DNSR_Reflections_UnpackFloat16_4(FfxUInt32x2 a) { return FfxFloat16x4(FFX_DNSR_Reflections_UnpackFloat16(a.x), FFX_DNSR_Reflections_UnpackFloat16(a.y)); }

// From "Temporal Reprojection Anti-Aliasing"
// https://github.com/playdeadgames/temporal
/**********************************************************************
Copyright (c) [2015] [Playdead]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
********************************************************************/
FfxFloat16x3 FFX_DNSR_Reflections_ClipAABB(FfxFloat16x3 aabb_min, FfxFloat16x3 aabb_max, FfxFloat16x3 prev_sample) {
    // Main idea behind clipping - it prevents clustering when neighbor color space
    // is distant from history sample

    // Here we find intersection between color vector and aabb color box

    // Note: only clips towards aabb center
    FfxFloat32x3 aabb_center = 0.5 * (aabb_max + aabb_min);
    FfxFloat32x3 extent_clip = 0.5 * (aabb_max - aabb_min) + 0.001;

    // Find color vector
    FfxFloat32x3 color_vector = prev_sample - aabb_center;
    // Transform into clip space
    FfxFloat32x3 color_vector_clip = color_vector / extent_clip;
    // Find max absolute component
    color_vector_clip       = abs(color_vector_clip);
    FfxFloat16 max_abs_unit = FfxFloat16(max(max(color_vector_clip.x, color_vector_clip.y), color_vector_clip.z));

    if (max_abs_unit > 1.0) {
        return FfxFloat16x3(aabb_center + color_vector / max_abs_unit); // clip towards color vector
    } else {
        return FfxFloat16x3(prev_sample); // point is inside aabb
    }
}

#ifdef FFX_DNSR_REFLECTIONS_ESTIMATES_LOCAL_NEIGHBORHOOD

#    ifndef FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS
#        define FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS 4
#    endif

FfxFloat16 FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(FfxFloat16 i) {
    const FfxFloat16 radius = FfxFloat16(FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS + 1.0f);
    return FfxFloat16(exp(-FFX_DNSR_REFLECTIONS_GAUSSIAN_K * (i * i) / (radius * radius)));
}

#endif // FFX_DNSR_REFLECTIONS_ESTIMATES_LOCAL_NEIGHBORHOOD

#else // FFX_HALF

FfxFloat32 FFX_DNSR_Reflections_Luminance(FfxFloat32x3 color)
{ 
    return max(FfxFloat32(dot(color, FfxFloat32x3(0.299f, 0.587f, 0.114f))), FfxFloat32(0.001));
}

FfxFloat32 FFX_DNSR_Reflections_ComputeTemporalVariance(FfxFloat32x3 history_radiance, FfxFloat32x3 radiance) {
    FfxFloat32 history_luminance = FFX_DNSR_Reflections_Luminance(history_radiance);
    FfxFloat32 luminance         = FFX_DNSR_Reflections_Luminance(radiance);
    FfxFloat32 diff              = abs(history_luminance - luminance) / max(max(history_luminance, luminance), FfxFloat32(0.5f));
    return diff * diff;
}

FfxUInt32 FFX_DNSR_Reflections_PackFloat16(FfxFloat32x2 v)
{
#if defined(FFX_GLSL)
    return ffxPackHalf2x16(v);
#elif defined(FFX_HLSL)
    FfxUInt32x2 p = ffxF32ToF16(v);
    return p.x | (p.y << 16);
#endif

    return 0;
}

FfxFloat32x2 FFX_DNSR_Reflections_UnpackFloat16(FfxUInt32 a)
{
#if defined(FFX_GLSL)
    return unpackHalf2x16(a);
#elif defined(FFX_HLSL)
    FfxFloat32x2 tmp = f16tof32(FfxUInt32x2(a & 0xFFFF, a >> 16));
    return tmp;
#endif

    return FfxFloat32x2(0.0f, 0.0f);
}

FfxUInt32x2 FFX_DNSR_Reflections_PackFloat16_4(FfxFloat32x4 v) { return FfxUInt32x2(FFX_DNSR_Reflections_PackFloat16(v.xy), FFX_DNSR_Reflections_PackFloat16(v.zw)); }

FfxFloat32x4 FFX_DNSR_Reflections_UnpackFloat16_4(FfxUInt32x2 a) { return FfxFloat32x4(FFX_DNSR_Reflections_UnpackFloat16(a.x), FFX_DNSR_Reflections_UnpackFloat16(a.y)); }

// From "Temporal Reprojection Anti-Aliasing"
// https://github.com/playdeadgames/temporal
/**********************************************************************
Copyright (c) [2015] [Playdead]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
********************************************************************/
FfxFloat32x3 FFX_DNSR_Reflections_ClipAABB(FfxFloat32x3 aabb_min, FfxFloat32x3 aabb_max, FfxFloat32x3 prev_sample) {
    // Main idea behind clipping - it prevents clustering when neighbor color space
    // is distant from history sample

    // Here we find intersection between color vector and aabb color box

    // Note: only clips towards aabb center
    FfxFloat32x3 aabb_center = 0.5 * (aabb_max + aabb_min);
    FfxFloat32x3 extent_clip = 0.5 * (aabb_max - aabb_min) + 0.001;

    // Find color vector
    FfxFloat32x3 color_vector = prev_sample - aabb_center;
    // Transform into clip space
    FfxFloat32x3 color_vector_clip = color_vector / extent_clip;
    // Find max absolute component
    color_vector_clip       = abs(color_vector_clip);
    FfxFloat32 max_abs_unit = FfxFloat32(max(max(color_vector_clip.x, color_vector_clip.y), color_vector_clip.z));

    if (max_abs_unit > 1.0) {
        return FfxFloat32x3(aabb_center + color_vector / max_abs_unit); // clip towards color vector
    } else {
        return FfxFloat32x3(prev_sample); // point is inside aabb
    }
}

#ifdef FFX_DNSR_REFLECTIONS_ESTIMATES_LOCAL_NEIGHBORHOOD

#    ifndef FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS
#        define FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS 4
#    endif

FfxFloat32 FFX_DNSR_Reflections_LocalNeighborhoodKernelWeight(FfxFloat32 i) {
    const FfxFloat32 radius = FFX_DNSR_REFLECTIONS_LOCAL_NEIGHBORHOOD_RADIUS + 1.0;
    return exp(-FFX_DNSR_REFLECTIONS_GAUSSIAN_K * (i * i) / (radius * radius));
}

#endif // FFX_DNSR_REFLECTIONS_ESTIMATES_LOCAL_NEIGHBORHOOD

#endif // FFX_HALF

#endif // FFX_DNSR_REFLECTIONS_COMMON
