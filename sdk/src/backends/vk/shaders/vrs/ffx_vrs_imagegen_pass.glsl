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
#extension GL_KHR_shader_subgroup_arithmetic : require

#define VRS_BIND_SRV_INPUT_COLOR             0
#define VRS_BIND_SRV_INPUT_MOTIONVECTORS     1   
#define VRS_BIND_UAV_OUTPUT_VRSIMAGE      2000

#define VRS_BIND_CB_VRS                   3000

#if FFX_VRS_OPTION_ADDITIONALSHADINGRATES
#define FFX_VARIABLESHADING_ADDITIONALSHADINGRATES
#endif

#define FFX_GPU  1
#include "vrs/ffx_vrs_callbacks_glsl.h"
#include "vrs/ffx_variable_shading.h"

#ifndef FFX_VRS_NUM_THREADS
#define FFX_VRS_NUM_THREADS layout (local_size_x = FFX_VariableShading_ThreadCount1D, local_size_y = FFX_VariableShading_ThreadCount1D, local_size_z = 1) in;
#endif // #ifndef FFX_VRS_NUM_THREADS


FFX_VRS_NUM_THREADS
void main()
{
    VrsGenerateVrsImage(gl_WorkGroupID.xyz, gl_LocalInvocationID.xyz, gl_LocalInvocationIndex);
}
