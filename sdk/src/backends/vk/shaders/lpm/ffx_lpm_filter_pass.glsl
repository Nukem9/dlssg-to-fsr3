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

// LPM pass 1
// SRV  0 : LPM_InputColor  : r_input_color
// UAV  0 : LPM_OutputColor : rw_output_color
// CB   0 : cbLPM

#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

#define LPM_BIND_SRV_INPUT_COLOR                  0
#define LPM_BIND_UAV_OUTPUT_COLOR              2000

#define LPM_BIND_CB_LPM                        3000

#include "lpm/ffx_lpm_callbacks_glsl.h"
#include "lpm/ffx_lpm_filter.h"

#ifndef FFX_LPM_THREAD_GROUP_WIDTH
#define FFX_LPM_THREAD_GROUP_WIDTH 64
#endif // #ifndef FFX_LPM_THREAD_GROUP_WIDTH
#ifndef FFX_LPM_THREAD_GROUP_HEIGHT
#define FFX_LPM_THREAD_GROUP_HEIGHT 1
#endif // FFX_LPM_THREAD_GROUP_HEIGHT
#ifndef FFX_LPM_THREAD_GROUP_DEPTH
#define FFX_LPM_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_LPM_THREAD_GROUP_DEPTH
#ifndef FFX_LPM_NUM_THREADS
#define FFX_LPM_NUM_THREADS layout (local_size_x = FFX_LPM_THREAD_GROUP_WIDTH, local_size_y = FFX_LPM_THREAD_GROUP_HEIGHT, local_size_z = FFX_LPM_THREAD_GROUP_DEPTH) in;
#endif // #ifndef FFX_LPM_NUM_THREADS

FFX_LPM_NUM_THREADS
void main()
{
    LPMFilter(gl_LocalInvocationID.xyz, gl_WorkGroupID.xyz, gl_GlobalInvocationID.xyz);
}
