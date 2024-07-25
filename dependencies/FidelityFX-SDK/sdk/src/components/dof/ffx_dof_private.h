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
#include <FidelityFX/gpu/dof/ffx_dof_resources.h>

/// An enumeration of all the permutations that can be passed to the DoF algorithm.
///
/// DoF features are organized through a set of pre-defined compile
/// permutation options that need to be specified. Which shader blob
/// is returned for pipeline creation will be determined by what combination
/// of shader permutations are enabled.
///
/// @ingroup DOF
typedef enum DofShaderPermutationOptions
{
    DOF_SHADER_PERMUTATION_REVERSE_DEPTH           = (1 << 0),  ///< Higher depth values are closer
    DOF_SHADER_PERMUTATION_COMBINE_IN_PLACE        = (1 << 1),  ///< Output texture contains input color
    DOF_SHADER_PERMUTATION_MERGE_RINGS             = (1 << 2),  ///< Allow merging rings together
    DOF_SHADER_PERMUTATION_USE_FP16                = (1 << 3),  ///< Use half precision
    DOF_SHADER_PERMUTATION_FORCE_WAVE64            = (1 << 4),  ///< doesn't map to a define, selects different table
} DofShaderPermutationOptions;

// Constants for DOF dispatches. Must be kept in sync with cbDOF in ffx_dof_callbacks_hlsl.h
typedef struct DofConstants
{
    float       cocScale;
    float       cocBias;
    uint32_t    inputSizeHalf[2];
    uint32_t    inputSize[2];
    float       inputSizeHalfRcp[2];
    float       cocLimit;
    uint32_t    maxRings;
} DofConstants;

struct FfxDofContextDescription;
struct FfxDeviceCapabilities;
struct FfxPipelineState;
struct FfxResource;

// FfxDofContext_Private
// The private implementation of the DOF context.
typedef struct FfxDofContext_Private {

    FfxDofContextDescription   contextDescription;
    FfxUInt32                  effectContextId;
    FfxDevice                  device;
    FfxDeviceCapabilities      deviceCapabilities;

    FfxPipelineState           pipelineDsColor;
    FfxPipelineState           pipelineDsDepth;
    FfxPipelineState           pipelineDilate;
    FfxPipelineState           pipelineBlur;
    FfxPipelineState           pipelineComposite;

    FfxResourceInternal srvResources[FFX_DOF_RESOURCE_IDENTIFIER_COUNT];
    FfxResourceInternal uavResources[FFX_DOF_RESOURCE_IDENTIFIER_COUNT];
    FfxConstantBuffer   constantBuffer;

} FfxDofContext_Private;
