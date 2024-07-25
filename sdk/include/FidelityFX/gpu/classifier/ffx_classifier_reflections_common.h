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

// Use hitcounter feedback
#define FFX_CLASSIFIER_FLAGS_USE_HIT_COUNTER (1 << 0)
// Traverse in screen space
#define FFX_CLASSIFIER_FLAGS_USE_SCREEN_SPACE (1 << 1)
// Traverse using HW ray tracing
#define FFX_CLASSIFIER_FLAGS_USE_RAY_TRACING (1 << 2)
// Iterate BVH to search for the opaque fragment
#define FFX_CLASSIFIER_FLAGS_RESOLVE_TRANSPARENT (1 << 3)
// Grab radiance from screen space shaded image for ray traced intersections, when possible
#define FFX_CLASSIFIER_FLAGS_SHADING_USE_SCREEN (1 << 5)
// defines FFX_HSR_OPTION_SHADING_USE_SCREEN

// Extra flags for debugging
#define FFX_CLASSIFIER_FLAGS_FLAG_0 (1 << 9)
#define FFX_CLASSIFIER_FLAGS_FLAG_1 (1 << 10)
#define FFX_CLASSIFIER_FLAGS_FLAG_2 (1 << 11)
#define FFX_CLASSIFIER_FLAGS_FLAG_3 (1 << 12)

// Visualization tweaking
#define FFX_CLASSIFIER_FLAGS_SHOW_DEBUG_TARGET       (1 << 13)
#define FFX_CLASSIFIER_FLAGS_SHOW_INTERSECTION       (1 << 14)
#define FFX_CLASSIFIER_FLAGS_SHOW_REFLECTION_TARGET  (1 << 15)
#define FFX_CLASSIFIER_FLAGS_APPLY_REFLECTIONS       (1 << 16)
#define FFX_CLASSIFIER_FLAGS_INTERSECTION_ACCUMULATE (1 << 17)

#define FFX_CLASSIFIER_FLAGS_VISUALIZE_WAVES             (1 << 18)
#define FFX_CLASSIFIER_FLAGS_VISUALIZE_AVG_RADIANCE      (1 << 19)
#define FFX_CLASSIFIER_FLAGS_VISUALIZE_VARIANCE          (1 << 20)
#define FFX_CLASSIFIER_FLAGS_VISUALIZE_NUM_SAMPLES       (1 << 21)
#define FFX_CLASSIFIER_FLAGS_VISUALIZE_RAY_LENGTH        (1 << 23)
#define FFX_CLASSIFIER_FLAGS_VISUALIZE_REPROJECTION      (1 << 25)
#define FFX_CLASSIFIER_FLAGS_VISUALIZE_TRANSPARENT_QUERY (1 << 26)
#define FFX_CLASSIFIER_FLAGS_VISUALIZE_HIT_COUNTER       (1 << 27)
#define FFX_CLASSIFIER_FLAGS_VISUALIZE_PRIMARY_RAYS      (1 << 28)

#if defined(FFX_GPU)

#define FFX_REFLECTIONS_SKY_DISTANCE 100.0f

// Helper defines for hitcouter and classification
#define FFX_HITCOUNTER_SW_HIT_FLAG       (1u << 0u)
#define FFX_HITCOUNTER_SW_HIT_SHIFT      0u
#define FFX_HITCOUNTER_SW_OLD_HIT_SHIFT  8u
#define FFX_HITCOUNTER_MASK              0xffu
#define FFX_HITCOUNTER_SW_MISS_FLAG      (1u << 16u)
#define FFX_HITCOUNTER_SW_MISS_SHIFT     16u
#define FFX_HITCOUNTER_SW_OLD_MISS_SHIFT 24u

#define FFX_Hitcounter_GetSWHits(counter)      ((counter >> FFX_HITCOUNTER_SW_HIT_SHIFT) & FFX_HITCOUNTER_MASK)
#define FFX_Hitcounter_GetSWMisses(counter)    ((counter >> FFX_HITCOUNTER_SW_MISS_SHIFT) & FFX_HITCOUNTER_MASK)
#define FFX_Hitcounter_GetOldSWHits(counter)   ((counter >> FFX_HITCOUNTER_SW_OLD_HIT_SHIFT) & FFX_HITCOUNTER_MASK)
#define FFX_Hitcounter_GetOldSWMisses(counter) ((counter >> FFX_HITCOUNTER_SW_OLD_MISS_SHIFT) & FFX_HITCOUNTER_MASK)

//=== Common functions of the HsrSample ===

void UnpackRayCoords(FfxUInt32 packed, FFX_PARAMETER_OUT FfxUInt32x2 ray_coord, FFX_PARAMETER_OUT FfxBoolean copy_horizontal, FFX_PARAMETER_OUT FfxBoolean copy_vertical, FFX_PARAMETER_OUT FfxBoolean copy_diagonal) {
    ray_coord.x = (packed >> 0) & 32767;    // 0b111111111111111;
    ray_coord.y = (packed >> 15) & 16383;   // 0b11111111111111;
    copy_horizontal = FfxBoolean((packed >> 29) & 1u);
    copy_vertical   = FfxBoolean((packed >> 30) & 1u);
    copy_diagonal   = FfxBoolean((packed >> 31) & 1u);
}

// Mat must be able to transform origin from texture space to a linear space.
FfxFloat32x3 InvProjectPosition(FfxFloat32x3 coord, FfxFloat32Mat4 mat) {
    coord.y = (1 - coord.y);
    coord.xy = 2 * coord.xy - 1;
    FfxFloat32x4 projected = FFX_MATRIX_MULTIPLY(mat, FfxFloat32x4(coord, 1));
    projected.xyz /= projected.w;
    return projected.xyz;
}

FfxBoolean IsGlossyReflection(FfxFloat32 roughness) {
    return roughness < RoughnessThreshold();
}

FfxFloat32x3 ScreenSpaceToViewSpace(FfxFloat32x3 screen_uv_coord) {
    return InvProjectPosition(screen_uv_coord, InvProjection());
}

FfxBoolean IsBackground(FfxFloat32 depth)
{
#if FFX_CLASSIFIER_OPTION_INVERTED_DEPTH
    return depth < 1.e-6f;
#else
    return depth >= (1.0f - 1.e-6f);
#endif
}

// Rounds value to the nearest multiple of 8
FfxUInt32x2 FFX_DNSR_Reflections_RoundUp8(FfxUInt32x2 value) {
    FfxUInt32x2 round_down = value & ~7;    // 0b111;
    return FFX_SELECT((round_down == value), value, value + 8);
}

#endif
