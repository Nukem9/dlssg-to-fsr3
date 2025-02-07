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

#include "core/backend_interface.h"
#include "core/backend_implementations/backend_interface_vk.h"

#include "core/framework.h"
#include "render/texture.h"
#include "render/swapchain.h"
#include "render/vk/swapchain_vk.h"
#include "render/vk/buffer_vk.h"

#if !SUPPORT_RUNTIME_SHADER_RECOMPILE
// If runtime shader recompile not supported then the backend is a
// static library or load-time linked dll, so use the header to define
// these functions.
static GetScratchMemorySizeFunc s_pFfxGetScratchMemorySizeFunc = ffxGetScratchMemorySizeVK;
static GetInterfaceFunc         s_pFfxGetInterfaceFunc         = ffxGetInterfaceVK;
static GetDeviceVKFunc          s_pFfxGetDeviceFunc            = ffxGetDeviceVK;
static GetCommandListFunc       s_pFfxGetCommandListFunc       = ffxGetCommandListVK;
static GetPipelineFunc          s_pFfxGetPipelineFunc          = ffxGetPipelineVK;
static GetResourceFunc          s_pFfxGetResourceFunc          = ffxGetResourceVK;
// These functions were added for FSR 3.
static ReplaceSwapchainForFrameinterpolationFunc s_pFfxReplaceSwapchainForFrameinterpolationFunc = ffxReplaceSwapchainForFrameinterpolationVK;
static RegisterFrameinterpolationUiResourceFunc  s_pFfxRegisterFrameinterpolationUiResourceFunc  = ffxRegisterFrameinterpolationUiResourceVK;
static GetInterpolationCommandlistFunc           s_pFfxGetInterpolationCommandlistFunc           = ffxGetFrameinterpolationCommandlistVK;
static GetSwapchainFunc                          s_pFfxGetSwapchainFunc                          = ffxGetSwapchainVK;
static GetCommandQueueFunc                       s_pFfxGetCommandQueueFunc                       = ffxGetCommandQueueVK;
static GetImageResourceDescriptionFunc           s_pFfxGetImageResourceDescriptionFunc           = ffxGetImageResourceDescriptionVK;
static GetBufferResourceDescriptionFunc          s_pFfxGetBufferResourceDescriptionFunc          = ffxGetBufferResourceDescriptionVK;
static GetFrameinterpolationTextureFunc          s_pFfxGetFrameinterpolationTextureFunc          = ffxGetFrameinterpolationTextureVK;
static GetVKSwapchainFunc                        s_pFfxGetVKSwapchainFunc                        = ffxGetVKSwapchain;
static GetSwapchainReplacementFunctionsFunc      s_pFfxGetSwapchainReplacementFunctionsFunc      = ffxGetSwapchainReplacementFunctionsVK;
#else // SUPPORT_RUNTIME_SHADER_RECOMPILE
// If runtime shader recompile is supported then the backend is a
// dll that is loaded at runtime by the backend_shader_reloader library.
// The definition/address of these functions is set at runtime.
static GetScratchMemorySizeFunc s_pFfxGetScratchMemorySizeFunc = nullptr;
static GetInterfaceFunc         s_pFfxGetInterfaceFunc         = nullptr;
static GetDeviceVKFunc          s_pFfxGetDeviceFunc            = nullptr;
static GetCommandListFunc       s_pFfxGetCommandListFunc       = nullptr;
static GetPipelineFunc          s_pFfxGetPipelineFunc          = nullptr;
static GetResourceFunc          s_pFfxGetResourceFunc          = nullptr;
// These functions were added for FSR 3.
static ReplaceSwapchainForFrameinterpolationFunc s_pFfxReplaceSwapchainForFrameinterpolationFunc = nullptr;
static RegisterFrameinterpolationUiResourceFunc  s_pFfxRegisterFrameinterpolationUiResourceFunc  = nullptr;
static GetInterpolationCommandlistFunc           s_pFfxGetInterpolationCommandlistFunc           = nullptr;
static GetSwapchainFunc                          s_pFfxGetSwapchainFunc                          = nullptr;
static GetCommandQueueFunc                       s_pFfxGetCommandQueueFunc                       = nullptr;
static GetImageResourceDescriptionFunc           s_pFfxGetImageResourceDescriptionFunc           = nullptr;
static GetBufferResourceDescriptionFunc          s_pFfxGetBufferResourceDescriptionFunc          = nullptr;
static GetFrameinterpolationTextureFunc          s_pFfxGetFrameinterpolationTextureFunc          = nullptr;
static GetVKSwapchainFunc                        s_pFfxGetVKSwapchainFunc                        = nullptr;
static GetSwapchainReplacementFunctionsFunc      s_pFfxGetSwapchainReplacementFunctionsFunc      = nullptr;
#endif // SUPPORT_RUNTIME_SHADER_RECOMPILE

void InitVKBackendInterface(GetScratchMemorySizeFunc                  pFfxGetScratchMemorySizeFunc,
                            GetInterfaceFunc                          pFfxGetInterfaceFunc,
                            GetDeviceVKFunc                           pFfxGetDeviceFunc,
                            GetCommandListFunc                        pFfxGetCommandListFunc,
                            GetPipelineFunc                           pFfxGetPipelineFunc,
                            GetResourceFunc                           pFfxGetResourceFunc,
                            ReplaceSwapchainForFrameinterpolationFunc pFfxReplaceSwapchainForFrameinterpolationFunc,
                            RegisterFrameinterpolationUiResourceFunc  pFfxRegisterFrameinterpolationUiResourceFunc,
                            GetInterpolationCommandlistFunc           pFfxGetInterpolationCommandlistFunc,
                            GetSwapchainFunc                          pFfxGetSwapchainFunc,
                            GetCommandQueueFunc                       pFfxGetCommandQueueFunc,
                            GetImageResourceDescriptionFunc           pFfxGetImageResourceDescriptionFunc,
                            GetBufferResourceDescriptionFunc          pFfxGetBufferResourceDescriptionFunc,
                            GetFrameinterpolationTextureFunc          pFfxGetFrameinterpolationTextureFunc,
                            GetVKSwapchainFunc                        pFfxGetVKSwapchainFunc,
                            GetSwapchainReplacementFunctionsFunc      pGetSwapchainReplacementFunctionsFunc)
{
    s_pFfxGetScratchMemorySizeFunc                  = pFfxGetScratchMemorySizeFunc;
    s_pFfxGetInterfaceFunc                          = pFfxGetInterfaceFunc;
    s_pFfxGetDeviceFunc                             = pFfxGetDeviceFunc;
    s_pFfxGetCommandListFunc                        = pFfxGetCommandListFunc;
    s_pFfxGetPipelineFunc                           = pFfxGetPipelineFunc;
    s_pFfxGetResourceFunc                           = pFfxGetResourceFunc;
    s_pFfxReplaceSwapchainForFrameinterpolationFunc = pFfxReplaceSwapchainForFrameinterpolationFunc;
    s_pFfxRegisterFrameinterpolationUiResourceFunc  = pFfxRegisterFrameinterpolationUiResourceFunc;
    s_pFfxGetInterpolationCommandlistFunc           = pFfxGetInterpolationCommandlistFunc;
    s_pFfxGetSwapchainFunc                          = pFfxGetSwapchainFunc;
    s_pFfxGetCommandQueueFunc                       = pFfxGetCommandQueueFunc;
    s_pFfxGetImageResourceDescriptionFunc           = pFfxGetImageResourceDescriptionFunc;
    s_pFfxGetBufferResourceDescriptionFunc          = pFfxGetBufferResourceDescriptionFunc;
    s_pFfxGetFrameinterpolationTextureFunc          = pFfxGetFrameinterpolationTextureFunc;
    s_pFfxGetVKSwapchainFunc                        = pFfxGetVKSwapchainFunc;
    s_pFfxGetSwapchainReplacementFunctionsFunc      = pGetSwapchainReplacementFunctionsFunc;
}

namespace SDKWrapper
{

size_t ffxGetScratchMemorySize(size_t maxContexts)
{
    CAULDRON_ASSERT(s_pFfxGetScratchMemorySizeFunc);
    return s_pFfxGetScratchMemorySizeFunc(cauldron::GetDevice()->GetImpl()->VKPhysicalDevice(), maxContexts);
}

FfxErrorCode ffxGetInterface(FfxInterface* backendInterface, cauldron::Device* device, void* scratchBuffer, size_t scratchBufferSize, size_t maxContexts)
{
    CAULDRON_ASSERT(s_pFfxGetInterfaceFunc);
    VkDeviceContext vkDeviceContext = {device->GetImpl()->VKDevice(), device->GetImpl()->VKPhysicalDevice(), vkGetDeviceProcAddr};
    return s_pFfxGetInterfaceFunc(backendInterface, s_pFfxGetDeviceFunc(&vkDeviceContext), scratchBuffer, scratchBufferSize, maxContexts);
}

FfxCommandList ffxGetCommandList(cauldron::CommandList* cauldronCmdList)
{
    CAULDRON_ASSERT(s_pFfxGetCommandListFunc);
    return s_pFfxGetCommandListFunc(cauldronCmdList->GetImpl()->VKCmdBuffer());
}

FfxPipeline ffxGetPipeline(cauldron::PipelineObject* cauldronPipeline)
{
    CAULDRON_ASSERT(s_pFfxGetPipelineFunc);
    return s_pFfxGetPipelineFunc(cauldronPipeline->GetImpl()->VKPipeline());
}

FfxResource ffxGetResource(const cauldron::GPUResource* cauldronResource,
                           const wchar_t* name,
                           FfxResourceStates state,
                           FfxResourceUsage additionalUsages)
{
    CAULDRON_ASSERT(s_pFfxGetResourceFunc);
    CAULDRON_ASSERT(s_pFfxGetImageResourceDescriptionFunc);
    CAULDRON_ASSERT(s_pFfxGetBufferResourceDescriptionFunc);
    if (cauldronResource == nullptr)
    {
        return s_pFfxGetResourceFunc(nullptr, FfxResourceDescription(), name, state);
    }
    else if (cauldronResource->GetImpl()->IsBuffer())
    {
        VkBuffer buffer = cauldronResource->GetImpl()->GetBuffer();
        return s_pFfxGetResourceFunc(
            (void*)buffer, s_pFfxGetBufferResourceDescriptionFunc(buffer, cauldronResource->GetImpl()->GetBufferCreateInfo(), additionalUsages), name, state);
    }
    else  //if (cauldronResource->GetImpl()->IsTexture())
    {
        VkImage image = cauldronResource->GetImpl()->GetImage();
        return s_pFfxGetResourceFunc(
            (void*)image, s_pFfxGetImageResourceDescriptionFunc(image, cauldronResource->GetImpl()->GetImageCreateInfo(), additionalUsages), name, state);
    }
}

FfxErrorCode ffxReplaceSwapchainForFrameinterpolation(FfxCommandQueue gameQueue, FfxSwapchain& gameSwapChain, const void* replacementParameters)
{
    CAULDRON_ASSERT(s_pFfxReplaceSwapchainForFrameinterpolationFunc);
    const FrameInterpolationReplacementParametersVK* parameters = static_cast<const FrameInterpolationReplacementParametersVK*>(replacementParameters);
    return s_pFfxReplaceSwapchainForFrameinterpolationFunc(gameQueue, gameSwapChain, parameters->swapchainCreateInfo, parameters->frameInterpolationInfo);
}

FfxErrorCode ffxRegisterFrameinterpolationUiResource(FfxSwapchain gameSwapChain, FfxResource uiResource, uint32_t flags)
{
    CAULDRON_ASSERT(s_pFfxRegisterFrameinterpolationUiResourceFunc);
    return s_pFfxRegisterFrameinterpolationUiResourceFunc(gameSwapChain, uiResource, flags);
}

FfxErrorCode ffxGetInterpolationCommandlist(FfxSwapchain gameSwapChain, FfxCommandList& gameCommandlist)
{
    CAULDRON_ASSERT(s_pFfxGetInterpolationCommandlistFunc);
    return s_pFfxGetInterpolationCommandlistFunc(gameSwapChain, gameCommandlist);
}

FfxSwapchain ffxGetSwapchain(cauldron::SwapChain* pSwapChain)
{
    CAULDRON_ASSERT(s_pFfxGetSwapchainFunc);
    return s_pFfxGetSwapchainFunc(pSwapChain->GetImpl()->VKSwapChain());
}

FfxCommandQueue ffxGetCommandQueue(cauldron::Device* pDevice)
{
    CAULDRON_ASSERT(s_pFfxGetCommandQueueFunc);
    CAULDRON_ASSERT(false && "Not Implemented!");
    return s_pFfxGetCommandQueueFunc(pDevice->GetImpl()->VKCmdQueue(cauldron::CommandQueue::Graphics));
}

FfxResourceDescription ffxGetResourceDescription(cauldron::GPUResource* pResource)
{
    CAULDRON_ASSERT(false && "Not Implemented!");
    return FfxResourceDescription{};
}

FfxResource ffxGetFrameinterpolationTexture(FfxSwapchain ffxSwapChain)
{
    CAULDRON_ASSERT(s_pFfxGetFrameinterpolationTextureFunc);
    return s_pFfxGetFrameinterpolationTextureFunc(ffxSwapChain);
}

void ffxSetupFrameInterpolationSwapChain()
{
    CAULDRON_ASSERT(s_pFfxGetSwapchainFunc);
    CAULDRON_ASSERT(s_pFfxGetCommandQueueFunc);
    CAULDRON_ASSERT(s_pFfxReplaceSwapchainForFrameinterpolationFunc);
    CAULDRON_ASSERT(s_pFfxGetVKSwapchainFunc);
    CAULDRON_ASSERT(s_pFfxGetSwapchainReplacementFunctionsFunc);

    const cauldron::FIQueue* pAsyncComputeQueue = cauldron::GetDevice()->GetImpl()->GetFIAsyncComputeQueue();
    const cauldron::FIQueue* pPresentQueue      = cauldron::GetDevice()->GetImpl()->GetFIPresentQueue();
    const cauldron::FIQueue* pImageAcquireQueue = cauldron::GetDevice()->GetImpl()->GetFIImageAcquireQueue();
    cauldron::CauldronAssert(cauldron::ASSERT_CRITICAL, pPresentQueue->queue != VK_NULL_HANDLE, L"Cannot create FI Swapchain because there is no present queue.");
    cauldron::CauldronAssert(cauldron::ASSERT_CRITICAL, pImageAcquireQueue->queue != VK_NULL_HANDLE, L"Cannot create FI Swapchain because there is no image acquire queue.");

    // Create frameinterpolation swapchain
    cauldron::SwapChain* pSwapchain   = cauldron::GetFramework()->GetSwapChain();
    FfxSwapchain         ffxSwapChain = s_pFfxGetSwapchainFunc(pSwapchain->GetImpl()->VKSwapChain());
    // make sure swapchain is not holding a ref to real swapchain
    cauldron::GetFramework()->GetSwapChain()->GetImpl()->SetVKSwapChain(VK_NULL_HANDLE);

    VkQueue         pCmdQueue    = cauldron::GetDevice()->GetImpl()->VKCmdQueue(cauldron::CommandQueue::Graphics);
    FfxCommandQueue ffxGameQueue = s_pFfxGetCommandQueueFunc(pCmdQueue);

    VkFrameInterpolationInfoFFX frameInterpolationInfo = {};
    frameInterpolationInfo.device                      = cauldron::GetDevice()->GetImpl()->VKDevice();
    frameInterpolationInfo.physicalDevice              = cauldron::GetDevice()->GetImpl()->VKPhysicalDevice();
    frameInterpolationInfo.pAllocator                  = nullptr;
    frameInterpolationInfo.gameQueue.queue             = cauldron::GetDevice()->GetImpl()->VKCmdQueue(cauldron::CommandQueue::Graphics);
    frameInterpolationInfo.gameQueue.familyIndex       = cauldron::GetDevice()->GetImpl()->VKCmdQueueFamily(cauldron::CommandQueue::Graphics);
    frameInterpolationInfo.gameQueue.submitFunc        = nullptr;  // this queue is only used in vkQueuePresentKHR, hence doesn't need a callback

    frameInterpolationInfo.asyncComputeQueue.queue       = pAsyncComputeQueue->queue;
    frameInterpolationInfo.asyncComputeQueue.familyIndex = pAsyncComputeQueue->family;
    frameInterpolationInfo.asyncComputeQueue.submitFunc  = nullptr;

    frameInterpolationInfo.presentQueue.queue       = pPresentQueue->queue;
    frameInterpolationInfo.presentQueue.familyIndex = pPresentQueue->family;
    frameInterpolationInfo.presentQueue.submitFunc  = nullptr;

    frameInterpolationInfo.imageAcquireQueue.queue       = pImageAcquireQueue->queue;
    frameInterpolationInfo.imageAcquireQueue.familyIndex = pImageAcquireQueue->family;
    frameInterpolationInfo.imageAcquireQueue.submitFunc  = nullptr;

    VkSwapchainCreateInfoKHR createInfo = *cauldron::GetFramework()->GetSwapChain()->GetImpl()->GetCreateInfo();
    FfxErrorCode             errorCode  = s_pFfxReplaceSwapchainForFrameinterpolationFunc(ffxGameQueue, ffxSwapChain, &createInfo, &frameInterpolationInfo);

    FfxSwapchainReplacementFunctions replacementFunctions = {};
    errorCode = s_pFfxGetSwapchainReplacementFunctionsFunc(cauldron::GetDevice()->GetImpl()->VKDevice(), &replacementFunctions);
    cauldron::GetDevice()->GetImpl()->SetSwapchainMethodsAndContext(replacementFunctions.createSwapchainFFX,
                                                                    replacementFunctions.destroySwapchainKHR,
                                                                    replacementFunctions.getSwapchainImagesKHR,
                                                                    replacementFunctions.acquireNextImageKHR,
                                                                    replacementFunctions.queuePresentKHR,
                                                                    replacementFunctions.setHdrMetadataEXT,
                                                                    nullptr,
                                                                    nullptr,
                                                                    replacementFunctions.getLastPresentCountFFX,
                                                                    nullptr,
                                                                    nullptr,
                                                                    &frameInterpolationInfo);

    // Set frameinterpolation swapchain to engine
    VkSwapchainKHR frameinterpolationSwapchain = s_pFfxGetVKSwapchainFunc(ffxSwapChain);
    cauldron::GetFramework()->GetSwapChain()->GetImpl()->SetVKSwapChain(frameinterpolationSwapchain, true);
}

void ffxRestoreApplicationSwapChain()
{
    const VkSwapchainCreateInfoKHR* pCreateInfo = cauldron::GetSwapChain()->GetImpl()->GetCreateInfo();
    VkSwapchainKHR                  swapchain   = cauldron::GetSwapChain()->GetImpl()->VKSwapChain();
    cauldron::GetSwapChain()->GetImpl()->SetVKSwapChain(VK_NULL_HANDLE);
    cauldron::GetDevice()->GetImpl()->DestroySwapchainKHR(swapchain, nullptr);
    cauldron::GetDevice()->GetImpl()->SetSwapchainMethodsAndContext(); // reset all
    swapchain    = VK_NULL_HANDLE;
    VkResult res = cauldron::GetDevice()->GetImpl()->CreateSwapchainKHR(pCreateInfo, nullptr, &swapchain);
    if (res == VK_SUCCESS)
    {
        // Swapchain creation can fail when this function is called when closing the application. In that case, just exit silently
        cauldron::GetSwapChain()->GetImpl()->SetVKSwapChain(swapchain);
    }
}

FfxConstantAllocation ffxAllocateConstantBuffer(void* data, FfxUInt64 dataSize)
{
    FfxConstantAllocation allocation;
    allocation.resource = FfxResource();

    cauldron::BufferAddressInfo  bufferInfo  = cauldron::GetDynamicBufferPool()->AllocConstantBuffer((uint32_t)dataSize, data);
    cauldron::BufferAddressInfo* pBufferInfo = &bufferInfo;

    allocation.resource.resource = bufferInfo.GetImpl()->Buffer;
    //allocation.resource.description = ffxGetResourceDescription(cauldron::GetDynamicBufferPool()->GetResource());  // not needed
    allocation.resource.state    = FFX_RESOURCE_STATE_COMMON;
    wcscpy_s(allocation.resource.name, L"cauldronDynamicBufferPool");

    allocation.handle = FfxUInt64(bufferInfo.GetImpl()->Offset);

    return allocation;
}

} // namespace SDKWrapper
