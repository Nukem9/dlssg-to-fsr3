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
#include <FidelityFX/gpu/lens/ffx_lens_resources.h>

/// An enumeration of all the permutations that can be passed to the Lens algorithm.
///
/// Lens features are organized through a set of pre-defined compile
/// permutation options that need to be specified. Which shader blob
/// is returned for pipeline creation will be determined by what combination
/// of shader permutations are enabled.
///
/// @ingroup Lens
typedef enum LensShaderPermutationOptions
{
    LENS_SHADER_PERMUTATION_FORCE_WAVE64             = (1 << 0),  ///< doesn't map to a define, selects different table
    LENS_SHADER_PERMUTATION_ALLOW_FP16               = (1 << 1),  ///< Enables fast math computations where possible
} LensShaderPermutationOptions;

// Constants for Lens dispatches. Must be kept in sync with cbLens in ffx_lens_callbacks_hlsl.h
typedef struct LensConstants
{
    float grainScale;
    float grainAmount;
    uint32_t grainSeed;
    uint32_t pad;

    uint32_t center[2];
    float chromAb;
    float vignette;

} LensConstants;

struct FfxLensContextDescription;
struct FfxDeviceCapabilities;
struct FfxPipelineState;
struct FfxResource;

// FfxLensContext_Private
// The private implementation of the Lens context.
typedef struct FfxLensContext_Private
{
    FfxLensContextDescription contextDescription;
    FfxUInt32                 effectContextId;
    FfxDevice                 device;
    FfxDeviceCapabilities     deviceCapabilities;

    FfxPipelineState          pipelineLens;
    FfxResourceInternal srvResources[FFX_LENS_RESOURCE_IDENTIFIER_COUNT];
    FfxResourceInternal uavResources[FFX_LENS_RESOURCE_IDENTIFIER_COUNT];

    FfxConstantBuffer constantBuffer;

} FfxLensContext_Private;
