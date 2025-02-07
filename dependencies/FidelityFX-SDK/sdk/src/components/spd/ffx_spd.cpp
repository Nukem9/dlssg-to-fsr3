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

#include <FidelityFX/host/ffx_spd.h>
#include <FidelityFX/gpu/ffx_core.h>
#include <FidelityFX/gpu/spd/ffx_spd.h>
#include <ffx_object_management.h>
#include <ffx_object_management.h>

#include "ffx_spd_private.h"

// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t    index;
    wchar_t     name[64];
}ResourceBinding;

static const ResourceBinding srvTextureBindingTable[] =
{
    {FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC,                   L"r_input_downsample_src"},
};

static const ResourceBinding uavBufferBindingTable[] =
{
    {FFX_SPD_RESOURCE_IDENTIFIER_INTERNAL_GLOBAL_ATOMIC,           L"rw_internal_global_atomic"},
};

static const ResourceBinding uavTextureBindingTable[] =
{
    {FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MID_MIPMAP,  L"rw_input_downsample_src_mid_mip"},
    {FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_0,    L"rw_input_downsample_src_mips"},
};

static const ResourceBinding cbResourceBindingTable[] =
{
    {FFX_SPD_CONSTANTBUFFER_IDENTIFIER_SPD,                        L"cbSPD"},
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

static uint32_t getPipelinePermutationFlags(uint32_t contextFlags, FfxSpdDownsampleFilter downsampleFilter, FfxSpdPass passId, bool fp16, bool force64)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (contextFlags & FFX_SPD_SAMPLER_LINEAR) ? SPD_SHADER_PERMUTATION_LINEAR_SAMPLE : 0;
    flags |= (contextFlags & FFX_SPD_WAVE_INTEROP_LDS) ? SPD_SHADER_PERMUTATION_WAVE_INTEROP_LDS : 0;
    flags |= (force64) ? SPD_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (fp16 && contextFlags & FFX_SPD_MATH_PACKED) ? SPD_SHADER_PERMUTATION_ALLOW_FP16 : 0;

    switch (downsampleFilter)
    {
    case FFX_SPD_DOWNSAMPLE_FILTER_MEAN:
        flags |= SPD_SHADER_PERMUTATION_DOWNSAMPLE_FILTER_MEAN;
        break;
    case FFX_SPD_DOWNSAMPLE_FILTER_MIN:
        flags |= SPD_SHADER_PERMUTATION_DOWNSAMPLE_FILTER_MIN;
        break;
    case FFX_SPD_DOWNSAMPLE_FILTER_MAX:
        flags |= SPD_SHADER_PERMUTATION_DOWNSAMPLE_FILTER_MAX;
        break;
    default:
        break;
    }

    return flags;
}

static FfxErrorCode createPipelineStates(FfxSpdContext_Private* context)
{
    FFX_ASSERT(context);

    FfxPipelineDescription pipelineDescription = {};
    pipelineDescription.contextFlags = context->contextDescription.flags;

    // Samplers
    pipelineDescription.samplerCount = 1;
    FfxSamplerDescription samplerDesc = { FFX_FILTER_TYPE_MINMAGMIP_LINEAR, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_BIND_COMPUTE_SHADER_STAGE };
    pipelineDescription.samplers = &samplerDesc;

    // Root constants
    pipelineDescription.rootConstantBufferCount = 1;
    FfxRootConstantDescription rootConstantDesc = { sizeof(SpdConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE };
    pipelineDescription.rootConstants = &rootConstantDesc;

    // Query device capabilities
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
    FfxSpdDownsampleFilter downsampleFilter = context->contextDescription.downsampleFilter;

    // Set up pipeline descriptors (basically RootSignature and binding)
    wcscpy_s(pipelineDescription.name, L"SPD-DOWNSAMPLE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_SPD, FFX_SPD_PASS_DOWNSAMPLE,
        getPipelinePermutationFlags(contextFlags, downsampleFilter, FFX_SPD_PASS_DOWNSAMPLE, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId, &context->pipelineDownsample));

    // For each pipeline: re-route/fix-up IDs based on names
    patchResourceBindings(&context->pipelineDownsample);

    return FFX_OK;
}

static void scheduleDispatch(FfxSpdContext_Private* context, const FfxSpdDispatchDescription* params, const FfxPipelineState* pipeline, uint32_t dispatchX, uint32_t dispatchY, uint32_t dispatchZ)
{
    FfxGpuJobDescription dispatchJob = { FFX_GPU_JOB_COMPUTE };
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);

    // Texture srv
    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex) {

        const uint32_t currentResourceId = pipeline->srvTextureBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = context->srvResources[currentResourceId];
        dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].name, pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    // Texture uav
    uint32_t uavEntry = 0;  // Uav resource offset (accounts for uav arrays)
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex) {

        uint32_t currentResourceId = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].name, pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].name);
#endif
        // Mid-level mip
        if (currentResourceId == FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MID_MIPMAP)
        {
            const FfxResourceInternal currentResource = context->uavResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC];
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry].resource = currentResource;

            // Get the resource Id for Downsample Src Mip0
            currentResourceId = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex+1].resourceIdentifier;
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry++].mip = 6;
        }

        // Full mip chain uav
        else if (currentResourceId == FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC)
        {
            // Need to map as many resources as we have binding indices
            const FfxResourceInternal currentResource = context->uavResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC];

            uint32_t bindEntry = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].arrayIndex;

            // Don't over-subscribe mips (default to mip 0 once we've exhausted min mip)
            FfxResourceDescription resDesc = context->contextDescription.backendInterface.fpGetResourceDescription(&context->contextDescription.backendInterface, currentResource);
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry].resource = currentResource;
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry++].mip = (bindEntry < resDesc.mipCount) ? bindEntry : 0;
        }

        // other
        else
        {
            const FfxResourceInternal currentResource = context->uavResources[currentResourceId];
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry].resource = currentResource;
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry++].mip = 0;
        }
    }

    // Buffer uav
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavBufferCount; ++currentUnorderedAccessViewIndex) {

        const uint32_t currentResourceId = pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = context->uavResources[currentResourceId];
        dispatchJob.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].name,
                 pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].name);
#endif
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

static FfxErrorCode spdDispatch(FfxSpdContext_Private* context, const FfxSpdDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;

    // Register resources for frame
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->resource, context->effectContextId, &context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC]);

    context->uavResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_GLOBAL_ATOMIC] = context->uavResources[FFX_SPD_RESOURCE_IDENTIFIER_INTERNAL_GLOBAL_ATOMIC];
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->resource, context->effectContextId, &context->uavResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC]);
    context->uavResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MID_MIPMAP] = context->uavResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC]; // Will offset the mip in the scheduler

    SpdConstants constants;

    // Get SPD info for run
    uint32_t dispatchThreadGroupCountXY[2];
    uint32_t numWorkGroupsAndMips[2];
    FfxResourceDescription desc = context->contextDescription.backendInterface.fpGetResourceDescription(&context->contextDescription.backendInterface, context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC]);
    uint32_t rectInfo[4] = { 0, 0, desc.width, desc.height }; // left, top, width, height
    ffxSpdSetup(dispatchThreadGroupCountXY, constants.workGroupOffset, numWorkGroupsAndMips, rectInfo);

    // Complete setting up the constant buffer data
    constants.mips = numWorkGroupsAndMips[1];
    constants.numWorkGroups = numWorkGroupsAndMips[0];
    constants.invInputSize[0] = 1.f / desc.width;
    constants.invInputSize[1] = 1.f / desc.height;

    // This value is the image region dimension that each thread group of the FSR shader operates on
    uint32_t dispatchX = dispatchThreadGroupCountXY[0];
    uint32_t dispatchY = dispatchThreadGroupCountXY[1];
    uint32_t dispatchZ = desc.depth;

    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(
        &context->contextDescription.backendInterface, &constants, sizeof(SpdConstants), &context->constantBuffer);

    scheduleDispatch(context, params,&context->pipelineDownsample, dispatchX, dispatchY, dispatchZ);

    // Execute all the work for the frame
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);
    
    // Release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    return FFX_OK;
}

static FfxErrorCode spdCreate(FfxSpdContext_Private* context, const FfxSpdContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxSpdContext_Private));
    context->device = contextDescription->backendInterface.device;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxSpdContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);

    // Setup constant buffer sizes.
    context->constantBuffer.num32BitEntries = sizeof(SpdConstants) / sizeof(uint32_t);

    // Create the backend context
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_SPD, nullptr, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Call out for device caps.
    errorCode = context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &context->deviceCapabilities);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Create the atomic buffer resource used as a counter in SPD
    const FfxInternalResourceDescription internalSurfaceDesc = {FFX_SPD_RESOURCE_IDENTIFIER_INTERNAL_GLOBAL_ATOMIC,
                                                                L"SPD_AtomicCounter",
                                                                FFX_RESOURCE_TYPE_BUFFER,
                                                                FFX_RESOURCE_USAGE_UAV,
                                                                FFX_SURFACE_FORMAT_UNKNOWN,
                                                                6 * sizeof(uint32_t),
                                                                sizeof(uint32_t),
                                                                1,
                                                                FFX_RESOURCE_FLAGS_NONE,
                                                                {FFX_RESOURCE_INIT_DATA_TYPE_VALUE, sizeof(uint32_t) * 6, 0}};

    // Clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    const FfxResourceDescription resourceDescription = { internalSurfaceDesc.type, internalSurfaceDesc.format, internalSurfaceDesc.width, internalSurfaceDesc.height, 0, 0, internalSurfaceDesc.flags, internalSurfaceDesc.usage };
    const FfxCreateResourceDescription createResourceDescription = {FFX_HEAP_TYPE_DEFAULT,
                                                                    resourceDescription,
                                                                    FFX_RESOURCE_STATE_UNORDERED_ACCESS,
                                                                    internalSurfaceDesc.name,
                                                                    internalSurfaceDesc.id,
                                                                    internalSurfaceDesc.initData};
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface, &createResourceDescription, context->effectContextId, &context->srvResources[internalSurfaceDesc.id]));

    // And copy resources to uavResrouces list
    memcpy(context->uavResources, context->srvResources, sizeof(context->srvResources));

    // Create shaders on initialize.
    errorCode = createPipelineStates(context);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    return FFX_OK;
}

static FfxErrorCode spdRelease(FfxSpdContext_Private* context)
{
    FFX_ASSERT(context);

    // Release all pipelines
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineDownsample, context->effectContextId);

    // Unregister resources not created internally
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MID_MIPMAP]    = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC]               = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_0]      = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_1]      = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_2]      = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_3]      = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_4]      = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_5]      = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_6]      = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_7]      = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_8]      = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_9]      = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_10]     = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_11]     = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INPUT_DOWNSAMPLE_SRC_MIPMAP_12]     = { FFX_SPD_RESOURCE_IDENTIFIER_NULL };

    // Release internal resources and copy resource
    ffxSafeReleaseCopyResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INTERNAL_GLOBAL_ATOMIC], context->effectContextId);
    ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SPD_RESOURCE_IDENTIFIER_INTERNAL_GLOBAL_ATOMIC], context->effectContextId);

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

FfxErrorCode ffxSpdContextCreate(FfxSpdContext* context, const FfxSpdContextDescription* contextDescription)
{
    // Zero context memory
    memset(context, 0, sizeof(FfxSpdContext));

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
    FFX_STATIC_ASSERT(sizeof(FfxSpdContext) >= sizeof(FfxSpdContext_Private));

    // create the context.
    FfxSpdContext_Private* contextPrivate = (FfxSpdContext_Private*)(context);
    const FfxErrorCode errorCode = spdCreate(contextPrivate, contextDescription);

    return errorCode;
}

FfxErrorCode ffxSpdContextDestroy(FfxSpdContext* context)
{
    FFX_RETURN_ON_ERROR(
        context,
        FFX_ERROR_INVALID_POINTER);

    // Destroy the context.
    FfxSpdContext_Private* contextPrivate = (FfxSpdContext_Private*)(context);
    const FfxErrorCode errorCode = spdRelease(contextPrivate);
    return errorCode;
}

FfxErrorCode ffxSpdContextDispatch(FfxSpdContext* context, const FfxSpdDispatchDescription* dispatchDescription)
{
    // check pointers are valid
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchDescription, FFX_ERROR_INVALID_POINTER);

    FfxSpdContext_Private* contextPrivate = (FfxSpdContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the SPD pass
    const FfxErrorCode errorCode = spdDispatch(contextPrivate, dispatchDescription);
    return errorCode;

    return FFX_OK;
}

FFX_API FfxVersionNumber ffxSpdGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_SPD_VERSION_MAJOR, FFX_SPD_VERSION_MINOR, FFX_SPD_VERSION_PATCH);
}
