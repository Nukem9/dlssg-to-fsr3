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

// Texture definitions
// RWTexture2D<uint> imgDestination : register(u0);
// Texture2D         texColor : register(t0);
// Texture2D         texVelocity : register(t1);

#define VRS_BIND_SRV_INPUT_COLOR 0
#define VRS_BIND_SRV_INPUT_MOTIONVECTORS 1
#define VRS_BIND_UAV_OUTPUT_VRSIMAGE 0

#define VRS_BIND_CB_VRS 0

#if FFX_VRS_OPTION_ADDITIONALSHADINGRATES
#define FFX_VARIABLESHADING_ADDITIONALSHADINGRATES
#endif

#define FFX_GPU  1
#define FFX_HLSL 1
#include "vrs/ffx_vrs_callbacks_hlsl.h"
#include "vrs/ffx_variable_shading.h"

#ifndef FFX_VRS_NUM_THREADS
#define FFX_VRS_NUM_THREADS [numthreads(FFX_VariableShading_ThreadCount1D, FFX_VariableShading_ThreadCount1D, 1)]
#endif // #ifndef FFX_VRS_NUM_THREADS

FFX_PREFER_WAVE64
FFX_VRS_NUM_THREADS
FFX_VRS_EMBED_ROOTSIG_CONTENT
void CS(
    uint3 Gid: SV_GroupID,
    uint3 Gtid: SV_GroupThreadID,
    uint Gidx: SV_GroupIndex)
{
    VrsGenerateVrsImage(Gid, Gtid, Gidx);
}
