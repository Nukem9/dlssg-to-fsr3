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

#include "ffx_brixelizer_common.h"
#include "ffx_brixelizer_host_gpu_shared.h"

void PushAABB(FfxBrixelizerDebugAABB aabb)
{
	FfxUInt32 offset;
	IncrementContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_NUM_DEBUG_AABBS, 1, offset);
	if (offset < GetDebugInfoMaxAABBs()) {
		StoreDebugAABB(offset, aabb);
	}
}

void FfxBrixelizerDrawAABBTreeAABB(FfxUInt32 aabb_index)
{
	// if g_debug_info.debug_state is set to 1 we only want to draw the cascade bounding box
	if (GetDebugInfoDebugState() == 1 && aabb_index > 0) {
		return;
	}
	if (aabb_index == 0) {
		FfxBrixelizerCascadeInfo cascade_info = GetCascadeInfo();
		FfxBrixelizerDebugAABB debug_aabb;
		debug_aabb.color = FfxFloat32x3(1.0f, 0.0f, 0.0f);
		debug_aabb.aabbMin = cascade_info.grid_min;
		debug_aabb.aabbMax = cascade_info.grid_max;
		PushAABB(debug_aabb);
	} else if (aabb_index == 1) {
		// root aabb
		FfxUInt32 index = 16*16*16 + 4*4*4*6;
		FfxBrixelizerDebugAABB debug_aabb;
		debug_aabb.color = FfxFloat32x3(0.0f, 1.0f, 1.0f);
		debug_aabb.aabbMin = LoadCascadeAABBTreeFloat3(index);
		debug_aabb.aabbMax = LoadCascadeAABBTreeFloat3(index + 3);
		PushAABB(debug_aabb);
	} else if (aabb_index <= 1 + 4*4*4) {
		// second level aabb
		FfxUInt32 index = 16*16*16 + (aabb_index - 2)*6;
		FfxBrixelizerDebugAABB debug_aabb;
		debug_aabb.color = FfxFloat32x3(1.0f, 1.0f, 0.0f);
		debug_aabb.aabbMin = LoadCascadeAABBTreeFloat3(index);
		debug_aabb.aabbMax = LoadCascadeAABBTreeFloat3(index + 3);

		if (ffxAsUInt32(debug_aabb.aabbMin.x) == ffxAsUInt32(debug_aabb.aabbMax.x)) {
			return;
		}

		PushAABB(debug_aabb);

	} else if (aabb_index <= 1 + 4*4*4 + 16*16*16) {
		// leaf aabb
		FfxBrixelizerCascadeInfo cascade_info = GetCascadeInfo();
		FfxUInt32 index = aabb_index - (4*4*4 + 1 + 1);
		FfxUInt32 packedAABB = LoadCascadeAABBTreeUInt(index);
		if (packedAABB == FFX_BRIXELIZER_INVALID_BOTTOM_AABB_NODE) {
			return;
		}

		FfxUInt32x3 leaf_offset = FfxBrixelizerUnflattenPOT(index, 4) * 32;
		FfxUInt32x3 aabbMin = leaf_offset + FfxBrixelizerUnflattenPOT(packedAABB & ((1 << 15) - 1), 5);
		FfxUInt32x3 aabbMax = leaf_offset + FfxBrixelizerUnflattenPOT((packedAABB >> 16) & ((1 << 15) - 1), 5) + 1;

		FfxBrixelizerDebugAABB debug_aabb;
		debug_aabb.color = FfxFloat32x3(0.0f, 1.0f, 0.0f);
		debug_aabb.aabbMin = cascade_info.grid_min + FfxFloat32x3(aabbMin) * cascade_info.voxel_size / 8.0f;
		debug_aabb.aabbMax = cascade_info.grid_min + FfxFloat32x3(aabbMax) * cascade_info.voxel_size / 8.0f;
		PushAABB(debug_aabb);
	}
}

void FfxBrixelizerDrawInstanceAABB(FfxUInt32 index)
{
	// XXX -- use debug_state to pass in number of instance IDs
	if (index >= GetDebugInfoDebugState()) {
		return;
	}

	FfxUInt32 instance_id = GetDebugInstanceID(index);
	FfxBrixelizerInstanceInfo instance_info = LoadInstanceInfo(instance_id);
	FfxBrixelizerDebugAABB debug_aabb;
	debug_aabb.color = FfxFloat32x3(0.0f, 0.0f, 1.0f);
	debug_aabb.aabbMin = instance_info.aabbMin;
	debug_aabb.aabbMax = instance_info.aabbMax;
	PushAABB(debug_aabb);
}