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

#include <string.h>     // for memset
#include <stdlib.h>     // for _countof

#include <FidelityFX/host/ffx_parallelsort.h>
#include "ffx_parallelsort_private.h"
#include <ffx_object_management.h>

// lists to map shader resource bind point name to resource identifier
typedef struct ResourceBinding
{
    uint32_t    index;
    wchar_t     name[64];
}ResourceBinding;

static const ResourceBinding uavBufferBindingTable[] =
{
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INDIRECT_COUNT_SCATTER_ARGS_BUFFER,   L"rw_count_scatter_args"},
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INDIRECT_REDUCE_SCAN_ARGS_BUFER,      L"rw_reduce_scan_args"},
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SUM_TABLE,                            L"rw_sum_table"},
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_REDUCE_TABLE,                         L"rw_reduce_table"},
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_SOURCE,                          L"rw_scan_source"},
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_DST,                             L"rw_scan_dest"},
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_SCRATCH,                         L"rw_scan_scratch"},
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_KEY_SRC,                              L"rw_source_keys"},
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_KEY_DST,                              L"rw_dest_keys"},
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_SRC,                          L"rw_source_payloads"},
    {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_DST,                          L"rw_dest_payloads"},
};

static const ResourceBinding cbResourceBindingTable[] =
{
    {FFX_PARALLELSORT_CONSTANTBUFFER_IDENTIFIER_PARALLEL_SORT,                  L"cbParallelSort"},
};

static FfxErrorCode patchResourceBindings(FfxPipelineState* inoutPipeline)
{
    // Buffer uavs
    for (uint32_t uavIndex = 0; uavIndex < inoutPipeline->uavBufferCount; ++uavIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(uavBufferBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(uavBufferBindingTable[mapIndex].name, inoutPipeline->uavBufferBindings[uavIndex].name))
                break;
        }
        if (mapIndex == _countof(uavBufferBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->uavBufferBindings[uavIndex].resourceIdentifier = uavBufferBindingTable[mapIndex].index;
    }

    // Constant buffers
    for (uint32_t cbIndex = 0; cbIndex < inoutPipeline->constCount; ++cbIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(cbResourceBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(cbResourceBindingTable[mapIndex].name, inoutPipeline->constantBufferBindings[cbIndex].name))
                break;
        }
        if (mapIndex == _countof(cbResourceBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->constantBufferBindings[cbIndex].resourceIdentifier = cbResourceBindingTable[mapIndex].index;
    }

    return FFX_OK;
}

static uint32_t getPipelinePermutationFlags(uint32_t contextFlags, FfxParallelSortPass passId, bool fp16, bool force64)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (contextFlags & FFX_PARALLELSORT_PAYLOAD_SORT) ? PARALLELSORT_SHADER_PERMUTATION_HAS_PAYLOAD : 0;
    flags |= (force64) ? PARALLELSORT_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (fp16) ? PARALLELSORT_SHADER_PERMUTATION_ALLOW_FP16 : 0;   // Currently ignored
    return flags;
}

static FfxErrorCode createPipelineStates(FfxParallelSortContext_Private* context)
{
    FFX_ASSERT(context);
    
    FfxPipelineDescription pipelineDescription = {};
    pipelineDescription.contextFlags = context->contextDescription.flags;

    // No samplers
    pipelineDescription.samplerCount = 0;

    // Root Constants
    pipelineDescription.rootConstantBufferCount = 1;
    FfxRootConstantDescription rootConstantDesc = { sizeof(ParallelSortConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE };
    pipelineDescription.rootConstants = &rootConstantDesc;

    // Query device capabilities
    FfxDevice             device = context->contextDescription.backendInterface.device;
    FfxDeviceCapabilities capabilities;
    context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &capabilities);

    // Setup a few options used to determine permutation flags
    bool haveShaderModel66 = capabilities.maximumSupportedShaderModel >= FFX_SHADER_MODEL_6_6;
    bool supportedFP16 = capabilities.fp16Supported;
    bool canForceWave64 = false;

    const uint32_t waveLaneCountMin = capabilities.waveLaneCountMin;
    const uint32_t waveLaneCountMax = capabilities.waveLaneCountMax;
    if (waveLaneCountMin <= 64 && waveLaneCountMax >= 64)
        canForceWave64 = haveShaderModel66;
    else
        canForceWave64 = false;

    // Work out what permutation to load.
    uint32_t contextFlags = context->contextDescription.flags;

    // Set up pipeline descriptors (basically RootSignature and binding)
    if (context->contextDescription.flags & FFX_PARALLELSORT_INDIRECT_SORT) 
    {
        wcscpy_s(pipelineDescription.name, L"PARALLELSORT-SETUPDINDIRECTARGS");
        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_PARALLEL_SORT, FFX_PARALLELSORT_PASS_SETUP_INDIRECT_ARGS,
            getPipelinePermutationFlags(contextFlags, FFX_PARALLELSORT_PASS_SETUP_INDIRECT_ARGS, supportedFP16, canForceWave64),
            &pipelineDescription, context->effectContextId, &context->pipelineSetupIndirectArgs));
    }

    // Need to create a pipeline for each iteration as resources views and constants are tied to pipelines in the backends
    // and cycling them could thrash the memory associated with them
    for (uint32_t i = 0; i < FFX_PARALLELSORT_ITERATION_COUNT; ++i)
    {
        // no indirect on this pipeline
        pipelineDescription.indirectWorkload = 0;
        wcscpy_s(pipelineDescription.name, L"PARALLELSORT-SCAN");
        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_PARALLEL_SORT, FFX_PARALLELSORT_PASS_SCAN,
            getPipelinePermutationFlags(contextFlags, FFX_PARALLELSORT_PASS_SCAN, supportedFP16, canForceWave64),
            &pipelineDescription, context->effectContextId, &context->pipelineScan[i]));

        // Setup the indirect argument stride if we are doing indirect execution for the rest
        if (context->contextDescription.flags & FFX_PARALLELSORT_INDIRECT_SORT) {
            pipelineDescription.indirectWorkload = 1;
        }

        wcscpy_s(pipelineDescription.name, L"PARALLELSORT-SUM");
        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_PARALLEL_SORT, FFX_PARALLELSORT_PASS_SUM,
            getPipelinePermutationFlags(contextFlags, FFX_PARALLELSORT_PASS_SUM, supportedFP16, canForceWave64),
            &pipelineDescription, context->effectContextId, &context->pipelineCount[i]));

        wcscpy_s(pipelineDescription.name, L"PARALLELSORT-REDUCE");
        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_PARALLEL_SORT, FFX_PARALLELSORT_PASS_REDUCE,
            getPipelinePermutationFlags(contextFlags, FFX_PARALLELSORT_PASS_REDUCE, supportedFP16, canForceWave64),
            &pipelineDescription, context->effectContextId, &context->pipelineReduce[i]));

        wcscpy_s(pipelineDescription.name, L"PARALLELSORT-SCAN_ADD");
        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_PARALLEL_SORT, FFX_PARALLELSORT_PASS_SCAN_ADD,
            getPipelinePermutationFlags(contextFlags, FFX_PARALLELSORT_PASS_SCAN_ADD, supportedFP16, canForceWave64),
            &pipelineDescription, context->effectContextId, &context->pipelineScanAdd[i]));

        wcscpy_s(pipelineDescription.name, L"PARALLELSORT-SCATTER");
        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_PARALLEL_SORT, FFX_PARALLELSORT_PASS_SCATTER,
            getPipelinePermutationFlags(contextFlags, FFX_PARALLELSORT_PASS_SCATTER, supportedFP16, canForceWave64),
            &pipelineDescription, context->effectContextId, &context->pipelineScatter[i]));
    }
    
    // For each pipeline: re-route/fix-up IDs based on names
    if (context->contextDescription.flags & FFX_PARALLELSORT_INDIRECT_SORT) {
        patchResourceBindings(&context->pipelineSetupIndirectArgs);
    }
    for (uint32_t i = 0; i < FFX_PARALLELSORT_ITERATION_COUNT; ++i)
    {
        patchResourceBindings(&context->pipelineCount[i]);
        patchResourceBindings(&context->pipelineReduce[i]);
        patchResourceBindings(&context->pipelineScan[i]);
        patchResourceBindings(&context->pipelineScanAdd[i]);
        patchResourceBindings(&context->pipelineScatter[i]);
    }    
    
    return FFX_OK;
}

static FfxErrorCode parallelSortCreate(FfxParallelSortContext_Private* context, const FfxParallelSortContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxParallelSortContext_Private));
    context->device = contextDescription->backendInterface.device;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxParallelSortContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);

    // Setup constant buffer sizes.
    context->constantBuffer.num32BitEntries = sizeof(ParallelSortConstants) / sizeof(uint32_t);

    // Create the context
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_PARALLEL_SORT, nullptr, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Call out for device caps.
    errorCode = context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &context->deviceCapabilities);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Calculate the size of the scratch buffers needed for radix sort
    uint32_t scratchBufferSize;
    uint32_t reducedScratchBufferSize;
    ffxParallelSortCalculateScratchResourceSize(contextDescription->maxEntries, scratchBufferSize, reducedScratchBufferSize);

    // declare internal resources needed
    const FfxInternalResourceDescription internalResourceDescs[] = {
        {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_KEY_SCRATCH_BUFFER,
         L"ParallelSort_SortScratchBuffer",
         FFX_RESOURCE_TYPE_BUFFER,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_UNKNOWN,
         contextDescription->maxEntries * sizeof(uint32_t),
         sizeof(uint32_t),
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_SCRATCH_BUFFER,
         L"ParallelSort_PayloadScratchBuffer",
         FFX_RESOURCE_TYPE_BUFFER,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_UNKNOWN,
         contextDescription->maxEntries * sizeof(uint32_t),
         sizeof(uint32_t),
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCRATCH_BUFFER,
         L"ParallelSort_ScratchBuffer",
         FFX_RESOURCE_TYPE_BUFFER,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_UNKNOWN,
         scratchBufferSize,
         sizeof(uint32_t),
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_REDUCED_SCRATCH_BUFFER,
         L"ParallelSort_ReducedScratchBuffer",
         FFX_RESOURCE_TYPE_BUFFER,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_UNKNOWN,
         reducedScratchBufferSize,
         sizeof(uint32_t),
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INDIRECT_COUNT_SCATTER_ARGS_BUFFER,
         L"ParallelSort_IndirectCountScatterArgsBuffer",
         FFX_RESOURCE_TYPE_BUFFER,
         (FfxResourceUsage)(FFX_RESOURCE_USAGE_UAV | FFX_RESOURCE_USAGE_INDIRECT),
         FFX_SURFACE_FORMAT_UNKNOWN,
         sizeof(uint32_t) * 3,
         sizeof(uint32_t),
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INDIRECT_REDUCE_SCAN_ARGS_BUFER,
         L"ParallelSort_IndirectReduceScanArgsBuffer",
         FFX_RESOURCE_TYPE_BUFFER,
         (FfxResourceUsage)(FFX_RESOURCE_USAGE_UAV | FFX_RESOURCE_USAGE_INDIRECT),
         FFX_SURFACE_FORMAT_UNKNOWN,
         sizeof(uint32_t) * 3,
         sizeof(uint32_t),
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},
    };

    // Clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    // Don't create indirect resources if not needed
    int32_t numResourcesToCreate = contextDescription->flags & FFX_PARALLELSORT_INDIRECT_SORT ? FFX_ARRAY_ELEMENTS(internalResourceDescs) : FFX_ARRAY_ELEMENTS(internalResourceDescs) - 2;

    for (int32_t currentResourceId = 0; currentResourceId < numResourcesToCreate; ++currentResourceId) {

        const FfxInternalResourceDescription* currentSurfaceDescription = &internalResourceDescs[currentResourceId];
        const FfxResourceType resourceType = internalResourceDescs[currentResourceId].type;
        const FfxResourceDescription resourceDescription = { resourceType, currentSurfaceDescription->format, currentSurfaceDescription->width, currentSurfaceDescription->height, 0, 0, FFX_RESOURCE_FLAGS_NONE, currentSurfaceDescription->usage };
        const FfxResourceStates initialState = FFX_RESOURCE_STATE_UNORDERED_ACCESS;
        const FfxCreateResourceDescription    createResourceDescription = {FFX_HEAP_TYPE_DEFAULT,
                                                                           resourceDescription,
                                                                           initialState,
                                                                           currentSurfaceDescription->name,
                                                                           currentSurfaceDescription->id,
                                                                           currentSurfaceDescription->initData};
        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface, &createResourceDescription, context->effectContextId, &context->srvResources[currentSurfaceDescription->id]));
    }

    // And copy resources to uavResrouces list
    memcpy(context->uavResources, context->srvResources, sizeof(context->srvResources));

    // Create shaders on initialize.
    errorCode = createPipelineStates(context);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    return FFX_OK;
}

static void scheduleDispatch(FfxParallelSortContext_Private* pContext, const FfxParallelSortDispatchDescription* pDescription, 
                                const FfxPipelineState* pPipeline, uint32_t dispatchX, uint32_t dispatchY, uint32_t dispatchZ)
{
    FfxGpuJobDescription dispatchJob = {FFX_GPU_JOB_COMPUTE};
    wcscpy_s(dispatchJob.jobLabel, pPipeline->name);

    // Buffer uavs
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pPipeline->uavBufferCount; ++currentUnorderedAccessViewIndex) {

        const uint32_t currentResourceId = pPipeline->uavBufferBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = pContext->uavResources[currentResourceId];
        dispatchJob.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].name,
                 pPipeline->uavBufferBindings[currentUnorderedAccessViewIndex].name);
#endif
    }

    dispatchJob.computeJobDescriptor.dimensions[0] = dispatchX;
    dispatchJob.computeJobDescriptor.dimensions[1] = dispatchY;
    dispatchJob.computeJobDescriptor.dimensions[2] = dispatchZ;
    dispatchJob.computeJobDescriptor.pipeline      = *pPipeline;

#ifdef FFX_DEBUG
    wcscpy_s(dispatchJob.computeJobDescriptor.cbNames[0], pPipeline->constantBufferBindings[0].name);
#endif
    dispatchJob.computeJobDescriptor.cbs[0] = pContext->constantBuffer;

    
    pContext->contextDescription.backendInterface.fpScheduleGpuJob(&pContext->contextDescription.backendInterface, &dispatchJob);
}

static void scheduleIndirectDispatch(FfxParallelSortContext_Private* pContext, const FfxParallelSortDispatchDescription* pDescription,
    const FfxPipelineState* pPipeline, FfxResourceInternal cmdArgument, uint32_t cmdOffset)
{
    FfxComputeJobDescription jobDescriptor = {};

    // Buffer uavs
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pPipeline->uavBufferCount; ++currentUnorderedAccessViewIndex) {

        const uint32_t currentResourceId = pPipeline->uavBufferBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = pContext->uavResources[currentResourceId];
        jobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].name, pPipeline->uavBufferBindings[currentUnorderedAccessViewIndex].name);
#endif
    }

    jobDescriptor.cmdArgument = cmdArgument;
    jobDescriptor.cmdArgumentOffset = cmdOffset;
    jobDescriptor.pipeline = *pPipeline;

    // Copy constants
#ifdef FFX_DEBUG
    wcscpy_s(jobDescriptor.cbNames[0], pPipeline->constantBufferBindings[0].name);
#endif
    jobDescriptor.cbs[0] = pContext->constantBuffer;

    FfxGpuJobDescription dispatchJob = { FFX_GPU_JOB_COMPUTE };
    wcscpy_s(dispatchJob.jobLabel, pPipeline->name);
    dispatchJob.computeJobDescriptor = jobDescriptor;

    pContext->contextDescription.backendInterface.fpScheduleGpuJob(&pContext->contextDescription.backendInterface, &dispatchJob);
}

static FfxErrorCode parallelSortDispatch(FfxParallelSortContext_Private* pContext, const FfxParallelSortDispatchDescription* pDescription)
{
    // take a short cut to the command list
    FfxCommandList commandList = pDescription->commandList;

    // Register resources for frame
    pContext->contextDescription.backendInterface.fpRegisterResource(&pContext->contextDescription.backendInterface, &pDescription->keyBuffer, pContext->effectContextId, &pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INPUT_KEY_BUFFER]);
    if (pContext->contextDescription.flags & FFX_PARALLELSORT_PAYLOAD_SORT) {
        pContext->contextDescription.backendInterface.fpRegisterResource(&pContext->contextDescription.backendInterface, &pDescription->payloadBuffer, pContext->effectContextId, &pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INPUT_PAYLOAD_BUFFER]);
    }
    
    // Initialize constants for the sort job
    ParallelSortConstants constants;
    memset(&constants, 0, sizeof(ParallelSortConstants));

    uint32_t numThreadGroupsToRun;
    uint32_t numReducedThreadGroupsToRun;
    ffxParallelSortSetConstantAndDispatchData(pDescription->numKeysToSort, FFX_PARALLELSORT_MAX_THREADGROUPS_TO_RUN, 
                                                constants, numThreadGroupsToRun, numReducedThreadGroupsToRun);

    // If we are doing indirect dispatch, schedule a job to setup the argument buffers for dispatch
    if (pContext->contextDescription.flags & FFX_PARALLELSORT_INDIRECT_SORT)
    {
        pContext->contextDescription.backendInterface.fpStageConstantBufferDataFunc(&pContext->contextDescription.backendInterface, 
                                                                                    &constants, 
                                                                                    sizeof(ParallelSortConstants), 
                                                                                    &pContext->constantBuffer);
        scheduleDispatch(pContext, pDescription, &pContext->pipelineSetupIndirectArgs, 1, 1, 1);
    }

    uint32_t srcKeyResource = FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INPUT_KEY_BUFFER;
    uint32_t dstKeyResource = FFX_PARALLELSORT_RESOURCE_IDENTIFIER_KEY_SCRATCH_BUFFER;

    uint32_t srcPayloadResource = FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INPUT_PAYLOAD_BUFFER;
    uint32_t dstPayloadResource = FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_SCRATCH_BUFFER;

    // Execute the sort algorithm in 4-bit increments
    constants.shift = 0;
    for (uint32_t i = 0; constants.shift < 32; constants.shift += FFX_PARALLELSORT_SORT_BITS_PER_PASS, ++i)
    {
        // Update the constant buffer
        pContext->contextDescription.backendInterface.fpStageConstantBufferDataFunc(
            &pContext->contextDescription.backendInterface, &constants, sizeof(ParallelSortConstants), &pContext->constantBuffer);

        // Sort - Sum Pass
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_KEY_SRC] = pContext->uavResources[srcKeyResource];
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SUM_TABLE] = pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCRATCH_BUFFER];
        if (pContext->contextDescription.flags & FFX_PARALLELSORT_INDIRECT_SORT) {
            scheduleIndirectDispatch(pContext, pDescription, &pContext->pipelineCount[i], pContext->srvResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INDIRECT_COUNT_SCATTER_ARGS_BUFFER], 0);
        }
        else {
            scheduleDispatch(pContext, pDescription, &pContext->pipelineCount[i], numThreadGroupsToRun, 1, 1);
        }          

        // Sort - Reduce Pass
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_REDUCE_TABLE] = pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_REDUCED_SCRATCH_BUFFER];
        if (pContext->contextDescription.flags & FFX_PARALLELSORT_INDIRECT_SORT) {
            scheduleIndirectDispatch(pContext, pDescription, &pContext->pipelineReduce[i], pContext->srvResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INDIRECT_REDUCE_SCAN_ARGS_BUFER], 0);
        }
        else {
            scheduleDispatch(pContext, pDescription, &pContext->pipelineReduce[i], numReducedThreadGroupsToRun, 1, 1);
        }

        // Sort - Scan
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_SOURCE] = pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_REDUCED_SCRATCH_BUFFER];
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_DST] = pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_REDUCED_SCRATCH_BUFFER];
        scheduleDispatch(pContext, pDescription, &pContext->pipelineScan[i], 1, 1, 1);
        
        // Sort - Scan Add
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_SOURCE] = pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCRATCH_BUFFER];
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_DST] = pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCRATCH_BUFFER];
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_SCRATCH] = pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_REDUCED_SCRATCH_BUFFER];
        if (pContext->contextDescription.flags & FFX_PARALLELSORT_INDIRECT_SORT) {
            scheduleIndirectDispatch(pContext, pDescription, &pContext->pipelineScanAdd[i], pContext->srvResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INDIRECT_REDUCE_SCAN_ARGS_BUFER], 0);
        }
        else {
            scheduleDispatch(pContext, pDescription, &pContext->pipelineScanAdd[i], numReducedThreadGroupsToRun, 1, 1);
        }

        // Sort - Scatter
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_KEY_SRC] = pContext->uavResources[srcKeyResource];
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_KEY_DST] = pContext->uavResources[dstKeyResource];
        pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SUM_TABLE] = pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCRATCH_BUFFER];
        if (pContext->contextDescription.flags & FFX_PARALLELSORT_PAYLOAD_SORT) 
        {
            pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_SRC] = pContext->uavResources[srcPayloadResource];
            pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_DST] = pContext->uavResources[dstPayloadResource];
        }
        else
        {
            pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_SRC] = pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_NULL];
            pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_DST] = pContext->uavResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_NULL];
        }
        if (pContext->contextDescription.flags & FFX_PARALLELSORT_INDIRECT_SORT) {
            scheduleIndirectDispatch(pContext, pDescription, &pContext->pipelineScatter[i], pContext->srvResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INDIRECT_COUNT_SCATTER_ARGS_BUFFER], 0);
        }
        else {
            scheduleDispatch(pContext, pDescription, &pContext->pipelineScatter[i], numThreadGroupsToRun, 1, 1);
        }

        // Swap
        uint32_t temp = dstKeyResource;
        dstKeyResource = srcKeyResource;
        srcKeyResource = temp;

        if (pContext->contextDescription.flags & FFX_PARALLELSORT_PAYLOAD_SORT) 
        {
            temp = dstPayloadResource;
            dstPayloadResource = srcPayloadResource;
            srcPayloadResource = temp;
        }
    }

    // Execute all the work for the frame
    pContext->contextDescription.backendInterface.fpExecuteGpuJobs(&pContext->contextDescription.backendInterface, commandList, pContext->effectContextId);

    // Release dynamic resources
    pContext->contextDescription.backendInterface.fpUnregisterResources(&pContext->contextDescription.backendInterface, commandList, pContext->effectContextId);

    return FFX_OK;
}

static FfxErrorCode parallelSortRelease(FfxParallelSortContext_Private* context)
{
    FFX_ASSERT(context);

    // Release all pipelines
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineSetupIndirectArgs, context->effectContextId);
    for (uint32_t i = 0; i < FFX_PARALLELSORT_ITERATION_COUNT; ++i)
    {
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCount[i], context->effectContextId);
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineReduce[i], context->effectContextId);
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineScan[i], context->effectContextId);
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineScanAdd[i], context->effectContextId);
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineScatter[i], context->effectContextId);
    }
    
    // Unregister resources not created internally
    context->srvResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INPUT_KEY_BUFFER]      = { FFX_PARALLELSORT_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INPUT_PAYLOAD_BUFFER]   = { FFX_PARALLELSORT_RESOURCE_IDENTIFIER_NULL };
    
    // release internal resources
    for (int32_t currentResourceIndex = 0; currentResourceIndex < FFX_PARALLELSORT_RESOURCE_IDENTIFIER_COUNT; ++currentResourceIndex) {

        ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[currentResourceIndex], context->effectContextId);
    }

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

FfxErrorCode ffxParallelSortContextCreate(FfxParallelSortContext* context, const FfxParallelSortContextDescription* contextDescription)
{
    // Check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(contextDescription, FFX_ERROR_INVALID_POINTER);

    // Zero context memory.
    memset(context, 0, sizeof(FfxParallelSortContext));

    // Validate that all callbacks are set for the interface
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetSDKVersion, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetDeviceCapabilities, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpCreateBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpDestroyBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE)

    // If a scratch buffer is declared, then we must have a size
    if (contextDescription->backendInterface.scratchBuffer) {

        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);
    }
    
    // Ensure public context is always larger (or equal) to private.
    FFX_STATIC_ASSERT(sizeof(FfxParallelSortContext) >= sizeof(FfxParallelSortContext_Private));

    // create the context.
    FfxParallelSortContext_Private* contextPrivate = (FfxParallelSortContext_Private*)(context);
    const FfxErrorCode errorCode = parallelSortCreate(contextPrivate, contextDescription);

    return errorCode;
}

FfxErrorCode ffxParallelSortContextDispatch(FfxParallelSortContext* pContext, const FfxParallelSortDispatchDescription* pDispatchDescription)
{
    // check pointers are valid.
    FFX_RETURN_ON_ERROR(pContext, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(pDispatchDescription, FFX_ERROR_INVALID_POINTER);

    FfxParallelSortContext_Private* pContextPrivate = (FfxParallelSortContext_Private*)pContext;
    const FfxErrorCode errorCode = parallelSortDispatch(pContextPrivate, pDispatchDescription);
    return FFX_OK;
}

FfxErrorCode ffxParallelSortContextDestroy(FfxParallelSortContext* pContext)
{
    FFX_RETURN_ON_ERROR(pContext, FFX_ERROR_INVALID_POINTER);

    // Destroy the context.
    FfxParallelSortContext_Private* contextPrivate = (FfxParallelSortContext_Private*)(pContext);
    const FfxErrorCode errorCode = parallelSortRelease(contextPrivate);

    return errorCode;
}

FFX_API FfxVersionNumber ffxParallelSortGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_PARALLELSORT_VERSION_MAJOR, FFX_PARALLELSORT_VERSION_MINOR, FFX_PARALLELSORT_VERSION_PATCH);
}
