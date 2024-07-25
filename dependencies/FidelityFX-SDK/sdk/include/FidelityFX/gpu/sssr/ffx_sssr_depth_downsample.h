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

FFX_GROUPSHARED FfxUInt32 spdCounter;
FFX_GROUPSHARED FfxFloat32 spdIntermediate[16][16];

// Define fetch and store functions
FfxUInt32 SpdGetAtomicCounter() {
	return spdCounter;
}

#if FFX_HALF

FfxFloat16x4 SpdReduce4H(FfxFloat16x4 v0, FfxFloat16x4 v1, FfxFloat16x4 v2, FfxFloat16x4 v3) {
#if FFX_SSSR_OPTION_INVERTED_DEPTH
    return max(max(v0, v1), max(v2, v3));
#else
    return min(min(v0, v1), min(v2, v3));
#endif
}

FfxFloat16x4 SpdLoadIntermediateH(FfxUInt32 x, FfxUInt32 y) {
	FfxFloat16 f = FfxFloat16(spdIntermediate[x][y]);
	return FfxFloat16x4(f.x, f.x, f.x, f.x);
}

void SpdStoreIntermediateH(FfxUInt32 x, FfxUInt32 y, FfxFloat16x4 value) {
	spdIntermediate[x][y] = value.x;
}

#endif	// FFX_HALF

void SpdStoreIntermediate(FfxUInt32 x, FfxUInt32 y, FfxFloat32x4 value) {
	spdIntermediate[x][y] = value.x;
}

FfxFloat32x4 SpdLoadIntermediate(FfxUInt32 x, FfxUInt32 y) {
	FfxFloat32 f = spdIntermediate[x][y];
	return FfxFloat32x4(f.x, f.x, f.x, f.x);
}

FfxFloat32x4 SpdReduce4(FfxFloat32x4 v0, FfxFloat32x4 v1, FfxFloat32x4 v2, FfxFloat32x4 v3) {
#if FFX_SSSR_OPTION_INVERTED_DEPTH
    return max(max(v0, v1), max(v2, v3));
#else
    return min(min(v0, v1), min(v2, v3));
#endif
}

void SpdIncreaseAtomicCounter(FfxUInt32 slice)
{
	FFX_SSSR_SPDIncreaseAtomicCounter(spdCounter);
}

#include "../spd/ffx_spd.h"

FfxUInt32 GetThreadgroupCount(FfxUInt32x2 image_size){
	// Each threadgroup works on 64x64 texels
	return ((image_size.x + 63) / 64) * ((image_size.y + 63) / 64);
}

// Returns mips count of a texture with specified size
FfxFloat32 GetMipsCount(FfxFloat32x2 texture_size){
    FfxFloat32 max_dim = max(texture_size.x, texture_size.y);
    return 1.0 + floor(log2(max_dim));
}

void DepthDownsample(FfxUInt32 group_index, FfxUInt32x3 group_id, FfxUInt32x3 dispatch_thread_id){
	FfxFloat32x2 depth_image_size = FfxFloat32x2(0.0f, 0.0f);
	FFX_SSSR_GetInputDepthDimensions(depth_image_size);
    // Copy most detailed level into the hierarchy and transform it.
	FfxUInt32x2 u_depth_image_size = FfxUInt32x2(depth_image_size);
	for (FfxInt32 i = 0; i < 2; ++i)
	{
		for (FfxInt32 j = 0; j < 8; ++j)
		{
			FfxUInt32x2 idx = FfxUInt32x2(2 * dispatch_thread_id.x + i, 8 * dispatch_thread_id.y + j);
			if (idx.x < u_depth_image_size.x && idx.y < u_depth_image_size.y)
			{
				FFX_SSSR_WriteDepthHierarchy(0, idx, FFX_SSSR_GetInputDepth(idx));
			}
		}
	}

    FfxFloat32x2 image_size = FfxFloat32x2(0.0f, 0.0f);
    FFX_SSSR_GetDepthHierarchyMipDimensions(0, image_size);
    FfxFloat32 mips_count = GetMipsCount(image_size);
    FfxUInt32 threadgroup_count = GetThreadgroupCount(FfxInt32x2(image_size));

    SpdDownsample(
		FfxUInt32x2(group_id.xy),
		FfxUInt32(group_index),
		FfxUInt32(mips_count),
		FfxUInt32(threadgroup_count),
		0);
}
