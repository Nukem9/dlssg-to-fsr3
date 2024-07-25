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
#include <FidelityFX/gpu/spd/ffx_spd_resources.h>

/// An enumeration of all the permutations that can be passed to the SPD algorithm.
///
/// SPD features are organized through a set of pre-defined compile
/// permutation options that need to be specified. Which shader blob
/// is returned for pipeline creation will be determined by what combination
/// of shader permutations are enabled.
///
/// @ingroup SPD
typedef enum SpdShaderPermutationOptions
{
    SPD_SHADER_PERMUTATION_LINEAR_SAMPLE          = (1 << 0),  ///< Sampling will be done with a linear sampler vs. via load
    SPD_SHADER_PERMUTATION_WAVE_INTEROP_LDS       = (1 << 1),  ///< Wave ops will be done via LDS rather than wave ops
    SPD_SHADER_PERMUTATION_FORCE_WAVE64           = (1 << 2),  ///< doesn't map to a define, selects different table
    SPD_SHADER_PERMUTATION_ALLOW_FP16             = (1 << 3),  ///< Enables fast math computations where possible
    SPD_SHADER_PERMUTATION_DOWNSAMPLE_FILTER_MEAN = (1 << 4),  ///< Get average of input values in SpdReduce
    SPD_SHADER_PERMUTATION_DOWNSAMPLE_FILTER_MIN  = (1 << 5),  ///< Get minimum of input values in SpdReduce
    SPD_SHADER_PERMUTATION_DOWNSAMPLE_FILTER_MAX  = (1 << 6),  ///< Get maximum of input values in SpdReduce
} SpdShaderPermutationOptions;

// Constants for SPD dispatches. Must be kept in sync with cbSPD in ffx_spd_callbacks_hlsl.h
typedef struct SpdConstants
{
    uint32_t mips;
    uint32_t numWorkGroups;
    uint32_t workGroupOffset[2];
    float    invInputSize[2];       // Only used for linear sampling mode
    float    padding[2];

} SpdConstants;

struct FfxSpdContextDescription;
struct FfxDeviceCapabilities;
struct FfxPipelineState;
struct FfxResource;

// FfxSpdContext_Private
// The private implementation of the SPD context.
typedef struct FfxSpdContext_Private
{
    FfxSpdContextDescription    contextDescription;
    FfxUInt32                   effectContextId;
    SpdConstants                constants;
    FfxDevice                   device;
    FfxDeviceCapabilities       deviceCapabilities;
    FfxConstantBuffer           constantBuffer;

    FfxPipelineState            pipelineDownsample;

    FfxResourceInternal srvResources[FFX_SPD_RESOURCE_IDENTIFIER_COUNT];
    FfxResourceInternal uavResources[FFX_SPD_RESOURCE_IDENTIFIER_COUNT];

} FfxSpdContext_Private;
