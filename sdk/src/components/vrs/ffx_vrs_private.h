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
#include <FidelityFX/gpu/vrs/ffx_vrs_resources.h>

struct FfxVrsContextDescription;
struct FfxPipelineState;
struct FfxResource;

typedef enum VrsShaderPermutationOptions
{
    VRS_SHADER_PERMUTATION_ADDITIONALSHADINGRATES = (1 << 0),
    VRS_SHADER_PERMUTATION_FORCE_WAVE64           = (1 << 1),  ///< doesn't map to a define, selects different table
    VRS_SHADER_PERMUTATION_ALLOW_FP16             = (1 << 2),  ///< Enables fast math computations where possible
    VRS_SHADER_PERMUTATION_TILESIZE_8             = (1 << 3),
    VRS_SHADER_PERMUTATION_TILESIZE_16            = (1 << 4),
    VRS_SHADER_PERMUTATION_TILESIZE_32            = (1 << 5),
} VrsShaderPermutationOptions;

typedef struct VrsConstants
{
    float       motionVectorScale[2];
    float       varianceCutoff;
    float       motionFactor;
    uint32_t    width, height;
    uint32_t    tileSize;
} VrsConstants;

// FfxFsr1Context_Private
// The private implementation of the FSR1 context.
typedef struct FfxVrsContext_Private
{
    FfxVrsContextDescription contextDescription;
    FfxUInt32                effectContextId;
    VrsConstants             constants;
    FfxConstantBuffer        constantBuffer;

    FfxDevice             device;
    FfxDeviceCapabilities deviceCapabilities;

    FfxPipelineState pipelineImageGen;

    FfxResourceInternal srvResources[FFX_VRS_RESOURCE_IDENTIFIER_COUNT];
    FfxResourceInternal uavResources[FFX_VRS_RESOURCE_IDENTIFIER_COUNT];

} FfxVrsContext_Private;
