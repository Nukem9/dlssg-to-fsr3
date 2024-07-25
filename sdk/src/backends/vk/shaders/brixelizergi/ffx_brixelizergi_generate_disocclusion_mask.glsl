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
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require

#define FFX_WAVE 1

#define BRIXELIZER_GI_BIND_SRV_INPUT_DEPTH                   0
#define BRIXELIZER_GI_BIND_SRV_INPUT_NORMAL                  1
#define BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_DEPTH           2
#define BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_NORMAL          3
#define BRIXELIZER_GI_BIND_SRV_INPUT_MOTION_VECTORS          4

#define BRIXELIZER_GI_BIND_UAV_DISOCCLUSION_MASK             2000

#define BRIXELIZER_GI_BIND_CB_SDFGI                          3000

#undef FFX_HALF
#define FFX_HALF 1

#include "ffx_brixelizergi_callbacks_glsl.h"
#include "ffx_brixelizergi_main.h"

#ifndef FFX_BRIXELIZER_GI_THREAD_GROUP_WIDTH
#define FFX_BRIXELIZER_GI_THREAD_GROUP_WIDTH 8
#endif // #ifndef FFX_BRIXELIZER_GI_THREAD_GROUP_WIDTH
#ifndef FFX_BRIXELIZER_GI_THREAD_GROUP_HEIGHT
#define FFX_BRIXELIZER_GI_THREAD_GROUP_HEIGHT 8
#endif // FFX_BRIXELIZER_GI_THREAD_GROUP_HEIGHT
#ifndef FFX_BRIXELIZER_GI_THREAD_GROUP_DEPTH
#define FFX_BRIXELIZER_GI_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_BRIXELIZER_GI_THREAD_GROUP_DEPTH
#ifndef FFX_BRIXELIZER_GI_NUM_THREADS
#define FFX_BRIXELIZER_GI_NUM_THREADS layout (local_size_x = FFX_BRIXELIZER_GI_THREAD_GROUP_WIDTH, local_size_y = FFX_BRIXELIZER_GI_THREAD_GROUP_HEIGHT, local_size_z = FFX_BRIXELIZER_GI_THREAD_GROUP_DEPTH) in;
#endif // #ifndef FFX_BRIXELIZER_GI_NUM_THREADS

FFX_BRIXELIZER_GI_NUM_THREADS

void main()
{
	FfxBrixelizerGIGenerateDisocclusionMask(gl_GlobalInvocationID.xy);
}