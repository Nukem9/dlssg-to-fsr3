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

#include <cstring>     // for memset
#include <cstdlib>     // for _countof
#include <cmath>       // for fabs, abs, sinf, sqrt, etc.
#include <array>

#define FFX_CPU
#include <FidelityFX/host/ffx_dof.h>
#include <FidelityFX/gpu/ffx_core.h>
#include <ffx_object_management.h>

#include "ffx_dof_private.h"

// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t    index;
    wchar_t     name[64];
}ResourceBinding;

static const ResourceBinding srvTextureBindingTable[] =
{
    {FFX_DOF_RESOURCE_IDENTIFIER_INPUT_DEPTH,                  L"r_input_depth"},
    {FFX_DOF_RESOURCE_IDENTIFIER_INPUT_COLOR,                  L"r_input_color"},
    {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_BILAT_COLOR,         L"r_internal_bilat_color"},
    {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_DILATED_RADIUS,      L"r_internal_dilated_radius"},
};

static const ResourceBinding uavTextureBindingTable[] =
{
    {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_BILAT_COLOR_MIP0,    L"rw_internal_bilat_color"},
    {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_RADIUS,              L"rw_internal_radius"},
    {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_DILATED_RADIUS,      L"rw_internal_dilated_radius"},
    {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_NEAR,                L"rw_internal_near"},
    {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_FAR,                 L"rw_internal_far"},
    {FFX_DOF_RESOURCE_IDENTIFIER_OUTPUT_COLOR,                 L"rw_output_color"},
    {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_GLOBALS,             L"rw_internal_globals"},
};

static const ResourceBinding cbResourceBindingTable[] =
{
    {FFX_DOF_CONSTANTBUFFER_IDENTIFIER_DOF,                    L"cbDOF"},
};

static FfxErrorCode patchResourceBindings(FfxPipelineState* inoutPipeline)
{
    for (uint32_t srvIndex = 0; srvIndex < inoutPipeline->srvTextureCount; ++srvIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(srvTextureBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(srvTextureBindingTable[mapIndex].name, inoutPipeline->srvTextureBindings[srvIndex].name))
                break;
        }
        if (mapIndex == _countof(srvTextureBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->srvTextureBindings[srvIndex].resourceIdentifier = srvTextureBindingTable[mapIndex].index;
    }

    for (uint32_t uavIndex = 0; uavIndex < inoutPipeline->uavTextureCount; ++uavIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(uavTextureBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(uavTextureBindingTable[mapIndex].name, inoutPipeline->uavTextureBindings[uavIndex].name))
                break;
        }
        if (mapIndex == _countof(uavTextureBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->uavTextureBindings[uavIndex].resourceIdentifier = uavTextureBindingTable[mapIndex].index;
    }

    for (uint32_t uavIndex = 0; uavIndex < inoutPipeline->uavBufferCount; ++uavIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(uavTextureBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(uavTextureBindingTable[mapIndex].name, inoutPipeline->uavBufferBindings[uavIndex].name))
                break;
        }
        if (mapIndex == _countof(uavTextureBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->uavBufferBindings[uavIndex].resourceIdentifier = uavTextureBindingTable[mapIndex].index;
    }

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

static uint32_t getPipelinePermutationFlags(uint32_t contextFlags, FfxDofPass passId, bool fp16, bool force64)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (contextFlags & FFX_DOF_REVERSE_DEPTH) ? DOF_SHADER_PERMUTATION_REVERSE_DEPTH : 0;
    flags |= (contextFlags & FFX_DOF_OUTPUT_PRE_INIT) ? DOF_SHADER_PERMUTATION_COMBINE_IN_PLACE : 0;
    flags |= (contextFlags & FFX_DOF_DISABLE_RING_MERGE) ? 0 : DOF_SHADER_PERMUTATION_MERGE_RINGS;
    flags |= fp16 ? DOF_SHADER_PERMUTATION_USE_FP16 : 0;
    flags |= force64 ? DOF_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    return flags;
}

static FfxErrorCode createPipelineStates(FfxDofContext_Private* context)
{
    FFX_ASSERT(context);

    FfxSamplerDescription samplers[] = { {FFX_FILTER_TYPE_MINMAGMIP_LINEAR, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_BIND_COMPUTE_SHADER_STAGE},
                                         {FFX_FILTER_TYPE_MINMAGMIP_POINT, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_BIND_COMPUTE_SHADER_STAGE}};

    FfxRootConstantDescription rootConstants[] { {sizeof(DofConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE} };

    FfxPipelineDescription pipelineDescription{};
    pipelineDescription.contextFlags = context->contextDescription.flags;
    pipelineDescription.samplerCount = 2;
    pipelineDescription.samplers = samplers;
    pipelineDescription.rootConstantBufferCount = 1;
    pipelineDescription.rootConstants = rootConstants;

    // Setup a few options used to determine permutation flags
    bool haveShaderModel66 = context->deviceCapabilities.maximumSupportedShaderModel >= FFX_SHADER_MODEL_6_6;
    bool supportedFP16 = context->deviceCapabilities.fp16Supported;
    bool canForceWave64 = false;

    const uint32_t waveLaneCountMin = context->deviceCapabilities.waveLaneCountMin;
    const uint32_t waveLaneCountMax = context->deviceCapabilities.waveLaneCountMax;
    if (waveLaneCountMin <= 64 && waveLaneCountMax >= 64)
        canForceWave64 = haveShaderModel66;
    else
        canForceWave64 = false;

    // Work out what permutation to load.
    uint32_t contextFlags = context->contextDescription.flags;

    // Set up pipeline descriptors (basically RootSignature and binding)
    wcscpy_s(pipelineDescription.name, L"DOF-DOWNSAMPLE-DEPTH");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DOF, FFX_DOF_PASS_DOWNSAMPLE_DEPTH,
        getPipelinePermutationFlags(contextFlags, FFX_DOF_PASS_DOWNSAMPLE_DEPTH, supportedFP16, false),
        &pipelineDescription, context->effectContextId, &context->pipelineDsDepth));
    wcscpy_s(pipelineDescription.name, L"DOF-DOWNSAMPLE-COLOR");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DOF, FFX_DOF_PASS_DOWNSAMPLE_COLOR,
        getPipelinePermutationFlags(contextFlags, FFX_DOF_PASS_DOWNSAMPLE_COLOR, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId, &context->pipelineDsColor));
    wcscpy_s(pipelineDescription.name, L"DOF-DILATE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DOF, FFX_DOF_PASS_DILATE,
        getPipelinePermutationFlags(contextFlags, FFX_DOF_PASS_DILATE, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId, &context->pipelineDilate));
    wcscpy_s(pipelineDescription.name, L"DOF-BLUR");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DOF, FFX_DOF_PASS_BLUR,
        getPipelinePermutationFlags(contextFlags, FFX_DOF_PASS_BLUR, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId, &context->pipelineBlur));
    wcscpy_s(pipelineDescription.name, L"DOF-COMPOSITE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DOF, FFX_DOF_PASS_COMPOSITE,
        getPipelinePermutationFlags(contextFlags, FFX_DOF_PASS_COMPOSITE, supportedFP16, false),
        &pipelineDescription, context->effectContextId, &context->pipelineComposite));

    // For each pipeline: re-route/fix-up IDs based on names
    patchResourceBindings(&context->pipelineDsDepth);
    patchResourceBindings(&context->pipelineDsColor);
    patchResourceBindings(&context->pipelineDilate);
    patchResourceBindings(&context->pipelineBlur);
    patchResourceBindings(&context->pipelineComposite);

    return FFX_OK;
}

static void scheduleDispatch(FfxDofContext_Private* context, const FfxDofDispatchDescription* params, const FfxPipelineState* pipeline, uint32_t dispatchX, uint32_t dispatchY)
{
    FfxGpuJobDescription dispatchJob = {FFX_GPU_JOB_COMPUTE};
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);

    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex)
    {
        const uint32_t currentResourceId = pipeline->srvTextureBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = context->srvResources[currentResourceId];
        dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].name,
                 pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    uint32_t uavEntry = 0;
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex)
    {
        const FfxResourceBinding binding = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex];

        const uint32_t currentResourceId = binding.resourceIdentifier;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].name, binding.name);
#endif
        if (currentResourceId == FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_BILAT_COLOR)
        {
            // Need to map as many resources as we have binding indices
            const FfxResourceInternal currentResource = context->uavResources[FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_BILAT_COLOR];
            const uint32_t            bindEntry       = binding.arrayIndex;

            // Don't over-subscribe mips (default to mip 0 once we've exhausted min mip)
                FfxResourceDescription resDesc = context->contextDescription.backendInterface.fpGetResourceDescription(&context->contextDescription.backendInterface, currentResource);
                dispatchJob.computeJobDescriptor.uavTextures[uavEntry].resource = currentResource;
                dispatchJob.computeJobDescriptor.uavTextures[uavEntry++].mip = (bindEntry < resDesc.mipCount) ? bindEntry : 0;
        }
        else
        {
            const FfxResourceInternal currentResource = context->uavResources[currentResourceId];
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry].resource = currentResource;
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry++].mip = 0;
        }
    }
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavBufferCount; ++currentUnorderedAccessViewIndex)
    {
        const uint32_t currentResourceId = pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].name,
                 pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].name);
#endif

        const FfxResourceInternal currentResource                       = context->uavResources[currentResourceId];
        dispatchJob.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].resource = currentResource;
    }

    dispatchJob.computeJobDescriptor.dimensions[0] = dispatchX;
    dispatchJob.computeJobDescriptor.dimensions[1] = dispatchY;
    dispatchJob.computeJobDescriptor.dimensions[2] = 1;
    dispatchJob.computeJobDescriptor.pipeline      = *pipeline;

#ifdef FFX_DEBUG
    wcscpy_s(dispatchJob.computeJobDescriptor.cbNames[0], pipeline->constantBufferBindings[0].name);
#endif
    dispatchJob.computeJobDescriptor.cbs[0] = context->constantBuffer;


    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchJob);
}

static FfxErrorCode dofDispatch(FfxDofContext_Private* context, const FfxDofDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;

    // Register resources for frame
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->color, context->effectContextId, &context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INPUT_COLOR]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->depth, context->effectContextId, &context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INPUT_DEPTH]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->output, context->effectContextId, &context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_OUTPUT_COLOR]);

    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->color, context->effectContextId, &context->uavResources[FFX_DOF_RESOURCE_IDENTIFIER_INPUT_COLOR]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->depth, context->effectContextId, &context->uavResources[FFX_DOF_RESOURCE_IDENTIFIER_INPUT_DEPTH]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->output, context->effectContextId, &context->uavResources[FFX_DOF_RESOURCE_IDENTIFIER_OUTPUT_COLOR]);


    // dispatch sizes
    uint32_t width = context->contextDescription.resolution.width, height = context->contextDescription.resolution.height;
    uint32_t halfWidth = (width + 1) / 2, halfHeight = (height + 1) / 2;

    uint32_t dsTilesX = (width + 63) / 64;
    uint32_t dsTilesY = (height + 63) / 64;
    uint32_t fullTilesX = (width + 7) / 8;
    uint32_t fullTilesY = (height + 7) / 8;
    uint32_t tilesX = (halfWidth + 7) / 8;
    uint32_t tilesY = (halfHeight + 7) / 8;

    // Constant buffer
    DofConstants cbuffer = {};
    cbuffer.cocScale = params->cocScale;
    cbuffer.cocBias = params->cocBias;
    cbuffer.inputSize[0] = context->contextDescription.resolution.width;
    cbuffer.inputSize[1] = context->contextDescription.resolution.height;
    cbuffer.inputSizeHalf[0] = (cbuffer.inputSize[0] + 1) / 2;
    cbuffer.inputSizeHalf[1] = (cbuffer.inputSize[1] + 1) / 2;
    cbuffer.inputSizeHalfRcp[0] = 1.0f / cbuffer.inputSizeHalf[0];
    cbuffer.inputSizeHalfRcp[1] = 1.0f / cbuffer.inputSizeHalf[1];
    // calculate actual coc limit. Limit in the shader is half res, so * 0.5 this.
    cbuffer.cocLimit = 0.5f * context->contextDescription.cocLimitFactor * context->contextDescription.resolution.height;
    cbuffer.maxRings = context->contextDescription.quality;

    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(&context->contextDescription.backendInterface, 
                                                                               &cbuffer, 
                                                                               sizeof(DofConstants),
                                                                               &context->constantBuffer);
    scheduleDispatch(context, params, &context->pipelineDsDepth, (fullTilesX + 7) / 8, (fullTilesY + 7) / 8);
    scheduleDispatch(context, params, &context->pipelineDsColor, dsTilesX, dsTilesY);
    scheduleDispatch(context, params, &context->pipelineDilate, (fullTilesX + 7) / 8, (fullTilesY + 7) / 8);
    scheduleDispatch(context, params, &context->pipelineBlur, tilesX, tilesY);
    scheduleDispatch(context, params, &context->pipelineComposite, tilesX, tilesY);

    // Execute all the work for the frame
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    // Release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    return FFX_OK;
}

static FfxErrorCode dofCreate(FfxDofContext_Private* context, const FfxDofContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxDofContext_Private));
    context->device = contextDescription->backendInterface.device;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxDofContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);

    context->constantBuffer.num32BitEntries = sizeof(DofConstants) / sizeof(uint32_t);

    // Create the context.
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_DOF, nullptr, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Call out for device caps.
    FFX_VALIDATE(context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &context->deviceCapabilities));

    // Create the intermediate resources
    uint32_t halfWidth = (contextDescription->resolution.width + 1) / 2;
    uint32_t halfHeight = (contextDescription->resolution.height + 1) / 2;
    uint32_t tileWidth = (halfWidth + 3) / 4;
    uint32_t tileHeight = (halfHeight + 4) / 4;
    bool is16bit = context->deviceCapabilities.fp16Supported;
    FfxInternalResourceDescription internalSurfaceDesc[] = {
        {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_BILAT_COLOR,
         L"DOF_InternalBilatColor",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         is16bit ? FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT : FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT,
         halfWidth,
         halfHeight,
         FFX_DOF_INTERNAL_BILAT_MIP_COUNT,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},
        {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_RADIUS,
         L"DOF_InternalRadius",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         is16bit ? FFX_SURFACE_FORMAT_R16G16_FLOAT : FFX_SURFACE_FORMAT_R32G32_FLOAT,
         tileWidth,
         tileHeight,
         1,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},
        {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_DILATED_RADIUS,
         L"DOF_InternalDilatedRadius",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         is16bit ? FFX_SURFACE_FORMAT_R16G16_FLOAT : FFX_SURFACE_FORMAT_R32G32_FLOAT,
         tileWidth,
         tileHeight,
         1,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},
        {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_NEAR,
         L"DOF_InternalNear",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         is16bit ? FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT : FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT,
         halfWidth,
         halfHeight,
         1,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},
        {FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_FAR,
         L"DOF_InternalFar",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         is16bit ? FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT : FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT,
         halfWidth,
         halfHeight,
         1,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},
    };

    // Clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    for (int32_t currentSurfaceIndex = 0; currentSurfaceIndex < FFX_ARRAY_ELEMENTS(internalSurfaceDesc); ++currentSurfaceIndex) {
        const FfxInternalResourceDescription* currentSurfaceDescription = &internalSurfaceDesc[currentSurfaceIndex];
        const FfxResourceDescription resourceDescription = { FFX_RESOURCE_TYPE_TEXTURE2D, currentSurfaceDescription->format,
            currentSurfaceDescription->width, currentSurfaceDescription->height, 1, currentSurfaceDescription->mipCount, currentSurfaceDescription->flags, currentSurfaceDescription->usage };
        const FfxResourceStates initialState = FFX_RESOURCE_STATE_UNORDERED_ACCESS;
        const FfxCreateResourceDescription    createResourceDescription = {FFX_HEAP_TYPE_DEFAULT,
                                                                           resourceDescription,
                                                                           initialState,
                                                                           currentSurfaceDescription->name,
                                                                           currentSurfaceDescription->id,
                                                                           currentSurfaceDescription->initData};

        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface, &createResourceDescription, context->effectContextId, &context->srvResources[currentSurfaceDescription->id]));
    }
    // globals buffer
    {
        const FfxResourceDescription resourceDescription = { FFX_RESOURCE_TYPE_BUFFER, FFX_SURFACE_FORMAT_UNKNOWN,
            4, 4, 0, 0, FFX_RESOURCE_FLAGS_NONE, FFX_RESOURCE_USAGE_UAV };
        const FfxCreateResourceDescription createResourceDescription = {FFX_HEAP_TYPE_DEFAULT,
                                                                        resourceDescription,
                                                                        FFX_RESOURCE_STATE_UNORDERED_ACCESS,
                                                                        L"DOF_InternalGlobals",
                                                                        FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_GLOBALS,
                                                                        {FFX_RESOURCE_INIT_DATA_TYPE_VALUE, sizeof(uint32_t), 0}};

        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface, &createResourceDescription, context->effectContextId, &context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_GLOBALS]));
    }

    // copy resources to uavResrouces list
    memcpy(context->uavResources, context->srvResources, sizeof(context->srvResources));

    // Create shaders on initialize.
    FFX_VALIDATE(createPipelineStates(context));

    return FFX_OK;
}

static FfxErrorCode dofRelease(FfxDofContext_Private* context)
{
    FFX_ASSERT(context);
    
    // Release all pipelines
    for (FfxPipelineState* pipe : {&context->pipelineDsDepth, &context->pipelineDsColor, &context->pipelineDilate, &context->pipelineBlur, &context->pipelineComposite})
    {
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, pipe, context->effectContextId);
    }

    // Unregister resources not created internally
    context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INPUT_COLOR] = { FFX_DOF_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INPUT_DEPTH] = { FFX_DOF_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_OUTPUT_COLOR] = { FFX_DOF_RESOURCE_IDENTIFIER_NULL };

    // Release internal resources
    ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_BILAT_COLOR], context->effectContextId);
    ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_RADIUS], context->effectContextId);
    ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_DILATED_RADIUS], context->effectContextId);
    ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_NEAR], context->effectContextId);
    ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_FAR], context->effectContextId);
    ffxSafeReleaseCopyResource(&context->contextDescription.backendInterface, context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_GLOBALS], context->effectContextId);
    ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[FFX_DOF_RESOURCE_IDENTIFIER_INTERNAL_GLOBALS], context->effectContextId);

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

FfxErrorCode ffxDofContextCreate(FfxDofContext* context, const FfxDofContextDescription* contextDescription)
{
    // Zero context memory
    memset(context, 0, sizeof(FfxDofContext));

    // Check pointers are valid.
    FFX_RETURN_ON_ERROR(
        context,
        FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(
        contextDescription,
        FFX_ERROR_INVALID_POINTER);

    // Validate that all callbacks are set for the interface
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetSDKVersion, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetDeviceCapabilities, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpCreateBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpDestroyBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);

    // If a scratch buffer is declared, then we must have a size
    if (contextDescription->backendInterface.scratchBuffer) {

        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);
    }

    // Ensure the context is large enough for the internal context.
    FFX_STATIC_ASSERT(sizeof(FfxDofContext) >= sizeof(FfxDofContext_Private));

    // create the context.
    FfxDofContext_Private* contextPrivate = new(context) FfxDofContext_Private;
    return dofCreate(contextPrivate, contextDescription);
}

FfxErrorCode ffxDofContextDestroy(FfxDofContext* context)
{
    FFX_RETURN_ON_ERROR(
        context,
        FFX_ERROR_INVALID_POINTER);

    // Destroy the context.
    FfxDofContext_Private* contextPrivate = reinterpret_cast<FfxDofContext_Private*>(context);
    return dofRelease(contextPrivate);
}

FfxErrorCode ffxDofContextDispatch(FfxDofContext* context, const FfxDofDispatchDescription* dispatchDescription)
{
    // check pointers are valid
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchDescription, FFX_ERROR_INVALID_POINTER);

    FfxDofContext_Private* contextPrivate = reinterpret_cast<FfxDofContext_Private*>(context);

    FFX_RETURN_ON_ERROR(
        contextPrivate->device,
        FFX_ERROR_NULL_DEVICE);

    return dofDispatch(contextPrivate, dispatchDescription);
}

FFX_API FfxVersionNumber ffxDofGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_DOF_VERSION_MAJOR, FFX_DOF_VERSION_MINOR, FFX_DOF_VERSION_PATCH);
}
