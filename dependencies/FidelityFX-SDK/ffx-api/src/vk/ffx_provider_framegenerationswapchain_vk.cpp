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

#include "ffx_provider_framegenerationswapchain_vk.h"
#include <ffx_api/ffx_framegeneration.hpp>
#include <ffx_api/vk/ffx_api_vk.hpp>
#include <FidelityFX/host/backends/vk/ffx_vk.h>

#include <stdlib.h>

struct InternalFgScContext
{
    InternalContextHeader            header;
    VkFrameInterpolationInfoFFX      frameInterpolationInfo;
    VkSwapchainKHR                   fiSwapChain;
    FfxSwapchainReplacementFunctions replacementFunctions;
};

//////////////////////////////////////////////
/// Vulkan FFX API overridden functions
//////////////////////////////////////////////

VkResult vkCreateSwapchainFFXAPI(
    VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain, void* pContext)
{
    if (pCreateInfo == nullptr || pContext == nullptr)
        return VK_ERROR_INITIALIZATION_FAILED;

    InternalFgScContext* internal_context = reinterpret_cast<InternalFgScContext*>(pContext);

    if (internal_context->fiSwapChain != VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    if (internal_context != nullptr && internal_context->replacementFunctions.createSwapchainFFX != nullptr)
    {
        result = internal_context->replacementFunctions.createSwapchainFFX(
            device, pCreateInfo, pAllocator, &internal_context->fiSwapChain, &internal_context->frameInterpolationInfo);
    }

    if (result == VK_SUCCESS)
        *pSwapchain = reinterpret_cast<VkSwapchainKHR>(internal_context->fiSwapChain);

    return result;
}

void vkDestroySwapchainFFXAPI(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator, void* pContext)
{
    if (swapchain != VK_NULL_HANDLE)
    {
        InternalFgScContext* internal_context = reinterpret_cast<InternalFgScContext*>(pContext);
        if (swapchain != internal_context->fiSwapChain)
            return;

        if (internal_context != nullptr && internal_context->fiSwapChain != VK_NULL_HANDLE &&
            internal_context->replacementFunctions.destroySwapchainKHR != nullptr)
        {
            internal_context->replacementFunctions.destroySwapchainKHR(device, internal_context->fiSwapChain, pAllocator);
            internal_context->fiSwapChain = VK_NULL_HANDLE;
        }
    }
}

///////////////////////////////////////////////////////////////
/// ffxProvider_FrameGenerationSwapChain_VK implementation
///////////////////////////////////////////////////////////////

bool ffxProvider_FrameGenerationSwapChain_VK::CanProvide(uint64_t type) const
{
    return (type & FFX_API_EFFECT_MASK) == FFX_API_EFFECT_ID_FGSC_VK;
}

uint64_t ffxProvider_FrameGenerationSwapChain_VK::GetId() const
{
    // FG SwapChain VK, version 1.1.2
    return 0xF65D'564B'01'001'002ui64;
}

const char* ffxProvider_FrameGenerationSwapChain_VK::GetVersionName() const
{
    return "1.1.2";
}

inline VkQueueInfoFFX convertQueueInfo(VkQueueInfoFFXAPI queueInfo)
{
    VkQueueInfoFFX info;
    info.queue       = queueInfo.queue;
    info.familyIndex = queueInfo.familyIndex;
    info.submitFunc  = queueInfo.submitFunc;
    return         info;
}

ffxReturnCode_t ffxProvider_FrameGenerationSwapChain_VK::CreateContext(ffxContext* context, ffxCreateContextDescHeader* header, Allocator& alloc) const
{
    if (auto desc = ffx::DynamicCast<ffxCreateContextDescFrameGenerationSwapChainVK>(header))
    {
        InternalFgScContext* internal_context = alloc.construct<InternalFgScContext>();
        VERIFY(internal_context, FFX_API_RETURN_ERROR_MEMORY);
        internal_context->header.provider = this;

        internal_context->frameInterpolationInfo.physicalDevice    = desc->physicalDevice;
        internal_context->frameInterpolationInfo.device            = desc->device;
        internal_context->frameInterpolationInfo.gameQueue         = convertQueueInfo(desc->gameQueue);
        internal_context->frameInterpolationInfo.asyncComputeQueue = convertQueueInfo(desc->asyncComputeQueue);
        internal_context->frameInterpolationInfo.presentQueue      = convertQueueInfo(desc->presentQueue);
        internal_context->frameInterpolationInfo.imageAcquireQueue = convertQueueInfo(desc->imageAcquireQueue);
        internal_context->frameInterpolationInfo.pAllocator        = desc->allocator;

        // set the default values
        internal_context->frameInterpolationInfo.compositionMode = VK_COMPOSITION_MODE_NOT_FORCED_FFX;

        // get the extensions
        for (auto it = header->pNext; it != nullptr; it = it->pNext)
        {
            if (auto mode = ffx::DynamicCast<ffxCreateContextDescFrameGenerationSwapChainModeVK>(it))
            {
                if (mode->composeOnPresentQueue)
                    internal_context->frameInterpolationInfo.compositionMode = VK_COMPOSITION_MODE_PRESENT_QUEUE_FFX;
                else
                    internal_context->frameInterpolationInfo.compositionMode = VK_COMPOSITION_MODE_GAME_QUEUE_FFX;
            }
        }

        FfxSwapchain swapChain = ffxGetSwapchainVK(*desc->swapchain);
        TRY2(ffxReplaceSwapchainForFrameinterpolationVK(desc->gameQueue.queue, swapChain, &desc->createInfo, &internal_context->frameInterpolationInfo));
        internal_context->fiSwapChain = *desc->swapchain = ffxGetVKSwapchain(swapChain);

        internal_context->replacementFunctions = {};
        TRY2(ffxGetSwapchainReplacementFunctionsVK(internal_context->frameInterpolationInfo.device, &internal_context->replacementFunctions));

        *context = internal_context;
        
        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    }
}

ffxReturnCode_t ffxProvider_FrameGenerationSwapChain_VK::DestroyContext(ffxContext* context, Allocator& alloc) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFgScContext* internal_context = reinterpret_cast<InternalFgScContext*>(*context);

    // call the correct function to release the frame interpolation swapchain
    FfxSwapchainReplacementFunctions functions = {};
    ffxGetSwapchainReplacementFunctionsVK(internal_context->frameInterpolationInfo.device, &functions);

    if (functions.destroySwapchainKHR != nullptr)
    {
        functions.destroySwapchainKHR(
            internal_context->frameInterpolationInfo.device, internal_context->fiSwapChain, internal_context->frameInterpolationInfo.pAllocator);
        internal_context->fiSwapChain = VK_NULL_HANDLE;
    }
    else
    {
        vkDestroySwapchainKHR(
            internal_context->frameInterpolationInfo.device, internal_context->fiSwapChain, internal_context->frameInterpolationInfo.pAllocator);
        internal_context->fiSwapChain = VK_NULL_HANDLE;
    }

    alloc.dealloc(internal_context);

    return FFX_API_RETURN_OK;
}

ffxReturnCode_t ffxProvider_FrameGenerationSwapChain_VK::Configure(ffxContext* context, const ffxConfigureDescHeader* header) const
{
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFgScContext* internal_context = reinterpret_cast<InternalFgScContext*>(*context);
    if (auto desc = ffx::DynamicCast<ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceVK>(header))
    {
        TRY2(ffxRegisterFrameinterpolationUiResourceVK(ffxGetSwapchainVK(internal_context->fiSwapChain), Convert(desc->uiResource), desc->flags));

        return FFX_API_RETURN_OK;
    }
    else if (auto desc = ffx::DynamicCast<ffxConfigureDescFrameGenerationSwapChainKeyValueVK>(header))
    {
        TRY2(ffxConfigureFrameInterpolationSwapchainVK(ffxGetSwapchainVK(internal_context->fiSwapChain), static_cast <FfxFrameInterpolationSwapchainConfigureKey> (desc->key), desc->ptr));

        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_PARAMETER;
    }
}

ffxReturnCode_t ffxProvider_FrameGenerationSwapChain_VK::Query(ffxContext* context, ffxQueryDescHeader* header) const
{
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFgScContext* internal_context = reinterpret_cast<InternalFgScContext*>(*context);
    if (auto desc = ffx::DynamicCast<ffxQueryDescFrameGenerationSwapChainInterpolationCommandListVK>(header))
    {
        FfxCommandList outCommandList{};
        TRY2(ffxGetFrameinterpolationCommandlistVK(ffxGetSwapchainVK(internal_context->fiSwapChain), outCommandList));
        *desc->pOutCommandList = outCommandList;

        return FFX_API_RETURN_OK;
    }
    else if (auto desc = ffx::DynamicCast<ffxQueryDescFrameGenerationSwapChainInterpolationTextureVK>(header))
    {
        *desc->pOutTexture = Convert(ffxGetFrameinterpolationTextureVK(ffxGetSwapchainVK(internal_context->fiSwapChain)));
        
        return FFX_API_RETURN_OK;
    }
    else if (auto desc = ffx::DynamicCast<ffxQueryFrameGenerationSwapChainGetGPUMemoryUsageVK>(header))
    {
        TRY2(ffxFrameInterpolationSwapchainGetGpuMemoryUsageVK(ffxGetSwapchainVK(internal_context->fiSwapChain), reinterpret_cast <FfxEffectMemoryUsage*> (desc->gpuMemoryUsageFrameGenerationSwapchain)));
        return FFX_API_RETURN_OK;
    }
    else if (auto desc = ffx::DynamicCast<ffxQueryDescSwapchainReplacementFunctionsVK>(header))
    {
        desc->pOutCreateSwapchainFFXAPI     = vkCreateSwapchainFFXAPI;
        desc->pOutDestroySwapchainFFXAPI    = vkDestroySwapchainFFXAPI;
        desc->pOutGetSwapchainImagesKHR     = internal_context->replacementFunctions.getSwapchainImagesKHR;
        desc->pOutAcquireNextImageKHR       = internal_context->replacementFunctions.acquireNextImageKHR;
        desc->pOutQueuePresentKHR           = internal_context->replacementFunctions.queuePresentKHR;
        desc->pOutSetHdrMetadataEXT         = internal_context->replacementFunctions.setHdrMetadataEXT;
        desc->pOutGetLastPresentCountFFXAPI = internal_context->replacementFunctions.getLastPresentCountFFX;  // Same signature so no need for indirection

        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_PARAMETER;
    }
}

ffxReturnCode_t ffxProvider_FrameGenerationSwapChain_VK::Dispatch(ffxContext* context, const ffxDispatchDescHeader* header) const
{
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);
    InternalFgScContext* internal_context = reinterpret_cast<InternalFgScContext*>(*context);
    if (auto desc = ffx::DynamicCast<ffxDispatchDescFrameGenerationSwapChainWaitForPresentsVK>(header))
    {
        ffxWaitForPresents(internal_context->fiSwapChain);
        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR;
    }
}

ffxProvider_FrameGenerationSwapChain_VK ffxProvider_FrameGenerationSwapChain_VK::Instance;
