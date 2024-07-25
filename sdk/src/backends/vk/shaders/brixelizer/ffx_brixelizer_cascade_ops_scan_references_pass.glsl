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

#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

#define BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAP                       2000
#define BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS                        2001
#define BRIXELIZER_BIND_UAV_SCRATCH_VOXEL_ALLOCATION_FAIL_COUNTER   2002
#define BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE_OFFSETS          2003
#define BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS                        2004
#define BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_COMPRESSION_LIST         2005
#define BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_CLEAR_LIST               2006
#define BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS                2007
#define BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTER_SCAN            2008
#define BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_GLOBAL_SCAN             2009
#define BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_SCAN                  2010
#define BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_GLOBAL_SCAN           2011
#define BRIXELIZER_BIND_UAV_INDIRECT_ARGS                           2012
#define BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_VOXEL_MAP                2013
#define BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_FREE_LIST                2014

#define BRIXELIZER_BIND_CB_BUILD_INFO                               3000
#define BRIXELIZER_BIND_CB_CASCADE_INFO                             3001

#include "ffx_brixelizer_callbacks_glsl.h"
#include "ffx_brixelizer_cascade_ops.h"

#ifndef FFX_BRIXELIZER_THREAD_GROUP_WIDTH
#define FFX_BRIXELIZER_THREAD_GROUP_WIDTH FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE
#endif // #ifndef FFX_BRIXELIZER_THREAD_GROUP_WIDTH
#ifndef FFX_BRIXELIZER_THREAD_GROUP_HEIGHT
#define FFX_BRIXELIZER_THREAD_GROUP_HEIGHT 1
#endif // FFX_BRIXELIZER_THREAD_GROUP_HEIGHT
#ifndef FFX_BRIXELIZER_THREAD_GROUP_DEPTH
#define FFX_BRIXELIZER_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_BRIXELIZER_THREAD_GROUP_DEPTH
#ifndef FFX_BRIXELIZER_NUM_THREADS
#define FFX_BRIXELIZER_NUM_THREADS layout (local_size_x = FFX_BRIXELIZER_THREAD_GROUP_WIDTH, local_size_y = FFX_BRIXELIZER_THREAD_GROUP_HEIGHT, local_size_z = FFX_BRIXELIZER_THREAD_GROUP_DEPTH) in;
#endif // #ifndef FFX_BRIXELIZER_NUM_THREADS

FFX_BRIXELIZER_NUM_THREADS

void main()
{
	FfxBrixelizerScanReferences(gl_GlobalInvocationID.x, gl_LocalInvocationID.x, gl_WorkGroupID.x);
}