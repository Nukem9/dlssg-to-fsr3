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

#define FFX_CPU
#include <FidelityFX/host/ffx_classifier.h>
#include <FidelityFX/host/ffx_util.h>
#include <FidelityFX/gpu/ffx_core.h>
#include <ffx_object_management.h>

#include "ffx_classifier_private.h"

static constexpr uint32_t k_tileSizeX = 8;
static constexpr uint32_t k_tileSizeY = 4;

// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t    index;
    wchar_t     name[64];
}ResourceBinding;

static const ResourceBinding srvTextureBindingTable[] =
{
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_DEPTH,                  L"r_input_depth"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_NORMAL,                 L"r_input_normal"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS,         L"r_input_motion_vectors"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_SPECULAR_ROUGHNESS,     L"r_input_material_parameters"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_ENVIRONMENT_MAP,        L"r_input_environment_map"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_HIT_COUNTER_HISTORY ,         L"r_hit_counter_history"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_VARIANCE_HISTORY,             L"r_variance_history"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_SHADOW_MAPS,            L"r_input_shadowMap"},
};

static const ResourceBinding srvBufferBindingTable[] =
{
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_WORK_QUEUE,                   L"rsb_tiles"},
};

static const ResourceBinding uavBufferBindingTable[] =
{
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_WORK_QUEUE,                   L"rwsb_tiles"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_OUTPUT_WORK_QUEUE_COUNTER,    L"rwb_tileCount"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_RAY_LIST,                     L"rw_ray_list"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_HW_RAY_LIST,                  L"rw_hw_ray_list"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST,           L"rw_denoiser_tile_list"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_RAY_COUNTER,                  L"rw_ray_counter"},
};

static const ResourceBinding uavTextureBindingTable[] =
{
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_OUTPUT_RAY_HIT,               L"rwt2d_rayHitResults"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_RADIANCE,                     L"rw_radiance"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS,          L"rw_extracted_roughness"},
    {FFX_CLASSIFIER_RESOURCE_IDENTIFIER_HIT_COUNTER,                  L"rw_hit_counter"},
};

static const ResourceBinding cbResourceBindingTable[] =
{
    {FFX_CLASSIFIER_CONSTANTBUFFER_IDENTIFIER_CLASSIFIER,             L"cbClassifier"},
    {FFX_CLASSIFIER_CONSTANTBUFFER_IDENTIFIER_REFLECTION ,            L"cbClassifierReflection"},
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

    for (uint32_t srvIndex = 0; srvIndex < inoutPipeline->srvBufferCount; ++srvIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(srvBufferBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(srvBufferBindingTable[mapIndex].name, inoutPipeline->srvBufferBindings[srvIndex].name))
                break;
        }
        if (mapIndex == _countof(srvBufferBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->srvBufferBindings[srvIndex].resourceIdentifier = srvBufferBindingTable[mapIndex].index;
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
        for (mapIndex = 0; mapIndex < _countof(uavBufferBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(uavBufferBindingTable[mapIndex].name, inoutPipeline->uavBufferBindings[uavIndex].name))
                break;
        }
        if (mapIndex == _countof(uavBufferBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->uavBufferBindings[uavIndex].resourceIdentifier = uavBufferBindingTable[mapIndex].index;
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

static uint32_t getPipelinePermutationFlags(uint32_t contextFlags, bool force64, bool fp16)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (force64) ? CLASSIFIER_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (fp16) ? CLASSIFIER_SHADER_PERMUTATION_ALLOW_FP16 : 0;
    flags |= (contextFlags & FFX_CLASSIFIER_ENABLE_DEPTH_INVERTED) ? CLASSIFIER_SHADER_PERMUTATION_DEPTH_INVERTED : 0;

    if (contextFlags & FFX_CLASSIFIER_SHADOW)
    {
        if (contextFlags & FFX_CLASSIFIER_CLASSIFY_BY_NORMALS)
            flags |= CLASSIFIER_SHADER_PERMUTATION_CLASSIFY_BY_NORMALS;
        else if (contextFlags & FFX_CLASSIFIER_CLASSIFY_BY_CASCADES)
            flags |= CLASSIFIER_SHADER_PERMUTATION_CLASSIFY_BY_CASCADES;
    }

    return flags;
}

static FfxErrorCode createShadowsPipelineStates(FfxClassifierContext_Private* context)
{
    FFX_ASSERT(context);

    FfxRootConstantDescription rootConstants[] {
        {sizeof(ClassifierConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE}};

    FfxPipelineDescription pipelineDescription{};
    pipelineDescription.contextFlags = context->contextDescription.flags;
    pipelineDescription.samplerCount = 0;
    pipelineDescription.samplers = nullptr;
    pipelineDescription.rootConstantBufferCount = 1;
    pipelineDescription.rootConstants = &rootConstants[0];

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
    wcscpy_s(pipelineDescription.name, L"CLASSIFIER-CLASSIFY");
    FFX_VALIDATE(
        context->contextDescription.backendInterface.fpCreatePipeline(
            &context->contextDescription.backendInterface,
            FFX_EFFECT_CLASSIFIER,
            FFX_CLASSIFIER_SHADOW_PASS_CLASSIFIER,
            getPipelinePermutationFlags(contextFlags, canForceWave64, supportedFP16),
            &pipelineDescription,
            context->effectContextId,
            &context->shadowClassifierPipeline
        )
    );

    // For each pipeline: re-route/fix-up IDs based on names
    patchResourceBindings(&context->shadowClassifierPipeline);

    return FFX_OK;
}

static void scheduleDispatchShadow(FfxClassifierContext_Private* context, const FfxClassifierShadowDispatchDescription* params, const FfxPipelineState* pipeline, uint32_t dispatchX, uint32_t dispatchY)
{
    FfxComputeJobDescription jobDescriptor = {};

    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex)
    {
        const FfxResourceBinding binding = pipeline->srvTextureBindings[currentShaderResourceViewIndex];

        uint32_t currentResourceId = binding.resourceIdentifier;

        uint32_t currResId                   = currentResourceId + binding.arrayIndex;

        const FfxResourceInternal currentResource = context->srvResources[currResId];

        if (currentResource.internalIndex == 0)
            break;
        jobDescriptor.srvTextures[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor.srvTextures[currentShaderResourceViewIndex].name, pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    uint32_t uavEntry = 0;
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex)
    {
        const uint32_t currentResourceId = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor.uavTextures[currentUnorderedAccessViewIndex].name, pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].name);
#endif
        const FfxResourceInternal currentResource = context->uavResources[currentResourceId];
        jobDescriptor.uavTextures[uavEntry].resource = currentResource;
        jobDescriptor.uavTextures[uavEntry++].mip = 0;
    }

    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavBufferCount; ++currentUnorderedAccessViewIndex)
    {
        const uint32_t currentResourceId = pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].name, pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].name);
#endif
        const FfxResourceInternal currentResource                          = context->uavResources[currentResourceId];
        jobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].resource = currentResource;
    }

    jobDescriptor.dimensions[0] = dispatchX;
    jobDescriptor.dimensions[1] = dispatchY;
    jobDescriptor.dimensions[2] = 1;
    jobDescriptor.pipeline      = *pipeline;

    // Only one constant buffer
#ifdef FFX_DEBUG
    wcscpy_s(jobDescriptor.cbNames[0], pipeline->constantBufferBindings[0].name);
#endif
    jobDescriptor.cbs[0] = context->classifierConstants;

    FfxGpuJobDescription dispatchJob = {FFX_GPU_JOB_COMPUTE};
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);
    dispatchJob.computeJobDescriptor = jobDescriptor;

    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchJob);
}

static FfxErrorCode shadowClassifierDispatch(FfxClassifierContext_Private* context, const FfxClassifierShadowDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;

    // Register resources for frame
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->depth, context->effectContextId, &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_DEPTH]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->normals, context->effectContextId, &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_NORMAL]);
    for (size_t i = 0; i < FFX_CLASSIFIER_MAX_SHADOW_MAP_TEXTURES_COUNT; ++i)
    {
        context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface,
                                                                        &params->shadowMaps[i],
                                                                        context->effectContextId,
                                                                        &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_SHADOW_MAPS + i]);
    }
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->workQueue, context->effectContextId, &context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_WORK_QUEUE]);

    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->workQueueCount, context->effectContextId, &context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_OUTPUT_WORK_QUEUE_COUNTER]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->rayHitTexture, context->effectContextId, &context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_OUTPUT_RAY_HIT]);

    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface,
                                                                    &params->workQueue,
                                                                    context->effectContextId,
                                                                    &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_WORK_QUEUE]);

    // Classifier Constant Buffer
    ClassifierConstants classifierConstants = {};
    classifierConstants.textureSize[0]      = (float)context->contextDescription.resolution.width;
    classifierConstants.textureSize[1]      = (float)context->contextDescription.resolution.height;
    classifierConstants.textureSize[2]      = 1.f / context->contextDescription.resolution.width;
    classifierConstants.textureSize[3]      = 1.f / context->contextDescription.resolution.height;
    classifierConstants.lightDir[0]         = params->lightDir[0];
    classifierConstants.lightDir[1]         = params->lightDir[1];
    classifierConstants.lightDir[2]         = params->lightDir[2];
    classifierConstants.skyHeight = 3.402823466e+38F; // FLT_MAX

    uint32_t local_cascadeCount_tileTolerance_pad_pad[4] = {params->cascadeCount, params->tileCutOff, 0, 0};
    memcpy(&classifierConstants.cascadeCount_tileTolerance_pad_pad,
           &local_cascadeCount_tileTolerance_pad_pad,
           sizeof(local_cascadeCount_tileTolerance_pad_pad));

    float local_blockerOffset_cascadeSize_sunSizeLightSpace_pad[4] = {params->blockerOffset, params->cascadeSize, params->sunSizeLightSpace, 0};
    memcpy(&classifierConstants.blockerOffset_cascadeSize_sunSizeLightSpace_pad,
           &local_blockerOffset_cascadeSize_sunSizeLightSpace_pad,
           sizeof(local_blockerOffset_cascadeSize_sunSizeLightSpace_pad));

    float local_bRejectLitPixels_bUseCascadesForRayT_normalsUnpackMul_unpackAdd[4] = {(float)params->bRejectLitPixels, (float)params->bUseCascadesForRayT, params->normalsUnPackMul, params->normalsUnPackAdd};
    memcpy(&classifierConstants.bRejectLitPixels_bUseCascadesForRayT_normalsUnpackMul_unpackAdd,
           &local_bRejectLitPixels_bUseCascadesForRayT_normalsUnpackMul_unpackAdd,
           sizeof(local_bRejectLitPixels_bUseCascadesForRayT_normalsUnpackMul_unpackAdd));

    memcpy(&classifierConstants.cascadeScale, &params->cascadeScale, sizeof(params->cascadeScale));
    memcpy(&classifierConstants.cascadeOffset, &params->cascadeOffset, sizeof(params->cascadeOffset));

    memcpy(&classifierConstants.viewToWorld, &params->viewToWorld, sizeof(params->viewToWorld));
    memcpy(&classifierConstants.lightView, &params->lightView, sizeof(params->lightView));
    memcpy(&classifierConstants.inverseLightView, &params->inverseLightView, sizeof(params->inverseLightView));

    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(
        &context->contextDescription.backendInterface, &classifierConstants, sizeof(classifierConstants), &context->classifierConstants);
    
    scheduleDispatchShadow(context, params, &context->shadowClassifierPipeline, FFX_DIVIDE_ROUNDING_UP(context->contextDescription.resolution.width, k_tileSizeX), FFX_DIVIDE_ROUNDING_UP(context->contextDescription.resolution.height, k_tileSizeY));

    // Execute all the work for the frame
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    // Release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    return FFX_OK;
}


static FfxErrorCode createReflectionsPipelineStates(FfxClassifierContext_Private* context)
{
    FFX_ASSERT(context);

    const size_t samplerCount = 2;
    FfxSamplerDescription samplerDescs[samplerCount] = {
        { FFX_FILTER_TYPE_MINMAGMIP_LINEAR, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_WRAP, FFX_BIND_COMPUTE_SHADER_STAGE },
        { FFX_FILTER_TYPE_MINMAGMIP_LINEAR, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_BIND_COMPUTE_SHADER_STAGE }
    };
    FfxRootConstantDescription rootConstantDesc = { sizeof(ClassifierReflectionsConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE };

    FfxPipelineDescription pipelineDescription = {};
    pipelineDescription.contextFlags = context->contextDescription.flags;
    pipelineDescription.samplerCount = samplerCount;
    pipelineDescription.samplers = samplerDescs;
    pipelineDescription.rootConstantBufferCount = 1;
    pipelineDescription.rootConstants = &rootConstantDesc;
    pipelineDescription.stage = FFX_BIND_COMPUTE_SHADER_STAGE;

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
    {
        canForceWave64 = haveShaderModel66;
    }
    else
    {
        canForceWave64 = false;
    }

    uint32_t contextFlags = context->contextDescription.flags;

    // Set up pipeline descriptor (basically RootSignature and binding)
    wcscpy_s(pipelineDescription.name, L"CLASSIFIER-REFLECTIONS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_CLASSIFIER, FFX_CLASSIFIER_REFLECTION_PASS_TILE_CLASSIFIER,
        getPipelinePermutationFlags(contextFlags, canForceWave64, supportedFP16), &pipelineDescription, context->effectContextId, &context->reflectionsClassifierPipeline));

    FFX_ASSERT(patchResourceBindings(&context->reflectionsClassifierPipeline) == FFX_OK);

    return FFX_OK;
}

static FfxErrorCode classifierCreate(FfxClassifierContext_Private* context, const FfxClassifierContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxClassifierContext_Private));
    context->device = contextDescription->backendInterface.device;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxClassifierContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);

    context->classifierConstants.num32BitEntries = sizeof(ClassifierConstants) / sizeof(uint32_t);
    context->reflectionsConstants.num32BitEntries = sizeof(ClassifierReflectionsConstants) / sizeof(uint32_t);

    // Create the context.
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_CLASSIFIER, nullptr, &context->effectContextId));

    // Call out for device caps.
    FFX_VALIDATE(context->contextDescription.backendInterface.fpGetDeviceCapabilities(
        &context->contextDescription.backendInterface,
        &context->deviceCapabilities));

    if (contextDescription->flags & FFX_CLASSIFIER_REFLECTION) {
        // Create shaders on initialize.
        FfxErrorCode errorCode = createReflectionsPipelineStates(context);
        FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);
    }
    else if (contextDescription->flags & FFX_CLASSIFIER_SHADOW) {
        // Create shaders on initialize.
        FfxErrorCode errorCode = createShadowsPipelineStates(context);
        FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);
    }

    // Clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    // copy resources to uavResrouces list
    memcpy(context->uavResources, context->srvResources, sizeof(context->srvResources));

    return FFX_OK;
}

static FfxErrorCode ClassifierRelease(FfxClassifierContext_Private* context)
{
    FFX_ASSERT(context);

    if (context->contextDescription.flags & FFX_CLASSIFIER_SHADOW) {
        // Release the pipelines
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->shadowClassifierPipeline, context->effectContextId);

        // Unregister resources not created internally
        context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_DEPTH] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_NORMAL] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_SHADOW_MAPS] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_WORK_QUEUE] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_OUTPUT_WORK_QUEUE_COUNTER] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_OUTPUT_RAY_HIT] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
    }

    if (context->contextDescription.flags & FFX_CLASSIFIER_REFLECTION) {
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->reflectionsClassifierPipeline, context->effectContextId);

        // unregister resources not created internally
        context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_DEPTH] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_NORMAL] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_SPECULAR_ROUGHNESS] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_ENVIRONMENT_MAP] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_VARIANCE_HISTORY] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_HIT_COUNTER_HISTORY] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_HIT_COUNTER] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_RAY_LIST] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_HW_RAY_LIST] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_RAY_COUNTER] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_RADIANCE] = { FFX_CLASSIFIER_RESOURCE_IDENTIFIER_NULL };

    }

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

FfxErrorCode ffxClassifierContextCreate(FfxClassifierContext* context, const FfxClassifierContextDescription* contextDescription)
{
    // Zero context memory
    memset(context, 0, sizeof(FfxClassifierContext));

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
    FFX_STATIC_ASSERT(sizeof(FfxClassifierContext) >= sizeof(FfxClassifierContext_Private));

    // create the context.
    FfxClassifierContext_Private* contextPrivate = (FfxClassifierContext_Private*)(context);
    const FfxErrorCode errorCode = classifierCreate(contextPrivate, contextDescription);

    return errorCode;
}

static void populateComputeJobResources(FfxClassifierContext_Private* context, const FfxPipelineState* pipeline, FfxComputeJobDescription* jobDescriptor)
{
    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex) {

        const uint32_t currentResourceId = pipeline->srvTextureBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = context->srvResources[currentResourceId];
        jobDescriptor->srvTextures[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor->srvTextures[currentShaderResourceViewIndex].name, pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    uint32_t uavEntry = 0;  // Uav resource offset (accounts for uav arrays)
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex) {

        const FfxResourceBinding binding = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex];
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor->uavTextures[currentUnorderedAccessViewIndex].name, binding.name);
#endif
        const uint32_t            bindEntry         = binding.arrayIndex;
        const uint32_t            currentResourceId = binding.resourceIdentifier;
        const FfxResourceInternal currentResource   = context->uavResources[currentResourceId];

        // Don't over-subscribe mips (default to mip 0 once we've exhausted min mip)
        FfxResourceDescription resDesc = context->contextDescription.backendInterface.fpGetResourceDescription(&context->contextDescription.backendInterface, currentResource);
        jobDescriptor->uavTextures[uavEntry].resource = currentResource;
        jobDescriptor->uavTextures[uavEntry++].mip    = (bindEntry < resDesc.mipCount) ? bindEntry : 0;
    }

    // Buffer uav
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavBufferCount; ++currentUnorderedAccessViewIndex) {

        const uint32_t currentResourceId = pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = context->uavResources[currentResourceId];
        jobDescriptor->uavBuffers[currentUnorderedAccessViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor->uavBuffers[currentUnorderedAccessViewIndex].name, pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].name);
#endif
    }

    // Only one constant buffer
#ifdef FFX_DEBUG
    wcscpy_s(jobDescriptor->cbNames[0], pipeline->constantBufferBindings[0].name);
#endif
    jobDescriptor->cbs[0] = context->reflectionsConstants;
}

static void scheduleDispatch(FfxClassifierContext_Private* context, const FfxPipelineState* pipeline, uint32_t dispatchX, uint32_t dispatchY)
{
    FfxComputeJobDescription jobDescriptor = {};
    jobDescriptor.dimensions[0] = dispatchX;
    jobDescriptor.dimensions[1] = dispatchY;
    jobDescriptor.dimensions[2] = 1;
    jobDescriptor.pipeline = *pipeline;
    populateComputeJobResources(context, pipeline, &jobDescriptor);

    FfxGpuJobDescription dispatchJob = { FFX_GPU_JOB_COMPUTE };
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);
    dispatchJob.computeJobDescriptor = jobDescriptor;
    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchJob);
}

static FfxErrorCode classifierDispatchReflections(FfxClassifierContext_Private* context, const FfxClassifierReflectionDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;

    // Prepare per frame descriptor tables
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->depth, context->effectContextId, &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_DEPTH]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->motionVectors, context->effectContextId, &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->normal, context->effectContextId, &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_NORMAL]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->materialParameters, context->effectContextId, &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_SPECULAR_ROUGHNESS]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->environmentMap, context->effectContextId, &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_ENVIRONMENT_MAP]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->varianceHistory, context->effectContextId, &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_VARIANCE_HISTORY]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->hitCounterHistory, context->effectContextId, &context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_HIT_COUNTER_HISTORY]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->hitCounter, context->effectContextId, &context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_HIT_COUNTER]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->rayList, context->effectContextId, &context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_RAY_LIST]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->rayListHW, context->effectContextId, &context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_HW_RAY_LIST]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->extractedRoughness, context->effectContextId, &context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->rayCounter, context->effectContextId, &context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_RAY_COUNTER]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->denoiserTileList, context->effectContextId, &context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->radiance, context->effectContextId, &context->uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_RADIANCE]);

    // actual resource size may differ from render/display resolution (e.g. due to Hw/API restrictions), so query the descriptor for UVs adjustment
    const FfxResourceDescription resourceDescInputDepth = context->contextDescription.backendInterface.fpGetResourceDescription(&context->contextDescription.backendInterface, context->srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_INPUT_DEPTH]);
    FFX_ASSERT(resourceDescInputDepth.type == FFX_RESOURCE_TYPE_TEXTURE2D);

    const uint32_t width = uint32_t(params->renderSize.width ? params->renderSize.width : resourceDescInputDepth.width);
    const uint32_t height = uint32_t(params->renderSize.height ? params->renderSize.height : resourceDescInputDepth.height);

    // Copy the matrices over
    ClassifierReflectionsConstants reflectionsConstants;
    memcpy(&reflectionsConstants.invViewProjection, &params->invViewProjection, sizeof(float) * 16 * 6);
    reflectionsConstants.renderSize[0] = width;
    reflectionsConstants.renderSize[1] = height;
    reflectionsConstants.iblFactor = params->iblFactor;
    reflectionsConstants.inverseRenderSize[0] = 1.0f / width;
    reflectionsConstants.inverseRenderSize[1] = 1.0f / height;
    reflectionsConstants.samplesPerQuad = params->samplesPerQuad;
    reflectionsConstants.temporalVarianceGuidedTracingEnabled = params->temporalVarianceGuidedTracingEnabled;
    reflectionsConstants.globalRoughnessThreshold = params->globalRoughnessThreshold;
    reflectionsConstants.rtRoughnessThreshold = params->rtRoughnessThreshold;
    reflectionsConstants.mask = params->mask;
    reflectionsConstants.reflectionWidth = params->reflectionWidth;
    reflectionsConstants.reflectionHeight = params->reflectionHeight;
    reflectionsConstants.hybridMissWeight = params->hybridMissWeight;
    reflectionsConstants.hybridSpawnRate = params->hybridSpawnRate;
    reflectionsConstants.vrtVarianceThreshold = params->vrtVarianceThreshold;
    reflectionsConstants.reflectionsBackfacingThreshold = params->reflectionsBackfacingThreshold;
    reflectionsConstants.randomSamplesPerPixel = params->randomSamplesPerPixel;
    reflectionsConstants.frameIndex = params->frameIndex;
    reflectionsConstants.motionVectorScale[0] = params->motionVectorScale[0];
    reflectionsConstants.motionVectorScale[1] = params->motionVectorScale[1];
    reflectionsConstants.normalsUnpackMul = params->normalsUnpackMul;
    reflectionsConstants.normalsUnpackAdd = params->normalsUnpackAdd;
    reflectionsConstants.roughnessChannel = params->roughnessChannel;
    reflectionsConstants.isRoughnessPerceptual = params->isRoughnessPerceptual;

    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(
        &context->contextDescription.backendInterface, &reflectionsConstants, sizeof(reflectionsConstants), &context->reflectionsConstants);

    scheduleDispatch(context, &context->reflectionsClassifierPipeline, FFX_DIVIDE_ROUNDING_UP(width, 8u), FFX_DIVIDE_ROUNDING_UP(height, 8u));

    // Execute all jobs up to date so resources will be in the correct state when importing into the denoiser
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    // release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    return FFX_OK;
}

FFX_API FfxErrorCode ffxClassifierContextReflectionDispatch(FfxClassifierContext* pContext, const FfxClassifierReflectionDispatchDescription* pDispatchDescription)
{
    FFX_RETURN_ON_ERROR(pContext, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(pDispatchDescription, FFX_ERROR_INVALID_POINTER);

    FfxClassifierContext_Private* contextPrivate = (FfxClassifierContext_Private*)(pContext);

    // validate that renderSize is within the maximum.
    FFX_RETURN_ON_ERROR(pDispatchDescription->renderSize.width <= contextPrivate->contextDescription.resolution.width, FFX_ERROR_OUT_OF_RANGE);
    FFX_RETURN_ON_ERROR(pDispatchDescription->renderSize.height <= contextPrivate->contextDescription.resolution.height, FFX_ERROR_OUT_OF_RANGE);
    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the HSR passes.
    const FfxErrorCode errorCode = classifierDispatchReflections(contextPrivate, pDispatchDescription);
    return errorCode;
}

FfxErrorCode ffxClassifierContextDestroy(FfxClassifierContext* context)
{
    FFX_RETURN_ON_ERROR(
        context,
        FFX_ERROR_INVALID_POINTER);
    
    // Destroy the context.
    FfxClassifierContext_Private* contextPrivate = reinterpret_cast<FfxClassifierContext_Private*>(context);
    return ClassifierRelease(contextPrivate);
}

FfxErrorCode ffxClassifierContextShadowDispatch(FfxClassifierContext* context, const FfxClassifierShadowDispatchDescription* dispatchDescription)
{
    // check pointers are valid
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchDescription, FFX_ERROR_INVALID_POINTER);

    FfxClassifierContext_Private* contextPrivate = reinterpret_cast<FfxClassifierContext_Private*>(context);

    FFX_RETURN_ON_ERROR(
        contextPrivate->device,
        FFX_ERROR_NULL_DEVICE);

    return shadowClassifierDispatch(contextPrivate, dispatchDescription);
}

FFX_API FfxVersionNumber ffxClassifierGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_CLASSIFIER_VERSION_MAJOR, FFX_CLASSIFIER_VERSION_MINOR, FFX_CLASSIFIER_VERSION_PATCH);
}
