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

#include "ffx_provider_fsr2.h"
#include "backends.h"
#include <ffx_api/ffx_upscale.hpp>
#include <FidelityFX/host/ffx_fsr2.h>

#include <stdlib.h>

static FfxFsr2QualityMode ConvertQuality(uint32_t apiMode)
{
    switch (apiMode)
    {
    case FFX_UPSCALE_QUALITY_MODE_QUALITY:
        return FFX_FSR2_QUALITY_MODE_QUALITY;
    case FFX_UPSCALE_QUALITY_MODE_BALANCED:
        return FFX_FSR2_QUALITY_MODE_BALANCED;
    case FFX_UPSCALE_QUALITY_MODE_PERFORMANCE:
        return FFX_FSR2_QUALITY_MODE_PERFORMANCE;
    case FFX_UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE:
        return FFX_FSR2_QUALITY_MODE_ULTRA_PERFORMANCE;
    default:
        // convert representation through int.
        return (FfxFsr2QualityMode)(int)apiMode;
    }
}

bool ffxProvider_FSR2::CanProvide(uint64_t type) const
{
    return (type & FFX_API_EFFECT_MASK) == FFX_API_EFFECT_ID_UPSCALE;
}

#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X) 
#define MAKE_VERSION_STRING(major, minor, patch) STRINGIFY major "." STRINGIFY minor "." STRINGIFY patch

uint64_t ffxProvider_FSR2::GetId() const
{
    // FSR Scale, version from header
    return 0xF5A5'CA1Eui64 << 32 | (FFX_SDK_MAKE_VERSION(FFX_FSR2_VERSION_MAJOR, FFX_FSR2_VERSION_MINOR, FFX_FSR2_VERSION_PATCH) & 0xFFFF'FFFF);
}

const char* ffxProvider_FSR2::GetVersionName() const
{
    return MAKE_VERSION_STRING(FFX_FSR2_VERSION_MAJOR, FFX_FSR2_VERSION_MINOR, FFX_FSR2_VERSION_PATCH);
}

struct InternalFsr2Context
{
    InternalContextHeader header;
    FfxInterface backendInterface;
    FfxFsr2Context context;
    ffxApiMessage fpMessage;
};

ffxReturnCode_t ffxProvider_FSR2::CreateContext(ffxContext* context, ffxCreateContextDescHeader* header, Allocator& alloc) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);

    if (auto desc = ffx::DynamicCast<ffxCreateContextDescUpscale>(header))
    {
        InternalFsr2Context* internal_context = alloc.construct<InternalFsr2Context>();
        VERIFY(internal_context, FFX_API_RETURN_ERROR_MEMORY);
        internal_context->header.provider = this;

        TRY(MustCreateBackend(header, &internal_context->backendInterface, FFX_FSR2_CONTEXT_COUNT, alloc));

        FfxFsr2ContextDescription initializationParameters = {0};
        initializationParameters.backendInterface          = internal_context->backendInterface;
        initializationParameters.maxRenderSize.width       = desc->maxRenderSize.width;
        initializationParameters.maxRenderSize.height      = desc->maxRenderSize.height;
        initializationParameters.displaySize.width         = desc->maxUpscaleSize.width;
        initializationParameters.displaySize.height        = desc->maxUpscaleSize.height;
        initializationParameters.flags                     = desc->flags;
        // Calling this casted function is undefined behaviour, but it's probably safe.
        initializationParameters.fpMessage                 = reinterpret_cast<FfxFsr2Message>(desc->fpMessage);

        // Grab this fp for use in extensions later
        internal_context->fpMessage = desc->fpMessage;

        // Create the FSR2 context
        TRY(ffxFsr2ContextCreate(&internal_context->context, &initializationParameters));

        *context = internal_context;
        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    }
}

ffxReturnCode_t ffxProvider_FSR2::DestroyContext(ffxContext* context, Allocator& alloc) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFsr2Context* internal_context = reinterpret_cast<InternalFsr2Context*>(*context);

    TRY2(ffxFsr2ContextDestroy(&internal_context->context));

    alloc.dealloc(internal_context->backendInterface.scratchBuffer);
    alloc.dealloc(internal_context);

    return FFX_API_RETURN_OK;
}

ffxReturnCode_t ffxProvider_FSR2::Configure(ffxContext*, const ffxConfigureDescHeader*) const
{
    return FFX_API_RETURN_ERROR_PARAMETER;
}

ffxReturnCode_t ffxProvider_FSR2::Query(ffxContext* context, ffxQueryDescHeader* header) const
{
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);

    switch (header->type)
    {
    case FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTEROFFSET:
    {
        auto desc = reinterpret_cast<ffxQueryDescUpscaleGetJitterOffset*>(header);
        float jitterX, jitterY;
        TRY2(ffxFsr2GetJitterOffset(&jitterX, &jitterY, desc->index, desc->phaseCount));
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
        const int32_t jitterPhaseCount = ffxFsr2GetJitterPhaseCount(desc->renderWidth, desc->displayWidth);

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

        TRY2(ffxFsr2GetRenderResolutionFromQualityMode(&renderWidth, &renderHeight, desc->displayWidth, desc->displayHeight, ConvertQuality(desc->qualityMode)));
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
        float ratio = ffxFsr2GetUpscaleRatioFromQualityMode(ConvertQuality(desc->qualityMode));

        if (desc->pOutUpscaleRatio != nullptr)
        {
            *desc->pOutUpscaleRatio = ratio;
        }
        break;
    }
    default:
        return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    }

    return FFX_API_RETURN_OK;
}

ffxReturnCode_t ffxProvider_FSR2::Dispatch(ffxContext* context, const ffxDispatchDescHeader* header) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFsr2Context* internal_context = (InternalFsr2Context*)context[0];

    ffxDispatchDescHeader* descExt = header->pNext;

    switch (header->type)
    {
    case FFX_API_DISPATCH_DESC_TYPE_UPSCALE:
    {
        auto desc = reinterpret_cast<const ffxDispatchDescUpscale*>(header);

        FfxFsr2DispatchDescription dispatchParameters = {};
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
        dispatchParameters.cameraFovAngleVertical     = desc->cameraFovAngleVertical;
        dispatchParameters.cameraFar                  = desc->cameraFar;
        dispatchParameters.cameraNear                 = desc->cameraNear;
        dispatchParameters.viewSpaceToMetersFactor    = desc->viewSpaceToMetersFactor;

        // Check for and process extensions
        //if (descExt != nullptr)
        //{
        //    TRY(ProcessExtensionsPreDispatch(internal_context, &dispatchParameters, &descExt));
        //}

        TRY2(ffxFsr2ContextDispatch(&internal_context->context, &dispatchParameters));
        break;
    }
    case FFX_API_DISPATCH_DESC_TYPE_UPSCALE_GENERATEREACTIVEMASK:
    {
        auto desc = reinterpret_cast<const ffxDispatchDescUpscaleGenerateReactiveMask*>(header);

        FfxFsr2GenerateReactiveDescription dispatchParameters = {};
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

        TRY2(ffxFsr2ContextGenerateReactiveMask(&internal_context->context, &dispatchParameters));
        break;
    }
    default:
        return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    }

    return FFX_API_RETURN_OK;
}

ffxProvider_FSR2 ffxProvider_FSR2::Instance;
