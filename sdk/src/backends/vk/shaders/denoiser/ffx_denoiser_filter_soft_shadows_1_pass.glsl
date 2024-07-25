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

#define FFX_WAVE 1
#extension GL_KHR_shader_subgroup_vote : require

#define DENOISER_SHADOWS_BIND_SRV_DEPTH                     0
#define DENOISER_SHADOWS_BIND_SRV_NORMAL                    1
#define DENOISER_SHADOWS_BIND_SRV_FILTER_INPUT              2

#define DENOISER_SHADOWS_BIND_UAV_TILE_METADATA             2000
#define DENOISER_SHADOWS_BIND_UAV_HISTORY                   2001

#define DENOISER_SHADOWS_BIND_CB2_DENOISER_SHADOWS          3000

#include "denoiser/ffx_denoiser_shadows_callbacks_glsl.h"
#include "denoiser/ffx_denoiser_shadows_filter.h"

#ifndef FFX_DENOISER_SHADOWS_THREAD_GROUP_WIDTH
#define FFX_DENOISER_SHADOWS_THREAD_GROUP_WIDTH 8
#endif // #ifndef FFX_DENOISER_SHADOWS_THREAD_GROUP_WIDTH
#ifndef FFX_DENOISER_SHADOWS_THREAD_GROUP_HEIGHT
#define FFX_DENOISER_SHADOWS_THREAD_GROUP_HEIGHT 8
#endif // FFX_DENOISER_SHADOWS_THREAD_GROUP_HEIGHT
#ifndef FFX_DENOISER_SHADOWS_THREAD_GROUP_DEPTH
#define FFX_DENOISER_SHADOWS_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_DENOISER_SHADOWS_THREAD_GROUP_DEPTH
#ifndef FFX_DENOISER_SHADOWS_NUM_THREADS
#define FFX_DENOISER_SHADOWS_NUM_THREADS layout (local_size_x = FFX_DENOISER_SHADOWS_THREAD_GROUP_WIDTH, local_size_y = FFX_DENOISER_SHADOWS_THREAD_GROUP_HEIGHT, local_size_z = FFX_DENOISER_SHADOWS_THREAD_GROUP_DEPTH) in;
#endif // #ifndef FFX_DENOISER_SHADOWS_NUM_THREADS

FFX_DENOISER_SHADOWS_NUM_THREADS
void main()
{
    DenoiserShadowsFilterPass1(gl_WorkGroupID.xy, gl_LocalInvocationID.xy, gl_GlobalInvocationID.xy);
}