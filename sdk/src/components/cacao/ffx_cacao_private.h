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
#include <FidelityFX/gpu/cacao/ffx_cacao_resources.h>

struct FfxPipelineState;
struct FfxResource;

typedef enum CacaoShaderPermutationOptions
{
    CACAO_SHADER_PERMUTATION_ALLOW_FP16              = (1 << 0),  ///< Enables fast math computations where possible
    CACAO_SHADER_PERMUTATION_APPLY_SMART             = (1 << 1), 
    CACAO_SHADER_PERMUTATION_FORCE_WAVE64 = (1 << 2),
} CacaoShaderPermutationOptions;

struct FfxCacaoContext_Private
{
    FfxCacaoSettings settings;
    bool               useDownsampledSsao;
    FfxCacaoConstants constants;
    FfxConstantBuffer  constantBuffer;

    FfxUInt32 effectContextId;

    FfxDevice device;
    FfxCacaoContextDescription contextDescription;

    FfxCacaoBufferSizeInfo bufferSizeInfo;
    FfxPipelineState pipelineClearLoadCounter;

    FfxPipelineState pipelinePrepareDownsampledDepths;
    FfxPipelineState pipelinePrepareNativeDepths;
    FfxPipelineState pipelinePrepareDownsampledDepthsAndMips;
    FfxPipelineState pipelinePrepareNativeDepthsAndMips;
    FfxPipelineState pipelinePrepareDownsampledDepthsHalf;
    FfxPipelineState pipelinePrepareNativeDepthsHalf;
    
    FfxPipelineState pipelinePrepareDownsampledNormals;
    FfxPipelineState pipelinePrepareNativeNormals;
    FfxPipelineState pipelinePrepareDownsampledNormalsFromInputNormals;
    FfxPipelineState pipelinePrepareNativeNormalsFromInputNormals;

    FfxPipelineState pipelineGenerateQ[5];

    FfxPipelineState pipelineGenerateImportanceMap;
    FfxPipelineState pipelineProcessImportanceMapA;
    FfxPipelineState pipelineProcessImportanceMapB;

    FfxPipelineState pipelineEdgeSensitiveBlur[8];

    FfxPipelineState pipelineApplyNonSmartHalf;
    FfxPipelineState pipelineApplyNonSmart;
    FfxPipelineState pipelineApply;

    FfxPipelineState pipelineUpscaleBilateral5x5Half;
    FfxPipelineState pipelineUpscaleBilateral5x5Smart;
    FfxPipelineState pipelineUpscaleBilateral5x5NonSmart;

    FfxResourceInternal textures[FFX_CACAO_RESOURCE_IDENTIFIER_COUNT];
};
