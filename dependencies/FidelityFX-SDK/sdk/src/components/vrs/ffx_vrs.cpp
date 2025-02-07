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
#include <cmath>        // for fabs, abs, sinf, sqrt, etc.

#ifdef __clang__
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#include <FidelityFX/host/ffx_vrs.h>
#include <FidelityFX/gpu/ffx_core.h>
#define FFX_CPP
#include <FidelityFX/gpu/vrs/ffx_variable_shading.h>
#include <ffx_object_management.h>

#include "ffx_vrs_private.h"


// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t index;
    wchar_t  name[64];
} ResourceBinding;

static const ResourceBinding srvTextureBindingTable[] = {
    {FFX_VRS_RESOURCE_IDENTIFIER_INPUT_COLOR, L"r_input_color"},
    {FFX_VRS_RESOURCE_IDENTIFIER_INPUT_MOTIONVECTORS, L"r_input_velocity"},
};

static const ResourceBinding uavTextureBindingTable[] = {
    {FFX_VRS_RESOURCE_IDENTIFIER_VRSIMAGE_OUTPUT, L"rw_vrsimage_output"},
};

static const ResourceBinding cbResourceBindingTable[] = {
    {FFX_VRS_CONSTANTBUFFER_IDENTIFIER_VRS, L"cbVRS"},
};

static FfxErrorCode patchResourceBindings(FfxPipelineState* inoutPipeline)
{
    // Texture srvs
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

    // Texture uavs
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

static uint32_t getPipelinePermutationFlags(uint32_t contextFlags, FfxVrsPass, uint32_t tileSize, bool, bool force64)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (contextFlags & FFX_VRS_ALLOW_ADDITIONAL_SHADING_RATES) ? VRS_SHADER_PERMUTATION_ADDITIONALSHADINGRATES : 0;
    flags |= (force64) ? VRS_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    // flags |= (fp16) ? VRS_SHADER_PERMUTATION_ALLOW_FP16 : 0;
    switch (tileSize)
    {
    case 8:
        flags |= VRS_SHADER_PERMUTATION_TILESIZE_8;
        break;
    case 16:
        flags |= VRS_SHADER_PERMUTATION_TILESIZE_16;
        break;
    case 32:
        flags |= VRS_SHADER_PERMUTATION_TILESIZE_32;
        break;
    default:
        break;
    }
    return flags;
}

static FfxErrorCode createPipelineState(FfxVrsContext_Private* context)
{
    FFX_ASSERT(context);

    FfxPipelineDescription pipelineDescription = {};
    pipelineDescription.contextFlags            = context->contextDescription.flags;

    // Samplers
    pipelineDescription.samplerCount            = 0;
    pipelineDescription.samplers                = nullptr;

    // Root constants
    pipelineDescription.rootConstantBufferCount = 1;
    FfxRootConstantDescription rootConstantDesc = {sizeof(VrsConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE};
    pipelineDescription.rootConstants           = &rootConstantDesc;

    // Query device capabilities
    FfxDeviceCapabilities capabilities;
    context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &capabilities);

    // Setup a few options used to determine permutation flags
    bool haveShaderModel66 = capabilities.maximumSupportedShaderModel >= FFX_SHADER_MODEL_6_6;
    bool supportedFP16     = capabilities.fp16Supported;
    bool canForceWave64    = false;

    const uint32_t waveLaneCountMin = capabilities.waveLaneCountMin;
    const uint32_t waveLaneCountMax = capabilities.waveLaneCountMax;
    if (waveLaneCountMin <= 64 && waveLaneCountMax >= 64)
        canForceWave64 = haveShaderModel66;
    else
        canForceWave64 = false;

    // Work out what permutation to load.
    uint32_t contextFlags = context->contextDescription.flags;

    // Set up pipeline descriptors (basically RootSignature and binding)
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(
        &context->contextDescription.backendInterface,
        FFX_EFFECT_VARIABLE_SHADING,
        FFX_VRS_PASS_IMAGEGEN,
        getPipelinePermutationFlags(contextFlags, FFX_VRS_PASS_IMAGEGEN, context->contextDescription.shadingRateImageTileSize, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId,
        &context->pipelineImageGen));

    // For each pipeline: re-route/fix-up IDs based on names
    patchResourceBindings(&context->pipelineImageGen);

    return FFX_OK;
}

static FfxErrorCode vrsCreate(FfxVrsContext_Private* context, const FfxVrsContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxVrsContext_Private));
    context->device = contextDescription->backendInterface.device;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxVrsContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);

    context->constantBuffer.num32BitEntries = sizeof(VrsConstants) / sizeof(uint32_t);

    // Create the context.
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_VARIABLE_SHADING, nullptr, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Call out for device caps.
    errorCode = context->contextDescription.backendInterface.fpGetDeviceCapabilities(
        &context->contextDescription.backendInterface, &context->deviceCapabilities);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    // Create shaders on initialize.
    errorCode = createPipelineState(context);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    return FFX_OK;
}

FFX_API FfxErrorCode ffxVrsContextCreate(FfxVrsContext* context, const FfxVrsContextDescription* contextDescription)
{
    // Zero context memory
    memset(context, 0, sizeof(FfxVrsContext));

    // Check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(contextDescription, FFX_ERROR_INVALID_POINTER);

    // Validate that all callbacks are set for the interface
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetSDKVersion, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetDeviceCapabilities, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpCreateBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpDestroyBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);

    // If a scratch buffer is declared, then we must have a size
    if (contextDescription->backendInterface.scratchBuffer)
    {
        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);
    }

    // Ensure the context is large enough for the internal context.
    FFX_STATIC_ASSERT(sizeof(FfxVrsContext) >= sizeof(FfxVrsContext_Private));

    // create the context.
    FfxVrsContext_Private* contextPrivate = (FfxVrsContext_Private*)(context);
    const FfxErrorCode     errorCode      = vrsCreate(contextPrivate, contextDescription);

    return errorCode;
}

static void scheduleDispatch(FfxVrsContext_Private*           context,
                             const FfxVrsDispatchDescription*,
                             const FfxPipelineState*          pipeline,
                             uint32_t                         dispatchX,
                             uint32_t                         dispatchY,
                             uint32_t                         dispatchZ)
{
    FfxGpuJobDescription     dispatchJob   = {FFX_GPU_JOB_COMPUTE};
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);

    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex)
    {
        const uint32_t            currentResourceId               = pipeline->srvTextureBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource                 = context->srvResources[currentResourceId];
        dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].name,
                 pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex)
    {
        const uint32_t currentResourceId = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].name,
                 pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].name);
#endif
        const FfxResourceInternal currentResource                     = context->uavResources[currentResourceId];
        dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].resource = currentResource;
        dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].mip = 0;
    }

    dispatchJob.computeJobDescriptor.dimensions[0] = dispatchX;
    dispatchJob.computeJobDescriptor.dimensions[1] = dispatchY;
    dispatchJob.computeJobDescriptor.dimensions[2] = dispatchZ;
    dispatchJob.computeJobDescriptor.pipeline      = *pipeline;

#ifdef FFX_DEBUG
    wcscpy_s(dispatchJob.computeJobDescriptor.cbNames[0], pipeline->constantBufferBindings[0].name);
#endif
    dispatchJob.computeJobDescriptor.cbs[0] = context->constantBuffer;

    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchJob);
}

static FfxErrorCode vrsDispatch(FfxVrsContext_Private* context, const FfxVrsDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;

    // Register resources for frame
    context->contextDescription.backendInterface.fpRegisterResource(
        &context->contextDescription.backendInterface, &params->historyColor, context->effectContextId, &context->srvResources[FFX_VRS_RESOURCE_IDENTIFIER_INPUT_COLOR]);
    context->contextDescription.backendInterface.fpRegisterResource(
        &context->contextDescription.backendInterface, &params->motionVectors, context->effectContextId, &context->srvResources[FFX_VRS_RESOURCE_IDENTIFIER_INPUT_MOTIONVECTORS]);
    context->contextDescription.backendInterface.fpRegisterResource(
        &context->contextDescription.backendInterface, &params->output, context->effectContextId, &context->uavResources[FFX_VRS_RESOURCE_IDENTIFIER_VRSIMAGE_OUTPUT]);


    VrsConstants constants;

    // Complete setting up the constant buffer data
    constants.width                = params->renderSize.width;
    constants.height               = params->renderSize.height;
    constants.tileSize             = params->tileSize;
    constants.motionFactor         = params->motionFactor;
    constants.varianceCutoff       = params->varianceCutoff;
    constants.motionVectorScale[0] = params->motionVectorScale.x;
    constants.motionVectorScale[1] = params->motionVectorScale.y;

    // This value is the image region dimension that each thread group of the FSR shader operates on
    uint32_t dispatchX;
    uint32_t dispatchY;
    uint32_t dispatchZ = 1;

    bool allowAdditionalVRSRates = context->contextDescription.flags & FFX_VRS_ALLOW_ADDITIONAL_SHADING_RATES;
    ffxVariableShadingGetDispatchInfo(params->renderSize, params->tileSize, allowAdditionalVRSRates, dispatchX, dispatchY);

    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(
        &context->contextDescription.backendInterface, &constants, sizeof(VrsConstants), &context->constantBuffer);
    scheduleDispatch(context, params, &context->pipelineImageGen, dispatchX, dispatchY, dispatchZ);

    // Execute all the work for the frame
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    // Release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    return FFX_OK;
}

FFX_API FfxErrorCode ffxVrsContextDispatch(FfxVrsContext* context, const FfxVrsDispatchDescription* dispatchDescription)
{
    // check pointers are valid
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchDescription, FFX_ERROR_INVALID_POINTER);
    
    FfxVrsContext_Private* contextPrivate = (FfxVrsContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the VRS pass
    const FfxErrorCode errorCode = vrsDispatch(contextPrivate, dispatchDescription);
    return errorCode;
}

static FfxErrorCode vrsRelease(FfxVrsContext_Private* context)
{
    FFX_ASSERT(context);

    // Release all pipelines
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineImageGen, context->effectContextId);

    // Unregister resources not created internally
    context->srvResources[FFX_VRS_RESOURCE_IDENTIFIER_INPUT_COLOR]                     = {FFX_VRS_RESOURCE_IDENTIFIER_NULL};
    context->srvResources[FFX_VRS_RESOURCE_IDENTIFIER_INPUT_MOTIONVECTORS]             = {FFX_VRS_RESOURCE_IDENTIFIER_NULL};

    context->uavResources[FFX_VRS_RESOURCE_IDENTIFIER_VRSIMAGE_OUTPUT]                 = {FFX_VRS_RESOURCE_IDENTIFIER_NULL};

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

FFX_API FfxErrorCode ffxVrsContextDestroy(FfxVrsContext* context)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    // Destroy the context.
    FfxVrsContext_Private* contextPrivate = (FfxVrsContext_Private*)(context);
    const FfxErrorCode     errorCode      = vrsRelease(contextPrivate);
    return errorCode;
}

FFX_API FfxErrorCode ffxVrsGetImageSizeFromeRenderResolution(
    uint32_t* imageWidth, uint32_t* imageHeight, uint32_t renderWidth, uint32_t renderHeight, uint32_t shadingRateImageTileSize)
{
    FFX_RETURN_ON_ERROR(renderWidth, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(renderHeight, FFX_ERROR_INVALID_POINTER);

    *imageWidth  = FFX_DIVIDE_ROUNDING_UP(renderWidth, shadingRateImageTileSize);
    *imageHeight = FFX_DIVIDE_ROUNDING_UP(renderHeight, shadingRateImageTileSize);

    return FFX_OK;
}

FFX_API FfxVersionNumber ffxVrsGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_VRS_VERSION_MAJOR, FFX_VRS_VERSION_MINOR, FFX_VRS_VERSION_PATCH);
}
