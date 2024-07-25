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

#pragma once
#include <FidelityFX/gpu/blur/ffx_blur_resources.h>

typedef enum BlurShaderPermutationOptions
{
    BLUR_SHADER_PERMUTATION_3x3_KERNEL   = (1 << 0),  ///< Size of blur kernel.
    BLUR_SHADER_PERMUTATION_5x5_KERNEL   = (1 << 1),  ///< Size of blur kernel.
    BLUR_SHADER_PERMUTATION_7x7_KERNEL   = (1 << 2),  ///< Size of blur kernel.
    BLUR_SHADER_PERMUTATION_9x9_KERNEL   = (1 << 3),  ///< Size of blur kernel.
    BLUR_SHADER_PERMUTATION_11x11_KERNEL = (1 << 4),  ///< Size of blur kernel.
    BLUR_SHADER_PERMUTATION_13x13_KERNEL = (1 << 5),  ///< Size of blur kernel.
    BLUR_SHADER_PERMUTATION_15x15_KERNEL = (1 << 6),  ///< Size of blur kernel.
    BLUR_SHADER_PERMUTATION_17x17_KERNEL = (1 << 7),  ///< Size of blur kernel.
    BLUR_SHADER_PERMUTATION_19x19_KERNEL = (1 << 8),  ///< Size of blur kernel.
    BLUR_SHADER_PERMUTATION_21x21_KERNEL = (1 << 9),  ///< Size of blur kernel.
    BLUR_SHADER_PERMUTATION_FORCE_WAVE64 = (1 << 10),  ///< doesn't map to a define, selects different table
    BLUR_SHADER_PERMUTATION_ALLOW_FP16   = (1 << 11),  ///< Enables fast math computations where possible
    BLUR_SHADER_PERMUTATION_KERNEL_0     = (1 << 12),  ///< Selects Gaussian kernel based on sigma permutation 0 (see ffx_blur_callbacks.hlsl for actual value).
    BLUR_SHADER_PERMUTATION_KERNEL_1     = (1 << 13),  ///< Selects Gaussian kernel based on sigma permutation 1 (see ffx_blur_callbacks.hlsl for actual value).
    BLUR_SHADER_PERMUTATION_KERNEL_2     = (1 << 14),  ///< Selects Gaussian kernel based on sigma permutation 2 (see ffx_blur_callbacks.hlsl for actual value).
} BlurShaderPermutationOptions;

// Constants for Blur dispatches. Must be kept in sync with cbBLUR in ffx_blur_callbacks_hlsl.h
typedef struct BlurConstants
{
    uint32_t width; ///< Width in pixels of input image.
    uint32_t height; ///< Height in pixels of input image.
} BlurConstants;

struct FfxBlurContextDescription;
struct FfxDeviceCapabilities;
struct FfxPipelineState;
struct FfxResource;

// FfxBlurContext_Private
// The private implementation of the Blur context.
typedef struct FfxBlurContext_Private
{
    FfxBlurContextDescription contextDescription;
    FfxUInt32                 effectContextId;
    FfxConstantBuffer         blurConstants;
    FfxDevice                 device;
    FfxDeviceCapabilities     deviceCapabilities;
    FfxUInt32                 numKernelSizes;
    FfxPipelineState*         pBlurPipelines;
    FfxResourceInternal       srvResources[FFX_BLUR_RESOURCE_IDENTIFIER_COUNT];
    FfxResourceInternal       uavResources[FFX_BLUR_RESOURCE_IDENTIFIER_COUNT];
} FfxBlurContext_Private;
