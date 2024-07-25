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

#pragma once

#include "FrameInterpolationSwapchainVK.h"
#include "FrameInterpolationSwapchainVK_UiComposition.h"

#include <FidelityFX/host/ffx_assert.h>

// enable manually what is needed
// this mode will compose the UI on the graphics queue in the present call on the main thread
#define FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD  0
#define FFX_COMPOSE_IN_PRESENTTHREAD                            1

#define FFX_COMPOSITION_MODE FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD


//////////////////////////////////////////////
/// Helper functions and classes
//////////////////////////////////////////////

#define EXIT_ON_VKRESULT_NOT_SUCCESS(res) if (res != VK_SUCCESS) return res;

#define FFX_ASSERT_MESSAGE_FORMAT(test, ...)            \
    {                                                   \
        char msg[128];                                  \
        snprintf(msg, 128 * sizeof(char), __VA_ARGS__); \
        FFX_ASSERT_MESSAGE(test, msg);                  \
    } \

#define GET_DEVICE_PROC_ADDR(name)       name##Proc = (PFN_##name)vkGetDeviceProcAddr(device, #name);


// Some known states during queue family ownership transfer
struct ImageState
{
    VkAccessFlags accessMask;
    VkImageLayout layout;
};
constexpr ImageState ReplacementBufferTransferState   = {VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
constexpr ImageState InterpolationBufferTransferState = {VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL};


VkImageMemoryBarrier getImageMemoryBarrier(VkImage       image,
                                           VkAccessFlags srcAccessMask,
                                           VkAccessFlags dstAccessMask,
                                           VkImageLayout oldLayout,
                                           VkImageLayout newLayout,
                                           uint32_t      srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                           uint32_t      dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                           uint32_t      levelCount          = VK_REMAINING_MIP_LEVELS,
                                           uint32_t      layerCount          = VK_REMAINING_ARRAY_LAYERS)
{
    VkImageMemoryBarrier barrier            = {};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext                           = nullptr;
    barrier.srcAccessMask                   = srcAccessMask;
    barrier.dstAccessMask                   = dstAccessMask;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex             = dstQueueFamilyIndex;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = levelCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = layerCount;

    return barrier;
}

struct ImageBarrierHelper
{
    static const uint32_t Capacity           = 3;
    VkImageMemoryBarrier  barriers[Capacity] = {};
    uint32_t              count              = 0;

    void add(VkImageMemoryBarrier barrier)
    {
        FFX_ASSERT_MESSAGE(count < Capacity, "ImageBarrierHelper capacity exceeded. Please increase it.");
        barriers[count] = barrier;
        ++count;
    }

    template<typename... T>
    void add(T... args)
    {
        add(getImageMemoryBarrier(args...));
    }

    void record(VkCommandBuffer      commandBuffer,
                VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
    {
        if (count > 0)
        {
            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, count, barriers);
        }
    }
};

inline void flipBarrier(VkImageMemoryBarrier& barrier)
{
    VkAccessFlags dstAccessMask = barrier.dstAccessMask;
    barrier.dstAccessMask       = barrier.srcAccessMask;
    barrier.srcAccessMask       = dstAccessMask;

    VkImageLayout newLayout = barrier.newLayout;
    barrier.newLayout       = barrier.oldLayout;
    barrier.oldLayout       = newLayout;
}

void recordCopy(VkCommandBuffer commandBuffer, VkImage srcImage, VkImage dstImage, uint32_t width, uint32_t height, uint32_t depth = 1)
{
    VkImageCopy imageCopy                   = {};
    imageCopy.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopy.srcSubresource.mipLevel       = 0;
    imageCopy.srcSubresource.baseArrayLayer = 0;
    imageCopy.srcSubresource.layerCount     = 1;
    imageCopy.srcOffset.x                   = 0;
    imageCopy.srcOffset.y                   = 0;
    imageCopy.srcOffset.z                   = 0;
    imageCopy.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopy.dstSubresource.mipLevel       = 0;
    imageCopy.dstSubresource.baseArrayLayer = 0;
    imageCopy.dstSubresource.layerCount     = 1;
    imageCopy.dstOffset.x                   = 0;
    imageCopy.dstOffset.y                   = 0;
    imageCopy.dstOffset.z                   = 0;
    imageCopy.extent.width                  = width;
    imageCopy.extent.height                 = height;
    imageCopy.extent.depth                  = depth;
    vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
}

/// Helper class to set the debug name
struct DebugNameSetter
{
    VkDevice device;
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXTProc;

    DebugNameSetter(VkDevice dev)
    {
        device = dev;
        GET_DEVICE_PROC_ADDR(vkSetDebugUtilsObjectNameEXT);
    }

    VkResult setDebugName(void* pObject, VkObjectType type, const char* name)
    {
        if (pObject != nullptr && vkSetDebugUtilsObjectNameEXTProc != nullptr && name != nullptr)
        {
            VkDebugUtilsObjectNameInfoEXT nameInfo = {};
            nameInfo.sType                         = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.pNext                         = nullptr;
            nameInfo.objectType                    = type;
            nameInfo.objectHandle                  = (uint64_t)pObject;
            nameInfo.pObjectName                   = name;
            return vkSetDebugUtilsObjectNameEXTProc(device, &nameInfo);
        }
        return VK_SUCCESS;
    }

    VkResult setDebugName(void* pObject, VkObjectType type, const char* name, uint32_t i)
    {
        constexpr size_t cBufferSize = 64;
        char             finalName[cBufferSize];
        snprintf(finalName, cBufferSize, name, i);
        return setDebugName(pObject, type, finalName);
    }
};


VkAccessFlags getVKAccessFlagsFromResourceState2(FfxResourceStates state)
{
    switch (state)
    {
    case FFX_RESOURCE_STATE_COMMON:
        return VK_ACCESS_NONE;
    case FFX_RESOURCE_STATE_GENERIC_READ:
        return VK_ACCESS_SHADER_READ_BIT;
    case FFX_RESOURCE_STATE_UNORDERED_ACCESS:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case FFX_RESOURCE_STATE_COMPUTE_READ:
    case FFX_RESOURCE_STATE_PIXEL_READ:
    case FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ:
        return VK_ACCESS_SHADER_READ_BIT;
    case FFX_RESOURCE_STATE_COPY_SRC:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case FFX_RESOURCE_STATE_COPY_DEST:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case FFX_RESOURCE_STATE_INDIRECT_ARGUMENT:
        return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    case FFX_RESOURCE_STATE_PRESENT:
        return VK_ACCESS_NONE;
    case FFX_RESOURCE_STATE_RENDER_TARGET:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    default:
        FFX_ASSERT_MESSAGE(false, "State flag not yet supported");
        return VK_ACCESS_SHADER_READ_BIT;
    }
}

VkImageLayout getVKImageLayoutFromResourceState2(FfxResourceStates state)
{
    switch (state)
    {
    case FFX_RESOURCE_STATE_COMMON:
        return VK_IMAGE_LAYOUT_GENERAL;
    case FFX_RESOURCE_STATE_GENERIC_READ:
        return VK_IMAGE_LAYOUT_GENERAL;
    case FFX_RESOURCE_STATE_UNORDERED_ACCESS:
        return VK_IMAGE_LAYOUT_GENERAL;
    case FFX_RESOURCE_STATE_COMPUTE_READ:
    case FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ:
    case FFX_RESOURCE_STATE_PIXEL_READ:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case FFX_RESOURCE_STATE_COPY_SRC:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case FFX_RESOURCE_STATE_COPY_DEST:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case FFX_RESOURCE_STATE_PRESENT:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case FFX_RESOURCE_STATE_RENDER_TARGET:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case FFX_RESOURCE_STATE_INDIRECT_ARGUMENT:
        // this case is for buffers
    default:
        FFX_ASSERT_MESSAGE(false, "Image layout flag not yet supported");
        return VK_IMAGE_LAYOUT_GENERAL;
    }
}

// Put the wait semaphores from the VkPresentInfo into the SubmissionSemaphores
void addPresentInfoSemaphores(const VkPresentInfoKHR* pPresentInfo, SubmissionSemaphores& toWait)
{
    for (uint32_t i = 0; i < pPresentInfo->waitSemaphoreCount; ++i)
    {
        toWait.add(pPresentInfo->pWaitSemaphores[i]);  // those aren't timeline semaphores
    }
}

uint32_t findMemoryType(const VkPhysicalDeviceMemoryProperties& memProperties, uint32_t memoryTypeBits, VkMemoryPropertyFlags properties)
{
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    return 0u;
};

bool waitForSemaphoreValue(VkDevice device, VkSemaphore semaphore, uint64_t value, uint64_t nanoseconds = UINT64_MAX)
{
    if (semaphore != VK_NULL_HANDLE)
    {
        VkSemaphoreWaitInfo waitInfo = {};
        waitInfo.sType               = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.pNext               = nullptr;
        waitInfo.flags               = 0;
        waitInfo.semaphoreCount      = 1;
        waitInfo.pSemaphores         = &semaphore;
        waitInfo.pValues             = &value;
        VkResult res                 = vkWaitSemaphores(device, &waitInfo, nanoseconds);

        return (res == VK_SUCCESS);
    }

    return false;
}

inline void SafeCloseHandle(HANDLE& handle)
{
    if (handle)
    {
        CloseHandle(handle);
        handle = NULL;
    }
}

FrameInterpolationSwapChainVK* createFrameInterpolationSwapChain(const VkAllocationCallbacks* pAllocator)
{
    FrameInterpolationSwapChainVK* pSwapChainVK = nullptr;
    if (pAllocator != nullptr && pAllocator->pfnAllocation != nullptr)
    {
        void* pData  = pAllocator->pfnAllocation(pAllocator->pUserData,
                                                sizeof(FrameInterpolationSwapChainVK),
                                                std::alignment_of<FrameInterpolationSwapChainVK>::value,
                                                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        pSwapChainVK = new (pData) FrameInterpolationSwapChainVK();
    }
    else
    {
        pSwapChainVK = new FrameInterpolationSwapChainVK();
        if (pAllocator != nullptr && pAllocator->pfnInternalAllocation != nullptr)
        {
            pAllocator->pfnInternalAllocation(
                pAllocator->pUserData, sizeof(FrameInterpolationSwapChainVK), VK_INTERNAL_ALLOCATION_TYPE_EXECUTABLE, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        }
    }
    return pSwapChainVK;
}

void deleteFrameInterpolationSwapChain(FrameInterpolationSwapChainVK* pSwapChainVK, const VkAllocationCallbacks* pAllocator)
{
    if (pSwapChainVK != nullptr)
    {
        if (pAllocator != nullptr && pAllocator->pfnFree != nullptr)
        {
            pSwapChainVK->~FrameInterpolationSwapChainVK();
            pAllocator->pfnFree(pAllocator->pUserData, pSwapChainVK);
        }
        else
        {
            delete pSwapChainVK;
            if (pAllocator != nullptr && pAllocator->pfnInternalFree != nullptr)
            {
                pAllocator->pfnInternalFree(
                    pAllocator->pUserData, sizeof(FrameInterpolationSwapChainVK), VK_INTERNAL_ALLOCATION_TYPE_EXECUTABLE, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
            }
        }
    }
}

inline void destroyImage(VkDevice device, ReplacementResource& resource, const VkAllocationCallbacks* pAllocator)
{
    if (resource.image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, resource.image, pAllocator);
        resource.image = VK_NULL_HANDLE;
    }
    if (resource.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, resource.memory, pAllocator);
        resource.memory = VK_NULL_HANDLE;
    }
}

inline VkResult createImage(VkDevice                                device,
                            ReplacementResource&                    resource,
                            VkImageCreateInfo&                      info,
                            FfxSurfaceFormat                        format,
                            const char*                             name,
                            const VkPhysicalDeviceMemoryProperties& memProperties,
                            const VkAllocationCallbacks*            pAllocator)
{
    VkResult res = vkCreateImage(device, &info, pAllocator, &resource.image);
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);

    if (res == VK_SUCCESS)
    {
        DebugNameSetter debugNameSetter(device);
        debugNameSetter.setDebugName(resource.image, VK_OBJECT_TYPE_IMAGE, name);  // it's fine if this fails

        resource.description.type     = FFX_RESOURCE_TYPE_TEXTURE2D;
        resource.description.format   = format;
        resource.description.width    = info.extent.width;
        resource.description.height   = info.extent.height;
        resource.description.depth    = info.extent.depth;
        resource.description.mipCount = info.mipLevels;
        resource.description.flags    = FFX_RESOURCE_FLAGS_NONE;
        resource.description.usage    = static_cast<FfxResourceUsage>(FFX_RESOURCE_USAGE_RENDERTARGET | FFX_RESOURCE_USAGE_UAV);

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, resource.image, &memRequirements);

        VkMemoryAllocateInfo allocateInfo = {};
        allocateInfo.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.pNext                = nullptr;
        allocateInfo.allocationSize       = memRequirements.size;
        allocateInfo.memoryTypeIndex      = findMemoryType(memProperties, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        res                               = vkAllocateMemory(device, &allocateInfo, pAllocator, &resource.memory);
    }

    if (res == VK_SUCCESS)
        res = vkBindImageMemory(device, resource.image, resource.memory, 0);

    if (res != VK_SUCCESS)
        destroyImage(device, resource, pAllocator);

    return res;
}

inline VkResult createImage(VkDevice                                device,
                            ReplacementResource&                    resource,
                            VkImageCreateInfo&                      info,
                            FfxSurfaceFormat                        format,
                            const char*                             name,
                            uint32_t                                index,
                            const VkPhysicalDeviceMemoryProperties& memProperties,
                            const VkAllocationCallbacks*            pAllocator)
{
    constexpr size_t cBufferSize = 64;
    char             finalName[cBufferSize];
    snprintf(finalName, cBufferSize, name, index);
    return createImage(device, resource, info, format, finalName, memProperties, pAllocator);
}


//////////////////////////////////////////////
/// Vulkan API overridden functions
//////////////////////////////////////////////

VkResult vkAcquireNextImageFFX(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex)
{
    if (swapchain != VK_NULL_HANDLE)
    {
        FrameInterpolationSwapChainVK* pSwapChainVK = reinterpret_cast<FrameInterpolationSwapChainVK*>(swapchain);
        return pSwapChainVK->acquireNextImage(device, swapchain, timeout, semaphore, fence, pImageIndex);
    }
    else
    {
        // vkAcquireNextImageKHR would crash if swapchain is null.
        return VK_ERROR_SURFACE_LOST_KHR;
    }
}

VkResult vkCreateSwapchainFFX(VkDevice                           device,
                              const VkSwapchainCreateInfoKHR*    pCreateInfo,
                              const VkAllocationCallbacks*       pAllocator,
                              VkSwapchainKHR*                    pSwapchain,
                              const VkFrameInterpolationInfoFFX* pFrameInterpolationInfo)
{
    if (pCreateInfo == nullptr || pFrameInterpolationInfo == nullptr)
        return VK_ERROR_INITIALIZATION_FAILED;

    FrameInterpolationSwapChainVK* pSwapChainVK = createFrameInterpolationSwapChain(pAllocator);
    VkResult result = pSwapChainVK->init(pCreateInfo, pFrameInterpolationInfo);
    if (result == VK_SUCCESS)
    {
        *pSwapchain = reinterpret_cast<VkSwapchainKHR>(pSwapChainVK);
    }
    else
    {
        pSwapChainVK->destroySwapchain(device, pAllocator);
        deleteFrameInterpolationSwapChain(pSwapChainVK, pAllocator);
        pSwapChainVK = nullptr;

    }
    return result;
}

void vkDestroySwapchainFFX(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator)
{
    if (swapchain != VK_NULL_HANDLE)
    {
        FrameInterpolationSwapChainVK* pSwapChainVK = reinterpret_cast<FrameInterpolationSwapChainVK*>(swapchain);
        pSwapChainVK->destroySwapchain(device, pAllocator);
        delete pSwapChainVK;
    }
}

VkResult vkGetSwapchainImagesFFX(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages)
{
    if (swapchain != VK_NULL_HANDLE)
    {
        FrameInterpolationSwapChainVK* pSwapChainVK = reinterpret_cast<FrameInterpolationSwapChainVK*>(swapchain);
        return pSwapChainVK->getSwapchainImages(device, pSwapchainImageCount, pSwapchainImages);
    }
    else
    {
        // vkGetSwapchainImagesKHR would crash if swapchain is null.
        // no need to handle the case where pSwapchainImageCount is null as Vulkan itself doesn't handle it
        pSwapchainImageCount = 0;
        return VK_INCOMPLETE;
    }
}

VkResult vkQueuePresentFFX(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    if (pPresentInfo->swapchainCount == 0)
    {
        return VK_SUCCESS;
    }
    // We DO NOT support multiple swapchains for now as there is no way to know which swapchain is a frame interpolation one.
    // We need to assume that the only one that is passed is indeed a frame interpolation one.
    FFX_ASSERT_MESSAGE(pPresentInfo->swapchainCount == 1, "vkQueuePresentFFX doesn't support multiple swapchains");
    FrameInterpolationSwapChainVK* pSwapChainVK = reinterpret_cast<FrameInterpolationSwapChainVK*>(pPresentInfo->pSwapchains[0]);
    if (pSwapChainVK != nullptr)
    {
        VkResult res = pSwapChainVK->queuePresent(queue, pPresentInfo);
        if (pPresentInfo->pResults != nullptr)
        {
            pPresentInfo->pResults[0] = res;
        }
        return res;
    }

    return VK_ERROR_SURFACE_LOST_KHR;
}

// Provided by VK_EXT_hdr_metadata
void vkSetHdrMetadataFFX(VkDevice device, uint32_t swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata)
{
    for (uint32_t i = 0; i < swapchainCount; ++i)
    {
        FrameInterpolationSwapChainVK* pSwapChainVK = reinterpret_cast<FrameInterpolationSwapChainVK*>(pSwapchains[i]);
        pSwapChainVK->setHdrMetadata(device, &pMetadata[i]);
    }
}

//////////////////////////////////////////////
/// FFX additional functions
//////////////////////////////////////////////

uint64_t getLastPresentCountFFX(VkSwapchainKHR swapchain)
{
    FrameInterpolationSwapChainVK* pSwapChainVK = reinterpret_cast<FrameInterpolationSwapChainVK*>(swapchain);
    return pSwapChainVK->getLastPresentCount();
}

//////////////////////////////////////////////
/// FFX API overridden functions
//////////////////////////////////////////////

FFX_API FfxErrorCode ffxGetSwapchainReplacementFunctionsVK(FfxDevice ffxDevice, FfxSwapchainReplacementFunctions* functions)
{
    functions->createSwapchainFFX    = vkCreateSwapchainFFX;
    functions->destroySwapchainKHR   = vkDestroySwapchainFFX;
    functions->getSwapchainImagesKHR = vkGetSwapchainImagesFFX;
    functions->acquireNextImageKHR   = vkAcquireNextImageFFX;
    functions->queuePresentKHR       = vkQueuePresentFFX;

    // for extensions, make sure the base functions exist
    VkDevice device = static_cast<VkDevice>(ffxDevice);
    FFX_ASSERT(device != VK_NULL_HANDLE);

    // VK_EXT_hdr_metadata
    if (vkGetDeviceProcAddr(device, "vkSetHdrMetadataEXT") != nullptr)
        functions->setHdrMetadataEXT = vkSetHdrMetadataFFX;
    else
        functions->setHdrMetadataEXT = nullptr;

    // additional functions only available for frame interpolation swapchain
    functions->getLastPresentCountFFX = getLastPresentCountFFX;

    return FFX_OK;
}

FfxErrorCode ffxRegisterFrameinterpolationUiResourceVK(FfxSwapchain gameSwapChain, FfxResource uiResource, uint32_t flags)
{
    FrameInterpolationSwapChainVK* pSwapChainVK = reinterpret_cast<FrameInterpolationSwapChainVK*>(gameSwapChain);
    pSwapChainVK->registerUiResource(uiResource, flags);
    return FFX_OK;
}

FFX_API FfxErrorCode ffxSetFrameGenerationConfigToSwapchainVK(FfxFrameGenerationConfig const* config)
{
    FfxErrorCode result = FFX_ERROR_INVALID_ARGUMENT;

    if (config->swapChain)
    {
        FrameInterpolationSwapChainVK* frameinterpolationSwapchain = reinterpret_cast<FrameInterpolationSwapChainVK*>(config->swapChain);
        if (frameinterpolationSwapchain != VK_NULL_HANDLE)
        {
            frameinterpolationSwapchain->setFrameGenerationConfig(config);
            result = FFX_OK;
        }
    }

    return result;
}

FfxResource ffxGetFrameinterpolationTextureVK(FfxSwapchain gameSwapChain)
{
    FrameInterpolationSwapChainVK* pSwapChainVK = reinterpret_cast<FrameInterpolationSwapChainVK*>(gameSwapChain);
    FfxResource res = pSwapChainVK->interpolationOutput(0);
    return res;
}

FfxErrorCode ffxGetFrameinterpolationCommandlistVK(FfxSwapchain gameSwapChain, FfxCommandList& gameCommandlist)
{
    FrameInterpolationSwapChainVK* frameinterpolationSwapchain = reinterpret_cast<FrameInterpolationSwapChainVK*>(gameSwapChain);

    gameCommandlist = reinterpret_cast<FfxCommandList>(frameinterpolationSwapchain->getInterpolationCommandList());

    return FFX_OK;
}

FfxErrorCode ffxReplaceSwapchainForFrameinterpolationVK(FfxCommandQueue                    gameQueue,
                                                        FfxSwapchain&                      gameSwapChain,
                                                        const VkSwapchainCreateInfoKHR*    swapchainCreateInfo,
                                                        const VkFrameInterpolationInfoFFX* frameInterpolationInfo)
{
    FfxErrorCode status = FFX_OK;

    VkSwapchainKHR gameSwapchain = reinterpret_cast<VkSwapchainKHR>(gameSwapChain);
    FFX_ASSERT(swapchainCreateInfo != VK_NULL_HANDLE);

    if (frameInterpolationInfo == nullptr)
        return FFX_ERROR_INVALID_ARGUMENT;
    
    FFX_ASSERT(frameInterpolationInfo->device != VK_NULL_HANDLE);
    FFX_ASSERT(frameInterpolationInfo->physicalDevice != VK_NULL_HANDLE);
    FFX_ASSERT(gameQueue != VK_NULL_HANDLE);
    FFX_ASSERT(gameQueue == frameInterpolationInfo->gameQueue.queue);

    const VkAllocationCallbacks* pAllocator = frameInterpolationInfo->pAllocator;
    VkSwapchainCreateInfoKHR     createInfo = *swapchainCreateInfo;  // copy
    
    // createInfo.oldSwapchain should be the same as gameSwapchain if not VK_NULL_HANDLE
    if (createInfo.oldSwapchain != VK_NULL_HANDLE && createInfo.oldSwapchain != gameSwapchain)
        return FFX_ERROR_INVALID_ARGUMENT;

    // use the old swapchain to help with resource reuse
    createInfo.oldSwapchain = gameSwapchain;

    FrameInterpolationSwapChainVK* pSwapChainVK = createFrameInterpolationSwapChain(pAllocator);
    VkResult                       result       = pSwapChainVK->init(&createInfo, frameInterpolationInfo);
    if (result != VK_SUCCESS)
    {
        pSwapChainVK->destroySwapchain(frameInterpolationInfo->device, pAllocator);
        deleteFrameInterpolationSwapChain(pSwapChainVK, pAllocator);
        pSwapChainVK = nullptr;
        status = FFX_ERROR_BACKEND_API_ERROR;
    }

    // as per Vulkan documentation, oldSwapchain is retired - even if creation of the new swapchain fails.
    vkDestroySwapchainKHR(frameInterpolationInfo->device, gameSwapchain, pAllocator);

    gameSwapChain = reinterpret_cast<VkSwapchainKHR>(pSwapChainVK);

    return status;
}

FfxErrorCode ffxWaitForPresents(FfxSwapchain gameSwapChain)
{
    FrameInterpolationSwapChainVK* frameinterpolationSwapchain = reinterpret_cast<FrameInterpolationSwapChainVK*>(gameSwapChain);

    frameinterpolationSwapchain->waitForPresents();

    return FFX_OK;
}

//////////////////////////////////////////////
/// Present
//////////////////////////////////////////////
VkResult presentToSwapChain(FrameinterpolationPresentInfo* pPresenter, uint32_t imageIndex, uint32_t semaphoreIndex = 0)
{
    VkPresentInfoKHR presentInfoKHR   = {};
    presentInfoKHR.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfoKHR.pNext              = nullptr;
    presentInfoKHR.waitSemaphoreCount = 1;
    presentInfoKHR.pWaitSemaphores    = &pPresenter->frameRenderedSemaphores[semaphoreIndex];
    presentInfoKHR.swapchainCount     = 1;
    presentInfoKHR.pSwapchains        = &pPresenter->realSwapchain;
    presentInfoKHR.pImageIndices      = &imageIndex;
    presentInfoKHR.pResults           = nullptr;  // Optional

    EnterCriticalSection(&pPresenter->swapchainCriticalSection);
    VkResult res = vkQueuePresentKHR(pPresenter->presentQueue.queue, &presentInfoKHR);
    LeaveCriticalSection(&pPresenter->swapchainCriticalSection);

    ++(pPresenter->realPresentCount);
    return res;
}

VkResult compositeSwapChainFrame(FrameinterpolationPresentInfo* pPresenter,
                                 const PacingData*              pPacingEntry,
                                 const PacingData::FrameType    frameType,
                                 const uint32_t                 realSwapchainImageIndex,
                                 const VulkanQueue              compositionQueue,
                                 SubmissionSemaphores&          semaphoresToWait,
                                 SubmissionSemaphores&          semaphoresToSignal,
                                 bool&                          uiSurfaceTransfered)
{
    const PacingData::FrameInfo& frameInfo = pPacingEntry->frames[frameType];

    semaphoresToWait.add(pPresenter->interpolationSemaphore, frameInfo.interpolationCompletedSemaphoreValue);
    semaphoresToSignal.add(pPresenter->compositionSemaphore, frameInfo.presentIndex);

    if (pPacingEntry->presentCallback)
    {
        auto compositeCommandList = pPresenter->commandPool.get(pPresenter->device, compositionQueue, "compositeCommandList");
        VkCommandBuffer compositeCommandBuffer = compositeCommandList->reset();

        FfxPresentCallbackDescription desc{};
        desc.commandList           = ffxGetCommandListVK(compositeCommandBuffer);
        desc.device                = pPresenter->device;
        desc.isInterpolatedFrame   = frameType != PacingData::FrameType::Real;
#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_PRESENTTHREAD
        desc.outputSwapChainBuffer = ffxGetResourceVK(pPresenter->realSwapchainImages[realSwapchainImageIndex],
                                                      pPresenter->realSwapchainImageDescription,
                                                      nullptr,
                                                      FFX_RESOURCE_STATE_PRESENT);
#elif FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
        desc.outputSwapChainBuffer = ffxGetResourceVK(pPresenter->compositionOutput.image, pPresenter->compositionOutput.description, nullptr, FFX_RESOURCE_STATE_COPY_SRC);
#endif
        desc.currentBackBuffer     = frameInfo.resource;
        desc.currentUI             = pPacingEntry->uiSurface;
        desc.usePremulAlpha        = pPacingEntry->usePremulAlphaComposite;
        desc.frameID               = pPacingEntry->currentFrameID;

        // queue family ownership transfer for interpolation output & UI surface
        ImageBarrierHelper preCallbackBarriers;
        if (pPresenter->interpolationQueue.familyIndex != compositionQueue.familyIndex)
        {
            if (frameType == PacingData::FrameType::Interpolated_1)
            {
                // this is the interpolation buffer
                preCallbackBarriers.add(static_cast<VkImage>(frameInfo.resource.resource),
                                        InterpolationBufferTransferState.accessMask,
                                        InterpolationBufferTransferState.accessMask,
                                        InterpolationBufferTransferState.layout,
                                        InterpolationBufferTransferState.layout,
                                        pPresenter->interpolationQueue.familyIndex,
                                        compositionQueue.familyIndex);

            }
            else if (frameType == PacingData::FrameType::Real)
            {
                // this is the replacement buffer
                preCallbackBarriers.add(static_cast<VkImage>(frameInfo.resource.resource),
                                        ReplacementBufferTransferState.accessMask,
                                        ReplacementBufferTransferState.accessMask,
                                        ReplacementBufferTransferState.layout,
                                        ReplacementBufferTransferState.layout,
                                        pPresenter->interpolationQueue.familyIndex,
                                        compositionQueue.familyIndex);
            }
        }
        if (!uiSurfaceTransfered)
        {
            preCallbackBarriers.add(pPresenter->queueFamilyOwnershipTransferGameToPresent(pPacingEntry->uiSurface));
            uiSurfaceTransfered = true;
        }


#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_PRESENTTHREAD
        // change real image to present layout
        preCallbackBarriers.add(pPresenter->realSwapchainImages[realSwapchainImageIndex], 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
#elif FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
        // prepare for copy
        preCallbackBarriers.add(
            pPresenter->compositionOutput.image, 0, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
#endif

        preCallbackBarriers.record(compositeCommandBuffer);

        pPacingEntry->presentCallback(&desc, pPacingEntry->presentCallbackContext);

#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
        ImageBarrierHelper preCopyBarriers;

        // composition output buffer is already in a VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL layout on the compose queue
        // just set resource to copy dest layout
        preCopyBarriers.add(static_cast<VkImage>(frameInfo.resource.resource),
                            0,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        preCopyBarriers.record(compositeCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        recordCopy(compositeCommandBuffer,
                   pPresenter->compositionOutput.image,
                   static_cast<VkImage>(frameInfo.resource.resource),
                   pPresenter->realSwapchainImageDescription.width,
                   pPresenter->realSwapchainImageDescription.height);

        ImageBarrierHelper postCopyBarriers;

        postCopyBarriers.add(static_cast<VkImage>(frameInfo.resource.resource),
                             VK_ACCESS_TRANSFER_WRITE_BIT,
                             VK_ACCESS_TRANSFER_READ_BIT,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             // this will handle the queue family ownership transfer if any
                             compositionQueue.familyIndex,
                             pPresenter->presentQueue.familyIndex);

        postCopyBarriers.record(compositeCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
#endif

        return compositeCommandList->execute(semaphoresToWait, semaphoresToSignal);
    }
    else
    {
        return pPresenter->presentQueue.submit(VK_NULL_HANDLE, semaphoresToWait, semaphoresToSignal);
    }
}

DWORD WINAPI copyAndPresent_presenterThread(LPVOID pParam)
{
    FrameinterpolationPresentInfo* presenter = static_cast<FrameinterpolationPresentInfo*>(pParam);

    if (presenter)
    {
        uint64_t numFramesSentForPresentation = 0;
        int64_t  previousPresentQpc           = 0;

        while (!presenter->shutdown)
        {
            WaitForSingleObject(presenter->pacerEvent, INFINITE);

            if (!presenter->shutdown)
            {
                EnterCriticalSection(&presenter->scheduledFrameCriticalSection);

                PacingData entry = presenter->scheduledPresents;
                presenter->scheduledPresents.invalidate();

                LeaveCriticalSection(&presenter->scheduledFrameCriticalSection);

                if (entry.numFramesToPresent > 0)
                {
                    // we might have dropped entries so have to update here, otherwise we might deadlock
                    // we need to track the latest signaled value to avoid validation warnings
                    if (presenter->lastPresentSemaphoreValue != entry.numFramesSentForPresentationBase)
                    {
                        presenter->presentQueue.submit(VK_NULL_HANDLE, presenter->presentSemaphore, entry.numFramesSentForPresentationBase);
                        presenter->lastPresentSemaphoreValue = entry.numFramesSentForPresentationBase;
                    }

                    for (uint32_t frameType = 0; frameType < PacingData::FrameType::Count; frameType++)
                    {
                        const PacingData::FrameInfo& frameInfo = entry.frames[frameType];
                        if (frameInfo.doPresent)
                        {
                            uint32_t    imageIndex              = 0;
                            VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
                            VkResult res = presenter->acquireNextRealImage(imageIndex, imageAvailableSemaphore);
                            FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR, "[copyAndPresent_presenterThread] failed to acquire swapchain image");

                            auto            presentCommandList   = presenter->commandPool.get(presenter->device, presenter->presentQueue, "presentCommandList");
                            VkCommandBuffer presentCommandBuffer = presentCommandList->reset();

                            ImageBarrierHelper preCopyBarriers;

                            // newly acquired image transition
                            preCopyBarriers.add(presenter->realSwapchainImages[imageIndex],
                                                0,
                                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                                VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                            
                            // queue family ownership transfer for the texture containing the final image
                            if (presenter->gameQueue.familyIndex != presenter->presentQueue.familyIndex)
                            {
                                preCopyBarriers.add(static_cast<VkImage>(frameInfo.resource.resource),
                                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                                    VK_ACCESS_TRANSFER_READ_BIT,
                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                    presenter->gameQueue.familyIndex,
                                                    presenter->presentQueue.familyIndex);
                            }

                            preCopyBarriers.record(presentCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                            // copy from resource to the swapchain image
                            recordCopy(presentCommandBuffer,
                                       static_cast<VkImage>(frameInfo.resource.resource),
                                       presenter->realSwapchainImages[imageIndex],
                                       presenter->realSwapchainImageDescription.width,
                                       presenter->realSwapchainImageDescription.height);

                            ImageBarrierHelper postCopyBarriers;
                            // transition to present
                            postCopyBarriers.add(presenter->realSwapchainImages[imageIndex],
                                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                                0,
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

                            postCopyBarriers.record(presentCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

                            SubmissionSemaphores toWait;
                            toWait.add(presenter->compositionSemaphore, frameInfo.presentIndex);  // composition to finish
                            toWait.add(imageAvailableSemaphore);  // swapchain image to be available

                            SubmissionSemaphores toSignal;
                            toSignal.add(presenter->frameRenderedSemaphores[imageIndex]);  // frame ready for present. Not a timeline semaphore

                            // signal replacement buffer availability
                            // this is the last present of this entry
                            if (frameInfo.presentIndex == entry.replacementBufferSemaphoreSignal)
                            {
                                toSignal.add(presenter->replacementBufferSemaphore, entry.replacementBufferSemaphoreSignal);
                            }

                            // There is no way to signal a semaphore after Present, so signal it before it.
                            if (frameInfo.presentIndex != entry.numFramesSentForPresentationBase)
                            {
                                // no need to signal twice
                                toSignal.add(presenter->presentSemaphore, frameInfo.presentIndex);
                                presenter->lastPresentSemaphoreValue = frameInfo.presentIndex;
                            }

                            res = presentCommandList->execute(toWait, toSignal);

                            waitForPerformanceCount(previousPresentQpc + frameInfo.presentQpcDelta);
                            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&previousPresentQpc));

                            res = presentToSwapChain(presenter, imageIndex, imageIndex);
                            // VK_SUBOPTIMAL_KHR & VK_ERROR_OUT_OF_DATE_KHR: the swapchain has been recreated
                            FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR,
                                                      "presentToSwapChain failed with error %d",
                                                      res);
                        }
                    }

                    numFramesSentForPresentation = entry.numFramesSentForPresentationBase + entry.numFramesToPresent;
                }
            }
        }

        waitForSemaphoreValue(presenter->device, presenter->presentSemaphore, numFramesSentForPresentation);
    }

    return 0;
}


DWORD WINAPI composeAndPresent_presenterThread(LPVOID pParam)
{
    FrameinterpolationPresentInfo* presenter = static_cast<FrameinterpolationPresentInfo*>(pParam);

    if (presenter)
    {
        uint64_t numFramesSentForPresentation = 0;
        int64_t  previousPresentQpc           = 0;

        while (!presenter->shutdown)
        {
            WaitForSingleObject(presenter->pacerEvent, INFINITE);

            if (!presenter->shutdown)
            {
                EnterCriticalSection(&presenter->scheduledFrameCriticalSection);

                PacingData entry = presenter->scheduledPresents;
                presenter->scheduledPresents.invalidate();

                LeaveCriticalSection(&presenter->scheduledFrameCriticalSection);

                if (entry.numFramesToPresent > 0)
                {
                    // we might have dropped entries so have to update here, otherwise we might deadlock
                    // we need to track the latest signaled value to avoid validation warnings
                    if (presenter->lastPresentSemaphoreValue != entry.numFramesSentForPresentationBase)
                    {
                        presenter->presentQueue.submit(VK_NULL_HANDLE, presenter->presentSemaphore, entry.numFramesSentForPresentationBase);
                        presenter->lastPresentSemaphoreValue = entry.numFramesSentForPresentationBase;
                    }

                    SubmissionSemaphores toWait;
                    toWait.add(presenter->gameSemaphore, entry.gameSemaphoreValue);

                    bool uiSurfaceTransfered = (entry.uiSurface.resource == nullptr) || (presenter->gameQueue.familyIndex == presenter->presentQueue.familyIndex);

                    for (uint32_t frameType = 0; frameType < PacingData::FrameType::Count; frameType++)
                    {
                        const PacingData::FrameInfo& frameInfo = entry.frames[frameType];
                        if (frameInfo.doPresent)
                        {
                            SubmissionSemaphores toSignal;
                            toSignal.add(presenter->frameRenderedSemaphores[0]);  // not a timeline semaphore

                            // signal replacement buffer availability
                            // this is the last present of this entry
                            if (frameInfo.presentIndex == entry.replacementBufferSemaphoreSignal)
                            {
                                toSignal.add(presenter->replacementBufferSemaphore, entry.replacementBufferSemaphoreSignal);
                            }

                            // this should be signaled after present, but this cannot be done in Vulkan. Signaling here
                            if (frameInfo.presentIndex != entry.numFramesSentForPresentationBase)
                            {
                                // no need to signal twice
                                toSignal.add(presenter->presentSemaphore, frameInfo.presentIndex);
                                presenter->lastPresentSemaphoreValue = frameInfo.presentIndex;
                            }

                            // get the swapchain image
                            uint32_t    realSwapchainImageIndex = 0;
                            VkSemaphore acquireSemaphore        = VK_NULL_HANDLE;
                            VkResult    res                     = presenter->acquireNextRealImage(realSwapchainImageIndex, acquireSemaphore);
                            toWait.add(acquireSemaphore);

                            res = compositeSwapChainFrame(presenter, &entry, (PacingData::FrameType)frameType, realSwapchainImageIndex, presenter->presentQueue, toWait, toSignal, uiSurfaceTransfered);
                            FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "compositeSwapChainFrame failed with error %d", res);

                            waitForPerformanceCount(previousPresentQpc + frameInfo.presentQpcDelta);
                            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&previousPresentQpc));
                            
                            res = presentToSwapChain(presenter, realSwapchainImageIndex);
                            // VK_SUBOPTIMAL_KHR & VK_ERROR_OUT_OF_DATE_KHR: the swapchain has been recreated
                            FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR,
                                                      "presentToSwapChain failed with error %d",
                                                      res);
                        }
                    }

                    // if no frame was presented, we still need to update the semaphore
                    if (toWait.count > 0)
                    {
                        presenter->presentQueue.submit(VK_NULL_HANDLE, toWait, SubmissionSemaphores());
                    }

                    numFramesSentForPresentation = entry.numFramesSentForPresentationBase + entry.numFramesToPresent;
                }
            }
        }

        waitForSemaphoreValue(presenter->device, presenter->presentSemaphore, numFramesSentForPresentation);
    }

    return 0;
}

DWORD WINAPI interpolationThread(LPVOID param)
{
    FrameinterpolationPresentInfo* presenter = static_cast<FrameinterpolationPresentInfo*>(param);

    if (presenter)
    {
#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
        HANDLE presenterThreadHandle = CreateThread(nullptr, 0, copyAndPresent_presenterThread, param, 0, nullptr);
#elif FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_PRESENTTHREAD
        HANDLE presenterThreadHandle = CreateThread(nullptr, 0, composeAndPresent_presenterThread, param, 0, nullptr);
#else
        HANDLE presenterThreadHandle = NULL;
#endif
        FFX_ASSERT(presenterThreadHandle != NULL);

        if (presenterThreadHandle != 0)
        {
            SetThreadPriority(presenterThreadHandle, THREAD_PRIORITY_HIGHEST);
            SetThreadDescription(presenterThreadHandle, L"AMD FSR Presenter Thread");

            SimpleMovingAverage<10, double> frameTime{};
            int64_t previousQpc = 0;

            while (!presenter->shutdown)
            {
                WaitForSingleObject(presenter->presentEvent, INFINITE);

                if (!presenter->shutdown)
                {
                    EnterCriticalSection(&presenter->scheduledFrameCriticalSection);

                    PacingData entry = presenter->scheduledInterpolations;
                    presenter->scheduledInterpolations.invalidate();

                    LeaveCriticalSection(&presenter->scheduledFrameCriticalSection);

                    waitForSemaphoreValue(presenter->device,
                                          presenter->interpolationSemaphore,
                                          entry.frames[PacingData::FrameType::Interpolated_1].interpolationCompletedSemaphoreValue);
                    SetEvent(presenter->interpolationEvent); // unlocks the queuePresent method

                    int64_t currentQpc = 0;
                    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentQpc));

                    const double deltaQpc = double(currentQpc - previousQpc) * (previousQpc > 0);
                    previousQpc           = currentQpc;

                    // reset pacing averaging if delta > 10 fps,
                    int64_t qpcFrequency;
                    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&qpcFrequency));
                    const float fTimeoutInSeconds       = 0.1f;
                    double      deltaQpcResetThreashold = double(qpcFrequency * fTimeoutInSeconds);
                    if ((deltaQpc > deltaQpcResetThreashold) || presenter->resetTimer)
                    {
                        frameTime.reset();
                    }
                    else
                    {
                        frameTime.update(deltaQpc);
                    }

                    // set presentation time for the real frame
                    double  safetyMarginInSec       = 0.0001; // = 0.1ms
                    int64_t       qpcSafetyMargin = int64_t(qpcFrequency * safetyMarginInSec);
                    const int64_t conservativeAvg = int64_t(frameTime.getAverage() * 0.5 - frameTime.getVariance() * 0.1);
                    const int64_t deltaToUse      = conservativeAvg > qpcSafetyMargin ? (conservativeAvg - qpcSafetyMargin) : 0;
                    entry.frames[PacingData::FrameType::Interpolated_1].presentQpcDelta = deltaToUse;
                    entry.frames[PacingData::FrameType::Real].presentQpcDelta           = deltaToUse;

                    // schedule presents
                    EnterCriticalSection(&presenter->scheduledFrameCriticalSection);
                    presenter->scheduledPresents = entry;
                    LeaveCriticalSection(&presenter->scheduledFrameCriticalSection);
                    SetEvent(presenter->pacerEvent);
                }
            }

            // signal event to allow thread to finish
            SetEvent(presenter->pacerEvent);
            WaitForSingleObject(presenterThreadHandle, INFINITE);
            SafeCloseHandle(presenterThreadHandle);
        }
    }

    return 0;
}

//////////////////////////////////////////////
/// FrameInterpolationSwapChainVK
//////////////////////////////////////////////

FrameInterpolationSwapChainVK::FrameInterpolationSwapChainVK()
{
    for (uint32_t i = 0; i < FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_BUFFER_COUNT; ++i)
    {
        presentInfo.realSwapchainImages[i]     = VK_NULL_HANDLE;
        presentInfo.frameRenderedSemaphores[i] = VK_NULL_HANDLE;
    }
    for (uint32_t i = 0; i < FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_ACQUIRE_SEMAPHORE_COUNT; ++i)
    {
        presentInfo.acquireSemaphores[i] = VK_NULL_HANDLE;
    }
}

FrameInterpolationSwapChainVK::~FrameInterpolationSwapChainVK()
{
}

VkResult FrameInterpolationSwapChainVK::acquireNextImage(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex)
{
    // this should always be the case
    if (semaphore == VK_NULL_HANDLE && fence == VK_NULL_HANDLE)
        return VK_INCOMPLETE;
    if (pImageIndex == nullptr)
        return VK_INCOMPLETE;

    EnterCriticalSection(&criticalSection);

    *pImageIndex = (uint32_t)(acquiredCount % gameBufferCount);

    if (replacementSwapBuffers[*pImageIndex].image == VK_NULL_HANDLE)
    {
        LeaveCriticalSection(&criticalSection);
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // limit the acquired count
    if (acquiredCount > presentCount && (acquiredCount - presentCount) >= gameBufferCount)
    {
        LeaveCriticalSection(&criticalSection);
        return VK_NOT_READY;
    }

    SubmissionSemaphores toWait;
    SubmissionSemaphores toSignal;

    toWait.add(presentInfo.replacementBufferSemaphore, replacementSwapBuffers[*pImageIndex].availabilitySemaphoreValue);

    if (semaphore != VK_NULL_HANDLE)
        toSignal.add(semaphore);  // not a timeline semaphore

    VkResult res = imageAcquireQueue.submit(VK_NULL_HANDLE, toWait, toSignal, fence);

    ++acquiredCount;

    LeaveCriticalSection(&criticalSection);

    return res;
}

// structure holding all the structures that can extend VkSwapchainCreateInfoKHR
struct SwapchainCreationInfo
{
    VkSwapchainCreateInfoKHR                 swapchain;

    VkImageCompressionControlEXT             imageCompressionControl;
    VkImageFormatListCreateInfo              imageFormatList;
    VkSurfaceFullScreenExclusiveInfoEXT      surfaceFullScreenExclusive;
    VkSurfaceFullScreenExclusiveWin32InfoEXT surfaceFullScreenExclusiveWin32;
    VkSwapchainCounterCreateInfoEXT          swapchainCounter;
    VkSwapchainDisplayNativeHdrCreateInfoAMD swapchainDisplayNativeHdr;
    VkSwapchainPresentModesCreateInfoEXT     swapchainPresentModes;
};

#define FFX_USE_PNEXT_AS_IS(name, type) \
    realSwapchainCreateInfo.name            = *reinterpret_cast<const type*>(pCurrent); \
    realSwapchainCreateInfo.name.pNext      = realSwapchainCreateInfo.swapchain.pNext;  \
    realSwapchainCreateInfo.swapchain.pNext = &realSwapchainCreateInfo.name;\

VkResult getRealSwapchainCreateInfo(const VkSwapchainCreateInfoKHR* pCreateInfo, SwapchainCreationInfo& realSwapchainCreateInfo)
{
    // fix VkSwapchainCreateInfoKHR
    realSwapchainCreateInfo.swapchain = *pCreateInfo;
    realSwapchainCreateInfo.swapchain.pNext = nullptr;

    realSwapchainCreateInfo.swapchain.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    realSwapchainCreateInfo.swapchain.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    realSwapchainCreateInfo.swapchain.queueFamilyIndexCount = 0;
    realSwapchainCreateInfo.swapchain.pQueueFamilyIndices   = nullptr;

    const VkBaseInStructure* pCurrent = reinterpret_cast<const VkBaseInStructure*>(pCreateInfo);
    while (pCurrent->pNext != nullptr)
    {
        pCurrent = pCurrent->pNext;
        switch (pCurrent->sType)
        {
        case VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT:
            FFX_USE_PNEXT_AS_IS(imageCompressionControl, VkImageCompressionControlEXT);
            break;
        case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO:
            FFX_USE_PNEXT_AS_IS(imageFormatList, VkImageFormatListCreateInfo);
            break;
        case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT:
            realSwapchainCreateInfo.surfaceFullScreenExclusive       = *reinterpret_cast<const VkSurfaceFullScreenExclusiveInfoEXT*>(pCurrent);
            realSwapchainCreateInfo.surfaceFullScreenExclusive.pNext = const_cast<void*>(realSwapchainCreateInfo.swapchain.pNext);  // because pNext is void* instead of const void* in vulkan header
            realSwapchainCreateInfo.swapchain.pNext                  = &realSwapchainCreateInfo.surfaceFullScreenExclusive;
            break;
        case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT:
            FFX_USE_PNEXT_AS_IS(surfaceFullScreenExclusiveWin32, VkSurfaceFullScreenExclusiveWin32InfoEXT);
            break;
        case VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT:
            FFX_USE_PNEXT_AS_IS(swapchainCounter, VkSwapchainCounterCreateInfoEXT);
            break;
        case VK_STRUCTURE_TYPE_SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD:
            FFX_USE_PNEXT_AS_IS(swapchainDisplayNativeHdr, VkSwapchainDisplayNativeHdrCreateInfoAMD);
            break;
        case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT:
            FFX_USE_PNEXT_AS_IS(swapchainPresentModes, VkSwapchainPresentModesCreateInfoEXT);
            break;
        default:
            FFX_ASSERT_MESSAGE_FORMAT(false, "Swapchain creation structure extension %d not supported", pCurrent->sType);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    return VK_SUCCESS;
}


VkResult FrameInterpolationSwapChainVK::init(const VkSwapchainCreateInfoKHR* pCreateInfo, const VkFrameInterpolationInfoFFX* pFrameInterpolationInfo)
{
    if (pFrameInterpolationInfo == nullptr || pCreateInfo == nullptr)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (pFrameInterpolationInfo->device == VK_NULL_HANDLE || pFrameInterpolationInfo->physicalDevice == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkDevice device                         = pFrameInterpolationInfo->device;
    presentInfo.device                      = device;
    physicalDevice                          = pFrameInterpolationInfo->physicalDevice;
    const VkAllocationCallbacks* pAllocator = pFrameInterpolationInfo->pAllocator;

    DebugNameSetter debugNameSetter(device);
    
    // create semaphores
    VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = {};
    semaphoreTypeCreateInfo.sType                     = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    semaphoreTypeCreateInfo.pNext                     = nullptr;
    semaphoreTypeCreateInfo.semaphoreType             = VK_SEMAPHORE_TYPE_TIMELINE;
    semaphoreTypeCreateInfo.initialValue              = 0;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext                 = &semaphoreTypeCreateInfo;
    semaphoreCreateInfo.flags                 = 0;

    auto createSemaphore = [device, &semaphoreCreateInfo, &debugNameSetter](
                               VkSemaphore& semaphore, const char* name, const VkAllocationCallbacks* pAllocator = nullptr) {
        VkResult res = vkCreateSemaphore(device, &semaphoreCreateInfo, pAllocator, &semaphore);

        if (res == VK_SUCCESS)
            debugNameSetter.setDebugName(semaphore, VK_OBJECT_TYPE_SEMAPHORE, name);  // it's fine if this fails

        return res;
    };

    auto createSemaphoreFormat = [device, &semaphoreCreateInfo, &debugNameSetter](
                                     VkSemaphore& semaphore, const char* name, uint32_t i, const VkAllocationCallbacks* pAllocator = nullptr) {
        VkResult res = vkCreateSemaphore(device, &semaphoreCreateInfo, pAllocator, &semaphore);

        if (res == VK_SUCCESS)
            debugNameSetter.setDebugName(semaphore, VK_OBJECT_TYPE_SEMAPHORE, name, i);  // it's fine if this fails

        return res;
    };

    VkResult res = VK_SUCCESS;
    res = createSemaphore(presentInfo.gameSemaphore, "AMD FSR GameSemaphore");
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);
    res = createSemaphore(presentInfo.interpolationSemaphore, "AMD FSR InterpolationSemaphore");
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);
    res = createSemaphore(presentInfo.presentSemaphore, "AMD FSR PresentSemaphore");
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);
    res = createSemaphore(presentInfo.replacementBufferSemaphore, "AMD FSR ReplacementBufferSemaphore");
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);
    res = createSemaphore(presentInfo.compositionSemaphore, "AMD FSR CompositionSemaphore");
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);


    // create non-timeline semaphores
    semaphoreCreateInfo.pNext = nullptr;

    // semaphores used for present to wait until rendering is over
    for (uint32_t i = 0; i < FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_BUFFER_COUNT; ++i)
    {
        res = createSemaphoreFormat(presentInfo.frameRenderedSemaphores[i], "AMD FSR FrameRenderedSemaphore %d", i);
        EXIT_ON_VKRESULT_NOT_SUCCESS(res);
    }
    for (uint32_t i = 0; i < FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_ACQUIRE_SEMAPHORE_COUNT; ++i)
    {
        res = createSemaphoreFormat(presentInfo.acquireSemaphores[i], "AMD FSR AcquireSemaphore %d", i);
        EXIT_ON_VKRESULT_NOT_SUCCESS(res);
    }

    // queues
    presentInfo.gameQueue          = pFrameInterpolationInfo->gameQueue;
    presentInfo.interpolationQueue = presentInfo.gameQueue;
    presentInfo.asyncComputeQueue  = pFrameInterpolationInfo->asyncComputeQueue;
    presentInfo.presentQueue       = pFrameInterpolationInfo->presentQueue;
    imageAcquireQueue              = pFrameInterpolationInfo->imageAcquireQueue;

    debugNameSetter.setDebugName(presentInfo.presentQueue.queue, VK_OBJECT_TYPE_QUEUE, "AMD FSR PresentQueue");
    debugNameSetter.setDebugName(presentInfo.asyncComputeQueue.queue, VK_OBJECT_TYPE_QUEUE, "AMD FSR AsyncComputeQueue");
    debugNameSetter.setDebugName(imageAcquireQueue.queue, VK_OBJECT_TYPE_QUEUE, "AMD FSR ImageAcquireQueue");

    InitializeCriticalSection(&criticalSection);
    InitializeCriticalSection(&criticalSectionUpdateConfig);
    InitializeCriticalSection(&presentInfo.scheduledFrameCriticalSection);
    InitializeCriticalSection(&presentInfo.swapchainCriticalSection);

    presentInfo.presentEvent       = CreateEvent(NULL, FALSE, FALSE, TEXT("PresentEvent"));
    presentInfo.interpolationEvent = CreateEvent(NULL, FALSE, TRUE, TEXT("InterpolationEvent"));
    presentInfo.pacerEvent         = CreateEvent(NULL, FALSE, FALSE, TEXT("PacerEvent"));

    // create the real swapchain
    SwapchainCreationInfo realSwapchainCreateInfo;
    res = getRealSwapchainCreateInfo(pCreateInfo, realSwapchainCreateInfo);
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);

    res = vkCreateSwapchainKHR(device, &realSwapchainCreateInfo.swapchain, pAllocator, &presentInfo.realSwapchain);
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);

    presentMode      = realSwapchainCreateInfo.swapchain.presentMode;
    tearingSupported = (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR || presentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR);

    switch (realSwapchainCreateInfo.swapchain.imageColorSpace)
    {
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
        break;
    case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD:
        if (realSwapchainCreateInfo.swapchain.imageFormat == VK_FORMAT_R16G16B16A16_SFLOAT)
            backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_SCRGB;
        else
            backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ;
        break;
    case VK_COLOR_SPACE_HDR10_ST2084_EXT:
        backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ;
        break;
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_SCRGB;
        break;
    default:
        break;
    }

    // get all the images of the swapchain
    presentInfo.realSwapchainImageCount = 0;
    res = vkGetSwapchainImagesKHR(device, presentInfo.realSwapchain, &presentInfo.realSwapchainImageCount, nullptr);
    if (res != VK_SUCCESS || presentInfo.realSwapchainImageCount > FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_BUFFER_COUNT)
        return VK_ERROR_INITIALIZATION_FAILED;
    res = vkGetSwapchainImagesKHR(device, presentInfo.realSwapchain, &presentInfo.realSwapchainImageCount, presentInfo.realSwapchainImages);
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);

    for (uint32_t i = 0; i < presentInfo.realSwapchainImageCount; ++i)
    {
        debugNameSetter.setDebugName(presentInfo.realSwapchainImages[i], VK_OBJECT_TYPE_IMAGE, "AMD FSR Real swapchain image %d", i);  // it's fine if this fails
    }
 
    presentInfo.realSwapchainImageDescription.type     = FFX_RESOURCE_TYPE_TEXTURE2D;
    presentInfo.realSwapchainImageDescription.format   = ffxGetSurfaceFormatVK(pCreateInfo->imageFormat);
    presentInfo.realSwapchainImageDescription.width    = pCreateInfo->imageExtent.width;
    presentInfo.realSwapchainImageDescription.height   = pCreateInfo->imageExtent.height;
    presentInfo.realSwapchainImageDescription.depth    = 1;
    presentInfo.realSwapchainImageDescription.mipCount = 1;
    presentInfo.realSwapchainImageDescription.flags    = FFX_RESOURCE_FLAGS_NONE;
    presentInfo.realSwapchainImageDescription.usage    = static_cast<FfxResourceUsage>(FFX_RESOURCE_USAGE_RENDERTARGET | FFX_RESOURCE_USAGE_UAV);

    // create the semaphore for each image to wait on
    for (uint32_t i = 0; i < presentInfo.realSwapchainImageCount; ++i)
    {
        res = createSemaphoreFormat(presentInfo.frameRenderedSemaphores[i], "AMD swapchain image semaphore %d", i);
        EXIT_ON_VKRESULT_NOT_SUCCESS(res);
    }

    // create the fake backbuffer images
    FFX_ASSERT_MESSAGE(pCreateInfo->minImageCount <= FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_BUFFER_COUNT, "Too many backbuffer requested");
    gameBufferCount = pCreateInfo->minImageCount;

    VkImageCreateInfo info = {};
    info.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext             = nullptr;
    info.flags             = 0;
    info.imageType         = VK_IMAGE_TYPE_2D;
    info.format            = pCreateInfo->imageFormat;
    info.extent.width      = pCreateInfo->imageExtent.width;
    info.extent.height     = pCreateInfo->imageExtent.height;
    info.extent.depth      = 1;
    info.mipLevels         = 1;
    info.arrayLayers       = 1;
    info.samples           = VK_SAMPLE_COUNT_1_BIT;
    info.tiling            = VK_IMAGE_TILING_OPTIMAL;
    info.usage             = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    info.sharingMode       = VK_SHARING_MODE_EXCLUSIVE; // queue family ownership transfer will be handled manually

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < gameBufferCount; ++i)
    {
        res = createImage(device,
                          replacementSwapBuffers[i],
                          info,
                          presentInfo.realSwapchainImageDescription.format,
                          "AMD FSR Replacement BackBuffer %d",
                          i,
                          memProperties,
                          pAllocator);
        EXIT_ON_VKRESULT_NOT_SUCCESS(res);
    }
    // No need to set the layout of the images as vulkan API states that a presentable image starts out at an undefined layout.

    for (uint32_t i = 0; i < _countof(interpolationOutputs); ++i)
    {
        res = createImage(device,
                          interpolationOutputs[i],
                          info,
                          presentInfo.realSwapchainImageDescription.format,
                          "AMD FSR Interpolation Output %d",
                          i,
                          memProperties,
                          pAllocator);
        EXIT_ON_VKRESULT_NOT_SUCCESS(res);
    }

    res = createImage(device,
                      presentInfo.compositionOutput,
                      info,
                      presentInfo.realSwapchainImageDescription.format,
                      "AMD FSR Composition Output",
                      memProperties,
                      pAllocator);
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);
    

    GET_DEVICE_PROC_ADDR(vkSetHdrMetadataEXT);

    return res;
}

void FrameInterpolationSwapChainVK::destroySwapchain(VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    // Wait for all operations to be finished
    waitForPresents();
    killPresenterThread();

    FFX_ASSERT_MESSAGE(device == presentInfo.device, "Attempting to destroy Frame interpolation swapchain with another device");

    if (device == VK_NULL_HANDLE)
        return;

    auto destroySemaphore = [device, pAllocator](VkSemaphore& semaphore) {
        vkDestroySemaphore(device, semaphore, pAllocator);
        semaphore = VK_NULL_HANDLE;
    };

    // destroy semaphores
    destroySemaphore(presentInfo.gameSemaphore);
    destroySemaphore(presentInfo.interpolationSemaphore);
    destroySemaphore(presentInfo.presentSemaphore);
    destroySemaphore(presentInfo.replacementBufferSemaphore);
    destroySemaphore(presentInfo.compositionSemaphore);

    for (uint32_t i = 0; i < FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_ACQUIRE_SEMAPHORE_COUNT; ++i)
    {
        destroySemaphore(presentInfo.acquireSemaphores[i]);
    }

    // reset queue information
    presentInfo.gameQueue.reset();
    presentInfo.interpolationQueue.reset();
    presentInfo.asyncComputeQueue.reset();
    presentInfo.presentQueue.reset();

    // delete win32 objects
    DeleteCriticalSection(&criticalSection);
    DeleteCriticalSection(&criticalSectionUpdateConfig);
    DeleteCriticalSection(&presentInfo.scheduledFrameCriticalSection);
    DeleteCriticalSection(&presentInfo.swapchainCriticalSection);

    SafeCloseHandle(presentInfo.presentEvent);
    SafeCloseHandle(presentInfo.interpolationEvent);
    SafeCloseHandle(presentInfo.pacerEvent);

    // delete real swapchain
    vkDestroySwapchainKHR(device, presentInfo.realSwapchain, pAllocator);
    presentInfo.realSwapchain = VK_NULL_HANDLE;

    // destroy semaphores of each image
    for (uint32_t i = 0; i < FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_BUFFER_COUNT; ++i)
    {
        presentInfo.realSwapchainImages[i] = VK_NULL_HANDLE;
        destroySemaphore(presentInfo.frameRenderedSemaphores[i]);
    }

    // destroy replacement buffers & interpolation outputs
    for (uint32_t i = 0; i < FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_BUFFER_COUNT; ++i)
        destroyImage(device, replacementSwapBuffers[i], pAllocator);
    
    for (uint32_t i = 0; i < _countof(interpolationOutputs); ++i)
        destroyImage(device, interpolationOutputs[i], pAllocator);

    destroyImage(device, presentInfo.compositionOutput, pAllocator);

    destroyImage(device, uiReplacementBuffer, pAllocator);
}

VkResult FrameInterpolationSwapChainVK::getSwapchainImages(VkDevice device, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages)
{
    FFX_ASSERT_MESSAGE(device == presentInfo.device, "Attempting to get frame interpolation swapchain images with another device");

    if (pSwapchainImages == nullptr)
    {
        *pSwapchainImageCount = gameBufferCount;
    }
    else
    {
        for (uint32_t i = 0; i < gameBufferCount && i < *pSwapchainImageCount; ++i)
        {
            pSwapchainImages[i] = replacementSwapBuffers[i].image;
        }
    }
    return (gameBufferCount <= *pSwapchainImageCount) ? VK_SUCCESS : VK_INCOMPLETE;
}

void FrameInterpolationSwapChainVK::discardOutstandingInterpolationCommandLists()
{
    // drop any outstanding interpolaton command lists
    for (int i = 0; i < _countof(registeredInterpolationCommandLists); i++)
    {
        if (registeredInterpolationCommandLists[i] != nullptr)
        {
            registeredInterpolationCommandLists[i]->drop();
            registeredInterpolationCommandLists[i] = nullptr;
        }
    }
}

void FrameInterpolationSwapChainVK::setFrameGenerationConfig(FfxFrameGenerationConfig const* config)
{
    FFX_ASSERT(config);

    EnterCriticalSection(&criticalSectionUpdateConfig);

    // if config is a pointer to the internal config ::present called this function to apply the changes
    bool applyChangesNow = (config == &nextFrameGenerationConfig);
    
    FfxPresentCallbackFunc inputPresentCallback    = (nullptr != config->presentCallback) ? config->presentCallback : ffxFrameInterpolationUiComposition;
    void*                  inputPresentCallbackCtx = (nullptr != config->presentCallback) ? config->presentCallbackContext : nullptr;
    VulkanQueue            inputInterpolationQueue = config->allowAsyncWorkloads ? presentInfo.asyncComputeQueue : presentInfo.gameQueue;

    // if this is called externally just copy the new config to the internal copy to avoid potentially stalling on criticalSection
    if (!applyChangesNow)
    {
        nextFrameGenerationConfig = *config;

#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
        // since we compose on gameQueue anyway we should run interpolation on it too, to avoid semaphores
        nextFrameGenerationConfig.allowAsyncWorkloads = false;
        inputInterpolationQueue                       = presentInfo.gameQueue;
#endif

        // in case of actual reconfiguration: apply the changes immediately
        if ( presentInfo.interpolationQueue.queue != inputInterpolationQueue.queue
            || interpolationEnabled != config->frameGenerationEnabled
            || presentCallback != inputPresentCallback
            || frameGenerationCallback != config->frameGenerationCallback)
        {
            applyChangesNow = true;
        }
    }

    if (applyChangesNow)
    {
        EnterCriticalSection(&criticalSection);

        currentFrameID          = config->frameID;
        presentInterpolatedOnly = config->onlyPresentInterpolated;

        if (presentInfo.interpolationQueue.queue != inputInterpolationQueue.queue)
        {
            waitForPresents();
            discardOutstandingInterpolationCommandLists();

            // change interpolation queue
            presentInfo.interpolationQueue = inputInterpolationQueue;
        }

        if (interpolationEnabled != config->frameGenerationEnabled || presentCallback != inputPresentCallback ||
            frameGenerationCallback != config->frameGenerationCallback || configFlags != static_cast<FfxFsr3FrameGenerationFlags>(config->flags))
        {
            waitForPresents();
            presentCallback                = inputPresentCallback;
            presentCallbackContext         = inputPresentCallbackCtx;
            frameGenerationCallback        = config->frameGenerationCallback;
            frameGenerationCallbackContext = config->frameGenerationCallbackContext;
            configFlags                    = static_cast<FfxFsr3FrameGenerationFlags>(config->flags);

            // handle interpolation mode change
            if (interpolationEnabled != config->frameGenerationEnabled)
            {
                interpolationEnabled = config->frameGenerationEnabled;
                if (interpolationEnabled)
                {
                    frameInterpolationResetCondition = true;

                    spawnPresenterThread();
                }
                else
                {
                    killPresenterThread();
                }
            }
        }

        LeaveCriticalSection(&criticalSection);
    }

    LeaveCriticalSection(&criticalSectionUpdateConfig);
}

bool FrameInterpolationSwapChainVK::waitForPresents()
{
    // wait for interpolation to finish
    bool waitRes = waitForSemaphoreValue(presentInfo.device, presentInfo.gameSemaphore, gameSemaphoreValue);
    waitRes &= waitForSemaphoreValue(presentInfo.device, presentInfo.interpolationSemaphore, interpolationSemaphoreValue);
    waitRes &= waitForSemaphoreValue(presentInfo.device, presentInfo.presentSemaphore, framesSentForPresentation);

    FFX_ASSERT(waitRes);

    // wait for queue idle too, to make sure all the semaphores are signaled
    VkResult res = VK_SUCCESS; 
    if (res == VK_SUCCESS && presentInfo.asyncComputeQueue.queue != VK_NULL_HANDLE)
        res = vkQueueWaitIdle(presentInfo.asyncComputeQueue.queue);
    if (res == VK_SUCCESS && presentInfo.presentQueue.queue != VK_NULL_HANDLE)
        res = vkQueueWaitIdle(presentInfo.presentQueue.queue);
    if (res == VK_SUCCESS && presentInfo.gameQueue.queue != VK_NULL_HANDLE)
        res = vkQueueWaitIdle(presentInfo.gameQueue.queue);
    if (res == VK_SUCCESS && imageAcquireQueue.queue != VK_NULL_HANDLE)
        res = vkQueueWaitIdle(imageAcquireQueue.queue);

    if (res == VK_SUCCESS)
        res = vkDeviceWaitIdle(presentInfo.device);

    return res == VK_SUCCESS;
}

FfxResource FrameInterpolationSwapChainVK::interpolationOutput(int index)
{
    index = interpolationBufferIndex;

    FfxResourceDescription interpolateDesc = interpolationOutputs[index].description;
    return ffxGetResourceVK(interpolationOutputs[index].image, interpolateDesc, nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
}

void FrameInterpolationSwapChainVK::dispatchInterpolationCommands(uint32_t              currentBackBufferIndex,
                                                                  FfxResource*          pInterpolatedFrame,
                                                                  FfxResource*          pRealFrame,
                                                                  SubmissionSemaphores& semaphoresToWait)
{
    FFX_ASSERT(pInterpolatedFrame);
    FFX_ASSERT(pRealFrame);

    VkImage                currentBackBuffer      = replacementSwapBuffers[currentBackBufferIndex].image;
    FfxResourceDescription gameFrameDesc          = replacementSwapBuffers[currentBackBufferIndex].description;
    FfxResource            backbuffer             = ffxGetResourceVK(currentBackBuffer, gameFrameDesc, nullptr, FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    *pRealFrame = backbuffer;

    // interpolation queue must wait for output resource to become available
 #if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_PRESENTTHREAD
    // in those cases, interpolation output is available after composition
    semaphoresToWait.add(presentInfo.compositionSemaphore, interpolationOutputs[interpolationBufferIndex].availabilitySemaphoreValue);
#elif FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
    // when there is a copy on the present queue from the present thread, the interpolation output will be available when the present semaphore is signaled
    semaphoresToWait.add(presentInfo.presentSemaphore, interpolationOutputs[interpolationBufferIndex].availabilitySemaphoreValue);
#endif

    // check necessary transitions
    ImageBarrierHelper preInterpolationBarriers;
    if (presentInfo.gameQueue.familyIndex != presentInfo.interpolationQueue.familyIndex)
    {
        preInterpolationBarriers.add(currentBackBuffer,
                                     ReplacementBufferTransferState.accessMask,
                                     ReplacementBufferTransferState.accessMask,
                                     ReplacementBufferTransferState.layout,
                                     ReplacementBufferTransferState.layout,
                                     presentInfo.gameQueue.familyIndex,
                                     presentInfo.interpolationQueue.familyIndex);
    }

    // interpolation texture previous content doesn't matter here so we can use an undefined source layout
    preInterpolationBarriers.add(
        interpolationOutputs[interpolationBufferIndex].image, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    
    auto pRegisteredCommandList = registeredInterpolationCommandLists[currentBackBufferIndex];
    if (pRegisteredCommandList != nullptr)
    {
        SubmissionSemaphores semaphoresToSignal;

        if (preInterpolationBarriers.count > 0)
        {
            // we have some necessary transitions, but we need a new command list

            VkCommands* interpolationCommandList =
                presentInfo.commandPool.get(presentInfo.device, presentInfo.interpolationQueue, "interpolationCommandList");
            VkCommandBuffer interpolationCommandBuffer = interpolationCommandList->reset();

            preInterpolationBarriers.record(interpolationCommandBuffer);
            // there is nothing to signal
            VkResult res = interpolationCommandList->execute(semaphoresToWait, semaphoresToSignal);
            FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "barriers prior to dispatchInterpolationCommands failed with error %d", res);
        }

        semaphoresToSignal.add(presentInfo.interpolationSemaphore, ++interpolationSemaphoreValue);

        VkResult res = pRegisteredCommandList->execute(semaphoresToWait, semaphoresToSignal);
        FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "dispatchInterpolationCommands failed with error %d", res);

        *pInterpolatedFrame = interpolationOutput();
    }
    else
    {
        VkCommands* interpolationCommandList = presentInfo.commandPool.get(presentInfo.device, presentInfo.interpolationQueue, "interpolationCommandList");
        VkCommandBuffer interpolationCommandBuffer = interpolationCommandList->reset();

        FfxResource interpolatedResource = interpolationOutput();
        VkImage     interpolatedImage    = static_cast<VkImage>(interpolatedResource.resource);

        FFX_ASSERT_MESSAGE(interpolationOutputs[interpolationBufferIndex].image == interpolatedImage, "Error when querying interpolation output");

        FfxFrameGenerationDispatchDescription desc{};
        desc.commandList                = ffxGetCommandListVK(interpolationCommandBuffer);
        desc.outputs[0]                 = interpolatedResource;
        desc.presentColor               = backbuffer;
        desc.reset                      = frameInterpolationResetCondition;
        desc.numInterpolatedFrames      = 1;
        desc.backBufferTransferFunction = static_cast<FfxBackbufferTransferFunction>(backBufferTransferFunction);
        desc.minMaxLuminance[0]         = minLuminance;
        desc.minMaxLuminance[1]         = maxLuminance;
        desc.frameID                    = currentFrameID;

        // execute the necessary transitions
        preInterpolationBarriers.record(interpolationCommandBuffer);

        if (frameGenerationCallback(&desc, frameGenerationCallbackContext) == FFX_OK)
        {
            SubmissionSemaphores semaphoresToSignal;

            ImageBarrierHelper postInterpolationBarriers;

#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
            uint32_t compositionQueueFamily = presentInfo.gameQueue.familyIndex;
#else
            uint32_t compositionQueueFamily = presentInfo.presentQueue.familyIndex;
#endif

            if (presentInfo.interpolationQueue.familyIndex != compositionQueueFamily)
            {
                // interpolation output queue family ownership transfer
                postInterpolationBarriers.add(interpolatedImage,
                                              InterpolationBufferTransferState.accessMask,
                                              InterpolationBufferTransferState.accessMask,
                                              InterpolationBufferTransferState.layout,
                                              InterpolationBufferTransferState.layout,
                                              presentInfo.interpolationQueue.familyIndex,
                                              compositionQueueFamily);

                // backbuffer queue family ownership transfer
                if (!presentInterpolatedOnly)
                {
                    postInterpolationBarriers.add(currentBackBuffer,
                                                  ReplacementBufferTransferState.accessMask,
                                                  ReplacementBufferTransferState.accessMask,
                                                  ReplacementBufferTransferState.layout,
                                                  ReplacementBufferTransferState.layout,
                                                  presentInfo.interpolationQueue.familyIndex,
                                                  compositionQueueFamily);
                }
                postInterpolationBarriers.record(interpolationCommandBuffer);
            }

            semaphoresToSignal.add(presentInfo.interpolationSemaphore, ++interpolationSemaphoreValue);

            VkResult res = interpolationCommandList->execute(semaphoresToWait, semaphoresToSignal);
            FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "dispatchInterpolationCommands failed wit error %d", res);
        }

        // reset condition if at least one frame was interpolated
        if (desc.numInterpolatedFrames > 0)
        {
            frameInterpolationResetCondition = false;
            *pInterpolatedFrame              = interpolationOutput();
        }
    }
}

VkImageMemoryBarrier FrameinterpolationPresentInfo::queueFamilyOwnershipTransferGameToPresent(FfxResource resource) const
{
    VkAccessFlags accessMask  = getVKAccessFlagsFromResourceState2(resource.state);
    VkImageLayout imageLayout = getVKImageLayoutFromResourceState2(resource.state);
    return getImageMemoryBarrier(
        static_cast<VkImage>(resource.resource), accessMask, accessMask, imageLayout, imageLayout, gameQueue.familyIndex, presentQueue.familyIndex);
}

void FrameInterpolationSwapChainVK::presentInterpolated(const VkPresentInfoKHR* pPresentInfo, uint32_t currentBackBufferIndex, bool needUICopy)
{
    SubmissionSemaphores ToWaitGameQueue;

    SubmissionSemaphores toSignalGameQueue;
    toSignalGameQueue.add(presentInfo.gameSemaphore, ++gameSemaphoreValue);

    // wait on the provided semaphores on the game queue
    addPresentInfoSemaphores(pPresentInfo, ToWaitGameQueue);  // we need to wait on the semaphores the user provided

#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
    // everything on the game queue
    bool replacementBufferNeedsOwnershipTransfer = (presentInfo.gameQueue.familyIndex != presentInfo.interpolationQueue.familyIndex);
    bool uiSurfaceNeedsOwnershipTransfer         = false;  // assumes UI texture is already on the game queue
#else
    bool replacementBufferNeedsOwnershipTransfer = (presentInfo.gameQueue.familyIndex != presentInfo.interpolationQueue.familyIndex);
    bool uiSurfaceNeedsOwnershipTransfer         = (presentInfo.gameQueue.familyIndex != presentInfo.presentQueue.familyIndex) && (presentInfo.currentUiSurface.resource != VK_NULL_HANDLE);
#endif

    if (replacementBufferNeedsOwnershipTransfer || uiSurfaceNeedsOwnershipTransfer || needUICopy)
    {
        VkCommands*     ownershipTransferCommandList = presentInfo.commandPool.get(presentInfo.device, presentInfo.gameQueue, "ownershipTranferCommandList");
        VkCommandBuffer ownershipCommandBuffer       = ownershipTransferCommandList->reset();

        ImageBarrierHelper imageBarriers;
        if (replacementBufferNeedsOwnershipTransfer)
        {
            // transfer the fake backbuffer from game queue to interpolation queue
            imageBarriers.add(replacementSwapBuffers[currentBackBufferIndex].image,
                              ReplacementBufferTransferState.accessMask,
                              ReplacementBufferTransferState.accessMask,
                              ReplacementBufferTransferState.layout,
                              ReplacementBufferTransferState.layout,
                              presentInfo.gameQueue.familyIndex,
                              presentInfo.interpolationQueue.familyIndex);
        }
        if (needUICopy)
        {
            // copy the UI texture and transfer the internal resource to the present queue
            copyUiResource(ownershipCommandBuffer, ToWaitGameQueue, uiSurfaceNeedsOwnershipTransfer);
        }
        else if (uiSurfaceNeedsOwnershipTransfer)
        {
            // transfer the UI texture from game queue to present queue
            imageBarriers.add(presentInfo.queueFamilyOwnershipTransferGameToPresent(presentInfo.currentUiSurface));
        }

        imageBarriers.record(ownershipCommandBuffer);

        VkResult res = ownershipTransferCommandList->execute(ToWaitGameQueue, toSignalGameQueue);
        FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[presentInterpolated] queue family ownership transfer failed with error %d", res);
    }
    else
    {
        // add a signal on the game queue for the interpolation queue to wait on
        VkResult res = presentInfo.gameQueue.submit(VK_NULL_HANDLE, ToWaitGameQueue, toSignalGameQueue);
        FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[presentInterpolated] game queue signal failed with error %d", res);
    }

    // interpolation should wait for the game semaphore
    SubmissionSemaphores ToWaitInterpolationQueue;
    ToWaitInterpolationQueue.add(presentInfo.gameSemaphore, gameSemaphoreValue);

    FfxResource interpolatedFrame{}, realFrame{};
    dispatchInterpolationCommands(
        currentBackBufferIndex, &interpolatedFrame, &realFrame, ToWaitInterpolationQueue);

    EnterCriticalSection(&presentInfo.scheduledFrameCriticalSection);

    PacingData entry{};
    entry.presentCallback                  = presentCallback;
    entry.presentCallbackContext           = presentCallbackContext;
    if (presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING)
    {
#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
        entry.uiSurface = ffxGetResourceVK(uiReplacementBuffer.image, uiReplacementBuffer.description, nullptr, FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
#else
        // by putting FFX_RESOURCE_STATE_COPY_DEST, the queue family ownership transfer from the game queue to the present queue will also include the layout change
        entry.uiSurface = ffxGetResourceVK(uiReplacementBuffer.image, uiReplacementBuffer.description, nullptr, FFX_RESOURCE_STATE_COPY_DEST);
#endif
    }
    else
    {
        entry.uiSurface                    = presentInfo.currentUiSurface;
    }
    entry.presentMode                      = presentMode;
    entry.numFramesSentForPresentationBase = framesSentForPresentation;
    entry.gameSemaphoreValue               = gameSemaphoreValue;
    entry.usePremulAlphaComposite          = (presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_USE_PREMUL_ALPHA) != 0;
    entry.currentFrameID                   = currentFrameID;

    // interpolated
    PacingData::FrameInfo& fiInterpolated = entry.frames[PacingData::FrameType::Interpolated_1];
    if (interpolatedFrame.resource != nullptr)
    {
        fiInterpolated.doPresent                            = true;
        fiInterpolated.resource                             = interpolatedFrame;
        fiInterpolated.interpolationCompletedSemaphoreValue = interpolationSemaphoreValue;
        fiInterpolated.presentIndex                         = ++framesSentForPresentation;
    }

    // real
    if (!presentInterpolatedOnly)
    {
        PacingData::FrameInfo& fiReal = entry.frames[PacingData::FrameType::Real];
        if (realFrame.resource != nullptr)
        {
            fiReal.doPresent                            = true;
            fiReal.resource                             = realFrame;
            fiReal.interpolationCompletedSemaphoreValue = interpolationSemaphoreValue;
            fiReal.presentIndex                         = ++framesSentForPresentation;
        }
    }

    entry.replacementBufferSemaphoreSignal = framesSentForPresentation;
    entry.numFramesToPresent               = static_cast<uint32_t>(framesSentForPresentation - entry.numFramesSentForPresentationBase);

    interpolationOutputs[interpolationBufferIndex].availabilitySemaphoreValue = entry.numFramesSentForPresentationBase + fiInterpolated.doPresent;

#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
    // launch composition now that PacingData has been set
    submitCompositionOnGameQueue(entry);
#endif

    presentInfo.resetTimer              = frameInterpolationResetCondition;
    presentInfo.scheduledInterpolations = entry;
    LeaveCriticalSection(&presentInfo.scheduledFrameCriticalSection);

    // Set event to kick off async CPU present thread
    SetEvent(presentInfo.presentEvent);
}

void FrameInterpolationSwapChainVK::registerUiResource(FfxResource uiResource, uint32_t flags)
{
    EnterCriticalSection(&criticalSection);

    presentInfo.currentUiSurface = uiResource;
    presentInfo.uiCompositionFlags = flags;
    if (nullptr == uiResource.resource)
        presentInfo.uiCompositionFlags &= ~FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING;

    LeaveCriticalSection(&criticalSection);
}

VkResult FrameInterpolationSwapChainVK::queuePresentNonInterpolated(VkCommands* pCommands, uint32_t imageIndex, SubmissionSemaphores& semaphoresToWait)
{
    SubmissionSemaphores semaphoresToSignal;
    semaphoresToSignal.add(presentInfo.replacementBufferSemaphore, ++framesSentForPresentation);
    semaphoresToSignal.add(presentInfo.frameRenderedSemaphores[imageIndex]);  // not a timeline semaphore

    semaphoresToSignal.add(presentInfo.presentSemaphore, framesSentForPresentation);
    presentInfo.lastPresentSemaphoreValue = framesSentForPresentation;

    pCommands->execute(semaphoresToWait, semaphoresToSignal);

    VkResult res = presentToSwapChain(&presentInfo, imageIndex, imageIndex);
    FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR, "[queuePresentNonInterpolated] present failed with error %d", res);

    semaphoresToWait.add(presentInfo.presentSemaphore, framesSentForPresentation);
    res = presentInfo.gameQueue.submit(VK_NULL_HANDLE, semaphoresToWait, semaphoresToSignal);
    FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[queuePresentNonInterpolated] game queue submit failed with error %d", res);

    return res;
}

VkResult FrameInterpolationSwapChainVK::presentPassthrough(uint32_t              imageIndex,
                                                           SubmissionSemaphores& gameQueueWait,
                                                           SubmissionSemaphores& gameQueueSignal,
                                                           SubmissionSemaphores& presentQueueWait)
{
    VkImage srcImage = replacementSwapBuffers[replacementSwapBufferIndex].image;
    VkImage dstImage = presentInfo.realSwapchainImages[imageIndex];

    ImageBarrierHelper imageBarriers;

    // replacement buffer barrier
    VkImageMemoryBarrier srcBarrier = getImageMemoryBarrier(
        srcImage,
        ReplacementBufferTransferState.accessMask,
        VK_ACCESS_TRANSFER_READ_BIT,
        ReplacementBufferTransferState.layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        presentInfo.gameQueue.familyIndex,  // if game queue and present queue families are different, this will automatically be a queue family ownership transfer barrier)
        presentInfo.presentQueue.familyIndex);

    if (presentInfo.presentQueue.familyIndex != presentInfo.gameQueue.familyIndex)
    {
        // queue family ownership transfer for replacement buffer
        // srcBarrier needs to be executed on the game queue
        auto            ownershipList          = presentInfo.commandPool.get(presentInfo.device, presentInfo.gameQueue, "ownershipList");
        VkCommandBuffer ownershipCommandBuffer = ownershipList->reset();

        ImageBarrierHelper ownershipBarriers;
        ownershipBarriers.add(srcBarrier);
        ownershipBarriers.record(ownershipCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkResult res = ownershipList->execute(gameQueueWait, gameQueueSignal);
        FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[presentPassthrough] queue submit failed with error %d", res);
    }
    else
    {
        // just wait/signal the semaphores
        VkResult res = presentInfo.gameQueue.submit(VK_NULL_HANDLE, gameQueueWait, gameQueueSignal);
        FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[presentPassthrough] queue submit failed with error %d", res);
    }

    auto            passthroughList          = presentInfo.commandPool.get(presentInfo.device, presentInfo.presentQueue, "passthroughList");
    VkCommandBuffer passthroughCommandBuffer = passthroughList->reset();

    // real swapchain buffer (that was just acquried) barrier
    VkImageMemoryBarrier dstBarrier =
        getImageMemoryBarrier(dstImage, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // change layout for copy & queue family ownership transfer (if necessary)
    ImageBarrierHelper preCopyBarriers;
    if (presentInfo.presentQueue.familyIndex != presentInfo.gameQueue.familyIndex || srcBarrier.oldLayout != srcBarrier.newLayout)
        preCopyBarriers.add(srcBarrier);
    preCopyBarriers.add(dstBarrier);
    preCopyBarriers.record(passthroughCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // copy
    recordCopy(passthroughCommandBuffer, srcImage, dstImage, presentInfo.realSwapchainImageDescription.width, presentInfo.realSwapchainImageDescription.height);

    // revert replacement buffer to shader read layout
    flipBarrier(srcBarrier);
    
    // set real swapchain buffer to present layout
    dstBarrier = getImageMemoryBarrier(dstImage, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    ImageBarrierHelper postCopyBarriers;
    postCopyBarriers.add(srcBarrier);
    postCopyBarriers.add(dstBarrier);
    postCopyBarriers.record(passthroughCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    return queuePresentNonInterpolated(passthroughList, imageIndex, presentQueueWait);
}

VkResult FrameInterpolationSwapChainVK::presentNonInterpolatedWithUiCompositionOnPresentQueue(uint32_t              imageIndex,
                                                                                              SubmissionSemaphores& gameQueueWait,
                                                                                              SubmissionSemaphores& gameQueueSignal,
                                                                                              SubmissionSemaphores& presentQueueWait,
                                                                                              bool                  needUICopy)
{
    bool needQueueOwnershipTransfer = presentInfo.presentQueue.familyIndex != presentInfo.gameQueue.familyIndex;
    
    VkImage srcImage = replacementSwapBuffers[replacementSwapBufferIndex].image;
    VkImage dstImage = presentInfo.realSwapchainImages[imageIndex];

    ImageBarrierHelper presentQueueBarriers;

    if (needQueueOwnershipTransfer || needUICopy)
    {
        auto            ownershipList          = presentInfo.commandPool.get(presentInfo.device, presentInfo.gameQueue, "ownershipList");
        VkCommandBuffer ownershipCommandBuffer = ownershipList->reset();

        ImageBarrierHelper gameQueueBarriers;
        
        if (needUICopy)
        {
            // copy on game queue and transfer to present queue if necessary
            VkImageMemoryBarrier uiReplacementBufferBarrier = copyUiResource(ownershipCommandBuffer, gameQueueWait, needQueueOwnershipTransfer);

            // transition of the UI replacement resource
            if (needQueueOwnershipTransfer)
            {
                presentQueueBarriers.add(uiReplacementBufferBarrier);
            }
        }

        if (needQueueOwnershipTransfer)
        {
            // queue family ownership transfer for replacement buffer
            VkImageMemoryBarrier ownershipBarrier = getImageMemoryBarrier(srcImage,
                                                                          ReplacementBufferTransferState.accessMask,
                                                                          ReplacementBufferTransferState.accessMask,
                                                                          ReplacementBufferTransferState.layout,
                                                                          ReplacementBufferTransferState.layout,
                                                                          presentInfo.gameQueue.familyIndex,
                                                                          presentInfo.presentQueue.familyIndex);

            // record the queue family ownership transfer on both queues
            presentQueueBarriers.add(ownershipBarrier);
            gameQueueBarriers.add(ownershipBarrier);

            // queue family ownership transfer for UI surface if not copied
            if (!needUICopy && presentInfo.currentUiSurface.resource != VK_NULL_HANDLE)
            {
                ownershipBarrier = presentInfo.queueFamilyOwnershipTransferGameToPresent(presentInfo.currentUiSurface);
                // record the queue family ownership transfer on both queues
                presentQueueBarriers.add(ownershipBarrier);
                gameQueueBarriers.add(ownershipBarrier);
            }
        }

        gameQueueBarriers.record(ownershipCommandBuffer);

        VkResult res = ownershipList->execute(gameQueueWait, gameQueueSignal);
        FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[presentWithUiComposition] queue submit failed with error %d", res);
    }
    else
    {
        VkResult res = presentInfo.gameQueue.submit(VK_NULL_HANDLE, gameQueueWait, gameQueueSignal);
        FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[presentWithUiComposition] queue submit failed with error %d", res);
    }

    // FFX doesn't have a undefined state. Transition to Present here. It will come back as Present after the callback
    presentQueueBarriers.add(dstImage, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    auto            uiCompositionList = presentInfo.commandPool.get(presentInfo.device, presentInfo.presentQueue, "uiCompositionList");
    VkCommandBuffer uiCompositionCommandBuffer     = uiCompositionList->reset();

    // transition layout & queue family ownership transfer (if necessary)
    presentQueueBarriers.record(uiCompositionCommandBuffer);

    FfxResourceDescription srcResourceDesc = replacementSwapBuffers[replacementSwapBufferIndex].description;
    FfxResourceDescription dstResourceDesc = presentInfo.realSwapchainImageDescription;

    FfxPresentCallbackDescription desc{};
    desc.commandList           = ffxGetCommandListVK(uiCompositionCommandBuffer);
    desc.device                = presentInfo.device;
    desc.isInterpolatedFrame   = false;
    desc.outputSwapChainBuffer = ffxGetResourceVK(dstImage, dstResourceDesc, nullptr, FFX_RESOURCE_STATE_PRESENT);
    desc.currentBackBuffer     = ffxGetResourceVK(srcImage, srcResourceDesc, nullptr, FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    if (presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING)
    {
        desc.currentUI = ffxGetResourceVK(uiReplacementBuffer.image, uiReplacementBuffer.description, nullptr, FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    }
    else
    {
        desc.currentUI = presentInfo.currentUiSurface;
    }
    desc.frameID               = currentFrameID;

    presentCallback(&desc, presentCallbackContext);

    return queuePresentNonInterpolated(uiCompositionList, imageIndex, presentQueueWait);
}

VkResult FrameInterpolationSwapChainVK::presentNonInterpolatedWithUiCompositionOnGameQueue(uint32_t              imageIndex,
                                                                                           SubmissionSemaphores& gameQueueWait,
                                                                                           SubmissionSemaphores& gameQueueSignal,
                                                                                           SubmissionSemaphores& presentQueueWait,
                                                                                           bool                  needUICopy)
{
    auto            uiCompositionList          = presentInfo.commandPool.get(presentInfo.device, presentInfo.gameQueue, "uiCompositionList");
    VkCommandBuffer uiCompositionCommandBuffer = uiCompositionList->reset();

    if (needUICopy)
        copyUiResource(uiCompositionCommandBuffer, gameQueueWait, false);
    
    VkImage srcImage = replacementSwapBuffers[replacementSwapBufferIndex].image;
    VkImage dstImage = presentInfo.realSwapchainImages[imageIndex];

    ImageBarrierHelper gameQueueBarriers;

    // FFX doesn't have a undefined state. Transition to Present here. It will come back as Present after the callback
    gameQueueBarriers.add(dstImage, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // transition layout & queue family ownership transfer (if necessary)
    gameQueueBarriers.record(uiCompositionCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    FfxResourceDescription srcResourceDesc = replacementSwapBuffers[replacementSwapBufferIndex].description;
    FfxResourceDescription dstResourceDesc = presentInfo.realSwapchainImageDescription;

    FfxPresentCallbackDescription desc{};
    desc.commandList           = ffxGetCommandListVK(uiCompositionCommandBuffer);
    desc.device                = presentInfo.device;
    desc.isInterpolatedFrame   = false;
    desc.outputSwapChainBuffer = ffxGetResourceVK(dstImage, dstResourceDesc, nullptr, FFX_RESOURCE_STATE_PRESENT);
    desc.currentBackBuffer     = ffxGetResourceVK(srcImage, srcResourceDesc, nullptr, FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    if (presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING)
    {
        desc.currentUI = ffxGetResourceVK(uiReplacementBuffer.image, uiReplacementBuffer.description, nullptr, FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    }
    else
    {
        desc.currentUI = presentInfo.currentUiSurface;
    }
    desc.usePremulAlpha        = ((presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_USE_PREMUL_ALPHA) != 0);
    desc.frameID               = currentFrameID;
    presentCallback(&desc, presentCallbackContext);

    gameQueueSignal.add(presentInfo.replacementBufferSemaphore, ++framesSentForPresentation);

    ImageBarrierHelper presentQueueBarriers;
    if (presentInfo.presentQueue.familyIndex != presentInfo.gameQueue.familyIndex)
    {
        // queue family ownership transfer for swapchain buffer
        presentQueueBarriers.add(dstImage,
                                 0,
                                 0,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                 presentInfo.gameQueue.familyIndex,
                                 presentInfo.presentQueue.familyIndex);
        presentQueueBarriers.record(uiCompositionCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }
    else
    {
        gameQueueSignal.add(presentInfo.frameRenderedSemaphores[imageIndex]);  // not a timeline semaphore
    }

    // cannot signal after present on the present queue, so signal here
    gameQueueSignal.add(presentInfo.presentSemaphore, framesSentForPresentation);
    presentInfo.lastPresentSemaphoreValue = framesSentForPresentation;

    VkResult res = uiCompositionList->execute(gameQueueWait, gameQueueSignal);
    FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[presentWithUiComposition2] queue submit failed with error %d", res);

    if (presentInfo.presentQueue.familyIndex != presentInfo.gameQueue.familyIndex)
    {
        auto            ownershipList          = presentInfo.commandPool.get(presentInfo.device, presentInfo.presentQueue, "ownershipList");
        VkCommandBuffer ownershipCommandBuffer = ownershipList->reset();

        presentQueueBarriers.record(ownershipCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

        SubmissionSemaphores presentQueueSignal;
        presentQueueSignal.add(presentInfo.frameRenderedSemaphores[imageIndex]);  // not a timeline semaphore
        res = ownershipList->execute(presentQueueWait, presentQueueSignal);
        FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[queuePresentNonInterpolated] queue family ownership transfer to present queue failed with error %d", res);
    }

    res = presentToSwapChain(&presentInfo, imageIndex, imageIndex);
    FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR, "[queuePresentNonInterpolated] present failed with error %d", res);

    SubmissionSemaphores semaphoresToWait;
    SubmissionSemaphores semaphoresToSignal;
    semaphoresToWait.add(presentInfo.presentSemaphore, framesSentForPresentation);
    res = presentInfo.gameQueue.submit(VK_NULL_HANDLE, semaphoresToWait, semaphoresToSignal);
    FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[queuePresentNonInterpolated] game queue submit failed with error %d", res);

    return res;
}

VkResult FrameinterpolationPresentInfo::acquireNextRealImage(uint32_t& imageIndex, VkSemaphore& acquireSemaphore)
{
    acquireSemaphore = acquireSemaphores[nextAcquireSemaphoreIndex];
    EnterCriticalSection(&swapchainCriticalSection);
    VkResult res = vkAcquireNextImageKHR(device, realSwapchain, UINT64_MAX, acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
    LeaveCriticalSection(&swapchainCriticalSection);
    EXIT_ON_VKRESULT_NOT_SUCCESS(res);

    nextAcquireSemaphoreIndex = (nextAcquireSemaphoreIndex + 1) % FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_ACQUIRE_SEMAPHORE_COUNT;
    return res;
}

VkResult FrameInterpolationSwapChainVK::queuePresent(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    if (queue == VK_NULL_HANDLE || pPresentInfo == nullptr)
    {
        return VK_INCOMPLETE;
    }

    setFrameGenerationConfig(&nextFrameGenerationConfig);

    EnterCriticalSection(&criticalSection);

    uint32_t currentBackBufferIndex = replacementSwapBufferIndex;

    // ensure that we aren't running too ahead of the
    FFX_ASSERT_MESSAGE(pPresentInfo->pImageIndices[0] == replacementSwapBufferIndex, "Presented image and internal replacement swap buffer index aren't in sync.");

    // first determine which codepath to run
    bool bRunInterpolation = true;

    // determine what present path to execute
    const bool fgCallbackConfigured    = frameGenerationCallback != nullptr;
    const bool fgCommandListConfigured = registeredInterpolationCommandLists[currentBackBufferIndex] != nullptr;
    const bool runInterpolation        = interpolationEnabled && (fgCallbackConfigured || fgCommandListConfigured);

    // Verify integrity of internal UI resource
    bool needUICopy = false;
    if (verifyUiDuplicateResource())
    {
        // check here if the UI resource will have to be copied to the internal resource
        needUICopy = ((presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING) != 0) &&
                     (presentInfo.currentUiSurface.resource != VK_NULL_HANDLE);
    }

    if (runInterpolation)
    {
        WaitForSingleObject(presentInfo.interpolationEvent, INFINITE);

        presentInterpolated(pPresentInfo, currentBackBufferIndex, needUICopy);
    }
    else
    {
        // if no interpolation, then we copy directly to the swapchain. Render UI, present and be done
        // it should also wait for the semaphore acquiring the image.

        // if there are some semaphores the present was supposed to wait on, let the game queue wait on them
        SubmissionSemaphores gameQueueWait;
        addPresentInfoSemaphores(pPresentInfo, gameQueueWait);

        // game queue will increment the gameSemaphore
        // we don't signal here because some queue family ownership transfer might be needed
        SubmissionSemaphores gameQueueSignal;
        gameQueueSignal.add(presentInfo.gameSemaphore, ++gameSemaphoreValue);

        // present queue should wait for the game queue to be finished
        SubmissionSemaphores presentQueueWait; // wait on the present queue
        presentQueueWait.add(presentInfo.gameSemaphore, gameSemaphoreValue);

        // fails 
        FFX_ASSERT_MESSAGE(pPresentInfo->pNext == nullptr, "VkPresentInfoKHR::pNext not yet supported.");

        uint32_t imageIndex = 0;
        VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
        
        VkResult res = presentInfo.acquireNextRealImage(imageIndex, acquireSemaphore);
        FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR, "[queuePresent] acquiring next image failed with error %d", res);
        if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR)
        {
            // composition queue should wait until the image is available to render into it
#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
            gameQueueWait.add(acquireSemaphore);
#else
            presentQueueWait.add(acquireSemaphore);
#endif

            if (presentCallback != nullptr)
            {
#if FFX_COMPOSITION_MODE == FFX_COMPOSE_IN_VKQUEUEPRESENT_ACQUIRE_IN_PRESENTTHREAD
                res = presentNonInterpolatedWithUiCompositionOnGameQueue(imageIndex, gameQueueWait, gameQueueSignal, presentQueueWait, needUICopy);
#else
                res = presentNonInterpolatedWithUiCompositionOnPresentQueue(imageIndex, gameQueueWait, gameQueueSignal, presentQueueWait, needUICopy);
#endif
                FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[queuePresent] presentWithUiComposition failed with error %d", res);
            }
            else
            {
                res = presentPassthrough(imageIndex, gameQueueWait, gameQueueSignal, presentQueueWait);
                FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "[queuePresent] presentPassthrough failed with error %d", res);
            }
        }
    }

    previousFrameWasInterpolated = runInterpolation;

    replacementSwapBuffers[currentBackBufferIndex].availabilitySemaphoreValue = framesSentForPresentation;

    // Unregister any potential command list
    registeredInterpolationCommandLists[currentBackBufferIndex] = nullptr;
    presentCount++;
    interpolationBufferIndex = presentCount % _countof(interpolationOutputs);

    // update active backbuffer and block when no buffer is available
    replacementSwapBufferIndex = presentCount % gameBufferCount;

    LeaveCriticalSection(&criticalSection);

    waitForSemaphoreValue(
        presentInfo.device, presentInfo.replacementBufferSemaphore, replacementSwapBuffers[replacementSwapBufferIndex].availabilitySemaphoreValue);
    
    return VK_SUCCESS;
}

bool FrameInterpolationSwapChainVK::spawnPresenterThread()
{
    if (interpolationThreadHandle == NULL)
    {
        presentInfo.shutdown = false;
        interpolationThreadHandle = CreateThread(nullptr, 0, interpolationThread, reinterpret_cast<void*>(&presentInfo), 0, nullptr);
        FFX_ASSERT(interpolationThreadHandle != NULL);

        if (interpolationThreadHandle != 0)
        {
            SetThreadPriority(interpolationThreadHandle, THREAD_PRIORITY_HIGHEST);
            SetThreadDescription(interpolationThreadHandle, L"AMD FSR Interpolation Thread");
        }

        SetEvent(presentInfo.interpolationEvent);
    }

    return interpolationThreadHandle != NULL;
}

bool FrameInterpolationSwapChainVK::killPresenterThread()
{
    if (interpolationThreadHandle != NULL)
    {
        // prepare present CPU thread for shutdown
        presentInfo.shutdown = true;

        // signal event to allow thread to finish
        SetEvent(presentInfo.presentEvent);
        WaitForSingleObject(interpolationThreadHandle, INFINITE);
        SafeCloseHandle(interpolationThreadHandle);
    }

    return interpolationThreadHandle == NULL;
}

void FrameInterpolationSwapChainVK::setHdrMetadata(VkDevice device, const VkHdrMetadataEXT* pMetadata)
{
    if (pMetadata != nullptr)
    {
        minLuminance = pMetadata->minLuminance / 10000.0f;  // see DX12 swapchain
        maxLuminance = pMetadata->maxLuminance;
        if (vkSetHdrMetadataEXTProc != nullptr)
            vkSetHdrMetadataEXTProc(device, 1, &presentInfo.realSwapchain, pMetadata);
    }
}

uint64_t FrameInterpolationSwapChainVK::getLastPresentCount()
{
    return presentInfo.realPresentCount;
}

VkCommandBuffer FrameInterpolationSwapChainVK::getInterpolationCommandList()
{
    EnterCriticalSection(&criticalSection);

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    // store active backbuffer index to the command list, used to verify list usage later
    if (interpolationEnabled)
    {
        uint32_t currentBackBufferIndex = replacementSwapBufferIndex;
        
        VkCommands* registeredCommands = registeredInterpolationCommandLists[currentBackBufferIndex];

        // drop if already existing
        if (registeredCommands != nullptr)
        {
            registeredCommands->drop();
            registeredCommands = nullptr;
        }

        registeredCommands = presentInfo.commandPool.get(presentInfo.device, presentInfo.interpolationQueue, "getInterpolationCommandList");
        FFX_ASSERT(registeredCommands);

        commandBuffer = registeredCommands->reset();

        registeredInterpolationCommandLists[currentBackBufferIndex] = registeredCommands;
    }

    LeaveCriticalSection(&criticalSection);

    return commandBuffer;
}

VkResult FrameInterpolationSwapChainVK::submitCompositionOnGameQueue(const PacingData& entry)
{
    VkResult res = VK_SUCCESS;

    FFX_ASSERT_MESSAGE(entry.numFramesToPresent > 0, "[submitCompositionOnGameQueue] need at least one frame to present");
    if (entry.numFramesToPresent > 0)
    {
        SubmissionSemaphores toWait;
        toWait.add(presentInfo.gameSemaphore, entry.gameSemaphoreValue);

        bool uiSurfaceTransfered = true; // the UI surface is already on the game queue. so there is no need to transition it

        for (uint32_t frameType = 0; frameType < PacingData::FrameType::Count; frameType++)
        {
            const PacingData::FrameInfo& frameInfo = entry.frames[frameType];
            if (frameInfo.doPresent)
            {
                SubmissionSemaphores toSignal;

                // unused value
                uint32_t realSwapchainImageIndex = 0;

                res = compositeSwapChainFrame(&presentInfo, &entry, (PacingData::FrameType)frameType, realSwapchainImageIndex, presentInfo.gameQueue, toWait, toSignal, uiSurfaceTransfered);
                FFX_ASSERT_MESSAGE_FORMAT(res == VK_SUCCESS, "compositeSwapChainFrame failed with error %d", res);
            }
        }

        // if no frame was presented, we still need to update the semaphore
        if (toWait.count > 0)
        {
            res = presentInfo.gameQueue.submit(VK_NULL_HANDLE, toWait, SubmissionSemaphores());
        }
    }

    return res;
}

bool FrameInterpolationSwapChainVK::verifyUiDuplicateResource()
{
    VkResult res = VK_SUCCESS;

    const VkAllocationCallbacks* pAllocator = nullptr;  // we don't have it at this point
    
    VkImage                uiResource     = static_cast<VkImage>(presentInfo.currentUiSurface.resource);
    FfxResourceDescription uiResourceDesc = presentInfo.currentUiSurface.description;

    if ((0 == (presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING)) || (VK_NULL_HANDLE == uiResource))
    {
        if (uiReplacementBuffer.image != VK_NULL_HANDLE)
        {
            waitForSemaphoreValue(presentInfo.device, presentInfo.compositionSemaphore, framesSentForPresentation);
            destroyImage(presentInfo.device, uiReplacementBuffer, pAllocator);
            uiReplacementBuffer = {};
        }
    }
    else
    {
        if (uiReplacementBuffer.image != VK_NULL_HANDLE)
        {
            auto internalDesc = uiReplacementBuffer.description;

            if (uiResourceDesc.format != internalDesc.format || uiResourceDesc.width != internalDesc.width || uiResourceDesc.height != internalDesc.height)
            {
                waitForSemaphoreValue(presentInfo.device, presentInfo.compositionSemaphore, framesSentForPresentation);
                destroyImage(presentInfo.device, uiReplacementBuffer, pAllocator);
                uiReplacementBuffer = {};
            }
        }

        if (uiReplacementBuffer.image == VK_NULL_HANDLE)
        {
            VkImageCreateInfo info = {};
            info.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.pNext             = nullptr;
            info.flags             = 0;
            info.imageType         = VK_IMAGE_TYPE_2D;
            info.format            = ffxGetVkFormatFromSurfaceFormat(uiResourceDesc.format);
            info.extent.width      = uiResourceDesc.width;
            info.extent.height     = uiResourceDesc.height;
            info.extent.depth      = 1;
            info.mipLevels         = 1;
            info.arrayLayers       = 1;
            info.samples           = VK_SAMPLE_COUNT_1_BIT;
            info.tiling            = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;  // queue family ownership transfer will be handled manually

            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

            res = createImage(presentInfo.device,
                              uiReplacementBuffer,
                              info,
                              uiResourceDesc.format,
                              "AMD FSR Ui Doublebuffer Resource",
                              memProperties,
                              pAllocator);
        }
    }

    return VK_SUCCESS == res;
}

VkImageMemoryBarrier FrameInterpolationSwapChainVK::copyUiResource(VkCommandBuffer       commandBuffer,
                                                                   SubmissionSemaphores& gameQueueWait,
                                                                   bool                  transferToPresentQueue)
{
    if ((previousFrameWasInterpolated) && (uiReplacementBuffer.image != VK_NULL_HANDLE))
    {
        // if doublebuffering is enabled and uiReplacementbuffer has been used last frame
        // game queue should wait for any outstanding composition workloads to finish
        gameQueueWait.add(presentInfo.compositionSemaphore, framesSentForPresentation);
    }

    VkImage imageSrc = reinterpret_cast<VkImage>(presentInfo.currentUiSurface.resource);
    VkImage imageDst = uiReplacementBuffer.image;

    // transitions for copy
    VkImageMemoryBarrier srcBarrier = getImageMemoryBarrier(imageSrc,
                                                            getVKAccessFlagsFromResourceState2(presentInfo.currentUiSurface.state),
                                                            VK_ACCESS_TRANSFER_READ_BIT,
                                                            getVKImageLayoutFromResourceState2(presentInfo.currentUiSurface.state),
                                                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                            VK_QUEUE_FAMILY_IGNORED,
                                                            VK_QUEUE_FAMILY_IGNORED,
                                                            1,   // we only consider the first mip
                                                            1);  // we don't support array

    VkImageMemoryBarrier dstBarrier =
        getImageMemoryBarrier(imageDst, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    ImageBarrierHelper preCopyBarriers;
    preCopyBarriers.add(srcBarrier);
    preCopyBarriers.add(dstBarrier);
    preCopyBarriers.record(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // copy
    recordCopy(commandBuffer,
               imageSrc,
               imageDst,
               uiReplacementBuffer.description.width,
               uiReplacementBuffer.description.height,
               uiReplacementBuffer.description.depth);

    // transition back to previous state
    flipBarrier(srcBarrier);

    // transition the ui replacement texture to shader read state
    dstBarrier = getImageMemoryBarrier(
        imageDst, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // if we are composing on the present queue, we need a queue family ownership transfer
    if (transferToPresentQueue)
    {
        dstBarrier.srcQueueFamilyIndex = presentInfo.gameQueue.familyIndex;
        dstBarrier.dstQueueFamilyIndex = presentInfo.presentQueue.familyIndex;
    }
    
    ImageBarrierHelper postCopyBarriers;
    postCopyBarriers.add(srcBarrier);
    postCopyBarriers.add(dstBarrier);
    postCopyBarriers.record(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    return dstBarrier;
}
