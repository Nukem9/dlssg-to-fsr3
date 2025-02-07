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

#include "ffx_provider_framegeneration.h"
#include "backends.h"
#include <ffx_api/ffx_framegeneration.hpp>
#include <FidelityFX/host/ffx_fsr3.h>
#include <FidelityFX/host/ffx_frameinterpolation.h>
#include <FidelityFX/host/ffx_opticalflow.h>
#include <FidelityFX/gpu/fsr3/ffx_fsr3_resources.h>

#include <stdlib.h>

bool ffxProvider_FrameGeneration::CanProvide(uint64_t type) const
{
    return (type & FFX_API_EFFECT_MASK) == FFX_API_EFFECT_ID_FRAMEGENERATION;
}

const uint32_t MAX_QUEUED_FRAMES = 2;

struct InternalFgContext
{
    InternalContextHeader header;

    FfxInterface backendInterfaceFi;
    FfxInterface backendInterfaceShared;
    FfxOpticalflowContext ofContext;
    FfxFrameInterpolationContext fiContext;
    FfxResourceInternal sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_COUNT];
    uint32_t            sharedResoureFrameToggle;
    uint32_t effectContextIdShared;
    float deltaTime;
    bool asyncWorkloadSupported;

    FfxResource HUDLessColor;
    FfxResource distortionField;

    bool frameGenEnabled;
    uint32_t frameGenFlags;
    ffxDispatchDescFrameGenerationPrepare prepareDescriptions[MAX_QUEUED_FRAMES];

    struct Callbacks {
        FfxApiPresentCallbackFunc presentCallback;
        void* presentCallbackUserContext;
        FfxApiFrameGenerationDispatchFunc frameGenerationCallback;
        void* frameGenerationCallbackUserContext;
    } callbacks[MAX_QUEUED_FRAMES];

    uint64_t lastConfigureFrameID;
};

#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X) 
#define MAKE_VERSION_STRING(major, minor, patch) STRINGIFY major "." STRINGIFY minor "." STRINGIFY patch

uint64_t ffxProvider_FrameGeneration::GetId() const
{
    // FG, version from header
    return 0xF600'0000ui64 << 32u | (FFX_SDK_MAKE_VERSION(FFX_FRAMEINTERPOLATION_VERSION_MAJOR, FFX_FRAMEINTERPOLATION_VERSION_MINOR, FFX_FRAMEINTERPOLATION_VERSION_PATCH) & 0xFFFF'FFFF);
}

const char* ffxProvider_FrameGeneration::GetVersionName() const
{
    return MAKE_VERSION_STRING(FFX_FRAMEINTERPOLATION_VERSION_MAJOR, FFX_FRAMEINTERPOLATION_VERSION_MINOR, FFX_FRAMEINTERPOLATION_VERSION_PATCH);
}

ffxReturnCode_t ffxProvider_FrameGeneration::CreateContext(ffxContext* context, ffxCreateContextDescHeader* header, Allocator& alloc) const
{
    if (auto desc = ffx::DynamicCast<ffxCreateContextDescFrameGeneration>(header))
    {
        InternalFgContext* internal_context = alloc.construct<InternalFgContext>();
        VERIFY(internal_context, FFX_API_RETURN_ERROR_MEMORY);
        internal_context->header.provider = this;

        TRY(MustCreateBackend(header, &internal_context->backendInterfaceShared, 1, alloc));
        TRY(MustCreateBackend(header, &internal_context->backendInterfaceFi, 2, alloc));

        { // copied from ffxFsr3ContextCreate, simplified.
            internal_context->asyncWorkloadSupported = (desc->flags & FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT) != 0;

            TRY2(internal_context->backendInterfaceShared.fpCreateBackendContext(&internal_context->backendInterfaceShared, FFX_EFFECT_SHAREDAPIBACKEND, nullptr, &internal_context->effectContextIdShared));
        
            FfxOpticalflowContextDescription ofDescription = {};
            ofDescription.backendInterface                 = internal_context->backendInterfaceFi;
            ofDescription.resolution.width                 = desc->displaySize.width;
            ofDescription.resolution.height                = desc->displaySize.height;

            // set up Opticalflow
            TRY2(ffxOpticalflowContextCreate(&internal_context->ofContext, &ofDescription));

            FfxFrameInterpolationContextDescription fiDescription = {};
            fiDescription.backendInterface  = internal_context->backendInterfaceFi;
            fiDescription.flags |= (desc->flags & FFX_FRAMEGENERATION_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS) ? FFX_FRAMEINTERPOLATION_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS : 0;
            fiDescription.flags |= (desc->flags & FFX_FRAMEGENERATION_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION) ? FFX_FRAMEINTERPOLATION_ENABLE_JITTER_MOTION_VECTORS : 0;
            fiDescription.flags |= (desc->flags & FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED) ? FFX_FRAMEINTERPOLATION_ENABLE_DEPTH_INVERTED : 0;
            fiDescription.flags |= (desc->flags & FFX_FRAMEGENERATION_ENABLE_DEPTH_INFINITE) ? FFX_FRAMEINTERPOLATION_ENABLE_DEPTH_INFINITE : 0;
            fiDescription.flags |= (desc->flags & FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE) ? FFX_FRAMEINTERPOLATION_ENABLE_HDR_COLOR_INPUT : 0;
            fiDescription.flags |= (desc->flags & FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT) ? FFX_FRAMEINTERPOLATION_ENABLE_ASYNC_SUPPORT : 0;
            fiDescription.maxRenderSize.width     = desc->maxRenderSize.width;
            fiDescription.maxRenderSize.height    = desc->maxRenderSize.height;
            fiDescription.displaySize.width       = desc->displaySize.width;
            fiDescription.displaySize.height      = desc->displaySize.height;
            fiDescription.backBufferFormat = ConvertEnum<FfxSurfaceFormat>(desc->backBufferFormat);
            fiDescription.previousInterpolationSourceFormat = ConvertEnum<FfxSurfaceFormat>(desc->backBufferFormat);
            for (auto it = header; it; it = it->pNext)
            {
                if (auto descHudless = ffx::DynamicCast<ffxCreateContextDescFrameGenerationHudless>(it))
                {
                    fiDescription.previousInterpolationSourceFormat = ConvertEnum<FfxSurfaceFormat>(descHudless->hudlessBackBufferFormat);
                }
            }
            // set up Frameinterpolation
            TRY2(ffxFrameInterpolationContextCreate(&internal_context->fiContext, &fiDescription));

            // set up optical flow resources
            FfxOpticalflowSharedResourceDescriptions ofResourceDescs = {};
            TRY2(ffxOpticalflowGetSharedResourceDescriptions(&internal_context->ofContext, &ofResourceDescs));

            TRY2(internal_context->backendInterfaceShared.fpCreateResource(&internal_context->backendInterfaceShared, &ofResourceDescs.opticalFlowVector, internal_context->effectContextIdShared, &internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR]));
            TRY2(internal_context->backendInterfaceShared.fpCreateResource(&internal_context->backendInterfaceShared, &ofResourceDescs.opticalFlowSCD, internal_context->effectContextIdShared, &internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_SCD_OUTPUT]));
        }
        {
            FfxFrameInterpolationSharedResourceDescriptions fiResourceDescs = {};
            TRY2(ffxFrameInterpolationGetSharedResourceDescriptions(&internal_context->fiContext, &fiResourceDescs));

            internal_context->sharedResoureFrameToggle = 0;
            wchar_t Name[256] = {};
            for (FfxUInt32 i = 0; i < 2; i++)
            {
                FfxCreateResourceDescription dilD = fiResourceDescs.dilatedDepth;
                swprintf(Name, 255, L"%s%d", fiResourceDescs.dilatedDepth.name, i);
                dilD.name = Name;
                TRY2(internal_context->backendInterfaceShared.fpCreateResource(
                    &internal_context->backendInterfaceShared,
                    &dilD,
                    internal_context->effectContextIdShared,
                    &internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0 + (i * FFX_FSR3_RESOURCE_IDENTIFIER_UPSCALED_COUNT)]));

                FfxCreateResourceDescription dilMVs = fiResourceDescs.dilatedMotionVectors;
                swprintf(Name, 255, L"%s%d", fiResourceDescs.dilatedMotionVectors.name, i);
                dilMVs.name = Name;
                TRY2(internal_context->backendInterfaceShared.fpCreateResource(
                    &internal_context->backendInterfaceShared,
                    &dilMVs,
                    internal_context->effectContextIdShared,
                    &internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0 + (i * FFX_FSR3_RESOURCE_IDENTIFIER_UPSCALED_COUNT)]));

                FfxCreateResourceDescription recND = fiResourceDescs.reconstructedPrevNearestDepth;
                swprintf(Name, 255, L"%s%d", fiResourceDescs.reconstructedPrevNearestDepth.name, i);
                recND.name = Name;
                TRY2(internal_context->backendInterfaceShared.fpCreateResource(
                    &internal_context->backendInterfaceShared,
                    &recND,
                    internal_context->effectContextIdShared,
                    &internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0 + (i * FFX_FSR3_RESOURCE_IDENTIFIER_UPSCALED_COUNT)]));
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

ffxReturnCode_t ffxProvider_FrameGeneration::DestroyContext(ffxContext* context, Allocator& alloc) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFgContext* internal_context = reinterpret_cast<InternalFgContext*>(*context);
    
    { // copied from ffxFsr3ContextDestroy, simplified.
        for (FfxUInt32 i = 0; i < FFX_FSR3_RESOURCE_IDENTIFIER_COUNT; i++)
        {
            TRY2(internal_context->backendInterfaceShared.fpDestroyResource(&internal_context->backendInterfaceShared, internal_context->sharedResources[i], internal_context->effectContextIdShared));
        }

        TRY2(ffxFrameInterpolationContextDestroy(&internal_context->fiContext));

        TRY2(ffxOpticalflowContextDestroy(&internal_context->ofContext));

        TRY2(internal_context->backendInterfaceShared.fpDestroyBackendContext(&internal_context->backendInterfaceShared, internal_context->effectContextIdShared));
    }

    alloc.dealloc(internal_context->backendInterfaceFi.scratchBuffer);
    alloc.dealloc(internal_context->backendInterfaceShared.scratchBuffer);
    alloc.dealloc(internal_context);

    return FFX_API_RETURN_OK;
}

ffxReturnCode_t ffxProvider_FrameGeneration::Configure(ffxContext* context, const ffxConfigureDescHeader* header) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFgContext* internal_context = reinterpret_cast<InternalFgContext*>(*context);
    if (auto desc = ffx::DynamicCast<ffxConfigureDescFrameGeneration>(header))
    {
        FfxFrameGenerationConfig config{};
        config.allowAsyncWorkloads = desc->allowAsyncWorkloads;
        config.flags = desc->flags;

        size_t callbacksIndex = desc->frameID % MAX_QUEUED_FRAMES;

        bool const bPresentCallbackChanged = (internal_context->callbacks[callbacksIndex].presentCallback != desc->presentCallback) || (desc->presentCallback && (internal_context->callbacks[callbacksIndex].presentCallbackUserContext != desc->presentCallbackUserContext));
        bool const bFrameGenerationCallback = (internal_context->callbacks[callbacksIndex].frameGenerationCallback != desc->frameGenerationCallback) || (desc->frameGenerationCallback && (internal_context->callbacks[callbacksIndex].frameGenerationCallbackUserContext != desc->frameGenerationCallbackUserContext));
        internal_context->callbacks[callbacksIndex].presentCallback = desc->presentCallback;
        internal_context->callbacks[callbacksIndex].frameGenerationCallback = desc->frameGenerationCallback;
        internal_context->callbacks[callbacksIndex].presentCallbackUserContext = desc->presentCallbackUserContext;
        internal_context->callbacks[callbacksIndex].frameGenerationCallbackUserContext = desc->frameGenerationCallbackUserContext;

        config.frameGenerationCallback = nullptr;
        config.frameGenerationCallbackContext = nullptr;
        if (desc->frameGenerationCallback != nullptr)
        {
            config.frameGenerationCallback = [](const FfxFrameGenerationDispatchDescription* desc, void* ctx) -> FfxErrorCode {
                size_t callbacksIndex = desc->frameID % MAX_QUEUED_FRAMES;
                InternalFgContext* internal_context = reinterpret_cast<InternalFgContext*>(ctx);
                auto callbacks = &internal_context->callbacks[callbacksIndex];
                VERIFY(callbacks->frameGenerationCallback, FFX_ERROR_BACKEND_API_ERROR);
                
                ffx::DispatchDescFrameGeneration dispatchDesc{};
                
                dispatchDesc.backbufferTransferFunction = desc->backBufferTransferFunction;
                dispatchDesc.commandList = desc->commandList;
                dispatchDesc.minMaxLuminance[0] = desc->minMaxLuminance[0];
                dispatchDesc.minMaxLuminance[1] = desc->minMaxLuminance[1];
                dispatchDesc.numGeneratedFrames = desc->numInterpolatedFrames;
                dispatchDesc.outputs[0] = Convert(desc->outputs[0]);
                dispatchDesc.outputs[1] = Convert(desc->outputs[1]);
                dispatchDesc.outputs[2] = Convert(desc->outputs[2]);
                dispatchDesc.outputs[3] = Convert(desc->outputs[3]);
                dispatchDesc.presentColor = Convert(desc->presentColor);
                dispatchDesc.reset = desc->reset;
                dispatchDesc.generationRect.top = desc->interpolationRect.top;
                dispatchDesc.generationRect.left = desc->interpolationRect.left;
                dispatchDesc.generationRect.height = desc->interpolationRect.height;
                dispatchDesc.generationRect.width = desc->interpolationRect.width;
                dispatchDesc.frameID = desc->frameID;
                
                if (FFX_API_RETURN_OK != callbacks->frameGenerationCallback(&dispatchDesc, callbacks->frameGenerationCallbackUserContext))
                    return FFX_ERROR_BACKEND_API_ERROR;
                return FFX_OK;
            };
            config.frameGenerationCallbackContext = internal_context;
        }

        config.presentCallback = nullptr;
        config.presentCallbackContext = nullptr;
        if (desc->presentCallback != nullptr)
        {
            config.presentCallback = [](const FfxPresentCallbackDescription* params, void* ctx) -> FfxErrorCode {
                size_t callbacksIndex = params->frameID % MAX_QUEUED_FRAMES;
                InternalFgContext* internal_context = reinterpret_cast<InternalFgContext*>(ctx);
                auto callbacks = &internal_context->callbacks[callbacksIndex];
                VERIFY(callbacks->presentCallback, FFX_ERROR_BACKEND_API_ERROR);
                
                ffxCallbackDescFrameGenerationPresent cbDesc{};
                cbDesc.header.pNext = nullptr;
                cbDesc.header.type = FFX_API_CALLBACK_DESC_TYPE_FRAMEGENERATION_PRESENT;

                cbDesc.commandList = params->commandList;
                cbDesc.currentBackBuffer = Convert(params->currentBackBuffer);
                cbDesc.currentUI = Convert(params->currentUI);
                cbDesc.device = params->device;
                cbDesc.isGeneratedFrame = params->isInterpolatedFrame;
                cbDesc.outputSwapChainBuffer = Convert(params->outputSwapChainBuffer);
                cbDesc.frameID = params->frameID;

                if (FFX_API_RETURN_OK != callbacks->presentCallback(&cbDesc, callbacks->presentCallbackUserContext))
                    return FFX_ERROR_BACKEND_API_ERROR;
                return FFX_OK;
            };
            config.presentCallbackContext = internal_context;
        }

        config.drawDebugPacingLines = false;
        if (desc->flags & FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_PACING_LINES)
        {
            config.drawDebugPacingLines = true;
        }

        config.frameGenerationEnabled = desc->frameGenerationEnabled;
        config.HUDLessColor = Convert(desc->HUDLessColor);
        config.onlyPresentInterpolated = desc->onlyPresentGenerated;
        config.swapChain = desc->swapChain;

        config.interpolationRect.top    = desc->generationRect.top;
        config.interpolationRect.left   = desc->generationRect.left;
        config.interpolationRect.width  = desc->generationRect.width;
        config.interpolationRect.height = desc->generationRect.height;

        config.frameID = desc->frameID;

        { // copied from ffxFsr3ConfigureFrameGeneration
            internal_context->frameGenFlags = config.flags;
            internal_context->HUDLessColor = config.HUDLessColor;

            if (config.flags & FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW)
            {
                config.onlyPresentInterpolated = true;
            }

            internal_context->frameGenEnabled = config.frameGenerationEnabled;

            if (!(config.flags & FFX_FRAMEGENERATION_FLAG_NO_SWAPCHAIN_CONTEXT_NOTIFY))
            {
                // When the frame ID is not incrementing by 1 we could end up overwriting a pointer that is in-use, so reset the swap-chain state
                if (((internal_context->lastConfigureFrameID + 1) != desc->frameID) && (bPresentCallbackChanged || bFrameGenerationCallback))
                {
                    FfxFrameGenerationConfig resetConfig = config;
                    resetConfig.frameGenerationCallback = nullptr;
                    resetConfig.frameGenerationCallbackContext = nullptr;
                    resetConfig.presentCallback = nullptr;
                    resetConfig.presentCallbackContext = nullptr;

                    TRY2(internal_context->backendInterfaceShared.fpSwapChainConfigureFrameGeneration(&resetConfig));
                }

                TRY2(internal_context->backendInterfaceShared.fpSwapChainConfigureFrameGeneration(&config));

                internal_context->lastConfigureFrameID = desc->frameID;
            }
        }

        internal_context->distortionField = FfxResource({});
        for (auto it = header; it; it = it->pNext)
        {
            if (auto distortionFieldDesc = ffx::DynamicCast<ffxConfigureDescFrameGenerationRegisterDistortionFieldResource>(it))
            {
                if (distortionFieldDesc->distortionField.resource)
                {
                    internal_context->distortionField = Convert(distortionFieldDesc->distortionField);
                }
            }
        }

        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_PARAMETER;
    }
}

ffxReturnCode_t ffxProvider_FrameGeneration::Query(ffxContext* context, ffxQueryDescHeader* header) const
{
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFgContext* internal_context = reinterpret_cast<InternalFgContext*>(*context);
    if (auto desc = ffx::DynamicCast<ffxQueryDescFrameGenerationGetGPUMemoryUsage>(header))
    {
        FfxEffectMemoryUsage pGpuMemoryUsageFrameGeneration;
        FfxEffectMemoryUsage pGpuMemoryUsageOpticalFlow;
        FfxEffectMemoryUsage pGpuMemoryUsageShared;

        TRY2(ffxFrameInterpolationContextGetGpuMemoryUsage(&internal_context->fiContext, &pGpuMemoryUsageFrameGeneration));
        TRY2(ffxOpticalflowContextGetGpuMemoryUsage(&internal_context->ofContext, &pGpuMemoryUsageOpticalFlow));
        TRY2(ffxSharedContextGetGpuMemoryUsage(&internal_context->backendInterfaceShared, &pGpuMemoryUsageShared));
        desc->gpuMemoryUsageFrameGeneration->totalUsageInBytes = pGpuMemoryUsageFrameGeneration.totalUsageInBytes + pGpuMemoryUsageOpticalFlow.totalUsageInBytes + pGpuMemoryUsageShared.totalUsageInBytes;
        desc->gpuMemoryUsageFrameGeneration->aliasableUsageInBytes = pGpuMemoryUsageFrameGeneration.aliasableUsageInBytes + pGpuMemoryUsageOpticalFlow.aliasableUsageInBytes + pGpuMemoryUsageShared.aliasableUsageInBytes;
        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    }
}

ffxReturnCode_t ffxProvider_FrameGeneration::Dispatch(ffxContext* context, const ffxDispatchDescHeader* header) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFgContext* internal_context = reinterpret_cast<InternalFgContext*>(*context);
    if (auto desc = ffx::DynamicCast<ffxDispatchDescFrameGeneration>(header))
    {
        const ffxDispatchDescFrameGenerationPrepare* prepDesc = &internal_context->prepareDescriptions[desc->frameID % MAX_QUEUED_FRAMES];

        // Optical flow
        {
            FfxOpticalflowDispatchDescription ofDispatchDesc{};
            ofDispatchDesc.commandList = desc->commandList;
            ofDispatchDesc.color = Convert(desc->presentColor);
            if (internal_context->HUDLessColor.resource)
            {
                ofDispatchDesc.color = internal_context->HUDLessColor;
            }
            ofDispatchDesc.reset = desc->reset;
            ofDispatchDesc.backbufferTransferFunction = desc->backbufferTransferFunction;
            ofDispatchDesc.minMaxLuminance.x = desc->minMaxLuminance[0];
            ofDispatchDesc.minMaxLuminance.y = desc->minMaxLuminance[1];
            ofDispatchDesc.opticalFlowVector = internal_context->backendInterfaceShared.fpGetResource(&internal_context->backendInterfaceShared, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR]);
            ofDispatchDesc.opticalFlowSCD = internal_context->backendInterfaceShared.fpGetResource(&internal_context->backendInterfaceShared, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_SCD_OUTPUT]);

            TRY2(ffxOpticalflowContextDispatch(&internal_context->ofContext, &ofDispatchDesc));
        }

        // Frame interpolation
        {
            FfxFrameInterpolationDispatchDescription fiDispatchDesc{};

            // don't dispatch interpolation async for now: use the same commandlist for copy and interpolate
            fiDispatchDesc.commandList = desc->commandList;
            fiDispatchDesc.displaySize.width = desc->presentColor.description.width;
            fiDispatchDesc.displaySize.height = desc->presentColor.description.height;
            fiDispatchDesc.currentBackBuffer = Convert(desc->presentColor);
            fiDispatchDesc.currentBackBuffer_HUDLess = internal_context->HUDLessColor;
            fiDispatchDesc.reset = desc->reset;

            fiDispatchDesc.renderSize.width  = prepDesc->renderSize.width;
            fiDispatchDesc.renderSize.height = prepDesc->renderSize.height;
            fiDispatchDesc.output = Convert(desc->outputs[0]);
            fiDispatchDesc.opticalFlowVector = internal_context->backendInterfaceShared.fpGetResource(&internal_context->backendInterfaceShared, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR]);
            fiDispatchDesc.opticalFlowSceneChangeDetection = internal_context->backendInterfaceShared.fpGetResource(&internal_context->backendInterfaceShared, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_SCD_OUTPUT]);
            fiDispatchDesc.opticalFlowBlockSize = 8;
            fiDispatchDesc.opticalFlowScale = { 1.f / fiDispatchDesc.displaySize.width, 1.f / fiDispatchDesc.displaySize.height };
            fiDispatchDesc.frameTimeDelta = prepDesc->frameTimeDelta;
            fiDispatchDesc.cameraNear = prepDesc->cameraNear;
            fiDispatchDesc.cameraFar = prepDesc->cameraFar;
            fiDispatchDesc.viewSpaceToMetersFactor = prepDesc->viewSpaceToMetersFactor;
            fiDispatchDesc.cameraFovAngleVertical = prepDesc->cameraFovAngleVertical;
            
            fiDispatchDesc.dilatedDepth = internal_context->backendInterfaceShared.fpGetResource( &internal_context->backendInterfaceShared, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0 + (internal_context->sharedResoureFrameToggle * FFX_FSR3_RESOURCE_IDENTIFIER_UPSCALED_COUNT)]);
            fiDispatchDesc.dilatedMotionVectors = internal_context->backendInterfaceShared.fpGetResource( &internal_context->backendInterfaceShared, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0 + (internal_context->sharedResoureFrameToggle * FFX_FSR3_RESOURCE_IDENTIFIER_UPSCALED_COUNT)]);
            fiDispatchDesc.reconstructedPrevDepth = internal_context->backendInterfaceShared.fpGetResource( &internal_context->backendInterfaceShared, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0 + (internal_context->sharedResoureFrameToggle * FFX_FSR3_RESOURCE_IDENTIFIER_UPSCALED_COUNT)]);

            if (desc->generationRect.height == 0 && desc->generationRect.width == 0)
            {
                fiDispatchDesc.interpolationRect.left   = 0;
                fiDispatchDesc.interpolationRect.top    = 0;
                fiDispatchDesc.interpolationRect.width  = desc->presentColor.description.width;
                fiDispatchDesc.interpolationRect.height = desc->presentColor.description.height;
            }
            else
            {
                fiDispatchDesc.interpolationRect.top    = desc->generationRect.top;
                fiDispatchDesc.interpolationRect.left   = desc->generationRect.left;
                fiDispatchDesc.interpolationRect.width  = desc->generationRect.width;
                fiDispatchDesc.interpolationRect.height = desc->generationRect.height;
            }
            
            if (internal_context->frameGenFlags & FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES)
            {
                fiDispatchDesc.flags |= FFX_FRAMEINTERPOLATION_DISPATCH_DRAW_DEBUG_TEAR_LINES;
            }

            
            if (internal_context->frameGenFlags & FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS)
            {
                fiDispatchDesc.flags |= FFX_FRAMEINTERPOLATION_DISPATCH_DRAW_DEBUG_RESET_INDICATORS;
            }

            if (internal_context->frameGenFlags & FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW)
            {
                fiDispatchDesc.flags |= FFX_FRAMEINTERPOLATION_DISPATCH_DRAW_DEBUG_VIEW;
            }

            fiDispatchDesc.backBufferTransferFunction = ConvertEnum<FfxBackbufferTransferFunction>(desc->backbufferTransferFunction);
            fiDispatchDesc.minMaxLuminance[0]         = desc->minMaxLuminance[0];
            fiDispatchDesc.minMaxLuminance[1]         = desc->minMaxLuminance[1];

            fiDispatchDesc.frameID = desc->frameID;

            if (internal_context->distortionField.resource)
            {
                fiDispatchDesc.distortionField = internal_context->distortionField;
            }
            TRY2(ffxFrameInterpolationDispatch(&internal_context->fiContext, &fiDispatchDesc));
        }

        return FFX_API_RETURN_OK;
    }
    else if (auto desc = ffx::DynamicCast<ffxDispatchDescFrameGenerationPrepare>(header))
    {
        internal_context->prepareDescriptions[desc->frameID % MAX_QUEUED_FRAMES] = *desc;

        internal_context->sharedResoureFrameToggle = (internal_context->sharedResoureFrameToggle + 1) & 1;

        FfxFrameInterpolationPrepareDescription dispatchDesc{};
        dispatchDesc.flags = desc->flags; // TODO: flag conversion?
        dispatchDesc.commandList = desc->commandList;
        dispatchDesc.renderSize.width  = desc->renderSize.width;
        dispatchDesc.renderSize.height = desc->renderSize.height;
        dispatchDesc.jitterOffset.x = desc->jitterOffset.x;
        dispatchDesc.jitterOffset.y = desc->jitterOffset.y;
        dispatchDesc.motionVectorScale.x = desc->motionVectorScale.x;
        dispatchDesc.motionVectorScale.y = desc->motionVectorScale.y;
        dispatchDesc.frameTimeDelta = desc->frameTimeDelta;
        dispatchDesc.cameraNear = desc->cameraNear;
        dispatchDesc.cameraFar = desc->cameraFar;
        dispatchDesc.viewSpaceToMetersFactor = desc->viewSpaceToMetersFactor;
        dispatchDesc.cameraFovAngleVertical = desc->cameraFovAngleVertical;
        dispatchDesc.depth = Convert(desc->depth);
        dispatchDesc.motionVectors = Convert(desc->motionVectors);
        dispatchDesc.frameID = desc->frameID;

        dispatchDesc.dilatedDepth = internal_context->backendInterfaceShared.fpGetResource( &internal_context->backendInterfaceShared, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0 + (internal_context->sharedResoureFrameToggle * FFX_FSR3_RESOURCE_IDENTIFIER_UPSCALED_COUNT)]);
        dispatchDesc.dilatedMotionVectors = internal_context->backendInterfaceShared.fpGetResource( &internal_context->backendInterfaceShared, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0 + (internal_context->sharedResoureFrameToggle * FFX_FSR3_RESOURCE_IDENTIFIER_UPSCALED_COUNT)]);
        dispatchDesc.reconstructedPrevDepth = internal_context->backendInterfaceShared.fpGetResource( &internal_context->backendInterfaceShared, internal_context->sharedResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0 + (internal_context->sharedResoureFrameToggle * FFX_FSR3_RESOURCE_IDENTIFIER_UPSCALED_COUNT)]);

        TRY2(ffxFrameInterpolationPrepare(&internal_context->fiContext, &dispatchDesc));

        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_PARAMETER;
    }
}

ffxProvider_FrameGeneration ffxProvider_FrameGeneration::Instance;
