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
#include "../ffx_api.h"
#include "../ffx_api_types.h"
#include <vulkan/vulkan.h>

/// FFX specific callback type when submitting a command buffer to a queue.
typedef VkResult (*PFN_vkQueueSubmitFFXAPI)(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);

/// Helper stucture
struct VkQueueInfoFFXAPI
{
    VkQueue                 queue;        ///< the vulkan queue
    uint32_t                familyIndex;  ///< the queue family index, that will be used to perform queue family ownership transfer
    PFN_vkQueueSubmitFFXAPI submitFunc;   ///< an optional submit function in case there might be some concurrent submissions
};

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK 0x0000003u
struct ffxCreateBackendVKDesc
{
    ffxCreateContextDescHeader header;
    VkDevice                   vkDevice;          ///< the logical device used by the program.
    VkPhysicalDevice           vkPhysicalDevice;  ///< the physical device used by the program.
    PFN_vkGetDeviceProcAddr    vkDeviceProcAddr;  ///< function pointer to get device procedure addresses
};

#define FFX_API_EFFECT_ID_FGSC_VK 0x00040000u

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_FGSWAPCHAIN_VK 0x40001u
struct ffxCreateContextDescFrameGenerationSwapChainVK
{
    ffxCreateContextDescHeader header;
    VkPhysicalDevice           physicalDevice;     ///< the physicak device used by the program.
    VkDevice                   device;             ///< the logical device used by the program.
    VkSwapchainKHR*            swapchain;          ///< the current swapchain to be replaced. Will be destroyed when the context is created. This can be VK_NULL_HANDLE. Will contain the new swapchain on return.
    VkAllocationCallbacks*     allocator;          ///< optional allocation callbacks.
    VkSwapchainCreateInfoKHR   createInfo;         ///< the description of the desired swapchain. If its VkSwapchainCreateInfoKHR::oldSwapchain field isn't VK_NULL_HANDLE, it should be the same as the ffxCreateContextDescFrameGenerationSwapChainVK::swapchain field above.
    VkQueueInfoFFXAPI          gameQueue;          ///< the main graphics queue, where Present is called.
    VkQueueInfoFFXAPI          asyncComputeQueue;  ///< A queue with Compute capability.
    VkQueueInfoFFXAPI          presentQueue;       ///< A queue with Transfer and Present capabilities.
    VkQueueInfoFFXAPI          imageAcquireQueue;  ///< A queue with no capability required.
};

#define FFX_API_CONFIGURE_DESC_TYPE_FGSWAPCHAIN_REGISTERUIRESOURCE_VK 0x40002u
struct ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceVK
{
    ffxConfigureDescHeader header;
    struct FfxApiResource  uiResource;  ///< Resource containing user interface for composition. May be empty.
    uint32_t               flags;       ///< Zero or combination of values from FfxApiUiCompositionFlags.
};

#define FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_INTERPOLATIONCOMMANDLIST_VK 0x40003u
struct ffxQueryDescFrameGenerationSwapChainInterpolationCommandListVK
{
    ffxQueryDescHeader header;
    void**             pOutCommandList;  ///< Output command nuffer (VkCommandBuffer) to be used for frame generation dispatch.
};

#define FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_INTERPOLATIONTEXTURE_VK 0x40004u
struct ffxQueryDescFrameGenerationSwapChainInterpolationTextureVK
{
    ffxQueryDescHeader     header;
    struct FfxApiResource* pOutTexture;  ///< Output resource in which the frame interpolation result should be placed.
};

#define FFX_API_DISPATCH_DESC_TYPE_FGSWAPCHAIN_WAIT_FOR_PRESENTS_VK 0x40007u
struct ffxDispatchDescFrameGenerationSwapChainWaitForPresentsVK
{
    ffxDispatchDescHeader header;
};

#define FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_KEYVALUE_VK 0x40008u
struct ffxConfigureDescFrameGenerationSwapChainKeyValueVK
{
    ffxConfigureDescHeader header;
    uint64_t               key;        ///< Configuration key, member of the FfxApiConfigureFrameGenerationSwapChainKeyVK enumeration.
    uint64_t               u64;        ///< Integer value or enum value to set.
    void*                  ptr;        ///< Pointer to set or pointer to value to set.
};

//enum value matches enum FfxFrameInterpolationSwapchainConfigureKey
enum FfxApiConfigureFrameGenerationSwapChainKeyVK
{
    FFX_API_CONFIGURE_FG_SWAPCHAIN_KEY_WAITCALLBACK = 0,                     ///< Sets FfxWaitCallbackFunc
    FFX_API_CONFIGURE_FG_SWAPCHAIN_KEY_FRAMEPACINGTUNING = 2,                ///< Sets FfxApiSwapchainFramePacingTuning casted from ptr
};

#define FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_GPU_MEMORY_USAGE_VK 0x00040009u
struct ffxQueryFrameGenerationSwapChainGetGPUMemoryUsageVK
{
    ffxQueryDescHeader header;
    struct FfxApiEffectMemoryUsage* gpuMemoryUsageFrameGenerationSwapchain;
};

/// Function to get the number of presents. This is useful when using frame interpolation
typedef uint64_t (*PFN_getLastPresentCountFFXAPI)(VkSwapchainKHR);

/// FFX API specific functions to create and destroy a swapchain
typedef VkResult(*PFN_vkCreateSwapchainFFXAPI)(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain, void* pContext);
typedef void(*PFN_vkDestroySwapchainFFXAPI)(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator, void* pContext);

#define FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_FUNCTIONS_VK 0x40005u
struct ffxQueryDescSwapchainReplacementFunctionsVK
{
    ffxQueryDescHeader header;
    PFN_vkCreateSwapchainFFXAPI   pOutCreateSwapchainFFXAPI;      ///< Replacement of vkCreateSwapchainKHR. Can be called when swapchain is recreated but swapchain context isn't (for example when toggling vsync).
    PFN_vkDestroySwapchainFFXAPI  pOutDestroySwapchainFFXAPI;     ///< Replacement of vkDestroySwapchainKHR. Can be called when swapchain is destroyed but swapchain context isn't.
    PFN_vkGetSwapchainImagesKHR   pOutGetSwapchainImagesKHR;      ///< Replacement of vkGetSwapchainImagesKHR.
    PFN_vkAcquireNextImageKHR     pOutAcquireNextImageKHR;        ///< Replacement of vkAcquireNextImageKHR.
    PFN_vkQueuePresentKHR         pOutQueuePresentKHR;            ///< Replacement of vkQueuePresentKHR.
    PFN_vkSetHdrMetadataEXT       pOutSetHdrMetadataEXT;          ///< Replacement of vkSetHdrMetadataEXT.
    PFN_getLastPresentCountFFXAPI pOutGetLastPresentCountFFXAPI;  ///< Additional function to get the number of times present has been called since the swapchain creation.
};

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_FGSWAPCHAIN_MODE_VK 0x40010u
struct ffxCreateContextDescFrameGenerationSwapChainModeVK
{
    ffxCreateContextDescHeader header;
    bool                       composeOnPresentQueue;  ///< flags indicating that composition will happen on the present queue
};

static inline uint32_t ffxApiGetSurfaceFormatVK(VkFormat fmt)
{
    switch (fmt)
    {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT;
    case VK_FORMAT_R32G32B32_SFLOAT:
        return FFX_API_SURFACE_FORMAT_R32G32B32_FLOAT;
    case VK_FORMAT_R32G32B32A32_UINT:
        return FFX_API_SURFACE_FORMAT_R32G32B32A32_UINT;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT;
    case VK_FORMAT_R32G32_SFLOAT:
        return FFX_API_SURFACE_FORMAT_R32G32_FLOAT;
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return FFX_API_SURFACE_FORMAT_R32_UINT;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM;
    case VK_FORMAT_R8G8B8A8_SNORM:
        return FFX_API_SURFACE_FORMAT_R8G8B8A8_SNORM;
    case VK_FORMAT_R8G8B8A8_SRGB:
        return FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB:
        return FFX_API_SURFACE_FORMAT_B8G8R8A8_SRGB;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        return FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        return FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM;
    case VK_FORMAT_R16G16_SFLOAT:
        return FFX_API_SURFACE_FORMAT_R16G16_FLOAT;
    case VK_FORMAT_R16G16_UINT:
        return FFX_API_SURFACE_FORMAT_R16G16_UINT;
    case VK_FORMAT_R16G16_SINT:
        return FFX_API_SURFACE_FORMAT_R16G16_SINT;
    case VK_FORMAT_R16_SFLOAT:
        return FFX_API_SURFACE_FORMAT_R16_FLOAT;
    case VK_FORMAT_R16_UINT:
        return FFX_API_SURFACE_FORMAT_R16_UINT;
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return FFX_API_SURFACE_FORMAT_R16_UNORM;
    case VK_FORMAT_R16_SNORM:
        return FFX_API_SURFACE_FORMAT_R16_SNORM;
    case VK_FORMAT_R8_UNORM:
        return FFX_API_SURFACE_FORMAT_R8_UNORM;
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_S8_UINT:
        return FFX_API_SURFACE_FORMAT_R8_UINT;
    case VK_FORMAT_R8G8_UNORM:
        return FFX_API_SURFACE_FORMAT_R8G8_UNORM;
    case VK_FORMAT_R8G8_UINT:
        return FFX_API_SURFACE_FORMAT_R8G8_UINT;
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return FFX_API_SURFACE_FORMAT_R32_FLOAT;
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
        return FFX_API_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP;
    case VK_FORMAT_UNDEFINED:
        return FFX_API_SURFACE_FORMAT_UNKNOWN;

    default:
        // NOTE: we do not support typeless formats here
        //FFX_ASSERT_MESSAGE(false, "Format not yet supported");
        return FFX_API_SURFACE_FORMAT_UNKNOWN;
    }
}

static inline uint32_t ffxApiGetSurfaceFormatToGamma(uint32_t fmt)
{
    switch (fmt)
    {
    case (FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM):
        return FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB;
    case (FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM):
        return FFX_API_SURFACE_FORMAT_B8G8R8A8_SRGB;
    default:
        return fmt;
    }
}

static inline bool ffxApiIsDepthFormat(VkFormat fmt)
{
    switch (fmt)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;

    default:
        return false;
    }
}

static inline bool ffxApiIsStencilFormat(VkFormat fmt)
{
    switch (fmt)
    {
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;

    default:
        return false;
    }
}

static inline FfxApiResourceDescription ffxApiGetBufferResourceDescriptionVK(const VkBuffer           buffer,
                                                                             const VkBufferCreateInfo createInfo,
                                                                             uint32_t                 additionalUsages)
{
    FfxApiResourceDescription resourceDescription = {};

    // This is valid
    if (buffer == VK_NULL_HANDLE)
        return resourceDescription;

    resourceDescription.flags  = FFX_API_RESOURCE_FLAGS_NONE;
    resourceDescription.usage  = additionalUsages;
    resourceDescription.size   = (uint32_t)createInfo.size;
    resourceDescription.stride = 0;
    resourceDescription.format = FFX_API_SURFACE_FORMAT_UNKNOWN;

    if ((createInfo.usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0)
        resourceDescription.usage |= FFX_API_RESOURCE_USAGE_UAV;
    if ((createInfo.usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) != 0)
        resourceDescription.usage |= FFX_API_RESOURCE_USAGE_INDIRECT;

    // What should we initialize this to?? No case for this yet
    resourceDescription.depth    = 0;
    resourceDescription.mipCount = 0;

    // Set the type
    resourceDescription.type = FFX_API_RESOURCE_TYPE_BUFFER;

    return resourceDescription;
}

static inline FfxApiResourceDescription ffxApiGetImageResourceDescriptionVK(const VkImage image, const VkImageCreateInfo createInfo, uint32_t additionalUsages)
{
    FfxApiResourceDescription resourceDescription = {};

    // This is valid
    if (image == VK_NULL_HANDLE)
        return resourceDescription;

    // Set flags properly for resource registration
    resourceDescription.flags = FFX_API_RESOURCE_FLAGS_NONE;
    resourceDescription.usage = FFX_API_RESOURCE_USAGE_READ_ONLY;

    // Check for depth stencil use
    if (ffxApiIsDepthFormat(createInfo.format))
        resourceDescription.usage |= FFX_API_RESOURCE_USAGE_DEPTHTARGET;
    if (ffxApiIsStencilFormat(createInfo.format))
        resourceDescription.usage |= FFX_API_RESOURCE_USAGE_STENCILTARGET;

    // Unordered access use
    if ((createInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
        resourceDescription.usage |= FFX_API_RESOURCE_USAGE_UAV;

    // Resource-specific supplemental use flags
    resourceDescription.usage |= additionalUsages;

    resourceDescription.width    = createInfo.extent.width;
    resourceDescription.height   = createInfo.extent.height;
    resourceDescription.mipCount = createInfo.mipLevels;
    resourceDescription.format   = ffxApiGetSurfaceFormatVK(createInfo.format);

    // if the mutable flag is present, assume that the real format is sRGB
    if ((createInfo.flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) != 0)
        resourceDescription.format = ffxApiGetSurfaceFormatToGamma(resourceDescription.format);

    switch (createInfo.imageType)
    {
    case VK_IMAGE_TYPE_1D:
        resourceDescription.type = FFX_API_RESOURCE_TYPE_TEXTURE1D;
        break;
    case VK_IMAGE_TYPE_2D:
        resourceDescription.depth = createInfo.arrayLayers;
        if ((additionalUsages & FFX_API_RESOURCE_USAGE_ARRAYVIEW) != 0)
            resourceDescription.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
        else if ((createInfo.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0)
            resourceDescription.type = FFX_API_RESOURCE_TYPE_TEXTURE_CUBE;
        else
            resourceDescription.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
        break;
    case VK_IMAGE_TYPE_3D:
        resourceDescription.depth = createInfo.extent.depth;
        resourceDescription.type  = FFX_API_RESOURCE_TYPE_TEXTURE3D;
        break;
    default:
        //FFX_ASSERT_MESSAGE(false, "FFXInterface: VK: Unsupported texture dimension requested. Please implement.");
        break;
    }

    return resourceDescription;
}

static inline FfxApiResource ffxApiGetResourceVK(void* vkResource, FfxApiResourceDescription ffxResDescription, uint32_t state)
{
    FfxApiResource resource = {};
    resource.resource       = vkResource;
    resource.state          = state;
    resource.description    = ffxResDescription;

    return resource;
}
