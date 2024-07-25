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

//=== Common functions of the SssrSample ===

void UnpackRayCoords(FfxUInt32 packed, FFX_PARAMETER_OUT FfxUInt32x2 ray_coord, FFX_PARAMETER_OUT FfxBoolean copy_horizontal, FFX_PARAMETER_OUT FfxBoolean copy_vertical, FFX_PARAMETER_OUT FfxBoolean copy_diagonal) {
    ray_coord.x = (packed >> 0) & 32767;    // 0b111111111111111;
    ray_coord.y = (packed >> 15) & 16383;   // 0b11111111111111;
    copy_horizontal = FfxBoolean((packed >> 29) & 1u);
    copy_vertical   = FfxBoolean((packed >> 30) & 1u);
    copy_diagonal   = FfxBoolean((packed >> 31) & 1u);
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
FfxFloat32x3 InvProjectPosition(FfxFloat32x3 coord, FfxFloat32Mat4 mat) {
    coord.y = (1 - coord.y);
    coord.xy = 2 * coord.xy - 1;
    FfxFloat32x4 projected = FFX_MATRIX_MULTIPLY(mat, FfxFloat32x4(coord, 1));
    projected.xyz /= projected.w;
    return projected.xyz;
}

// Origin and direction must be in the same space and mat must be able to transform from that space into clip space.
FfxFloat32x3 ProjectDirection(FfxFloat32x3 origin, FfxFloat32x3 direction, FfxFloat32x3 screen_space_origin, FfxFloat32Mat4 mat) {
    FfxFloat32x3 offsetted = ProjectPosition(origin + direction, mat);
    return offsetted - screen_space_origin;
}

FfxBoolean IsGlossyReflection(FfxFloat32 roughness) {
    return roughness < RoughnessThreshold();
}

FfxBoolean IsMirrorReflection(FfxFloat32 roughness) {
    return roughness < 0.0001;
}

FfxFloat32x3 ScreenSpaceToViewSpace(FfxFloat32x3 screen_uv_coord) {
    return InvProjectPosition(screen_uv_coord, InvProjection());
}
