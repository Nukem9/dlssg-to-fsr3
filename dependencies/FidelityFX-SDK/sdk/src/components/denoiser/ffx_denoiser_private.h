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
#include <FidelityFX/gpu/denoiser/ffx_denoiser_resources.h>

/// An enumeration of all the permutations that can be passed to the Denoiser algorithm.
///
/// Denoiser features are organized through a set of pre-defined compile
/// permutation options that need to be specified. Which shader blob
/// is returned for pipeline creation will be determined by what combination
/// of shader permutations are enabled.
///
/// @ingroup Denoiser
typedef enum DenoiserShaderPermutationOptions
{
    DENOISER_SHADER_PERMUTATION_FORCE_WAVE64             = (1 << 0),  ///< doesn't map to a define, selects different table
    DENOISER_SHADER_PERMUTATION_ALLOW_FP16               = (1 << 1),  ///< Enables fast math computations where possible
    DENOISER_SHADER_PERMUTATION_DEPTH_INVERTED           = (1 << 2),  ///< Indicates input resources were generated with inverted depth
} DenoiserShaderPermutationOptions;

#define DENOISER_SHADOWS_CONSTANT_BUFFER_0_SIZE 2
#define DENOISER_SHADOWS_CONSTANT_BUFFER_1_SIZE 60
#define DENOISER_SHADOWS_CONSTANT_BUFFER_2_SIZE 24

// FfxDenoiserContext_Private
// The private implementation of the Denoiser context.
typedef struct FfxDenoiserContext_Private
{
    FfxDenoiserContextDescription   contextDescription;
    FfxUInt32                       effectContextId;
    FfxDevice                       device;
    FfxDeviceCapabilities           deviceCapabilities;

    FfxPipelineState    pipelinePrepareShadowMask;
    FfxPipelineState    pipelineTileClassification;
    FfxPipelineState    pipelineFilterSoftShadows0;
    FfxPipelineState    pipelineFilterSoftShadows1;
    FfxPipelineState    pipelineFilterSoftShadows2;

    FfxPipelineState    pipelineReprojectReflections;
    FfxPipelineState    pipelinePrefilterReflections;
    FfxPipelineState    pipelineResolveTemporalReflections;

    FfxResourceInternal srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_COUNT];
    FfxResourceInternal uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_COUNT];
    FfxConstantBuffer   shadowsConstants[FFX_DENOISER_SHADOWS_CONSTANTBUFFER_IDENTIFIER_DENOISER_SHADOWS_COUNT];
    FfxConstantBuffer   reflectionsConstants[FFX_DENOISER_REFLECTIONS_CONSTANTBUFFER_IDENTIFIER_COUNT];

    bool isFirstShadowFrame;
    bool isFirstReflectionsFrame;
} FfxDenoiserContext_Private;
