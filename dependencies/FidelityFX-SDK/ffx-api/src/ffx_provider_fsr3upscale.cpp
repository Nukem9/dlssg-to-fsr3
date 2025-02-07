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

#include "ffx_provider_fsr3upscale.h"
#include "backends.h"
#include "validation.h"
#include <ffx_api/ffx_upscale.hpp>
#ifdef FFX_BACKEND_DX12
#include <ffx_api/dx12/ffx_api_dx12.h>
#endif // FFX_BACKEND_DX12
#ifdef FFX_BACKEND_VK
#include <ffx_api/vk/ffx_api_vk.h>
#endif // #ifdef FFX_BACKEND_VK
#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <FidelityFX/gpu/fsr3/ffx_fsr3_resources.h>

#include <stdlib.h>

static uint32_t ConvertFlags(uint32_t apiFlags)
{
    uint32_t outFlags = 0;
    if (apiFlags & FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE)
        outFlags |= FFX_FSR3UPSCALER_ENABLE_HIGH_DYNAMIC_RANGE;
    if (apiFlags & FFX_UPSCALE_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS)
        outFlags |= FFX_FSR3UPSCALER_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS;
    if (apiFlags & FFX_UPSCALE_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION)
        outFlags |= FFX_FSR3UPSCALER_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION;
    if (apiFlags & FFX_UPSCALE_ENABLE_DEPTH_INVERTED)
        outFlags |= FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED;
    if (apiFlags & FFX_UPSCALE_ENABLE_DEPTH_INFINITE)
        outFlags |= FFX_FSR3UPSCALER_ENABLE_DEPTH_INFINITE;
    if (apiFlags & FFX_UPSCALE_ENABLE_AUTO_EXPOSURE)
        outFlags |= FFX_FSR3UPSCALER_ENABLE_AUTO_EXPOSURE;
    if (apiFlags & FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION)
        outFlags |= FFX_FSR3UPSCALER_ENABLE_DYNAMIC_RESOLUTION;
    if (apiFlags & FFX_UPSCALE_ENABLE_DEBUG_CHECKING)
        outFlags |= FFX_FSR3UPSCALER_ENABLE_DEBUG_CHECKING;
    return outFlags;
}

bool ffxProvider_FSR3Upscale::CanProvide(uint64_t type) const
{
    return (type & FFX_API_EFFECT_MASK) == FFX_API_EFFECT_ID_UPSCALE;
}

#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X) 
#define MAKE_VERSION_STRING(major, minor, patch) STRINGIFY major "." STRINGIFY minor "." STRINGIFY patch

uint64_t ffxProvider_FSR3Upscale::GetId() const
{
    // FSR Scale, version from header
    return 0xF5A5'CA1Eui64 << 32 | (FFX_SDK_MAKE_VERSION(FFX_FSR3UPSCALER_VERSION_MAJOR, FFX_FSR3UPSCALER_VERSION_MINOR, FFX_FSR3UPSCALER_VERSION_PATCH) & 0xFFFF'FFFF);
}

const char* ffxProvider_FSR3Upscale::GetVersionName() const
{
    return MAKE_VERSION_STRING(FFX_FSR3UPSCALER_VERSION_MAJOR, FFX_FSR3UPSCALER_VERSION_MINOR, FFX_FSR3UPSCALER_VERSION_PATCH);
}

struct InternalFsr3UpscalerUContext
{
    InternalContextHeader   header;
    FfxInterface            backendInterface;
    FfxResourceInternal     sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_COUNT];
    FfxFsr3UpscalerContext  context;
    ffxApiMessage           fpMessage;
};

ffxReturnCode_t ffxProvider_FSR3Upscale::CreateContext(ffxContext* context, ffxCreateContextDescHeader* header, Allocator& alloc) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);

    if (auto desc = ffx::DynamicCast<ffxCreateContextDescUpscale>(header))
    {
        if (desc->fpMessage)
        {
#ifdef FFX_BACKEND_DX12
            Validator{desc->fpMessage, header}.AcceptExtensions({FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12, FFX_API_DESC_TYPE_OVERRIDE_VERSION});
#elif FFX_BACKEND_VK
            Validator{ desc->fpMessage, header }.AcceptExtensions({ FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK, FFX_API_DESC_TYPE_OVERRIDE_VERSION });
#endif // FFX_BACKEND_DX12
        }
        InternalFsr3UpscalerUContext* internal_context = alloc.construct<InternalFsr3UpscalerUContext>();
        VERIFY(internal_context, FFX_API_RETURN_ERROR_MEMORY);
        internal_context->header.provider = this;

        TRY(MustCreateBackend(header, &internal_context->backendInterface, 1, alloc));

        FfxFsr3UpscalerContextDescription initializationParameters = {0};
        initializationParameters.backendInterface = internal_context->backendInterface;
        initializationParameters.maxRenderSize.width       = desc->maxRenderSize.width;
        initializationParameters.maxRenderSize.height      = desc->maxRenderSize.height;
        initializationParameters.maxUpscaleSize.width      = desc->maxUpscaleSize.width;
        initializationParameters.maxUpscaleSize.height     = desc->maxUpscaleSize.height;
        initializationParameters.flags                     = ConvertFlags(desc->flags);
        // Calling this casted function is undefined behaviour, but it's probably safe.
        initializationParameters.fpMessage                 = reinterpret_cast<FfxFsr3UpscalerMessage>(desc->fpMessage);

        // Grab this fp for use in extensions later
        internal_context->fpMessage = desc->fpMessage;

        // Create the FSR3UPSCALER context
        TRY2(ffxFsr3UpscalerContextCreate(&internal_context->context, &initializationParameters));

        // set up FSR3Upscaler "shared" resources (no resource sharing in the upscaler provider though, since providers are fully independent and we can't guarantee all upscale providers will be compatible with other effects)
        {
            FfxFsr3UpscalerSharedResourceDescriptions fs3UpscalerResourceDescs = {};
            TRY2(ffxFsr3UpscalerGetSharedResourceDescriptions(&internal_context->context, &fs3UpscalerResourceDescs));

            {
                FfxCreateResourceDescription dilD = fs3UpscalerResourceDescs.dilatedDepth;
                dilD.name = fs3UpscalerResourceDescs.dilatedDepth.name;
                TRY2(internal_context->backendInterface.fpCreateResource(
                    &internal_context->backendInterface,
                    &dilD,
                    0,
                    &internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0]));

                FfxCreateResourceDescription dilMVs = fs3UpscalerResourceDescs.dilatedMotionVectors;
                dilD.name   = fs3UpscalerResourceDescs.dilatedMotionVectors.name;
                TRY2(internal_context->backendInterface.fpCreateResource(
                    &internal_context->backendInterface,
                    &dilMVs,
                    0,
                    &internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0]));

                FfxCreateResourceDescription recND = fs3UpscalerResourceDescs.reconstructedPrevNearestDepth;
                recND.name = fs3UpscalerResourceDescs.reconstructedPrevNearestDepth.name;
                TRY2(internal_context->backendInterface.fpCreateResource(
                    &internal_context->backendInterface,
                    &recND,
                    0,
                    &internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0]));
            }
        }

        *context = internal_context;
        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    }
}

ffxReturnCode_t ffxProvider_FSR3Upscale::DestroyContext(ffxContext* context, Allocator& alloc) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFsr3UpscalerUContext* internal_context = reinterpret_cast<InternalFsr3UpscalerUContext*>(*context);
    
    for (FfxUInt32 i = 0; i < FFX_FSR3_RESOURCE_IDENTIFIER_COUNT; i++)
    {
        TRY2(internal_context->backendInterface.fpDestroyResource(
            &internal_context->backendInterface, internal_context->sharedResources[i], 0));
    }

    TRY2(ffxFsr3UpscalerContextDestroy(&internal_context->context));

    alloc.dealloc(internal_context->backendInterface.scratchBuffer);
    alloc.dealloc(internal_context);
    
    return FFX_API_RETURN_OK;
}

ffxReturnCode_t ffxProvider_FSR3Upscale::Configure(ffxContext* context, const ffxConfigureDescHeader* header) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);
    InternalFsr3UpscalerUContext* internal_context = reinterpret_cast<InternalFsr3UpscalerUContext*>(*context);
    switch (header->type)
    {
    case FFX_API_CONFIGURE_DESC_TYPE_UPSCALE_KEYVALUE:
    {
        auto desc = reinterpret_cast<const ffxConfigureDescUpscaleKeyValue*>(header);
        TRY2(ffxFsr3UpscalerSetConstant(&internal_context->context, static_cast<FfxFsr3UpscalerConfigureKey>(desc->key), desc->ptr));
        break;
    }
    default:
        return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    }
    return FFX_API_RETURN_OK;
}

ffxReturnCode_t ffxProvider_FSR3Upscale::Query(ffxContext* context, ffxQueryDescHeader* header) const
{
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);

    if (context)
    {
        InternalFsr3UpscalerUContext* internal_context = reinterpret_cast<InternalFsr3UpscalerUContext*>(*context);
        if (internal_context->fpMessage)
        {
            Validator{internal_context->fpMessage, header}.NoExtensions();
        }
    }

    switch (header->type)
    {
    case FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTEROFFSET:
    {
        auto desc = reinterpret_cast<ffxQueryDescUpscaleGetJitterOffset*>(header);
        float jitterX, jitterY;
        TRY2(ffxFsr3UpscalerGetJitterOffset(&jitterX, &jitterY, desc->index, desc->phaseCount));
        if (desc->pOutX != nullptr)
        {
            *desc->pOutX = jitterX;
        }
        if (desc->pOutY != nullptr)
        {
            *desc->pOutY = jitterY;
        }
        break;
    }
    case FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTERPHASECOUNT:
    {
        auto desc = reinterpret_cast<ffxQueryDescUpscaleGetJitterPhaseCount*>(header);
        const int32_t jitterPhaseCount = ffxFsr3UpscalerGetJitterPhaseCount(desc->renderWidth, desc->displayWidth);

        if (desc->pOutPhaseCount != nullptr)
        {
            *desc->pOutPhaseCount = jitterPhaseCount;
        }
        break;
    }
    case FFX_API_QUERY_DESC_TYPE_UPSCALE_GETRENDERRESOLUTIONFROMQUALITYMODE:
    {
        auto desc = reinterpret_cast<ffxQueryDescUpscaleGetRenderResolutionFromQualityMode*>(header);
        uint32_t renderWidth;
        uint32_t renderHeight;

        TRY2(ffxFsr3UpscalerGetRenderResolutionFromQualityMode(&renderWidth, &renderHeight, desc->displayWidth, desc->displayHeight, ConvertEnum<FfxFsr3UpscalerQualityMode>(desc->qualityMode)));
        if (desc->pOutRenderWidth != nullptr)
        {
            *desc->pOutRenderWidth = renderWidth;
        }
        if (desc->pOutRenderHeight != nullptr)
        {
            *desc->pOutRenderHeight = renderHeight;
        }
        break;
    }
    case FFX_API_QUERY_DESC_TYPE_UPSCALE_GETUPSCALERATIOFROMQUALITYMODE:
    {
        auto desc = reinterpret_cast<ffxQueryDescUpscaleGetUpscaleRatioFromQualityMode*>(header);
        float ratio = ffxFsr3UpscalerGetUpscaleRatioFromQualityMode(ConvertEnum<FfxFsr3UpscalerQualityMode>(desc->qualityMode));

        if (desc->pOutUpscaleRatio != nullptr)
        {
            *desc->pOutUpscaleRatio = ratio;
        }
        break;
    }
    case FFX_API_QUERY_DESC_TYPE_UPSCALE_GPU_MEMORY_USAGE:
    {
        InternalFsr3UpscalerUContext* internal_context = reinterpret_cast<InternalFsr3UpscalerUContext*>(*context);
        auto desc = reinterpret_cast<ffxQueryDescUpscaleGetGPUMemoryUsage*>(header);
        
        TRY2(ffxFsr3UpscalerContextGetGpuMemoryUsage(&internal_context->context, reinterpret_cast <FfxEffectMemoryUsage*> (desc->gpuMemoryUsageUpscaler)));
        break;
    }
    default:
        return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    }

    return FFX_API_RETURN_OK;
}

ffxReturnCode_t ffxProvider_FSR3Upscale::Dispatch(ffxContext* context, const ffxDispatchDescHeader* header) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFsr3UpscalerUContext* internal_context = reinterpret_cast<InternalFsr3UpscalerUContext*>(*context);
    if (internal_context->fpMessage)
    {
        Validator{internal_context->fpMessage, header}.NoExtensions();
    }

    switch (header->type)
    {
    case FFX_API_DISPATCH_DESC_TYPE_UPSCALE:
    {
        auto desc = reinterpret_cast<const ffxDispatchDescUpscale*>(header);

        FfxFsr3UpscalerDispatchDescription dispatchParameters = {};
        dispatchParameters.commandList                = desc->commandList;
        dispatchParameters.color                      = Convert(desc->color);
        dispatchParameters.depth                      = Convert(desc->depth);
        dispatchParameters.motionVectors              = Convert(desc->motionVectors);
        dispatchParameters.exposure                   = Convert(desc->exposure);
        dispatchParameters.output                     = Convert(desc->output);
        dispatchParameters.reactive                   = Convert(desc->reactive);
        dispatchParameters.transparencyAndComposition = Convert(desc->transparencyAndComposition);
        dispatchParameters.jitterOffset.x             = desc->jitterOffset.x;
        dispatchParameters.jitterOffset.y             = desc->jitterOffset.y;
        dispatchParameters.motionVectorScale.x        = desc->motionVectorScale.x;
        dispatchParameters.motionVectorScale.y        = desc->motionVectorScale.y;
        dispatchParameters.reset                      = desc->reset;
        dispatchParameters.enableSharpening           = desc->enableSharpening;
        dispatchParameters.sharpness                  = desc->sharpness;
        dispatchParameters.frameTimeDelta             = desc->frameTimeDelta;
        dispatchParameters.preExposure                = desc->preExposure;
        dispatchParameters.renderSize.width           = desc->renderSize.width;
        dispatchParameters.renderSize.height          = desc->renderSize.height;
        dispatchParameters.upscaleSize.width          = desc->upscaleSize.width;
        dispatchParameters.upscaleSize.height         = desc->upscaleSize.height;
        dispatchParameters.cameraFovAngleVertical     = desc->cameraFovAngleVertical;
        dispatchParameters.cameraFar                  = desc->cameraFar;
        dispatchParameters.cameraNear                 = desc->cameraNear;
        dispatchParameters.viewSpaceToMetersFactor    = desc->viewSpaceToMetersFactor;
        dispatchParameters.flags = 0;

        dispatchParameters.dilatedDepth                     = internal_context->backendInterface.fpGetResource( &internal_context->backendInterface, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0]);
        dispatchParameters.dilatedMotionVectors             = internal_context->backendInterface.fpGetResource( &internal_context->backendInterface, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0]);
        dispatchParameters.reconstructedPrevNearestDepth    = internal_context->backendInterface.fpGetResource( &internal_context->backendInterface, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0]);

        if (desc->flags & FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW)
        {
            dispatchParameters.flags |= FFX_FSR3UPSCALER_DISPATCH_DRAW_DEBUG_VIEW;
        }

        TRY2(ffxFsr3UpscalerContextDispatch(&internal_context->context, &dispatchParameters));
        break;
    }
    case FFX_API_DISPATCH_DESC_TYPE_UPSCALE_GENERATEREACTIVEMASK:
    {
        auto desc = reinterpret_cast<const ffxDispatchDescUpscaleGenerateReactiveMask*>(header);

        FfxFsr3UpscalerGenerateReactiveDescription dispatchParameters = {};
        dispatchParameters.commandList       = desc->commandList;
        dispatchParameters.colorOpaqueOnly   = Convert(desc->colorOpaqueOnly);
        dispatchParameters.colorPreUpscale   = Convert(desc->colorPreUpscale);
        dispatchParameters.outReactive       = Convert(desc->outReactive);
        dispatchParameters.renderSize.width  = desc->renderSize.width;
        dispatchParameters.renderSize.height = desc->renderSize.height;
        dispatchParameters.scale             = desc->scale;
        dispatchParameters.cutoffThreshold   = desc->cutoffThreshold;
        dispatchParameters.binaryValue       = desc->binaryValue;
        dispatchParameters.flags             = desc->flags;

        TRY2(ffxFsr3UpscalerContextGenerateReactiveMask(&internal_context->context, &dispatchParameters));
        break;
    }
    default:
        return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    }

    return FFX_API_RETURN_OK;
}

ffxProvider_FSR3Upscale ffxProvider_FSR3Upscale::Instance;
