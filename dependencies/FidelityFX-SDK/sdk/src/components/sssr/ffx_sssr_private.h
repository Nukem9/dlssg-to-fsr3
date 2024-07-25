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
#include <FidelityFX/gpu/sssr/ffx_sssr_resources.h>
#include <FidelityFX/host/ffx_denoiser.h>
#include <stdint.h>

typedef enum SssrShaderPermutationOptions
{
    SSSR_SHADER_PERMUTATION_FORCE_WAVE64    = (1 << 0),  ///< doesn't map to a define, selects different table
    SSSR_SHADER_PERMUTATION_ALLOW_FP16      = (1 << 1),  ///< Enables fast math computations where possible
    SSSR_SHADER_PERMUTATION_DEPTH_INVERTED  = (1 << 2),  ///< Indicates input resources were generated with inverted depth
} SssrShaderPermutationOptions;

struct SSSRConstants
{
    float           invViewProjection[16];
    float           projection[16];
    float           invProjection[16];
    float           view[16];
    float           invView[16];
    float           prevViewProjection[16];
    uint32_t        renderSize[2];
    float           inverseRenderSize[2];
    float           normalsUnpackMul;
    float           normalsUnpackAdd;
    uint32_t        roughnessChannel;
    bool            isRoughnessPerceptual;
    float           iblFactor;
    float           temporalStabilityFactor;
    float           depthBufferThickness;
    float           roughnessThreshold;
    float           varianceThreshold;
    uint32_t        frameIndex;
    uint32_t        maxTraversalIntersections;
    uint32_t        minTraversalOccupancy;
    uint32_t        mostDetailedMip;
    uint32_t        samplesPerQuad;
    uint32_t        temporalVarianceGuidedTracingEnabled;
};

// FfxSsssr2Context_Private
// The private implementation of the SSSR context.
typedef struct FfxSssrContext_Private
{
    FfxSssrContextDescription   contextDescription;
    FfxUInt32                   effectContextId;
    SSSRConstants               constants;
    FfxDevice                   device;
    FfxDeviceCapabilities       deviceCapabilities;
    FfxPipelineState            pipelineDepthDownsample;
    FfxPipelineState            pipelineClassifyTiles;
    FfxPipelineState            pipelinePrepareBlueNoiseTexture;
    FfxPipelineState            pipelinePrepareIndirectArgs;
    FfxPipelineState            pipelineIntersection;

    FfxDenoiserContext          denoiserContext;
    
    FfxResourceInternal srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_COUNT];
    FfxResourceInternal uavResources[FFX_SSSR_RESOURCE_IDENTIFIER_COUNT];
    FfxConstantBuffer   constantBuffers[FFX_SSSR_CONSTANTBUFFER_IDENTIFIER_COUNT];

    bool        refreshPipelineStates;
    uint32_t    resourceFrameIndex;
} FfxSssrContext_Private;
