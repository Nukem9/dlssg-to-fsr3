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
#include <FidelityFX/gpu/parallelsort/ffx_parallelsort_resources.h>
#include <FidelityFX/gpu/parallelsort/ffx_parallelsort.h>

typedef enum ParallelSortShaderPermutationOptions
{
    PARALLELSORT_SHADER_PERMUTATION_HAS_PAYLOAD     = (1 << 0),  ///< On means we are sorting keys + payload
    PARALLELSORT_SHADER_PERMUTATION_FORCE_WAVE64    = (1 << 1),  ///< On means we are forcing 64 lane waves
    PARALLELSORT_SHADER_PERMUTATION_ALLOW_FP16      = (1 << 2),  ///< On means we are forcing half precision math

} ParallelSortShaderPermutationOptions;

// Constants for parallel sort dispatches. Must be kept in sync with cbParallelSort in ffx_parallel_sort_callbacks_hlsl.h
typedef FfxParallelSortConstants ParallelSortConstants;

#define FFX_PARALLELSORT_ITERATION_COUNT (32u / FFX_PARALLELSORT_SORT_BITS_PER_PASS)

// FfxParallelSortContext_Private
// The private implementation of the parallel sort context
typedef struct FfxParallelSortContext_Private {

    FfxParallelSortContextDescription   contextDescription;
    FfxUInt32                           effectContextId;
    ParallelSortConstants               constants;
    FfxDevice                           device;
    FfxDeviceCapabilities               deviceCapabilities;
    FfxConstantBuffer                   constantBuffer;

    FfxPipelineState                    pipelineSetupIndirectArgs;
    FfxPipelineState                    pipelineCount[FFX_PARALLELSORT_ITERATION_COUNT];
    FfxPipelineState                    pipelineReduce[FFX_PARALLELSORT_ITERATION_COUNT];
    FfxPipelineState                    pipelineScan[FFX_PARALLELSORT_ITERATION_COUNT];
    FfxPipelineState                    pipelineScanAdd[FFX_PARALLELSORT_ITERATION_COUNT];
    FfxPipelineState                    pipelineScatter[FFX_PARALLELSORT_ITERATION_COUNT];

    FfxResourceInternal                 srvResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_COUNT];
    FfxResourceInternal                 uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_COUNT];

} FfxParallelSortContext_Private;
