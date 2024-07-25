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

#define FSR3UPSCALER_BIND_SRV_RECONSTRUCTED_PREV_NEAREST_DEPTH              0
#define FSR3UPSCALER_BIND_SRV_DILATED_MOTION_VECTORS                        1
#define FSR3UPSCALER_BIND_SRV_DILATED_DEPTH                                 2
#define FSR3UPSCALER_BIND_SRV_REACTIVE_MASK                                 3
#define FSR3UPSCALER_BIND_SRV_TRANSPARENCY_AND_COMPOSITION_MASK             4
#define FSR3UPSCALER_BIND_SRV_ACCUMULATION                                  5
#define FSR3UPSCALER_BIND_SRV_SHADING_CHANGE                                6
#define FSR3UPSCALER_BIND_SRV_CURRENT_LUMA                                  7
#define FSR3UPSCALER_BIND_SRV_INPUT_EXPOSURE                                8

#define FSR3UPSCALER_BIND_UAV_DILATED_REACTIVE_MASKS                        0
#define FSR3UPSCALER_BIND_UAV_NEW_LOCKS                                     1
#define FSR3UPSCALER_BIND_UAV_ACCUMULATION                                  2

#define FSR3UPSCALER_BIND_CB_FSR3UPSCALER                                   0

#include "fsr3upscaler/ffx_fsr3upscaler_callbacks_hlsl.h"
#include "fsr3upscaler/ffx_fsr3upscaler_common.h"
#include "fsr3upscaler/ffx_fsr3upscaler_prepare_reactivity.h"

#ifndef FFX_FSR3UPSCALER_THREAD_GROUP_WIDTH
#define FFX_FSR3UPSCALER_THREAD_GROUP_WIDTH 8
#endif // #ifndef FFX_FSR3UPSCALER_THREAD_GROUP_WIDTH
#ifndef FFX_FSR3UPSCALER_THREAD_GROUP_HEIGHT
#define FFX_FSR3UPSCALER_THREAD_GROUP_HEIGHT 8
#endif // #ifndef FFX_FSR3UPSCALER_THREAD_GROUP_HEIGHT
#ifndef FFX_FSR3UPSCALER_THREAD_GROUP_DEPTH
#define FFX_FSR3UPSCALER_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_FSR3UPSCALER_THREAD_GROUP_DEPTH
#ifndef FFX_FSR3UPSCALER_NUM_THREADS
#define FFX_FSR3UPSCALER_NUM_THREADS [numthreads(FFX_FSR3UPSCALER_THREAD_GROUP_WIDTH, FFX_FSR3UPSCALER_THREAD_GROUP_HEIGHT, FFX_FSR3UPSCALER_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_FSR3UPSCALER_NUM_THREADS

FFX_PREFER_WAVE64
FFX_FSR3UPSCALER_NUM_THREADS
FFX_FSR3UPSCALER_EMBED_ROOTSIG_CONTENT
void CS(int2 iDispatchThreadId : SV_DispatchThreadID)
{
    PrepareReactivity(iDispatchThreadId);
}
