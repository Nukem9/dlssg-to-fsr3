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

#define FSR3UPSCALER_BIND_SRV_INPUT_OPAQUE_ONLY                     0
#define FSR3UPSCALER_BIND_SRV_INPUT_COLOR                           1

#define FSR3UPSCALER_BIND_UAV_AUTOREACTIVE                          0
#define FSR3UPSCALER_BIND_UAV_AUTOCOMPOSITION                       1

#define FSR3UPSCALER_BIND_CB_FSR3UPSCALER                           0
#define FSR3UPSCALER_BIND_CB_REACTIVE                               1

#include "fsr3upscaler/ffx_fsr3upscaler_callbacks_hlsl.h"
#include "fsr3upscaler/ffx_fsr3upscaler_common.h"

#ifndef FFX_FSR3UPSCALER_THREAD_GROUP_WIDTH
#define FFX_FSR3UPSCALER_THREAD_GROUP_WIDTH 8
#endif // #ifndef FFX_FSR3UPSCALER_THREAD_GROUP_WIDTH
#ifndef FFX_FSR3UPSCALER_THREAD_GROUP_HEIGHT
#define FFX_FSR3UPSCALER_THREAD_GROUP_HEIGHT 8
#endif // FFX_FSR3UPSCALER_THREAD_GROUP_HEIGHT
#ifndef FFX_FSR3UPSCALER_THREAD_GROUP_DEPTH
#define FFX_FSR3UPSCALER_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_FSR3UPSCALER_THREAD_GROUP_DEPTH
#ifndef FFX_FSR3UPSCALER_NUM_THREADS
#define FFX_FSR3UPSCALER_NUM_THREADS [numthreads(FFX_FSR3UPSCALER_THREAD_GROUP_WIDTH, FFX_FSR3UPSCALER_THREAD_GROUP_HEIGHT, FFX_FSR3UPSCALER_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_FSR3UPSCALER_NUM_THREADS

FFX_FSR3UPSCALER_NUM_THREADS
FFX_FSR3UPSCALER_EMBED_ROOTSIG_CONTENT
void CS(uint2 uGroupId : SV_GroupID, uint2 uGroupThreadId : SV_GroupThreadID)
{
    uint2 uDispatchThreadId = uGroupId * uint2(FFX_FSR3UPSCALER_THREAD_GROUP_WIDTH, FFX_FSR3UPSCALER_THREAD_GROUP_HEIGHT) + uGroupThreadId;

    float3 ColorPreAlpha    = LoadOpaqueOnly( FFX_MIN16_I2(uDispatchThreadId) ).rgb;
    float3 ColorPostAlpha   = LoadInputColor(uDispatchThreadId).rgb;
    
    if (GenReactiveFlags() & FFX_FSR3UPSCALER_AUTOREACTIVEFLAGS_APPLY_TONEMAP)
    {
        ColorPreAlpha = Tonemap(ColorPreAlpha);
        ColorPostAlpha = Tonemap(ColorPostAlpha);
    }

    if (GenReactiveFlags() & FFX_FSR3UPSCALER_AUTOREACTIVEFLAGS_APPLY_INVERSETONEMAP)
    {
        ColorPreAlpha = InverseTonemap(ColorPreAlpha);
        ColorPostAlpha = InverseTonemap(ColorPostAlpha);
    }

    float out_reactive_value = 0.f;
    float3 delta = abs(ColorPostAlpha - ColorPreAlpha);
    
    out_reactive_value = (GenReactiveFlags() & FFX_FSR3UPSCALER_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX) ? max(delta.x, max(delta.y, delta.z)) : length(delta);
    out_reactive_value *= GenReactiveScale();

    out_reactive_value = (GenReactiveFlags() & FFX_FSR3UPSCALER_AUTOREACTIVEFLAGS_APPLY_THRESHOLD) ? (out_reactive_value < GenReactiveThreshold() ? 0 : GenReactiveBinaryValue()) : out_reactive_value;

    rw_output_autoreactive[uDispatchThreadId] = out_reactive_value;
}
