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

#include <FidelityFX/gpu/ffx_core.h>

#define FFX_BRIXELIZER_GI_SCREEN_PROBE_SIZE 8
#define FFX_BRIXELIZER_GI_BRICK_TILE_SIZE   4

typedef enum BrixelizerGIShaderPermutationOptions
{
    BRIXELIZER_GI_SHADER_PERMUTATION_DEPTH_INVERTED   = (1 << 0),  ///< Indicates input resources were generated with inverted depth.
    BRIXELIZER_GI_SHADER_PERMUTATION_DISABLE_SPECULAR = (1 << 1),  ///< Disable specular GI.
    BRIXELIZER_GI_SHADER_PERMUTATION_DISABLE_DENOISER = (1 << 2),  ///< Disable denoising.
    BRIXELIZER_GI_SHADER_PERMUTATION_FORCE_WAVE64     = (1 << 3),  ///< doesn't map to a define, selects different table
    BRIXELIZER_GI_SHADER_PERMUTATION_ALLOW_FP16       = (1 << 4),  ///< Enables fast math computations where possible
} BrixelizerGIShaderPermutationOptions;

// FfxBrixelizerGIContext_Private
// The private implementation of the brixelizer GI context.
typedef struct FfxBrixelizerGIContext_Private
{
    FfxBrixelizerGIContextDescription   contextDescription;
    FfxUInt32                          effectContextId;
    FfxDevice                          device;
    FfxDeviceCapabilities              deviceCapabilities;
    FfxPipelineState                   pipelinePrepareClearCache;
    FfxPipelineState                   pipelineClearCache;
    FfxPipelineState                   pipelineEmitPrimaryRayRadiance;
    FfxPipelineState                   pipelinePropagateSH;
    FfxPipelineState                   pipelineSpawnScreenProbes;
    FfxPipelineState                   pipelineReprojectScreenProbes;
    FfxPipelineState                   pipelineFillScreenProbes;
    FfxPipelineState                   pipelineSpecularPreTrace;
    FfxPipelineState                   pipelineSpecularTrace;
    FfxPipelineState                   pipelineReprojectGI;
    FfxPipelineState                   pipelineProjectScreenProbes;
    FfxPipelineState                   pipelineEmitIrradianceCache;
    FfxPipelineState                   pipelineInterpolateScreenProbes;
    FfxPipelineState                   pipelineBlurX;
    FfxPipelineState                   pipelineBlurY;
    FfxPipelineState                   pipelineDebugVisualization;
    FfxPipelineState                   pipelineGenerateDisocclusionMask;
    FfxPipelineState                   pipelineDownsample;
    FfxPipelineState                   pipelineUpsample;
    FfxResourceInternal                resources[FFX_BRIXELIZER_GI_RESOURCE_IDENTIFIER_COUNT];
    uint32_t                           pingPongResourceIds[FFX_BRIXELIZER_GI_PING_PONG_RESOURCE_COUNT];
    uint32_t                           currentScreenProbesId;
    uint32_t                           currentGITargetId;
    uint32_t                           currentSpecularTargetId;
    uint32_t                           historyScreenProbesId;
    uint32_t                           historyGITargetId;
    uint32_t                           historySpecularTargetId;
    uint32_t                           frameIndex;
    FfxDimensions2D                    internalSize;
	FfxGpuJobDescription               gpuJobDescription;
    FfxConstantBuffer                  constantBuffers[3];
} FfxBrixelizerGIContext_Private;
