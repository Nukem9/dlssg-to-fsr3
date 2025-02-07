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

#include <FidelityFX/host/ffx_denoiser.h>
#include <FidelityFX/gpu/denoiser/ffx_denoiser_resources.h>
#include <ffx_object_management.h>

#include "ffx_denoiser_private.h"

// Tile size for the shadow denoiser is hardcoded to (8x4)
constexpr uint32_t k_tileSizeX = 8;
constexpr uint32_t k_tileSizeY = 4;

// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t    index;
    wchar_t     name[64];
}ResourceBinding;

static const ResourceBinding srvTextureBindingTable[] =
{
    {FFX_DENOISER_RESOURCE_IDENTIFIER_HIT_MASK_RESULTS,         L"r_hit_mask_results"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_DEPTH,                    L"r_depth"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_VELOCITY,                 L"r_velocity"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL,                   L"r_normal"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_HISTORY,                  L"r_history"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_PREVIOUS_DEPTH,           L"r_previous_depth"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_PREVIOUS_MOMENTS,         L"r_previous_moments"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL_FP16,              L"r_fp16_normal"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_FILTER_INPUT,             L"r_filter_input"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_DEPTH_HIERARCHY,    L"r_input_depth_hierarchy"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS,     L"r_input_motion_vectors"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_NORMAL,             L"r_input_normal"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE,                 L"r_radiance"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_HISTORY,         L"r_radiance_history"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE,                 L"r_variance"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT,             L"r_sample_count"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE,         L"r_average_radiance"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS,      L"r_extracted_roughness"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_DEPTH_HISTORY,            L"r_depth_history"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL_HISTORY,           L"r_normal_history"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_ROUGHNESS_HISTORY,        L"r_roughness_history"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_REPROJECTED_RADIANCE,     L"r_reprojected_radiance"},
};

static const ResourceBinding srvBufferBindingTable[] = {
    {FFX_DENOISER_RESOURCE_IDENTIFIER_RAYTRACER_RESULT, L"sb_raytracer_result"},
};

static const ResourceBinding uavBufferBindingTable[] =
{
    {FFX_DENOISER_RESOURCE_IDENTIFIER_SHADOW_MASK,          L"rw_shadow_mask"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_RAYTRACER_RESULT,     L"rw_raytracer_result"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_TILE_META_DATA,       L"rw_tile_metadata"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST,   L"rw_denoiser_tile_list"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_INDIRECT_ARGS,        L"rw_indirect_args"},

};

static const ResourceBinding uavTextureBindingTable[] =
{
    {FFX_DENOISER_RESOURCE_IDENTIFIER_FILTER_OUTPUT,            L"rw_filter_output"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_REPROJECTION_RESULTS,     L"rw_reprojection_results"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_CURRENT_MOMENTS,          L"rw_current_moments"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_HISTORY,                  L"rw_history"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE,                 L"rw_radiance"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE,                 L"rw_variance"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT,             L"rw_sample_count"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE,         L"rw_average_radiance"},
    {FFX_DENOISER_RESOURCE_IDENTIFIER_REPROJECTED_RADIANCE,     L"rw_reprojected_radiance"},
};

static const ResourceBinding cbResourceBindingTable[] =
{
    {FFX_DENOISER_SHADOWS_CONSTANTBUFFER_IDENTIFIER_DENOISER_SHADOWS0,  L"cb0DenoiserShadows"},
    {FFX_DENOISER_SHADOWS_CONSTANTBUFFER_IDENTIFIER_DENOISER_SHADOWS1,  L"cb1DenoiserShadows"},
    {FFX_DENOISER_SHADOWS_CONSTANTBUFFER_IDENTIFIER_DENOISER_SHADOWS2,  L"cb2DenoiserShadows"},
    {FFX_DENOISER_REFLECTIONS_CONSTANTBUFFER_IDENTIFIER,                L"cbDenoiserReflections"},

};

typedef struct DenoiserReflectionsConstants
{
    float       invProjection[16];
    float       invView[16];
    float       prevViewProjection[16];
    uint32_t    renderSize[2];
    float       inverseRenderSize[2];
    float       motionVectorScale[2];
    float       normalsUnpackMul;
    float       normalsUnpackAdd;
    bool        isRoughnessPerceptual;
    float       temporalStabilityFactor;
    float       roughnessThreshold;
} DenoiserReflectionsConstants;

typedef struct DenoiserShadowsTileClassificationConstants {

    float   eye[3];
    int     isFirstFrame;
    int     bufferDimensions[2];
    float   invBufferDimensions[2];
    float   motionVectorScale[2];
    float   normalsUnpackMul_unpackAdd[2];
    float   projectionInverse[16];
    float   reprojectionMatrix[16];
    float   viewProjectionInverse[16];
} DenoiserShadowsTileClassificationConstants;

typedef struct DenoiserShadowsFilterConstants {

    float   projectionInverse[16];
    float   invBufferDimensions[2];
    float   normalsUnpackMul_unpackAdd[2];
    int     bufferDimensions[2];
    float   depthSimilaritySigma;
    float   pad[1];
} DenoiserShadowsFilterConstants;

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

    // Buffer srvs
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

static uint32_t getPipelinePermutationFlags(uint32_t contextFlags, bool fp16, bool force64)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (force64) ? DENOISER_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (fp16) ? DENOISER_SHADER_PERMUTATION_ALLOW_FP16 : 0;
    flags |= (contextFlags & FFX_DENOISER_ENABLE_DEPTH_INVERTED) ? DENOISER_SHADER_PERMUTATION_DEPTH_INVERTED : 0;
    return flags;
}

static FfxErrorCode createShadowsPipelineStates(FfxDenoiserContext_Private* context)
{
    FFX_ASSERT(context);

    const size_t samplerCount = 1;
    FfxSamplerDescription samplerDescs[samplerCount] = { { FFX_FILTER_TYPE_MINMAGMIP_LINEAR, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_BIND_COMPUTE_SHADER_STAGE } };

    const size_t rootConstantCount = 3;
    uint32_t rootConstants[rootConstantCount] = {
        DENOISER_SHADOWS_CONSTANT_BUFFER_0_SIZE,
        DENOISER_SHADOWS_CONSTANT_BUFFER_1_SIZE,
        DENOISER_SHADOWS_CONSTANT_BUFFER_2_SIZE
    };
    FfxRootConstantDescription rootConstantDesc[rootConstantCount] = { { DENOISER_SHADOWS_CONSTANT_BUFFER_0_SIZE, FFX_BIND_COMPUTE_SHADER_STAGE },
                                                                       { DENOISER_SHADOWS_CONSTANT_BUFFER_1_SIZE, FFX_BIND_COMPUTE_SHADER_STAGE },
                                                                       { DENOISER_SHADOWS_CONSTANT_BUFFER_2_SIZE, FFX_BIND_COMPUTE_SHADER_STAGE } };

    FfxPipelineDescription pipelineDescription{};
    pipelineDescription.contextFlags = context->contextDescription.flags;
    pipelineDescription.samplerCount = samplerCount;
    pipelineDescription.samplers = samplerDescs;
    pipelineDescription.rootConstantBufferCount = rootConstantCount;
    pipelineDescription.rootConstants = rootConstantDesc;

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
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DENOISER, FFX_DENOISER_PASS_PREPARE_SHADOW_MASK,
        getPipelinePermutationFlags(contextFlags, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId, &context->pipelinePrepareShadowMask));
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DENOISER, FFX_DENOISER_PASS_SHADOWS_TILE_CLASSIFICATION,
        getPipelinePermutationFlags(contextFlags, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId, &context->pipelineTileClassification));

    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DENOISER, FFX_DENOISER_PASS_FILTER_SOFT_SHADOWS_0,
        getPipelinePermutationFlags(contextFlags, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId, &context->pipelineFilterSoftShadows0));
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DENOISER, FFX_DENOISER_PASS_FILTER_SOFT_SHADOWS_1,
        getPipelinePermutationFlags(contextFlags, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId, &context->pipelineFilterSoftShadows1));
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DENOISER, FFX_DENOISER_PASS_FILTER_SOFT_SHADOWS_2,
        getPipelinePermutationFlags(contextFlags, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId, &context->pipelineFilterSoftShadows2));

    // For each pipeline: re-route/fix-up IDs based on names
    patchResourceBindings(&context->pipelinePrepareShadowMask);
    patchResourceBindings(&context->pipelineTileClassification);
    patchResourceBindings(&context->pipelineFilterSoftShadows0);
    patchResourceBindings(&context->pipelineFilterSoftShadows1);
    patchResourceBindings(&context->pipelineFilterSoftShadows2);

    return FFX_OK;
}

static FfxErrorCode createReflectionsPipelineStates(FfxDenoiserContext_Private* context)
{
    FFX_ASSERT(context);

    const size_t samplerCount = 1;
    FfxSamplerDescription samplerDescs[samplerCount] = { { FFX_FILTER_TYPE_MINMAGLINEARMIP_POINT, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_BIND_COMPUTE_SHADER_STAGE } };
    FfxRootConstantDescription rootConstantDesc = { sizeof(DenoiserReflectionsConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE };

    FfxPipelineDescription pipelineDescription = {};
    pipelineDescription.contextFlags = 0;
    pipelineDescription.samplerCount = samplerCount;
    pipelineDescription.samplers = samplerDescs;
    pipelineDescription.rootConstantBufferCount = 1;
    pipelineDescription.rootConstants = &rootConstantDesc;
    pipelineDescription.indirectWorkload = 1;
    pipelineDescription.stage = FFX_BIND_COMPUTE_SHADER_STAGE;

    // Query device capabilities
    FfxDevice             device = context->contextDescription.backendInterface.device;
    FfxDeviceCapabilities capabilities;
    context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &capabilities);

    // Setup a few options used to determine permutation flags
    bool haveShaderModel66 = capabilities.maximumSupportedShaderModel >= FFX_SHADER_MODEL_6_6;
    bool supportedFP16 = capabilities.fp16Supported;
    bool canForceWave64 = false;
    bool useLut = false;

    const uint32_t waveLaneCountMin = capabilities.waveLaneCountMin;
    const uint32_t waveLaneCountMax = capabilities.waveLaneCountMax;
    if (waveLaneCountMin <= 64 && waveLaneCountMax >= 64)
    {
        useLut = true;
        canForceWave64 = haveShaderModel66;
    }
    else
    {
        canForceWave64 = false;
    }

    // Indirect workloads
    wcscpy_s(pipelineDescription.name, L"DENOISER-REFLECTIONS_REPROJECT");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DENOISER, FFX_DENOISER_PASS_REPROJECT_REFLECTIONS,
        getPipelinePermutationFlags(FFX_DENOISER_PASS_REPROJECT_REFLECTIONS, supportedFP16, canForceWave64), &pipelineDescription, context->effectContextId, &context->pipelineReprojectReflections));
    wcscpy_s(pipelineDescription.name, L"DENOISER-REFLECTIONS_PREFILTER");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DENOISER, FFX_DENOISER_PASS_PREFILTER_REFLECTIONS,
        getPipelinePermutationFlags(FFX_DENOISER_PASS_PREFILTER_REFLECTIONS, supportedFP16, canForceWave64), &pipelineDescription, context->effectContextId, &context->pipelinePrefilterReflections));
    wcscpy_s(pipelineDescription.name, L"DENOISER-REFLECTIONS_RESOLVE_TEMPORAL");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_DENOISER, FFX_DENOISER_PASS_RESOLVE_TEMPORAL_REFLECTIONS,
        getPipelinePermutationFlags(FFX_DENOISER_PASS_RESOLVE_TEMPORAL_REFLECTIONS, supportedFP16, canForceWave64), &pipelineDescription, context->effectContextId, &context->pipelineResolveTemporalReflections));

    // for each pipeline: re-route/fix-up IDs based on names
    FFX_ASSERT(patchResourceBindings(&context->pipelineReprojectReflections) == FFX_OK);
    FFX_ASSERT(patchResourceBindings(&context->pipelinePrefilterReflections) == FFX_OK);
    FFX_ASSERT(patchResourceBindings(&context->pipelineResolveTemporalReflections) == FFX_OK);

    return FFX_OK;
}

static void populateReflectionsJobResources(FfxDenoiserContext_Private* context, const FfxPipelineState* pipeline, FfxComputeJobDescription* jobDescriptor)
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

    // Constant buffers
    for (uint32_t currentRootConstantIndex = 0; currentRootConstantIndex < pipeline->constCount; ++currentRootConstantIndex) {
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor->cbNames[currentRootConstantIndex], pipeline->constantBufferBindings[currentRootConstantIndex].name);
#endif
        jobDescriptor->cbs[currentRootConstantIndex] = context->reflectionsConstants[pipeline->constantBufferBindings[currentRootConstantIndex].resourceIdentifier];
    }
}

static void scheduleIndirectReflectionsDispatch(FfxDenoiserContext_Private* context, const FfxPipelineState* pipeline, const FfxResourceInternal* commandArgument, const uint32_t offset = 0)
{
    FfxComputeJobDescription jobDescriptor = {};
    jobDescriptor.pipeline = *pipeline;
    jobDescriptor.cmdArgument = *commandArgument;
    jobDescriptor.cmdArgumentOffset = offset;
    populateReflectionsJobResources(context, pipeline, &jobDescriptor);

    FfxGpuJobDescription dispatchJob = { FFX_GPU_JOB_COMPUTE };
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);
    dispatchJob.computeJobDescriptor = jobDescriptor;
    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchJob);
}

static void scheduleDispatch(FfxDenoiserContext_Private* context, const FfxDenoiserShadowsDispatchDescription* params, const FfxPipelineState* pipeline, uint32_t dispatchX, uint32_t dispatchY)
{
    FfxComputeJobDescription jobDescriptor = {};

    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex) {

        const uint32_t currentResourceId = pipeline->srvTextureBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = context->srvResources[currentResourceId];
        jobDescriptor.srvTextures[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor.srvTextures[currentShaderResourceViewIndex].name, pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex) {

        const uint32_t currentResourceId = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor.uavTextures[currentUnorderedAccessViewIndex].name, pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].name);
#endif

        const FfxResourceInternal currentResource                           = context->uavResources[currentResourceId];
        jobDescriptor.uavTextures[currentUnorderedAccessViewIndex].resource = currentResource;
        jobDescriptor.uavTextures[currentUnorderedAccessViewIndex].mip      = 0;
    }

    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavBufferCount; ++currentUnorderedAccessViewIndex) {

        const uint32_t currentResourceId = pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = context->uavResources[currentResourceId];
        jobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].name, pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].name);
#endif
    }

    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvBufferCount; ++currentShaderResourceViewIndex) {

        const uint32_t currentResourceId = pipeline->srvBufferBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = context->srvResources[currentResourceId];
        jobDescriptor.srvBuffers[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor.srvBuffers[currentShaderResourceViewIndex].name, pipeline->srvBufferBindings[currentShaderResourceViewIndex].name);
#endif
    }

    jobDescriptor.dimensions[0] = dispatchX;
    jobDescriptor.dimensions[1] = dispatchY;
    jobDescriptor.dimensions[2] = 1;
    jobDescriptor.pipeline      = *pipeline;

    for (uint32_t currentRootConstantIndex = 0; currentRootConstantIndex < pipeline->constCount; ++currentRootConstantIndex) {
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor.cbNames[currentRootConstantIndex], pipeline->constantBufferBindings[currentRootConstantIndex].name);
#endif
        jobDescriptor.cbs[currentRootConstantIndex] = context->shadowsConstants[pipeline->constantBufferBindings[currentRootConstantIndex].resourceIdentifier];
    }

    FfxGpuJobDescription dispatchJob = {FFX_GPU_JOB_COMPUTE};
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);
    dispatchJob.computeJobDescriptor = jobDescriptor;

    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchJob);
}

static FfxErrorCode denoiserDispatchShadows(FfxDenoiserContext_Private* context, const FfxDenoiserShadowsDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;

    // Register resources for frame
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->hitMaskResults,
        context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_HIT_MASK_RESULTS]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->depth,
        context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_DEPTH]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->velocity,
        context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VELOCITY]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->normal,
        context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->shadowMaskOutput,
        context->effectContextId, &context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_FILTER_OUTPUT]);

    // Set aliased resource view
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL_FP16] = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SHADOW_MASK] = context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_TILE_BUFFER];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RAYTRACER_RESULT] = context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_TILE_BUFFER];
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RAYTRACER_RESULT] = context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_TILE_BUFFER];
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_HISTORY] = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH1];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_REPROJECTION_RESULTS] = context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH0];

    if (context->isFirstShadowFrame)
    {
        FfxGpuJobDescription job        = {};
        job.jobType                     = FFX_GPU_JOB_CLEAR_FLOAT;
        wcscpy_s(job.jobLabel, L"Clear shadow map");
        job.clearJobDescriptor.color[0] = 0.0f;
        job.clearJobDescriptor.color[1] = 0.0f;
        job.clearJobDescriptor.color[2] = 0.0f;
        job.clearJobDescriptor.color[3] = 0.0f;

        uint32_t resourceIDs[] = {
            FFX_DENOISER_RESOURCE_IDENTIFIER_MOMENTS0,
            FFX_DENOISER_RESOURCE_IDENTIFIER_MOMENTS1,
            FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH0,
            FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH1,
        };

        for (const uint32_t* resourceID = resourceIDs; resourceID < resourceIDs + FFX_ARRAY_ELEMENTS(resourceIDs); ++resourceID)
        {
            job.clearJobDescriptor.target = context->uavResources[*resourceID];
            context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &job);
        }
    }

    // Get DenoiserShadows info for run
    uint32_t bufferDimensions[2] = {context->contextDescription.windowSize.width, context->contextDescription.windowSize.height};
    float invBufferDimensions[2] = {1.f / bufferDimensions[0], 1.f / bufferDimensions[1]};
    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(&context->contextDescription.backendInterface,
                                                                               &bufferDimensions,
                                                                               DENOISER_SHADOWS_CONSTANT_BUFFER_0_SIZE * sizeof(uint32_t),
                                                                               &context->shadowsConstants[0]);
    
    DenoiserShadowsTileClassificationConstants tileClassificationConstants;
    memcpy(&tileClassificationConstants.eye, params->eye, sizeof(params->eye));
    tileClassificationConstants.isFirstFrame = context->isFirstShadowFrame;
    memcpy(&tileClassificationConstants.bufferDimensions, bufferDimensions, sizeof(bufferDimensions));
    memcpy(&tileClassificationConstants.invBufferDimensions, invBufferDimensions, sizeof(invBufferDimensions));
    memcpy(&tileClassificationConstants.motionVectorScale, params->motionVectorScale, sizeof(params->motionVectorScale));
    float local_normalsUnpackMul_unpackAdd[2] = {params->normalsUnpackMul, params->normalsUnpackAdd};
    memcpy(&tileClassificationConstants.normalsUnpackMul_unpackAdd, &local_normalsUnpackMul_unpackAdd, sizeof(local_normalsUnpackMul_unpackAdd));
    memcpy(&tileClassificationConstants.projectionInverse, &params->projectionInverse, sizeof(params->projectionInverse));
    memcpy(&tileClassificationConstants.reprojectionMatrix, &params->reprojectionMatrix, sizeof(params->reprojectionMatrix));
    memcpy(&tileClassificationConstants.viewProjectionInverse, &params->viewProjectionInverse, sizeof(params->viewProjectionInverse));

    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(&context->contextDescription.backendInterface,
                                                                               &tileClassificationConstants,
                                                                               DENOISER_SHADOWS_CONSTANT_BUFFER_1_SIZE * sizeof(uint32_t),
                                                                               &context->shadowsConstants[1]);

    uint32_t dispatchX = FFX_DIVIDE_ROUNDING_UP(context->contextDescription.windowSize.width, k_tileSizeX);
    uint32_t dispatchY = FFX_DIVIDE_ROUNDING_UP(context->contextDescription.windowSize.height, k_tileSizeY);
    uint32_t dispatchZ = 1;

    const uint32_t tileX2 = k_tileSizeX * 4;
    const uint32_t tileY2 = k_tileSizeY * 4;
    uint32_t const ThreadGroupCountX2 = FFX_DIVIDE_ROUNDING_UP(context->contextDescription.windowSize.width, tileX2);
    uint32_t const ThreadGroupCountY2 = FFX_DIVIDE_ROUNDING_UP(context->contextDescription.windowSize.height, tileY2);
    scheduleDispatch(context, params, &context->pipelinePrepareShadowMask, ThreadGroupCountX2, ThreadGroupCountY2);

    // Update moments ping-pong buffer
    const bool isEvenFrame = !(params->frameIndex & 1);
    if(isEvenFrame)
    {
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_PREVIOUS_MOMENTS] = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_MOMENTS0];
        context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_CURRENT_MOMENTS] = context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_MOMENTS1];
    }
    else
    {
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_PREVIOUS_MOMENTS] = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_MOMENTS1];
        context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_CURRENT_MOMENTS] = context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_MOMENTS0];
    }

    scheduleDispatch(context, params,&context->pipelineTileClassification, dispatchX, dispatchY);

    // Copy current depth to previous depth
    FfxGpuJobDescription copyJob = { FFX_GPU_JOB_COPY };
    wcscpy_s(copyJob.jobLabel, L"Copy current depth -> previous depth");
    copyJob.copyJobDescriptor.src       = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_DEPTH];
    copyJob.copyJobDescriptor.dst       = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_PREVIOUS_DEPTH];
    copyJob.copyJobDescriptor.srcOffset = 0; 
    copyJob.copyJobDescriptor.dstOffset = 0;
    copyJob.copyJobDescriptor.size      = 0;
    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &copyJob);
    
    DenoiserShadowsFilterConstants filterConstants;
    memcpy(&filterConstants.bufferDimensions, bufferDimensions, sizeof(bufferDimensions));
    memcpy(&filterConstants.invBufferDimensions, invBufferDimensions, sizeof(invBufferDimensions));
    memcpy(&filterConstants.normalsUnpackMul_unpackAdd, &local_normalsUnpackMul_unpackAdd, sizeof(local_normalsUnpackMul_unpackAdd));
    memcpy(&filterConstants.projectionInverse, &params->projectionInverse, sizeof(params->projectionInverse));
    filterConstants.depthSimilaritySigma = params->depthSimilaritySigma;
    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(&context->contextDescription.backendInterface,
                                                                               &filterConstants,
                                                                               DENOISER_SHADOWS_CONSTANT_BUFFER_2_SIZE * sizeof(uint32_t),
                                                                               &context->shadowsConstants[2]);
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_FILTER_INPUT] = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH0];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_HISTORY] = context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH1];
    scheduleDispatch(context, params,&context->pipelineFilterSoftShadows0, dispatchX, dispatchY);

    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_FILTER_INPUT] = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH1];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_HISTORY] = context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH0];
    scheduleDispatch(context, params,&context->pipelineFilterSoftShadows1, dispatchX, dispatchY);

    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_FILTER_INPUT] = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH0];
    scheduleDispatch(context, params,&context->pipelineFilterSoftShadows2, dispatchX, dispatchY);

    // Execute all the work for the frame
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    // Release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    context->isFirstShadowFrame = false;

    return FFX_OK;
}


static FfxErrorCode denoiserDispatchReflections(FfxDenoiserContext_Private* context, const FfxDenoiserReflectionsDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;

    // Prepare per frame descriptor tables
    const bool isOddFrame = !!(params->frameIndex & 1);

    const uint32_t radianceAResourceIndex           = FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_0;
    const uint32_t radianceHistorySrvResourceIndex  = FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_1;
    const uint32_t varianceAResourceIndex           = FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_1;
    const uint32_t sampleCountSrvResourceIndex      = isOddFrame ? FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT_1      : FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT_0;
    const uint32_t averageRadianceSrvResourceIndex  = isOddFrame ? FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_1  : FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_0;

    const uint32_t radianceBResourceIndex           = FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_1;
    const uint32_t varianceBResourceIndex           = FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_0;
    const uint32_t sampleCountUavResourceIndex      = isOddFrame ? FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT_0      : FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT_1;
    const uint32_t averageRadianceUavResourceIndex  = isOddFrame ? FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_0  : FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_1;

    // zero initialise resources on first frame
    if (context->isFirstReflectionsFrame) {
        FfxGpuJobDescription job = {};
        job.jobType = FFX_GPU_JOB_CLEAR_FLOAT;
        wcscpy_s(job.jobLabel, L"Zero initialize resource");
        job.clearJobDescriptor.color[0] = 0.0f;
        job.clearJobDescriptor.color[1] = 0.0f;
        job.clearJobDescriptor.color[2] = 0.0f;
        job.clearJobDescriptor.color[3] = 0.0f;

        uint32_t resourceIDs[] = {
            FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT_0,
            FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT_1,
            FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_0,
            FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_1,
            FFX_DENOISER_RESOURCE_IDENTIFIER_REPROJECTED_RADIANCE,
        };

        for (const uint32_t *resourceID = resourceIDs; resourceID < resourceIDs + FFX_ARRAY_ELEMENTS(resourceIDs); ++resourceID) {
            job.clearJobDescriptor.target = context->uavResources[*resourceID];
            context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &job);
        }

        context->isFirstReflectionsFrame = false;
    }

    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->depthHierarchy,             context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_DEPTH_HIERARCHY]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->motionVectors,              context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->normal,                     context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_NORMAL]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->radianceA,                  context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_0]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->radianceB,                  context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_1]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->varianceA,                  context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_0]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->varianceB,                  context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_1]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->extractedRoughness,         context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->denoiserTileList,           context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->indirectArgumentsBuffer,    context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INDIRECT_ARGS]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->output,                     context->effectContextId, &context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_OUTPUT]);

    // Don't need to register it twice
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_0]          = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_0];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_1]          = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_1];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_0]          = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_0];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_1]          = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_1];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS] = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST]  = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INDIRECT_ARGS]       = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INDIRECT_ARGS];

    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE]            = context->srvResources[radianceAResourceIndex];
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_HISTORY]    = context->srvResources[radianceHistorySrvResourceIndex];
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE]            = context->srvResources[varianceAResourceIndex];
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT]        = context->srvResources[sampleCountSrvResourceIndex];
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE]    = context->srvResources[averageRadianceSrvResourceIndex];

    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE]            = context->uavResources[radianceBResourceIndex];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE]            = context->uavResources[varianceBResourceIndex];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT]        = context->uavResources[sampleCountUavResourceIndex];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE]    = context->uavResources[averageRadianceUavResourceIndex];

    if (params->reset)
    {
        FfxGpuJobDescription job = {};
        job.jobType = FFX_GPU_JOB_CLEAR_FLOAT;
        wcscpy_s(job.jobLabel, L"Zero initialize resource");
        job.clearJobDescriptor.color[0] = 0.0f;
        job.clearJobDescriptor.color[1] = 0.0f;
        job.clearJobDescriptor.color[2] = 0.0f;
        job.clearJobDescriptor.color[3] = 0.0f;

        uint32_t resourceIDs[] = {
            FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_0,
            FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_1,
        };

        for (const uint32_t* resourceID = resourceIDs; resourceID < resourceIDs + FFX_ARRAY_ELEMENTS(resourceIDs); ++resourceID)
        {
            job.clearJobDescriptor.target = context->uavResources[*resourceID];
            context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &job);
        }
    }

    DenoiserReflectionsConstants reflectionsConstants{};
    memcpy(&reflectionsConstants.invProjection, &params->invProjection, sizeof(float) * 16 * 3);
    reflectionsConstants.renderSize[0]             = context->contextDescription.windowSize.width;
    reflectionsConstants.renderSize[1]             = context->contextDescription.windowSize.height;
    reflectionsConstants.inverseRenderSize[0]      = 1.0f / context->contextDescription.windowSize.width;
    reflectionsConstants.inverseRenderSize[1]      = 1.0f / context->contextDescription.windowSize.height;
    reflectionsConstants.motionVectorScale[0]      = params->motionVectorScale.x;
    reflectionsConstants.motionVectorScale[1]      = params->motionVectorScale.y;
    reflectionsConstants.normalsUnpackMul          = params->normalsUnpackMul;
    reflectionsConstants.normalsUnpackAdd          = params->normalsUnpackAdd;
    reflectionsConstants.isRoughnessPerceptual     = params->isRoughnessPerceptual;
    reflectionsConstants.temporalStabilityFactor   = params->temporalStabilityFactor;
    reflectionsConstants.roughnessThreshold        = params->roughnessThreshold;

    // initialize constantBuffers data
    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(
        &context->contextDescription.backendInterface,
        &reflectionsConstants,
        sizeof(reflectionsConstants),
        &context->reflectionsConstants[FFX_DENOISER_REFLECTIONS_CONSTANTBUFFER_IDENTIFIER]);

    // Denoising
    scheduleIndirectReflectionsDispatch(context, &context->pipelineReprojectReflections, &context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INDIRECT_ARGS], 12);

    // Ping-Pong variance targets
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE] = context->srvResources[varianceBResourceIndex];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE] = context->srvResources[varianceAResourceIndex];
    scheduleIndirectReflectionsDispatch(context, &context->pipelinePrefilterReflections, &context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INDIRECT_ARGS], 12);

    // Ping-Pong variance & radiance targets
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE] = context->srvResources[radianceBResourceIndex];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE] = context->uavResources[radianceAResourceIndex];
    context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE] = context->srvResources[varianceAResourceIndex];
    context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE] = context->srvResources[varianceBResourceIndex];
    scheduleIndirectReflectionsDispatch(context, &context->pipelineResolveTemporalReflections, &context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INDIRECT_ARGS], 12);

    // Copy Final result to output target
    FfxGpuJobDescription dispatchCopyJobDescriptor = { FFX_GPU_JOB_COPY };
    wcscpy_s(dispatchCopyJobDescriptor.jobLabel, L"Copy to output");
    dispatchCopyJobDescriptor.copyJobDescriptor.src = context->srvResources[radianceAResourceIndex];
    dispatchCopyJobDescriptor.copyJobDescriptor.dst = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_OUTPUT];
    dispatchCopyJobDescriptor.copyJobDescriptor.srcOffset = 0;
    dispatchCopyJobDescriptor.copyJobDescriptor.dstOffset = 0;
    dispatchCopyJobDescriptor.copyJobDescriptor.size      = 0;
    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchCopyJobDescriptor);

    // Normal history
    dispatchCopyJobDescriptor.copyJobDescriptor.src = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_NORMAL];
    dispatchCopyJobDescriptor.copyJobDescriptor.dst = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL_HISTORY];
    dispatchCopyJobDescriptor.copyJobDescriptor.srcOffset = 0;
    dispatchCopyJobDescriptor.copyJobDescriptor.dstOffset = 0;
    dispatchCopyJobDescriptor.copyJobDescriptor.size      = 0;
    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchCopyJobDescriptor);

    // Roughness history
    dispatchCopyJobDescriptor.copyJobDescriptor.src = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS];
    dispatchCopyJobDescriptor.copyJobDescriptor.dst = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_ROUGHNESS_HISTORY];
    dispatchCopyJobDescriptor.copyJobDescriptor.srcOffset = 0;
    dispatchCopyJobDescriptor.copyJobDescriptor.dstOffset = 0;
    dispatchCopyJobDescriptor.copyJobDescriptor.size      = 0;
    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchCopyJobDescriptor);

    // Depth history
    dispatchCopyJobDescriptor.copyJobDescriptor.src = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_DEPTH_HIERARCHY];
    dispatchCopyJobDescriptor.copyJobDescriptor.dst = context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_DEPTH_HISTORY];
    dispatchCopyJobDescriptor.copyJobDescriptor.srcOffset = 0;
    dispatchCopyJobDescriptor.copyJobDescriptor.dstOffset = 0;
    dispatchCopyJobDescriptor.copyJobDescriptor.size      = 0;
    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchCopyJobDescriptor);

    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    // release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    return FFX_OK;
}

static FfxErrorCode denoiserShadowsCreateResources(FfxDenoiserContext_Private* context, const FfxDenoiserContextDescription* contextDescription)
{
    // Clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    uint32_t const tileCount = FFX_DIVIDE_ROUNDING_UP(contextDescription->windowSize.width, k_tileSizeX) * FFX_DIVIDE_ROUNDING_UP(contextDescription->windowSize.height, k_tileSizeY);

    // Declare internal resources needed
    const FfxInternalResourceDescription internalSurfaceDesc[] = {{FFX_DENOISER_RESOURCE_IDENTIFIER_PREVIOUS_DEPTH,
                                                                   L"DenoiserShadows_PreviousDepth",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_READ_ONLY,
                                                                   FFX_SURFACE_FORMAT_R32_FLOAT,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_MOMENTS0,
                                                                   L"DenoiserShadows_Moments0",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_R11G11B10_FLOAT,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_MOMENTS1,
                                                                   L"DenoiserShadows_Moments1",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_R11G11B10_FLOAT,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH0,
                                                                   L"DenoiserShadows_Scratch0",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_R16G16_FLOAT,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH1,
                                                                   L"DenoiserShadows_Scratch1",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_R16G16_FLOAT,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_TILE_BUFFER,
                                                                   L"DenoiserShadows_TileBuffer",
                                                                   FFX_RESOURCE_TYPE_BUFFER,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_UNKNOWN,
                                                                   sizeof(uint32_t) * tileCount,
                                                                   sizeof(uint32_t),
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_TILE_META_DATA,
                                                                   L"DenoiserShadows_TileMetadata",
                                                                   FFX_RESOURCE_TYPE_BUFFER,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_UNKNOWN,
                                                                   sizeof(uint32_t) * tileCount,
                                                                   sizeof(uint32_t),
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}}};

    for (int32_t currentSurfaceIndex = 0; currentSurfaceIndex < FFX_ARRAY_ELEMENTS(internalSurfaceDesc); ++currentSurfaceIndex) {

        const FfxInternalResourceDescription* currentSurfaceDescription = &internalSurfaceDesc[currentSurfaceIndex];
        const bool isBuffer = currentSurfaceDescription->format == FFX_SURFACE_FORMAT_UNKNOWN;
        uint32_t depth = 1, alignment = 0;
        const FfxResourceDescription resourceDescription = { currentSurfaceDescription->type, currentSurfaceDescription->format, currentSurfaceDescription->width, currentSurfaceDescription->height, isBuffer ? alignment : depth, isBuffer ? 0 : currentSurfaceDescription->mipCount, currentSurfaceDescription->flags, currentSurfaceDescription->usage };
        const FfxResourceStates initialState = (currentSurfaceDescription->usage == FFX_RESOURCE_USAGE_READ_ONLY) ? FFX_RESOURCE_STATE_COMPUTE_READ : FFX_RESOURCE_STATE_UNORDERED_ACCESS;
        const FfxCreateResourceDescription createResourceDescription = { FFX_HEAP_TYPE_DEFAULT, resourceDescription, initialState, currentSurfaceDescription->name, currentSurfaceDescription->id, currentSurfaceDescription->initData };

        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface, &createResourceDescription, context->effectContextId, &context->srvResources[currentSurfaceDescription->id]));
    }

    // And copy resources to uavResrouces list
    memcpy(context->uavResources, context->srvResources, sizeof(context->srvResources));

    return FFX_OK;
}

static FfxErrorCode denoiserReflectionsCreateResources(FfxDenoiserContext_Private* context, const FfxDenoiserContextDescription* contextDescription)
{

    // Declare internal resources needed
    const FfxInternalResourceDescription internalSurfaceDesc[] = {{FFX_DENOISER_RESOURCE_IDENTIFIER_DEPTH_HISTORY,
                                                                   L"DENOISER_DepthHistory",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_READ_ONLY,
                                                                   FFX_SURFACE_FORMAT_R32_FLOAT,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL_HISTORY,
                                                                   L"DENOISER_NormalHistory",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_READ_ONLY,
                                                                   contextDescription->normalsHistoryBufferFormat,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_ROUGHNESS_HISTORY,
                                                                   L"DENOISER_RoughnessHistory",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_READ_ONLY,
                                                                   FFX_SURFACE_FORMAT_R8_UNORM,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT_0,
                                                                   L"DENOISER_SampleCount0",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_R16_FLOAT,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT_1,
                                                                   L"DENOISER_SampleCount1",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_R16_FLOAT,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_0,
                                                                   L"DENOISER_AverageRadiance0",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_R11G11B10_FLOAT,
                                                                   FFX_DIVIDE_ROUNDING_UP(contextDescription->windowSize.width, 8u),
                                                                   FFX_DIVIDE_ROUNDING_UP(contextDescription->windowSize.height, 8u),
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_1,
                                                                   L"DENOISER_AverageRadiance1",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_R11G11B10_FLOAT,
                                                                   FFX_DIVIDE_ROUNDING_UP(contextDescription->windowSize.width, 8u),
                                                                   FFX_DIVIDE_ROUNDING_UP(contextDescription->windowSize.height, 8u),
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

                                                                  {FFX_DENOISER_RESOURCE_IDENTIFIER_REPROJECTED_RADIANCE,
                                                                   L"DENOISER_ReprojectedRadiance",
                                                                   FFX_RESOURCE_TYPE_TEXTURE2D,
                                                                   FFX_RESOURCE_USAGE_UAV,
                                                                   FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT,
                                                                   contextDescription->windowSize.width,
                                                                   contextDescription->windowSize.height,
                                                                   1,
                                                                   FFX_RESOURCE_FLAGS_NONE,
                                                                   {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}}};

    // clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    for (int32_t currentSurfaceIndex = 0; currentSurfaceIndex < FFX_ARRAY_ELEMENTS(internalSurfaceDesc); ++currentSurfaceIndex) {

        const FfxInternalResourceDescription* currentSurfaceDescription = &internalSurfaceDesc[currentSurfaceIndex];
        const FfxResourceDescription resourceDescription = { currentSurfaceDescription->type, currentSurfaceDescription->format, currentSurfaceDescription->width, currentSurfaceDescription->height, currentSurfaceDescription->type == FFX_RESOURCE_TYPE_BUFFER ? 0u : 1u, currentSurfaceDescription->mipCount, FFX_RESOURCE_FLAGS_NONE, currentSurfaceDescription->usage };
        const FfxResourceStates initialState = (currentSurfaceDescription->usage == FFX_RESOURCE_USAGE_READ_ONLY) ? FFX_RESOURCE_STATE_COMPUTE_READ : FFX_RESOURCE_STATE_UNORDERED_ACCESS;
        const FfxCreateResourceDescription createResourceDescription = { FFX_HEAP_TYPE_DEFAULT, resourceDescription, initialState, currentSurfaceDescription->name, currentSurfaceDescription->id, currentSurfaceDescription->initData };

        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface, &createResourceDescription, context->effectContextId, &context->srvResources[currentSurfaceDescription->id]));
    }

    // copy resources to uavResrouces list
    memcpy(context->uavResources, context->srvResources, sizeof(context->srvResources));

    return FFX_OK;
}

static FfxErrorCode denoiserCreate(FfxDenoiserContext_Private* context, const FfxDenoiserContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxDenoiserContext_Private));
    context->device = contextDescription->backendInterface.device;
    context->isFirstShadowFrame = true;
    context->isFirstReflectionsFrame = true;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxDenoiserContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);

    // Create the device.
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_DENOISER, nullptr, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Call out for device caps.
    errorCode = context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &context->deviceCapabilities);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Create internal resources.
    if(contextDescription->flags & FFX_DENOISER_SHADOWS){
        context->shadowsConstants[0].num32BitEntries = DENOISER_SHADOWS_CONSTANT_BUFFER_0_SIZE;
        context->shadowsConstants[1].num32BitEntries = DENOISER_SHADOWS_CONSTANT_BUFFER_1_SIZE;
        context->shadowsConstants[2].num32BitEntries = DENOISER_SHADOWS_CONSTANT_BUFFER_2_SIZE;

        errorCode = denoiserShadowsCreateResources(context, contextDescription);
        FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

        // Create shaders on initialize.
        errorCode = createShadowsPipelineStates(context);
        FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);
    }
    if(contextDescription->flags & FFX_DENOISER_REFLECTIONS){
        context->reflectionsConstants[FFX_DENOISER_REFLECTIONS_CONSTANTBUFFER_IDENTIFIER].num32BitEntries = sizeof(DenoiserReflectionsConstants) / sizeof(uint32_t);
        errorCode = denoiserReflectionsCreateResources(context, contextDescription);
        FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

        // Create shaders on initialize.
        errorCode = createReflectionsPipelineStates(context);
        FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);
    }

    return FFX_OK;
}

static FfxErrorCode denoiserRelease(FfxDenoiserContext_Private* context)
{
    FFX_ASSERT(context);

    // Release denoiser shadows resources
    if(context->contextDescription.flags & FFX_DENOISER_SHADOWS){
        // Release all pipelines
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareShadowMask,  context->effectContextId);
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineTileClassification, context->effectContextId);
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineFilterSoftShadows0, context->effectContextId);
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineFilterSoftShadows1, context->effectContextId);
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineFilterSoftShadows2, context->effectContextId);

        // Unregister resources not created internally
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_HIT_MASK_RESULTS]    = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_DEPTH]               = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VELOCITY]            = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL]              = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL_FP16]         = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->uavResources[FFX_DENOISER_RESOURCE_IDENTIFIER_FILTER_OUTPUT]       = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
    }

    // Release denoiser reflections resources
    if(context->contextDescription.flags & FFX_DENOISER_REFLECTIONS){
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrefilterReflections, context->effectContextId);
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineReprojectReflections, context->effectContextId);
        ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineResolveTemporalReflections, context->effectContextId);

        // Unregister resources not created internally
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_DEPTH_HIERARCHY]   = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS]    = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_NORMAL]            = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE]                = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_0]              = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_1]              = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_HISTORY]        = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE]                = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_0]              = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_1]              = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT]            = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE]        = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST]      = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS]     = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_INDIRECT_ARGS]           = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
        context->srvResources[FFX_DENOISER_RESOURCE_IDENTIFIER_OUTPUT]                  = { FFX_DENOISER_RESOURCE_IDENTIFIER_NULL };
    }

    // release internal resources
    for (int32_t currentResourceIndex = 0; currentResourceIndex < FFX_DENOISER_RESOURCE_IDENTIFIER_COUNT; ++currentResourceIndex) {

        ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[currentResourceIndex], context->effectContextId);
    }

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

FfxErrorCode ffxDenoiserContextCreate(FfxDenoiserContext* context, const FfxDenoiserContextDescription* contextDescription)
{
    // Zero context memory
    memset(context, 0, sizeof(FfxDenoiserContext));

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
    FFX_STATIC_ASSERT(sizeof(FfxDenoiserContext) >= sizeof(FfxDenoiserContext_Private));

    // create the context.
    FfxDenoiserContext_Private* contextPrivate = (FfxDenoiserContext_Private*)(context);
    const FfxErrorCode errorCode = denoiserCreate(contextPrivate, contextDescription);

    return errorCode;
}

FfxErrorCode ffxDenoiserContextDestroy(FfxDenoiserContext* context)
{
    FFX_RETURN_ON_ERROR(
        context,
        FFX_ERROR_INVALID_POINTER);

    // Destroy the context.
    FfxDenoiserContext_Private* contextPrivate = (FfxDenoiserContext_Private*)(context);
    const FfxErrorCode errorCode = denoiserRelease(contextPrivate);
    return errorCode;
}

FfxErrorCode ffxDenoiserContextDispatchShadows(FfxDenoiserContext* context, const FfxDenoiserShadowsDispatchDescription* dispatchDescription)
{
    // check pointers are valid
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchDescription, FFX_ERROR_INVALID_POINTER);

    FfxDenoiserContext_Private* contextPrivate = (FfxDenoiserContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the Denoiser pass
    return denoiserDispatchShadows(contextPrivate, dispatchDescription);;
}

FfxErrorCode ffxDenoiserContextDispatchReflections(FfxDenoiserContext* context, const FfxDenoiserReflectionsDispatchDescription* dispatchDescription)
{
    // check pointers are valid
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchDescription, FFX_ERROR_INVALID_POINTER);
    
    FfxDenoiserContext_Private* contextPrivate = (FfxDenoiserContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the Denoiser pass
    return denoiserDispatchReflections(contextPrivate, dispatchDescription);;
}

FFX_API FfxVersionNumber ffxDenoiserGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_DENOISER_VERSION_MAJOR, FFX_DENOISER_VERSION_MINOR, FFX_DENOISER_VERSION_PATCH);
}
