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
#include <math.h>       // for ceil, log2
#include <algorithm>    // for max
using namespace std;

#include <FidelityFX/host/ffx_sssr.h>
#include <FidelityFX/gpu/sssr/ffx_sssr_resources.h>
#include <ffx_object_management.h>

#include <FidelityFX/host/ffx_denoiser.h>
#include "ffx_sssr_private.h"

namespace _noiseBuffers
{
#include "samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp.cpp"
}

// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t    index;
    wchar_t     name[64];
}ResourceBinding;

static const ResourceBinding srvTextureBindingTable[] =
{
    {FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_COLOR,                  L"r_input_color"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_DEPTH,                  L"r_input_depth"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS,         L"r_input_motion_vectors"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_NORMAL,                 L"r_input_normal"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_MATERIAL_PARAMETERS,    L"r_input_material_parameters"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_ENVIRONMENT_MAP,        L"r_input_environment_map"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_DEPTH_HIERARCHY,              L"r_depth_hierarchy"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE,                     L"r_radiance"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_HISTORY,             L"r_radiance_history"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE,                     L"r_variance"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS,          L"r_extracted_roughness"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_SOBOL_BUFFER,                 L"r_sobol_buffer"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_SCRAMBLING_TILE_BUFFER,       L"r_scrambling_tile_buffer"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_BLUE_NOISE_TEXTURE,           L"r_blue_noise_texture"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_BRDF_TEXTURE,           L"r_input_brdf_texture"},
};

static const ResourceBinding uavTextureBindingTable[] =
{
    {FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE,                        L"rw_radiance"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE,                        L"rw_variance"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS,             L"rw_extracted_roughness"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_BLUE_NOISE_TEXTURE,              L"rw_blue_noise_texture"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_DEPTH_HIERARCHY,                 L"rw_depth_hierarchy"},
};

static const ResourceBinding uavBufferBindingTable[] =
{
    {FFX_SSSR_RESOURCE_IDENTIFIER_RAY_LIST,                        L"rw_ray_list"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST,              L"rw_denoiser_tile_list"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_RAY_COUNTER,                     L"rw_ray_counter"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_INTERSECTION_PASS_INDIRECT_ARGS, L"rw_intersection_pass_indirect_args"},
    {FFX_SSSR_RESOURCE_IDENTIFIER_SPD_GLOBAL_ATOMIC,               L"rw_spd_global_atomic"},
};

static const ResourceBinding constantBufferBindingTable[] =
{
    {FFX_SSSR_CONSTANTBUFFER_IDENTIFIER_SSSR,     L"cbSSSR"},
};

template<typename T> inline T DivideRoundingUp(T a, T b)
{
    return (a + b - (T)1) / b;
}

static FfxErrorCode patchResourceBindings(FfxPipelineState* inoutPipeline)
{
    for (uint32_t srvIndex = 0; srvIndex < inoutPipeline->srvTextureCount; ++srvIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < FFX_ARRAY_ELEMENTS(srvTextureBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(srvTextureBindingTable[mapIndex].name, inoutPipeline->srvTextureBindings[srvIndex].name))
                break;
        }
        if (mapIndex == FFX_ARRAY_ELEMENTS(srvTextureBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->srvTextureBindings[srvIndex].resourceIdentifier = srvTextureBindingTable[mapIndex].index;
    }

    for (uint32_t uavIndex = 0; uavIndex < inoutPipeline->uavTextureCount; ++uavIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < FFX_ARRAY_ELEMENTS(uavTextureBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(uavTextureBindingTable[mapIndex].name, inoutPipeline->uavTextureBindings[uavIndex].name))
                break;
        }
        if (mapIndex == FFX_ARRAY_ELEMENTS(uavTextureBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->uavTextureBindings[uavIndex].resourceIdentifier = uavTextureBindingTable[mapIndex].index;
    }

    // Buffer uavs
    for (uint32_t uavIndex = 0; uavIndex < inoutPipeline->uavBufferCount; ++uavIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < FFX_ARRAY_ELEMENTS(uavBufferBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(uavBufferBindingTable[mapIndex].name, inoutPipeline->uavBufferBindings[uavIndex].name))
                break;
        }
        if (mapIndex == FFX_ARRAY_ELEMENTS(uavBufferBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->uavBufferBindings[uavIndex].resourceIdentifier = uavBufferBindingTable[mapIndex].index;
    }

    for (uint32_t cbIndex = 0; cbIndex < inoutPipeline->constCount; ++cbIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < FFX_ARRAY_ELEMENTS(constantBufferBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(constantBufferBindingTable[mapIndex].name, inoutPipeline->constantBufferBindings[cbIndex].name))
                break;
        }
        if (mapIndex == FFX_ARRAY_ELEMENTS(constantBufferBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->constantBufferBindings[cbIndex].resourceIdentifier = constantBufferBindingTable[mapIndex].index;
    }

    return FFX_OK;
}

static uint32_t getPipelinePermutationFlags(uint32_t contextFlags, FfxSssrPass passId, bool fp16, bool force64)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (force64) ? SSSR_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (fp16) ? SSSR_SHADER_PERMUTATION_ALLOW_FP16 : 0;
    flags |= (contextFlags & FFX_SSSR_ENABLE_DEPTH_INVERTED) ? SSSR_SHADER_PERMUTATION_DEPTH_INVERTED : 0;
    return flags;
}

static FfxErrorCode createPipelineStates(FfxSssrContext_Private* context)
{
    FFX_ASSERT(context);

    const size_t samplerCount = 2;
    FfxSamplerDescription samplerDescs[samplerCount] = { 
        { FFX_FILTER_TYPE_MINMAGMIP_LINEAR, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_WRAP, FFX_BIND_COMPUTE_SHADER_STAGE },
        { FFX_FILTER_TYPE_MINMAGMIP_LINEAR, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_BIND_COMPUTE_SHADER_STAGE }
    };
    FfxRootConstantDescription rootConstantDesc = { sizeof(SSSRConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE };

    FfxPipelineDescription pipelineDescription = {};
    pipelineDescription.contextFlags = 0;
    pipelineDescription.samplerCount = samplerCount;
    pipelineDescription.samplers = samplerDescs;
    pipelineDescription.rootConstantBufferCount = 1;
    pipelineDescription.rootConstants = &rootConstantDesc;
    pipelineDescription.stage = FFX_BIND_COMPUTE_SHADER_STAGE;
    pipelineDescription.indirectWorkload = 0;

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
    wcscpy_s(pipelineDescription.name, L"SSSR-DEPTH_DOWNSAMPLE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_SSSR, FFX_SSSR_PASS_DEPTH_DOWNSAMPLE,
        getPipelinePermutationFlags(contextFlags, FFX_SSSR_PASS_DEPTH_DOWNSAMPLE, supportedFP16, false), &pipelineDescription, context->effectContextId, &context->pipelineDepthDownsample ));
    wcscpy_s(pipelineDescription.name, L"SSSR-CLASSIFY_TILES");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_SSSR, FFX_SSSR_PASS_CLASSIFY_TILES,
        getPipelinePermutationFlags(contextFlags, FFX_SSSR_PASS_CLASSIFY_TILES, supportedFP16, canForceWave64), &pipelineDescription, context->effectContextId, &context->pipelineClassifyTiles));
    wcscpy_s(pipelineDescription.name, L"SSSR-PREPARE_BLUE_NOISE_TEXTURE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_SSSR, FFX_SSSR_PASS_PREPARE_BLUE_NOISE_TEXTURE, 
        getPipelinePermutationFlags(contextFlags, FFX_SSSR_PASS_PREPARE_BLUE_NOISE_TEXTURE, supportedFP16, canForceWave64), &pipelineDescription, context->effectContextId, &context->pipelinePrepareBlueNoiseTexture));
    wcscpy_s(pipelineDescription.name, L"SSSR-PREPARE_INDIRECT_ARGS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_SSSR, FFX_SSSR_PASS_PREPARE_INDIRECT_ARGS, 
        getPipelinePermutationFlags(contextFlags, FFX_SSSR_PASS_PREPARE_INDIRECT_ARGS, supportedFP16, canForceWave64), &pipelineDescription, context->effectContextId, &context->pipelinePrepareIndirectArgs));
    
    // Indirect workloads
    pipelineDescription.indirectWorkload = 1;
    wcscpy_s(pipelineDescription.name, L"SSSR-INTERSECTION");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface, FFX_EFFECT_SSSR, FFX_SSSR_PASS_INTERSECTION, 
        getPipelinePermutationFlags(contextFlags ,FFX_SSSR_PASS_INTERSECTION, supportedFP16, canForceWave64), &pipelineDescription, context->effectContextId, &context->pipelineIntersection));

    // for each pipeline: re-route/fix-up IDs based on names
    FFX_ASSERT(patchResourceBindings(&context->pipelineDepthDownsample)         == FFX_OK);
    FFX_ASSERT(patchResourceBindings(&context->pipelineClassifyTiles)           == FFX_OK);
    FFX_ASSERT(patchResourceBindings(&context->pipelinePrepareBlueNoiseTexture) == FFX_OK);
    FFX_ASSERT(patchResourceBindings(&context->pipelinePrepareIndirectArgs)     == FFX_OK);
    FFX_ASSERT(patchResourceBindings(&context->pipelineIntersection)            == FFX_OK);

    return FFX_OK;
}

static FfxErrorCode sssrCreate(FfxSssrContext_Private* context, const FfxSssrContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxSssrContext_Private));
    context->device = contextDescription->backendInterface.device;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxSssrContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);

    // Create the context.
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_SSSR, nullptr, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // call out for device caps.
    errorCode = context->contextDescription.backendInterface.fpGetDeviceCapabilities(
        &context->contextDescription.backendInterface, &context->deviceCapabilities);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // set defaults
    context->constants.frameIndex = 0;

    const uint32_t elementSize = 4;
    const uint32_t numPixels = contextDescription->renderSize.width * contextDescription->renderSize.height;
    uint32_t depthHierarchyMipCount = (uint32_t)ceil(log2(max(contextDescription->renderSize.width, contextDescription->renderSize.height)));
    depthHierarchyMipCount = min(7u, depthHierarchyMipCount);  // We generate 6 mips from the input depth buffer and keep a copy of it at mip 0 

    const FfxInternalResourceDescription internalSurfaceDesc[] = {

        {FFX_SSSR_RESOURCE_IDENTIFIER_DEPTH_HIERARCHY,
         L"SSSR_DepthHierarchy",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R32_FLOAT,
         contextDescription->renderSize.width,
         contextDescription->renderSize.height,
         depthHierarchyMipCount,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},
        
        {FFX_SSSR_RESOURCE_IDENTIFIER_RAY_LIST,
         L"SSSR_RayList",
         FFX_RESOURCE_TYPE_BUFFER,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R32_UINT,
         numPixels * sizeof(uint32_t),
         sizeof(uint32_t),
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST,
         L"SSSR_DenoiserTileList",
         FFX_RESOURCE_TYPE_BUFFER,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R32_UINT,
         numPixels * sizeof(uint32_t),
         sizeof(uint32_t),
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_RAY_COUNTER,
         L"SSSR_RayCounter",
         FFX_RESOURCE_TYPE_BUFFER,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R8_UNORM,
         4ull * sizeof(uint32_t),
         sizeof(uint32_t),
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_VALUE, 48, 0}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_INTERSECTION_PASS_INDIRECT_ARGS,
         L"SSSR_IntersectionPassIndirectArgs",
         FFX_RESOURCE_TYPE_BUFFER,
         (FfxResourceUsage)(FFX_RESOURCE_USAGE_UAV | FFX_RESOURCE_USAGE_INDIRECT),
         FFX_SURFACE_FORMAT_R8_UNORM,
         6ull * sizeof(uint32_t),
         sizeof(uint32_t),
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS,
         L"SSSR_ExtractedRoughness",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R8_UNORM,
         contextDescription->renderSize.width,
         contextDescription->renderSize.height,
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_0,
         L"SSSR_Radiance0",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT,
         contextDescription->renderSize.width,
         contextDescription->renderSize.height,
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_1,
         L"SSSR_Radiance1",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT,
         contextDescription->renderSize.width,
         contextDescription->renderSize.height,
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE_0,
         L"SSSR_Variance0",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R16_FLOAT,
         contextDescription->renderSize.width,
         contextDescription->renderSize.height,
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE_1,
         L"SSSR_Variance1",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R16_FLOAT,
         contextDescription->renderSize.width,
         contextDescription->renderSize.height,
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_SOBOL_BUFFER,
         L"SSSR_SobolBuffer",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_READ_ONLY,
         FFX_SURFACE_FORMAT_R32_UINT,
         256,
         256,
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_BUFFER, sizeof(_noiseBuffers::sobol_256spp_256d), (void*)_noiseBuffers::sobol_256spp_256d}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_SCRAMBLING_TILE_BUFFER,
         L"SSSR_ScramblingTileBuffer",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_READ_ONLY,
         FFX_SURFACE_FORMAT_R32_UINT,
         128 * 4,
         128 * 2,
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_BUFFER, sizeof(_noiseBuffers::scramblingTile), (void*)_noiseBuffers::scramblingTile}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_BLUE_NOISE_TEXTURE,
         L"SSSR_BlueNoiseTexture",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R8G8_UNORM,
         128,
         128,
         1,
         FFX_RESOURCE_FLAGS_NONE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_SSSR_RESOURCE_IDENTIFIER_SPD_GLOBAL_ATOMIC,
         L"SSSR_SpdAtomicCounter",
         FFX_RESOURCE_TYPE_BUFFER,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R32_UINT,
         1,
         1,
         1,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_VALUE, 1, 0}},
    };

    // clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    for (int32_t currentSurfaceIndex = 0; currentSurfaceIndex < FFX_ARRAY_ELEMENTS(internalSurfaceDesc); ++currentSurfaceIndex) {

        const FfxInternalResourceDescription* currentSurfaceDescription = &internalSurfaceDesc[currentSurfaceIndex];
        const FfxResourceDescription resourceDescription = { currentSurfaceDescription->type, currentSurfaceDescription->format, currentSurfaceDescription->width, currentSurfaceDescription->height, currentSurfaceDescription->type == FFX_RESOURCE_TYPE_BUFFER ? 0u : 1u, currentSurfaceDescription->mipCount, FFX_RESOURCE_FLAGS_NONE, currentSurfaceDescription->usage };
        const FfxResourceStates initialState = (currentSurfaceDescription->usage == FFX_RESOURCE_USAGE_READ_ONLY) ? FFX_RESOURCE_STATE_COMPUTE_READ : FFX_RESOURCE_STATE_UNORDERED_ACCESS;
        const FfxCreateResourceDescription createResourceDescription = {FFX_HEAP_TYPE_DEFAULT,
                                                                        resourceDescription,
                                                                        initialState,
                                                                        currentSurfaceDescription->name,
                                                                        currentSurfaceDescription->id,
                                                                        currentSurfaceDescription->initData};

        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface, &createResourceDescription, context->effectContextId, &context->srvResources[currentSurfaceDescription->id]));
    }

    // copy resources to uavResrouces list
    memcpy(context->uavResources, context->srvResources, sizeof(context->srvResources));

    // avoid compiling pipelines on first render
    {
        context->refreshPipelineStates = false;
        errorCode                      = createPipelineStates(context);
        FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);
    }

    // Setup constant buffer resources
    context->constantBuffers[0].num32BitEntries = sizeof(SSSRConstants) / sizeof(uint32_t);

    // Create denoiser context
    FfxDenoiserContextDescription initializationParameters = {};
    initializationParameters.flags = FfxDenoiserInitializationFlagBits::FFX_DENOISER_REFLECTIONS;
    initializationParameters.windowSize.width = contextDescription->renderSize.width;
    initializationParameters.windowSize.height = contextDescription->renderSize.height;
    initializationParameters.normalsHistoryBufferFormat = contextDescription->normalsHistoryBufferFormat;
    initializationParameters.backendInterface = contextDescription->backendInterface;

    FFX_ASSERT(ffxDenoiserContextCreate(&context->denoiserContext, &initializationParameters) == FFX_OK);

    return FFX_OK;
}

static FfxErrorCode sssrRelease(FfxSssrContext_Private* context)
{
    FFX_ASSERT(context);

    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineDepthDownsample, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineClassifyTiles, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareBlueNoiseTexture, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareIndirectArgs, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineIntersection, context->effectContextId);

    // unregister resources not created internally 
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_COLOR]                 = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_DEPTH]                 = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS]        = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_NORMAL]                = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_MATERIAL_PARAMETERS]   = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_ENVIRONMENT_MAP]       = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE]                    = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_HISTORY]            = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE]                    = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_BRDF_TEXTURE]          = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_OUTPUT]                      = { FFX_SSSR_RESOURCE_IDENTIFIER_NULL };

    // Release the copy resources for those that had init data
    ffxSafeReleaseCopyResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_SOBOL_BUFFER], context->effectContextId);
    ffxSafeReleaseCopyResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_SCRAMBLING_TILE_BUFFER], context->effectContextId);
    ffxSafeReleaseCopyResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_SPD_GLOBAL_ATOMIC], context->effectContextId);
    ffxSafeReleaseCopyResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_RAY_COUNTER], context->effectContextId);

    // release internal resources
    for (int32_t currentResourceIndex = 0; currentResourceIndex < FFX_SSSR_RESOURCE_IDENTIFIER_COUNT; ++currentResourceIndex) {

        ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->srvResources[currentResourceIndex], context->effectContextId);
    }

    FFX_ASSERT(ffxDenoiserContextDestroy(&context->denoiserContext) == FFX_OK);

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

static void populateComputeJobResources(FfxSssrContext_Private* context, const FfxPipelineState* pipeline, FfxComputeJobDescription* jobDescriptor)
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
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor->uavTextures[currentUnorderedAccessViewIndex].name, pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].name);
#endif
        const uint32_t bindEntry = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].arrayIndex;
        const uint32_t currentResourceId = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource = context->uavResources[currentResourceId];

        // Don't over-subscribe mips (default to mip 0 once we've exhausted min mip)
        FfxResourceDescription resDesc = context->contextDescription.backendInterface.fpGetResourceDescription(&context->contextDescription.backendInterface, currentResource);
        jobDescriptor->uavTextures[uavEntry].resource = currentResource;
        jobDescriptor->uavTextures[uavEntry++].mip = (bindEntry < resDesc.mipCount) ? bindEntry : 0;
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

    for (uint32_t currentRootConstantIndex = 0; currentRootConstantIndex < pipeline->constCount; ++currentRootConstantIndex) {
#ifdef FFX_DEBUG
        wcscpy_s(jobDescriptor->cbNames[currentRootConstantIndex], pipeline->constantBufferBindings[currentRootConstantIndex].name);
#endif
        jobDescriptor->cbs[currentRootConstantIndex] = context->constantBuffers[pipeline->constantBufferBindings[currentRootConstantIndex].resourceIdentifier];
    }
}

static void scheduleIndirectDispatch(FfxSssrContext_Private* context, const FfxPipelineState* pipeline, const FfxResourceInternal* commandArgument, const uint32_t offset = 0)
{
    FfxComputeJobDescription jobDescriptor = {};
    jobDescriptor.pipeline = *pipeline;
    jobDescriptor.cmdArgument = *commandArgument;
    jobDescriptor.cmdArgumentOffset = offset;
    populateComputeJobResources(context , pipeline, &jobDescriptor);

    FfxGpuJobDescription dispatchJob = { FFX_GPU_JOB_COMPUTE };
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);
    dispatchJob.computeJobDescriptor = jobDescriptor;
    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchJob);
}

static void scheduleDispatch(FfxSssrContext_Private* context, const FfxPipelineState* pipeline, uint32_t dispatchX, uint32_t dispatchY)
{
    FfxGpuJobDescription dispatchJob = {FFX_GPU_JOB_COMPUTE};
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);
    dispatchJob.computeJobDescriptor.dimensions[0] = dispatchX;
    dispatchJob.computeJobDescriptor.dimensions[1] = dispatchY;
    dispatchJob.computeJobDescriptor.dimensions[2] = 1;
    dispatchJob.computeJobDescriptor.pipeline      = *pipeline;
    populateComputeJobResources(context, pipeline, &dispatchJob.computeJobDescriptor);

    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchJob);
}

static FfxErrorCode sssrDispatch(FfxSssrContext_Private* context, const FfxSssrDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;
    // try and refresh shaders first. Early exit in case of error.
    if (context->refreshPipelineStates) {

        context->refreshPipelineStates = false;

        const FfxErrorCode errorCode = createPipelineStates(context);
        FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);
    }

    // zero initialise radiance and variance buffers
    if (context->constants.frameIndex == 0) {
        FfxGpuJobDescription job = {};
        job.jobType = FFX_GPU_JOB_CLEAR_FLOAT;
        wcscpy_s(job.jobLabel, L"Zero initialize resource");
        job.clearJobDescriptor.color[0] = 0.0f;
        job.clearJobDescriptor.color[1] = 0.0f;
        job.clearJobDescriptor.color[2] = 0.0f;
        job.clearJobDescriptor.color[3] = 0.0f;

        uint32_t resourceIDs[] = {
            FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_0,
            FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_1,
            FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE_0,
            FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE_1,
        };

        for (const uint32_t *resourceID = resourceIDs; resourceID < resourceIDs + FFX_ARRAY_ELEMENTS(resourceIDs); ++resourceID) {
            job.clearJobDescriptor.target = context->uavResources[*resourceID];
            context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &job);
        }
    }

    // Prepare per frame descriptor tables
    const bool isOddFrame = !!(context->constants.frameIndex & 1);
    
    const uint32_t radianceAResourceIndex   = isOddFrame ? FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_0  : FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_1;
    const uint32_t radianceBResourceIndex   = isOddFrame ? FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_1  : FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_0;
    const uint32_t varianceAResourceIndex   = isOddFrame ? FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE_0  : FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE_1;
    const uint32_t varianceBResourceIndex   = isOddFrame ? FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE_1  : FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE_0;

    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->color,                  context->effectContextId, &context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_COLOR]); 
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->depth,                  context->effectContextId, &context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_DEPTH]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->motionVectors,          context->effectContextId, &context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->normal,                 context->effectContextId, &context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_NORMAL]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->materialParameters,     context->effectContextId, &context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_MATERIAL_PARAMETERS]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->environmentMap,         context->effectContextId, &context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_ENVIRONMENT_MAP]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->brdfTexture,            context->effectContextId, &context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_BRDF_TEXTURE]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->output,                 context->effectContextId, &context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_OUTPUT]);

    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE]                = context->srvResources[radianceAResourceIndex];
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_HISTORY]        = context->srvResources[radianceBResourceIndex];
    context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE]                = context->srvResources[varianceBResourceIndex];

    context->uavResources[FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE]                = context->uavResources[radianceAResourceIndex];
    context->uavResources[FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE]                = context->uavResources[varianceAResourceIndex];

    // actual resource size may differ from render/display resolution (e.g. due to Hw/API restrictions), so query the descriptor for UVs adjustment
    const FfxResourceDescription resourceDescInputColor = context->contextDescription.backendInterface.fpGetResourceDescription(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_COLOR]);
    FFX_ASSERT(resourceDescInputColor.type == FFX_RESOURCE_TYPE_TEXTURE2D);

    const uint32_t width  = uint32_t(params->renderSize.width ? params->renderSize.width : resourceDescInputColor.width);
    const uint32_t height = uint32_t(params->renderSize.height ? params->renderSize.height : resourceDescInputColor.height);

    // Copy the matrices over
    memcpy(&context->constants.invViewProjection, &params->invViewProjection, sizeof(float) * 16 * 6);
    context->constants.renderSize[0]                        = width;
    context->constants.renderSize[1]                        = height;
    context->constants.inverseRenderSize[0]                 = 1.0f / width;
    context->constants.inverseRenderSize[1]                 = 1.0f / height;
    context->constants.normalsUnpackMul                     = params->normalUnPackMul;
    context->constants.normalsUnpackAdd                     = params->normalUnPackAdd;
    context->constants.roughnessChannel                     = params->roughnessChannel;
    context->constants.isRoughnessPerceptual                = params->isRoughnessPerceptual;
    context->constants.iblFactor                            = params->iblFactor;
    context->constants.temporalStabilityFactor              = params->temporalStabilityFactor;
    context->constants.depthBufferThickness                 = params->depthBufferThickness;
    context->constants.roughnessThreshold                   = params->roughnessThreshold;
    context->constants.varianceThreshold                    = params->varianceThreshold;
    context->constants.maxTraversalIntersections            = params->maxTraversalIntersections;
    context->constants.minTraversalOccupancy                = params->minTraversalOccupancy;
    context->constants.mostDetailedMip                      = params->mostDetailedMip;
    context->constants.samplesPerQuad                       = params->samplesPerQuad;
    context->constants.temporalVarianceGuidedTracingEnabled = params->temporalVarianceGuidedTracingEnabled ? 1 : 0;

    // initialize constantBuffers data
    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(&context->contextDescription.backendInterface, &context->constants, sizeof(context->constants), &context->constantBuffers[FFX_SSSR_CONSTANTBUFFER_IDENTIFIER_SSSR]);

    // Mip map depth hierarchy
    scheduleDispatch(context, &context->pipelineDepthDownsample, DivideRoundingUp(width, 64u), DivideRoundingUp(height, 64u));

    // SSSR
    scheduleDispatch(context, &context->pipelineClassifyTiles, DivideRoundingUp(width, 8u), DivideRoundingUp(height, 8u));
    scheduleDispatch(context, &context->pipelinePrepareBlueNoiseTexture, 128 / 8u, 128 / 8u);
    scheduleDispatch(context, &context->pipelinePrepareIndirectArgs, 1, 1);
    scheduleIndirectDispatch(context, &context->pipelineIntersection, &context->uavResources[FFX_SSSR_RESOURCE_IDENTIFIER_INTERSECTION_PASS_INDIRECT_ARGS], 0);

    // Execute all jobs up to date so resources will be in the correct state when importing into the denoiser
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);
    
    FfxDenoiserReflectionsDispatchDescription denoiserDispatchParameters = {};
    denoiserDispatchParameters.commandList              = commandList;
    denoiserDispatchParameters.depthHierarchy           = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_DEPTH_HIERARCHY]);
    denoiserDispatchParameters.motionVectors            = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS]);
    denoiserDispatchParameters.normal                   = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_NORMAL]);
    denoiserDispatchParameters.radianceA                = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE]);
    denoiserDispatchParameters.radianceB                = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_HISTORY]);
    denoiserDispatchParameters.varianceA                = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->uavResources[FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE]);
    denoiserDispatchParameters.varianceB                = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE]);
    denoiserDispatchParameters.extractedRoughness       = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS]);
    denoiserDispatchParameters.denoiserTileList         = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->uavResources[FFX_SSSR_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST]);
    denoiserDispatchParameters.indirectArgumentsBuffer  = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->uavResources[FFX_SSSR_RESOURCE_IDENTIFIER_INTERSECTION_PASS_INDIRECT_ARGS]);
    denoiserDispatchParameters.output                   = context->contextDescription.backendInterface.fpGetResource(&context->contextDescription.backendInterface, context->srvResources[FFX_SSSR_RESOURCE_IDENTIFIER_OUTPUT]);
    denoiserDispatchParameters.renderSize               = params->renderSize;
    denoiserDispatchParameters.motionVectorScale        = params->motionVectorScale;
    denoiserDispatchParameters.normalsUnpackMul         = params->normalUnPackMul;
    denoiserDispatchParameters.normalsUnpackAdd         = params->normalUnPackAdd; 
    denoiserDispatchParameters.isRoughnessPerceptual    = params->isRoughnessPerceptual;
    denoiserDispatchParameters.temporalStabilityFactor  = params->temporalStabilityFactor;
    denoiserDispatchParameters.roughnessThreshold       = params->roughnessThreshold;
    denoiserDispatchParameters.frameIndex               = context->constants.frameIndex;

    memcpy(&denoiserDispatchParameters.invProjection, &params->invProjection, sizeof(params->invProjection));
    memcpy(&denoiserDispatchParameters.invView, &params->invView, sizeof(params->invView));
    memcpy(&denoiserDispatchParameters.prevViewProjection, &params->prevViewProjection, sizeof(params->prevViewProjection));

    FfxErrorCode errorCode = ffxDenoiserContextDispatchReflections(&context->denoiserContext, &denoiserDispatchParameters);

    context->constants.frameIndex++;

    // release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    return FFX_OK;
}

FfxErrorCode ffxSssrContextCreate(FfxSssrContext* context, const FfxSssrContextDescription* contextDescription)
{
    // zero context memory
    memset(context, 0, sizeof(FfxSssrContext));

    // check pointers are valid.
    FFX_RETURN_ON_ERROR(
        context,
        FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(
        contextDescription,
        FFX_ERROR_INVALID_POINTER);

    // validate that all callbacks are set for the interface
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetSDKVersion, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetDeviceCapabilities, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpCreateBackendContext,  FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpDestroyBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);

    // if a scratch buffer is declared, then we must have a size
    if (contextDescription->backendInterface.scratchBuffer) {

        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);
    }

    // ensure the context is large enough for the internal context.
    FFX_STATIC_ASSERT(sizeof(FfxSssrContext) >= sizeof(FfxSssrContext_Private));

    // create the context.
    FfxSssrContext_Private* contextPrivate = (FfxSssrContext_Private*)(context);
    const FfxErrorCode errorCode = sssrCreate(contextPrivate, contextDescription);

    return errorCode;
}

FfxErrorCode ffxSssrContextDestroy(FfxSssrContext* context)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    // destroy the context.
    FfxSssrContext_Private* contextPrivate = (FfxSssrContext_Private*)(context);
    const FfxErrorCode errorCode = sssrRelease(contextPrivate);
    return errorCode;
}

FfxErrorCode ffxSssrContextDispatch(FfxSssrContext* context, const FfxSssrDispatchDescription* dispatchParams)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchParams, FFX_ERROR_INVALID_POINTER);
    
    FfxSssrContext_Private* contextPrivate = (FfxSssrContext_Private*)(context);

    // validate that renderSize is within the maximum.
    FFX_RETURN_ON_ERROR(dispatchParams->renderSize.width  <= contextPrivate->contextDescription.renderSize.width,  FFX_ERROR_OUT_OF_RANGE);
    FFX_RETURN_ON_ERROR(dispatchParams->renderSize.height <= contextPrivate->contextDescription.renderSize.height, FFX_ERROR_OUT_OF_RANGE);
    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the SSSR passes.
    const FfxErrorCode errorCode = sssrDispatch(contextPrivate, dispatchParams);
    return errorCode;
}

FFX_API FfxVersionNumber ffxSssrGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_SSSR_VERSION_MAJOR, FFX_SSSR_VERSION_MINOR, FFX_SSSR_VERSION_PATCH);
}
