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
#include <FidelityFX/gpu/lpm/ffx_lpm_resources.h>

/// An enumeration of all the permutations that can be passed to the LPM algorithm.
///
/// LPM features are organized through a set of pre-defined compile
/// permutation options that need to be specified. Which shader blob
/// is returned for pipeline creation will be determined by what combination
/// of shader permutations are enabled.
///
/// @ingroup LPM
typedef enum LpmShaderPermutationOptions
{
    LPM_SHADER_PERMUTATION_FORCE_WAVE64             = (1 << 1),  ///< doesn't map to a define, selects different table
    LPM_SHADER_PERMUTATION_ALLOW_FP16               = (1 << 2),  ///< Enables fast math computations where possible
} LpmShaderPermutationOptions;

// Constants for LPM dispatches. Must be kept in sync with cbLPM in ffx_lpm_callbacks_hlsl.h
typedef struct LpmConstants
{
    FfxUInt32 ctl[24 * 4];  // Store data from LPMSetup call
    FfxUInt32 shoulder;     // Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).
    FfxUInt32 con;          // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
    FfxUInt32 soft;         // Use soft gamut mapping.
    FfxUInt32 con2;         // Use last RGB conversion matrix.
    FfxUInt32 clip;         // Use clipping in last conversion matrix.
    FfxUInt32 scaleOnly;    // Scale only for last conversion matrix (used for 709 HDR to scRGB).
    FfxUInt32 displayMode;  // Display mode of monitor
    FfxUInt32 pad;          // Struct padding
} LpmConstants;

struct FfxLpmContextDescription;
struct FfxDeviceCapabilities;
struct FfxPipelineState;
struct FfxResource;

// FfxLpmContext_Private
// The private implementation of the LPM context.
typedef struct FfxLpmContext_Private
{
    FfxLpmContextDescription    contextDescription;
    FfxUInt32                   effectContextId;
    LpmConstants                constants;
    FfxDevice                   device;
    FfxDeviceCapabilities       deviceCapabilities;
    FfxConstantBuffer           constantBuffer;

    FfxPipelineState            pipelineLPMFilter;

    FfxResourceInternal srvResources[FFX_LPM_RESOURCE_IDENTIFIER_COUNT];
    FfxResourceInternal uavResources[FFX_LPM_RESOURCE_IDENTIFIER_COUNT];

} FfxSpdContext_Private;
