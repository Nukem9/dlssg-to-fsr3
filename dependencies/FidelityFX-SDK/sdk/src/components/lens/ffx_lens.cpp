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
#include <cmath>        // for fabs, abs, sinf, sqrt, etc.

#include <FidelityFX/host/ffx_lens.h>
#include <ffx_object_management.h>

#include "ffx_lens_private.h"

// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t    index;
    wchar_t     name[64];
}ResourceBinding;

static const ResourceBinding srvTextureBindingTable[] =
{
    {FFX_LENS_RESOURCE_IDENTIFIER_INPUT_TEXTURE,                   L"r_input_texture"},
};

static const ResourceBinding uavTextureBindingTable[] =
{
    {FFX_LENS_RESOURCE_IDENTIFIER_OUTPUT_TEXTURE,                  L"rw_output_texture"},
};

static const ResourceBinding cbResourceBindingTable[] =
{
    {FFX_LENS_CONSTANTBUFFER_IDENTIFIER_LENS,                      L"cbLens"},
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

static uint32_t getPipelinePermutationFlags(bool force64, bool fp16)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (force64) ? LENS_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (fp16) ? LENS_SHADER_PERMUTATION_ALLOW_FP16 : 0;
    return flags;
}

static FfxErrorCode createPipelineStates(FfxLensContext_Private* context)
{
    FFX_ASSERT(context);

    FfxPipelineDescription pipelineDescription = {};
    pipelineDescription.contextFlags = context->contextDescription.flags;

    // Samplers
    pipelineDescription.samplerCount  = 1;
    FfxSamplerDescription samplerDesc = {FFX_FILTER_TYPE_MINMAGMIP_LINEAR, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_BIND_COMPUTE_SHADER_STAGE};
    pipelineDescription.samplers      = &samplerDesc;

    // Root constants
    pipelineDescription.rootConstantBufferCount = 1;
    FfxRootConstantDescription rootConstantDesc = {sizeof(LensConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE};
    pipelineDescription.rootConstants           = &rootConstantDesc;

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

    bool useFP16 = context->contextDescription.floatPrecision == FfxLensFloatPrecision::FFX_LENS_FLOAT_PRECISION_16BIT;

    // Set up pipeline descriptors (basically RootSignature and binding)
    wcscpy_s(pipelineDescription.name, L"LENS-MAIN");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_LENS, FFX_LENS_PASS_MAIN_PASS,
        getPipelinePermutationFlags(canForceWave64, supportedFP16 && useFP16),
        &pipelineDescription, context->effectContextId, &context->pipelineLens));

    // For each pipeline: re-route/fix-up IDs based on names
    patchResourceBindings(&context->pipelineLens);

    return FFX_OK;
}

static void scheduleDispatch(FfxLensContext_Private* context, const FfxLensDispatchDescription* params, const FfxPipelineState* pipeline, uint32_t dispatchX, uint32_t dispatchY, uint32_t dispatchZ)
{
    FfxGpuJobDescription dispatchJob = {FFX_GPU_JOB_COMPUTE};
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);

    // Texture srv
    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex) {

        const uint32_t currentResourceId = pipeline->srvTextureBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = context->srvResources[currentResourceId];
        dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].name,
                 pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    // Texture uav
    uint32_t uavEntry = 0;  // Uav resource offset (accounts for uav arrays)
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex) {

        uint32_t currentResourceId = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].name,
                 pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].name);
#endif
        const FfxResourceInternal currentResource = context->uavResources[currentResourceId];
        dispatchJob.computeJobDescriptor.uavTextures[uavEntry].resource = currentResource;
        dispatchJob.computeJobDescriptor.uavTextures[uavEntry].mip = 0;

        ++uavEntry;
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

static FfxErrorCode lensDispatch(FfxLensContext_Private* context, const FfxLensDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;

    // Register resources for frame
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->resource, context->effectContextId, &context->srvResources[FFX_LENS_RESOURCE_IDENTIFIER_INPUT_TEXTURE]);

    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->resourceOutput, context->effectContextId, &context->uavResources[FFX_LENS_RESOURCE_IDENTIFIER_OUTPUT_TEXTURE]);

    FfxResourceDescription desc = context->contextDescription.backendInterface.fpGetResourceDescription(&context->contextDescription.backendInterface, context->srvResources[FFX_LENS_RESOURCE_IDENTIFIER_INPUT_TEXTURE]);
    static const int threadGroupWorkRegionDim = 8;
    int      dispatchX = FFX_DIVIDE_ROUNDING_UP(params->renderSize.width, threadGroupWorkRegionDim);
    int      dispatchY = FFX_DIVIDE_ROUNDING_UP(params->renderSize.height, threadGroupWorkRegionDim);
    uint32_t dispatchZ = desc.depth;

    // Complete setting up the constant buffer data
    LensConstants lensConst = {};
    lensConst.grainScale  = params->grainScale;
    lensConst.grainAmount = params->grainAmount;
    lensConst.grainSeed   = params->grainSeed;
    lensConst.center[0]   = params->renderSize.width / 2;
    lensConst.center[1]   = params->renderSize.height / 2;
    lensConst.chromAb     = params->chromAb;
    lensConst.vignette    = params->vignette;

    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(&context->contextDescription.backendInterface,
                                                                               &lensConst,
                                                                               sizeof(LensConstants),
                                                                               &context->constantBuffer);
    scheduleDispatch(context, params, &context->pipelineLens, dispatchX, dispatchY, dispatchZ);

    // Execute all the work for the frame
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    // Release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    return FFX_OK;
}

static FfxErrorCode lensCreate(FfxLensContext_Private* context, const FfxLensContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxLensContext_Private));
    context->device = contextDescription->backendInterface.device;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxLensContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);

    context->constantBuffer.num32BitEntries = sizeof(LensConstants) / sizeof(uint32_t);

    // Create the context.
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_LENS, nullptr, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Call out for device caps.
    errorCode = context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &context->deviceCapabilities);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    // And copy resources to uavResrouces list
    memcpy(context->uavResources, context->srvResources, sizeof(context->srvResources));

    // Create shaders on initialize.
    errorCode = createPipelineStates(context);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    return FFX_OK;
}

static FfxErrorCode lensRelease(FfxLensContext_Private* context)
{
    FFX_ASSERT(context);
    
    // Release all pipelines
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineLens, context->effectContextId);

    // Unregister resources not created internally
    context->srvResources[FFX_LENS_RESOURCE_IDENTIFIER_INPUT_TEXTURE]  = { FFX_LENS_RESOURCE_IDENTIFIER_NULL };
    context->uavResources[FFX_LENS_RESOURCE_IDENTIFIER_OUTPUT_TEXTURE] = { FFX_LENS_RESOURCE_IDENTIFIER_NULL };

    // Release internal resources and copy resource
    ffxSafeReleaseCopyResource(&context->contextDescription.backendInterface, context->srvResources[FFX_LENS_RESOURCE_IDENTIFIER_INPUT_TEXTURE], context->effectContextId);
    ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[FFX_LENS_RESOURCE_IDENTIFIER_INPUT_TEXTURE], context->effectContextId);

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

FfxErrorCode ffxLensContextCreate(FfxLensContext* context, const FfxLensContextDescription* contextDescription)
{
    // Zero context memory
    memset(context, 0, sizeof(FfxLensContext));

    // Check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(contextDescription, FFX_ERROR_INVALID_POINTER);

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
    FFX_STATIC_ASSERT(sizeof(FfxLensContext) >= sizeof(FfxLensContext_Private));

    // create the context.
    FfxLensContext_Private* contextPrivate = (FfxLensContext_Private*)(context);
    const FfxErrorCode errorCode = lensCreate(contextPrivate, contextDescription);

    return errorCode;
}

FfxErrorCode ffxLensContextDestroy(FfxLensContext* context)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    // Destroy the context.
    FfxLensContext_Private* contextPrivate = (FfxLensContext_Private*)(context);
    const FfxErrorCode errorCode = lensRelease(contextPrivate);
    return errorCode;
}

FfxErrorCode ffxLensContextDispatch(FfxLensContext* context, const FfxLensDispatchDescription* dispatchDescription)
{
    // check pointers are valid
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchDescription, FFX_ERROR_INVALID_POINTER);

    FfxLensContext_Private* contextPrivate = (FfxLensContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the Lens pass
    const FfxErrorCode errorCode = lensDispatch(contextPrivate, dispatchDescription);
    return errorCode;

    return FFX_OK;
}

FFX_API FfxVersionNumber ffxLensGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_LENS_VERSION_MAJOR, FFX_LENS_VERSION_MINOR, FFX_LENS_VERSION_PATCH);
}
