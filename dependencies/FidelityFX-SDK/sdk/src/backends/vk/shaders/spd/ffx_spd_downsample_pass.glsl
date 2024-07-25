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

// SPD pass
// SRV  0 : SPD_InputDownsampleSrc          : r_input_downsample_src
// UAV  0 : SPD_InternalGlobalAtomic        : rw_internal_global_atomic
// UAV  1 : SPD_InputDownsampleSrcMidMip    : rw_input_downsample_src_mid_mip
// UAV  2 : SPD_InputDownsampleSrcMips      : rw_input_downsample_src_mips
// CB   0 : cbSPD

#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

#define FFX_SPD_BIND_SRV_INPUT_DOWNSAMPLE_SRC                  0

#define FFX_SPD_BIND_UAV_INTERNAL_GLOBAL_ATOMIC             2000
#define FFX_SPD_BIND_UAV_INPUT_DOWNSAMPLE_SRC_MID_MIPMAP    2001
#define FFX_SPD_BIND_UAV_INPUT_DOWNSAMPLE_SRC_MIPS          2002

#define FFX_SPD_BIND_CB_SPD                                 3000

#include "spd/ffx_spd_callbacks_glsl.h"
#include "spd/ffx_spd_downsample.h"

#ifndef FFX_SPD_THREAD_GROUP_WIDTH
#define FFX_SPD_THREAD_GROUP_WIDTH 256
#endif // #ifndef FFX_SPD_THREAD_GROUP_WIDTH
#ifndef FFX_SPD_THREAD_GROUP_HEIGHT
#define FFX_SPD_THREAD_GROUP_HEIGHT 1
#endif // FFX_SPD_THREAD_GROUP_HEIGHT
#ifndef FFX_SPD_THREAD_GROUP_DEPTH
#define FFX_SPD_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_SPD_THREAD_GROUP_DEPTH
#ifndef FFX_SPD_NUM_THREADS
#define FFX_SPD_NUM_THREADS layout (local_size_x = FFX_SPD_THREAD_GROUP_WIDTH, local_size_y = FFX_SPD_THREAD_GROUP_HEIGHT, local_size_z = FFX_SPD_THREAD_GROUP_DEPTH) in;
#endif // #ifndef FFX_SPD_NUM_THREADS

FFX_SPD_NUM_THREADS
void main()
{
    DOWNSAMPLE(gl_LocalInvocationIndex, gl_WorkGroupID.xyz);
}
