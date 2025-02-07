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

#if defined(_VK)

// _VK define implies windows
#include "core/win/framework_win.h"
#include "misc/assert.h"

#include "render/vk/commandlist_vk.h"
#include "render/vk/device_vk.h"
#include "render/vk/gpuresource_vk.h"
#include "render/vk/helpers.h"
#include "render/vk/resourceviewallocator_vk.h"
#include "render/vk/swapchain_vk.h"
#include "render/vk/texture_vk.h"

// To dump screenshot to file
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <unordered_map>
#include <vulkan/vulkan_win32.h>

using namespace std::experimental;

namespace cauldron
{
    struct SwapChainSupportDetails {
        VkSurfaceCapabilities2KHR capabilities2 = {};
        VkPhysicalDeviceSurfaceInfo2KHR physicalDeviceSurfaceInfo2 = {};
        VkDisplayNativeHdrSurfaceCapabilitiesAMD displayNativeHdrSurfaceCapabilitiesAMD = {};
        VkSwapchainDisplayNativeHdrCreateInfoAMD swapchainDisplayNativeHdrCreateInfoAMD = {};
        std::vector<VkSurfaceFormat2KHR> formats2;
        std::vector<VkPresentModeKHR> presentModes;
    };

    void QuerySwapChainSupport(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR surface, SwapChainSupportDetails& details)
    {
        details.physicalDeviceSurfaceInfo2.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
        details.physicalDeviceSurfaceInfo2.pNext   = nullptr;
        details.physicalDeviceSurfaceInfo2.surface = surface;

        DeviceInternal* pDevice = GetDevice()->GetImpl();

        bool isAMD = (wcsstr(pDevice->GetDeviceName(), L"AMD") != nullptr);

        details.capabilities2.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
        details.capabilities2.pNext = nullptr;

        if (isAMD)
        {
            // checking Local dimming support
            details.capabilities2.pNext = &details.displayNativeHdrSurfaceCapabilitiesAMD;

            details.displayNativeHdrSurfaceCapabilitiesAMD.sType = VK_STRUCTURE_TYPE_DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD;
            details.displayNativeHdrSurfaceCapabilitiesAMD.pNext = nullptr;
        }

        // Calling this to check surface capabilities, and Local dimming support (AMD only)
        pDevice->GetPhysicalDeviceSurfaceCapabilities2KHR()(PhysicalDevice, &details.physicalDeviceSurfaceInfo2, &details.capabilities2);

        if (isAMD)
        {
            details.swapchainDisplayNativeHdrCreateInfoAMD.sType              = VK_STRUCTURE_TYPE_SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD;
            details.swapchainDisplayNativeHdrCreateInfoAMD.pNext              = nullptr;
            details.swapchainDisplayNativeHdrCreateInfoAMD.localDimmingEnable = details.displayNativeHdrSurfaceCapabilitiesAMD.localDimmingSupport;
        }

        uint32_t formatCount;
        pDevice->GetPhysicalDeviceSurfaceFormats2()(PhysicalDevice, &details.physicalDeviceSurfaceInfo2, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats2.resize(formatCount);
            for(auto& surfaceFormat : details.formats2)
            {
                surfaceFormat.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
                surfaceFormat.pNext = nullptr;
            }
            pDevice->GetPhysicalDeviceSurfaceFormats2()(PhysicalDevice, &details.physicalDeviceSurfaceInfo2, &formatCount, details.formats2.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, surface, &presentModeCount, details.presentModes.data());
        }
    }

    std::unordered_map<DisplayMode, VkSurfaceFormatKHR> GetAvailableFormats(const std::vector<VkSurfaceFormat2KHR>& surfaceFormats2, VkFormat preferredFormat)
    {
        std::unordered_map<DisplayMode, VkSurfaceFormatKHR> modes;

        // small utility
        auto addSurfaceFormatToMode = [preferredFormat, &modes](const VkSurfaceFormatKHR surfaceFormat, DisplayMode mode, VkFormat expectedFormat) {
            if (surfaceFormat.format == preferredFormat)
            {
                modes[mode] = surfaceFormat;
            }
            else if (surfaceFormat.format == expectedFormat)
            {
                auto found = modes.find(mode);
                if (found == modes.end() || found->second.format != preferredFormat)
                {
                    // add only if the preferred format hasn't been added yet
                    modes[mode] = surfaceFormat;
                }
            }
        };

        for (const auto& surfaceFormat2 : surfaceFormats2)
        {
            VkSurfaceFormatKHR surfaceFormat     = surfaceFormat2.surfaceFormat;
            bool               isPreferredFormat = (surfaceFormat.format == preferredFormat);

            if (surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                addSurfaceFormatToMode(surfaceFormat, DisplayMode::DISPLAYMODE_LDR, VK_FORMAT_R8G8B8A8_UNORM);
                addSurfaceFormatToMode(surfaceFormat, DisplayMode::DISPLAYMODE_LDR, VK_FORMAT_B8G8R8A8_UNORM);
            }
            else if (surfaceFormat.colorSpace == VK_COLOR_SPACE_DISPLAY_NATIVE_AMD)
            {
                // no override possible here because colorspace and format are linked
                if (surfaceFormat.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
                    modes[DisplayMode::DISPLAYMODE_FSHDR_2084] = surfaceFormat;
                else if (surfaceFormat.format == VK_FORMAT_R16G16B16A16_SFLOAT)
                    modes[DisplayMode::DISPLAYMODE_FSHDR_SCRGB] = surfaceFormat;
            }
            else if (surfaceFormat.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
            {
                addSurfaceFormatToMode(surfaceFormat, DisplayMode::DISPLAYMODE_HDR10_2084, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
            }
            else if (surfaceFormat.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
            {
                addSurfaceFormatToMode(surfaceFormat, DisplayMode::DISPLAYMODE_HDR10_SCRGB, VK_FORMAT_R16G16B16A16_SFLOAT);
            }
        }

        return modes;
    }

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool vsync)
    {
        auto search = [&availablePresentModes](VkPresentModeKHR Mode) {
            return std::find(availablePresentModes.begin(), availablePresentModes.end(), Mode) != availablePresentModes.end();
        };

        if (vsync)
        {
            if (search(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) // adaptive vsync
            {
                return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            }
            else if (search(VK_PRESENT_MODE_MAILBOX_KHR))
            {
                return VK_PRESENT_MODE_MAILBOX_KHR;
            }
            else
            {
                return VK_PRESENT_MODE_FIFO_KHR;  // FIFO_KHR is guaranteed to exist
            }
        }
        else
        {
            if (search(VK_PRESENT_MODE_IMMEDIATE_KHR))
            {
                return VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
            else
            {
                return VK_PRESENT_MODE_FIFO_KHR;  // as a last resort
            }
        }
    }

    VkSurfaceTransformFlagBitsKHR ChooseSurfaceTransform(const VkSurfaceCapabilitiesKHR capabilities)
    {
        return (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
    }

    VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(const VkSurfaceCapabilitiesKHR capabilities)
    {
        VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        // in the order of preference
        VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[4] =
        {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };

        for (uint32_t i = 0; i < sizeof(compositeAlphaFlags); i++)
        {
            if (capabilities.supportedCompositeAlpha & compositeAlphaFlags[i])
            {
                compositeAlpha = compositeAlphaFlags[i];
                break;
            }
        }

        return compositeAlpha;
    }

    ResourceFormat ConvertFormat(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_UNDEFINED:
            return ResourceFormat::Unknown;

        // 16-bit
        case VK_FORMAT_R16_SFLOAT:
            return ResourceFormat::R16_FLOAT;

        // 32-bit
        case VK_FORMAT_R8G8B8A8_UNORM:
            return ResourceFormat::RGBA8_UNORM;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return ResourceFormat::BGRA8_UNORM;
        case VK_FORMAT_R8G8B8A8_SNORM:
            return ResourceFormat::RGBA8_SNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return ResourceFormat::RGBA8_SRGB;
        case VK_FORMAT_B8G8R8A8_SRGB:
            return ResourceFormat::BGRA8_SRGB;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            return ResourceFormat::RGB10A2_UNORM;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
            return ResourceFormat::RGB9E5_SHAREDEXP;
        case VK_FORMAT_R16G16_SFLOAT:
            return ResourceFormat::RG16_FLOAT;
        case VK_FORMAT_R32_SFLOAT:
            return ResourceFormat::R32_FLOAT;

        // 64-bit
        case VK_FORMAT_R16G16B16A16_UNORM:
            return ResourceFormat::RGBA16_UNORM;
        case VK_FORMAT_R16G16B16A16_SNORM:
            return ResourceFormat::RGBA16_SNORM;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return ResourceFormat::RGBA16_FLOAT;
        case VK_FORMAT_R32G32_SFLOAT:
            return ResourceFormat::RG32_FLOAT;

        // 128-bit
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return ResourceFormat::RGBA32_FLOAT;

        // compressed
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            return ResourceFormat::BC1_UNORM;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            return ResourceFormat::BC1_SRGB;
        case VK_FORMAT_BC2_UNORM_BLOCK:
            return ResourceFormat::BC2_UNORM;
        case VK_FORMAT_BC2_SRGB_BLOCK:
            return ResourceFormat::BC2_SRGB;
        case VK_FORMAT_BC3_UNORM_BLOCK:
            return ResourceFormat::BC3_UNORM;
        case VK_FORMAT_BC3_SRGB_BLOCK:
            return ResourceFormat::BC3_SRGB;
        case VK_FORMAT_BC4_UNORM_BLOCK:
            return ResourceFormat::BC4_UNORM;
        case VK_FORMAT_BC4_SNORM_BLOCK:
            return ResourceFormat::BC4_SNORM;
        case VK_FORMAT_BC5_UNORM_BLOCK:
            return ResourceFormat::BC5_UNORM;
        case VK_FORMAT_BC5_SNORM_BLOCK:
            return ResourceFormat::BC5_SNORM;
        case VK_FORMAT_BC7_UNORM_BLOCK:
            return ResourceFormat::BC7_UNORM;
        case VK_FORMAT_BC7_SRGB_BLOCK:
            return ResourceFormat::BC7_SRGB;

        default:
            CauldronError(L"Cannot convert unknown format.");
            return ResourceFormat::Unknown;
        };
    }

    //////////////////////////////////////////////////////////////////////////
    // SwapChain
    SwapChain* SwapChain::CreateSwapchain()
    {
        return new SwapChainInternal();
    }

    SwapChainInternal::SwapChainInternal()
        : SwapChain()
    {
        // Will need config settings to initialize the swapchain
        const CauldronConfig* pConfig = GetConfig();

        m_VSyncEnabled = pConfig->Vsync;

        DeviceInternal* pDevice = GetDevice()->GetImpl();

        // create semaphores to acquire the swapchain images
        m_ImageAvailableSemaphores.resize(pConfig->BackBufferCount + 1);
        VkSemaphoreCreateInfo info = {};
        info.sType             = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.flags             = 0;
        for (uint8_t i = 0; i < pConfig->BackBufferCount + 1; ++i)
        {
            VkResult res = vkCreateSemaphore(pDevice->VKDevice(), &info, nullptr, &m_ImageAvailableSemaphores[i]);
            CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS, L"Unable to create semaphore to acquire swapchain images");

            char buf[48];
            snprintf(buf, 48 * sizeof(char), "CauldronImageAcquireSemaphore %d", i);
            pDevice->SetResourceName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)(m_ImageAvailableSemaphores[i]), buf);
        }

        // create the swapchain
        CreateSwapChain(pConfig->Width, pConfig->Height);

        // create the rendertargets
        CreateSwapChainRenderTargets();
    }

    SwapChainInternal::~SwapChainInternal()
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();

        for (VkSemaphore semaphore : m_ImageAvailableSemaphores)
            vkDestroySemaphore(pDevice->VKDevice(), semaphore, nullptr);
        m_ImageAvailableSemaphores.clear();

        pDevice->DestroySwapchainKHR(m_SwapChain, nullptr);
    }

    void SwapChainInternal::CreateSwapChain(uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;
        m_CurrentVSync = m_VSyncEnabled;

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        const CauldronConfig* pConfig = GetConfig();

        m_BackBufferFences.resize(pConfig->BackBufferCount);
        for (uint8_t i = 0; i < pConfig->BackBufferCount; ++i)
            m_BackBufferFences[i] = 0;

        // query the swapchain capabilities to find the correct format and the correct present mode
        SwapChainSupportDetails swapChainSupport = {};
        QuerySwapChainSupport(pDevice->VKPhysicalDevice(), pDevice->GetSurface(), swapChainSupport);;

        // Find all HDR modes supported by current display and pick surface format
        EnumerateDisplayModesAndFormats(swapChainSupport.formats2);

        SwapChainCreationParams swapchainCreationParams                   = {};
        swapchainCreationParams.swapchainCreateInfo                       = {};
        swapchainCreationParams.swapchainCreateInfo.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreationParams.swapchainCreateInfo.pNext                 = wcsstr(pDevice->GetDeviceName(), L"AMD") != nullptr ? &swapChainSupport.swapchainDisplayNativeHdrCreateInfoAMD : nullptr;
        swapchainCreationParams.swapchainCreateInfo.flags                 = 0;
        swapchainCreationParams.swapchainCreateInfo.surface               = pDevice->GetSurface();
        swapchainCreationParams.swapchainCreateInfo.imageFormat           = m_SurfaceFormat.format;
        swapchainCreationParams.swapchainCreateInfo.minImageCount         = pConfig->BackBufferCount;
        swapchainCreationParams.swapchainCreateInfo.imageColorSpace       = m_SurfaceFormat.colorSpace;
        swapchainCreationParams.swapchainCreateInfo.imageExtent.width     = m_Width;
        swapchainCreationParams.swapchainCreateInfo.imageExtent.height    = m_Height;
        swapchainCreationParams.swapchainCreateInfo.imageArrayLayers      = 1;
        swapchainCreationParams.swapchainCreateInfo.imageUsage            = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                                            VK_IMAGE_USAGE_SAMPLED_BIT;  // render to texture, copy and shader access
        swapchainCreationParams.swapchainCreateInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreationParams.swapchainCreateInfo.queueFamilyIndexCount = 0;
        swapchainCreationParams.swapchainCreateInfo.pQueueFamilyIndices   = nullptr;
        swapchainCreationParams.swapchainCreateInfo.preTransform          = ChooseSurfaceTransform(swapChainSupport.capabilities2.surfaceCapabilities);
        swapchainCreationParams.swapchainCreateInfo.compositeAlpha        = ChooseCompositeAlpha(swapChainSupport.capabilities2.surfaceCapabilities);
        swapchainCreationParams.swapchainCreateInfo.presentMode           = ChooseSwapPresentMode(swapChainSupport.presentModes, m_VSyncEnabled);
        swapchainCreationParams.swapchainCreateInfo.clipped               = true;
        swapchainCreationParams.swapchainCreateInfo.oldSwapchain          = VK_NULL_HANDLE;
        
        SwapChain* pSwapchain = this;
        pDevice->CreateSwapChain(pSwapchain, swapchainCreationParams, CommandQueue::Graphics);

        // TODO: fix that. Keep all structures in pNext
        m_CreateInfo = swapchainCreationParams.swapchainCreateInfo;
        m_CreateInfo.pNext = nullptr;

        // Can only do this for Freesync Premium Pro HDR display on AMD hardware
        if (wcsstr(pDevice->GetDeviceName(), L"AMD") != nullptr)
        {
            EnumerateHDRMetadata(pDevice->VKPhysicalDevice(), swapChainSupport.physicalDeviceSurfaceInfo2, swapChainSupport.capabilities2);
        }

        // Set primaies based on displaymode
        PopulateHDRMetadataBasedOnDisplayMode();

        SetHDRMetadataAndColorspace();
    }

    void SwapChainInternal::OnResize(uint32_t width, uint32_t height)
    {
        DestroySwapChainRenderTargets();

        // delete the swapchain
        if (m_SwapChain != VK_NULL_HANDLE)
        {
            GetDevice()->GetImpl()->DestroySwapchainKHR(m_SwapChain, nullptr);
            m_SwapChain = VK_NULL_HANDLE;
        }

        // recreate the swapchain
        CreateSwapChain(width, height);

        CreateSwapChainRenderTargets();
    }

    void SwapChainInternal::CreateSwapChainRenderTargets()
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        const CauldronConfig* pConfig = GetConfig();

        // we are querying the swapchain count so the next call doesn't generate a validation warning
        uint32_t backBufferCount;
        VkResult res = pDevice->GetSwapchainImagesKHR(m_SwapChain, &backBufferCount, NULL);
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS, L"Unable to get the swapchain images");

        CauldronAssert(ASSERT_CRITICAL, backBufferCount == pConfig->BackBufferCount, L"Requested swapchain images count is different that the available ones");

        std::vector<VkImage> images(backBufferCount);
        res = pDevice->GetSwapchainImagesKHR(m_SwapChain, &backBufferCount, images.data());
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS, L"Unable to get the swapchain images");

        std::vector<GPUResource*> gpuResourceArray;
        gpuResourceArray.reserve(backBufferCount);

        // create a fake VkImageCreateInfo to put in the resource
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.pNext = nullptr;
        imageInfo.flags = 0;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = m_SurfaceFormat.format;
        imageInfo.extent.width = m_Width;
        imageInfo.extent.height = m_Height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        ResourceFormat format = ConvertFormat(m_SurfaceFormat.format);

        for (uint32_t i = 0; i < backBufferCount; ++i)
        {
            std::wstring name = L"BackBuffer ";
            name += std::to_wstring(i);
            pDevice->SetResourceName(VK_OBJECT_TYPE_IMAGE, (uint64_t)images[i], name.c_str());

            GPUResourceInitParams initParams = {};
            initParams.imageInfo = imageInfo;
            initParams.image = images[i];
            initParams.type = GPUResourceType::Swapchain;

            GPUResource* pResource = GPUResource::CreateGPUResource(name.c_str(), this, ResourceState::Present, &initParams, true);
            gpuResourceArray.push_back(pResource);
        }

        TextureDesc rtDesc = TextureDesc::Tex2D(L"BackBuffer", format, m_Width, m_Height, 1, 1, ResourceFlags::AllowRenderTarget);
        if (m_pRenderTarget == nullptr)
            m_pRenderTarget = new SwapChainRenderTarget(&rtDesc, gpuResourceArray);
        else
            m_pRenderTarget->Update(&rtDesc, gpuResourceArray);

        // Get the views
        CauldronAssert(ASSERT_CRITICAL, m_pSwapChainRTV == nullptr || m_pSwapChainRTV->GetCount() == backBufferCount, L"SwapChain RTV has a wrong size");
        if (m_pSwapChainRTV == nullptr)
            GetResourceViewAllocator()->AllocateCPURenderViews(&m_pSwapChainRTV, backBufferCount);
        for (uint32_t i = 0; i < backBufferCount; ++i)
            m_pSwapChainRTV->BindTextureResource(m_pRenderTarget->GetResource(i), m_pRenderTarget->GetDesc(), ResourceViewType::RTV, ViewDimension::Texture2D, 0, 1, 0, i);
    }

    void SwapChainInternal::WaitForSwapChain()
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();

        if (m_VSyncEnabled != m_CurrentVSync)
        {
            GetDevice()->FlushAllCommandQueues();

            // call OnResize to recreate the swapchain
            OnResize(m_Width, m_Height);
        }

        // get the next image in the swapchain
        uint32_t imageIndex;
        m_ImageAvailableSemaphoreIndex      = (m_ImageAvailableSemaphoreIndex + 1) % m_ImageAvailableSemaphores.size();
        VkSemaphore imageAvailableSemaphore = m_ImageAvailableSemaphores[m_ImageAvailableSemaphoreIndex];
        VkResult    res                     = pDevice->AcquireNextImageKHR(m_SwapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        if (res != VK_SUCCESS)
        {
            // Flush everything before resizing resources (can't have anything in the pipes)
            CauldronAssert(ASSERT_ERROR,
                            std::this_thread::get_id() == GetFramework()->MainThreadID(),
                            L"Cauldron: OnResize: Expecting OnResize to be called on MainThread. Not thread safe!");
            GetDevice()->FlushAllCommandQueues();

            // Resize swapchain (only takes display resolution)
            GetSwapChain()->OnResize(GetFramework()->GetResolutionInfo().DisplayWidth, GetFramework()->GetResolutionInfo().DisplayHeight);

            // Trigger a resize event for the framework
            GetFramework()->ResizeEvent();
            return;
        }

        m_CurrentBackBuffer = static_cast<uint8_t>(imageIndex);

        // wait for the last submission to the queue fo finish
        pDevice->WaitOnQueue(m_BackBufferFences[m_CurrentBackBuffer], CommandQueue::Graphics);
        
        // the command lists will wait for the swapchain image to be available

        m_pRenderTarget->SetCurrentBackBufferIndex(imageIndex);

        // Note that in Vulkan, swapchain images are in an undefined state after being acquired
    }

    void SwapChainInternal::Present()
    {
        uint64_t waitValue = GetDevice()->PresentSwapChain(this);
        m_BackBufferFences[m_CurrentBackBuffer] = waitValue;
    }

    void SwapChainInternal::DumpSwapChainToFile(filesystem::path filePath)
    {
        const VkImageCreateInfo swapchainImageInfo = m_pRenderTarget->GetCurrentResource()->GetImpl()->GetImageCreateInfo();

        // create destination image
        VkImageCreateInfo imageCreateInfo     = {};
        imageCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext                 = nullptr;
        imageCreateInfo.flags                 = 0;
        imageCreateInfo.imageType             = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format                = swapchainImageInfo.format;
        imageCreateInfo.extent                = swapchainImageInfo.extent;
        imageCreateInfo.mipLevels             = 1;
        imageCreateInfo.arrayLayers           = 1;
        imageCreateInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling                = VK_IMAGE_TILING_LINEAR;
        imageCreateInfo.usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.queueFamilyIndexCount = 0;
        imageCreateInfo.pQueueFamilyIndices   = nullptr;
        imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
        
        VmaAllocationCreateInfo allocationCreateInfo = {};
        allocationCreateInfo.usage                   = VMA_MEMORY_USAGE_UNKNOWN;
        allocationCreateInfo.requiredFlags           = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        VkImage       image      = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkResult res = vmaCreateImage(pDevice->GetVmaAllocator(), &imageCreateInfo, &allocationCreateInfo, &image, &allocation, nullptr);
        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Unable to create buffer for dumping swapchain");

        CommandList* pCmdList = pDevice->CreateCommandList(L"SwapchainToFileCL", CommandQueue::Graphics);
        
        // transition swapchain and dest image
        VkImageMemoryBarrier imageBarriers[2];
        // swapchain
        imageBarriers[0].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarriers[0].pNext                           = nullptr;
        imageBarriers[0].srcAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
        imageBarriers[0].dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarriers[0].oldLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        imageBarriers[0].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageBarriers[0].srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        imageBarriers[0].dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        imageBarriers[0].image                           = m_pRenderTarget->GetCurrentResource()->GetImpl()->GetImage();
        imageBarriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarriers[0].subresourceRange.baseMipLevel   = 0;
        imageBarriers[0].subresourceRange.levelCount     = 1;
        imageBarriers[0].subresourceRange.baseArrayLayer = 0;
        imageBarriers[0].subresourceRange.layerCount     = 1;
        // dest image
        imageBarriers[1].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarriers[1].pNext                           = nullptr;
        imageBarriers[1].srcAccessMask                   = 0;
        imageBarriers[1].dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarriers[1].oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarriers[1].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarriers[1].srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        imageBarriers[1].dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        imageBarriers[1].image                           = image;
        imageBarriers[1].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarriers[1].subresourceRange.baseMipLevel   = 0;
        imageBarriers[1].subresourceRange.levelCount     = 1;
        imageBarriers[1].subresourceRange.baseArrayLayer = 0;
        imageBarriers[1].subresourceRange.layerCount     = 1;

        vkCmdPipelineBarrier(
            pCmdList->GetImpl()->VKCmdBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, imageBarriers);
        
        VkImageCopy copyRegion                   = {};
        copyRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.mipLevel       = 0;
        copyRegion.srcSubresource.layerCount     = 1;
        copyRegion.srcSubresource.baseArrayLayer = 0;
        copyRegion.srcOffset.x                   = 0;
        copyRegion.srcOffset.y                   = 0;
        copyRegion.srcOffset.z                   = 0;
        copyRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.mipLevel       = 0;
        copyRegion.dstSubresource.layerCount     = 1;
        copyRegion.dstSubresource.baseArrayLayer = 0;
        copyRegion.dstOffset.x                   = 0;
        copyRegion.dstOffset.y                   = 0;
        copyRegion.dstOffset.z                   = 0;
        copyRegion.extent                        = imageCreateInfo.extent;
        vkCmdCopyImage(pCmdList->GetImpl()->VKCmdBuffer(),
                       m_pRenderTarget->GetCurrentResource()->GetImpl()->GetImage(),
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1,
                       &copyRegion);
        
        // transition swapchain back to present
        imageBarriers[0].srcAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
        imageBarriers[0].dstAccessMask                   = VK_ACCESS_MEMORY_READ_BIT;
        imageBarriers[0].oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageBarriers[0].newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // transition dest image
        imageBarriers[1].srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarriers[1].dstAccessMask                   = VK_ACCESS_MEMORY_READ_BIT;
        imageBarriers[1].oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarriers[1].newLayout                       = VK_IMAGE_LAYOUT_GENERAL;

        vkCmdPipelineBarrier(
            pCmdList->GetImpl()->VKCmdBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, imageBarriers);


        CloseCmdList(pCmdList);

        std::vector<CommandList*> lists(1);
        lists[0] = pCmdList;
        pDevice->ExecuteCommandListsImmediate(lists, CommandQueue::Graphics);

        void* pData = nullptr;
        res = vmaMapMemory(pDevice->GetVmaAllocator(), allocation, &pData);
        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Unable to map buffer for dumping swapchain");
        stbi_write_jpg(WStringToString(filePath.c_str()).c_str(), (int)swapchainImageInfo.extent.width, (int)swapchainImageInfo.extent.height, 4, pData, 100);
        vmaUnmapMemory(pDevice->GetVmaAllocator(), allocation);

        // Destroy resources
        vmaDestroyImage(pDevice->GetVmaAllocator(), image, allocation);
        delete pCmdList;
    }

    void SwapChainInternal::EnumerateDisplayModesAndFormats(const std::vector<VkSurfaceFormat2KHR>& formats2)
    {
        m_SupportedDisplayModes.clear();

        const CauldronConfig* pConfig = GetConfig();

        // get the requested override swapchain format
        VkFormat overrideFormat = VK_FORMAT_UNDEFINED;
        if (pConfig->SwapChainFormat != ResourceFormat::Unknown)
        {
            overrideFormat = GetVkFormat(pConfig->SwapChainFormat);
        }

        std::unordered_map<DisplayMode, VkSurfaceFormatKHR> modes = GetAvailableFormats(formats2, overrideFormat);

        for (const auto& it : modes)
        {
            m_SupportedDisplayModes.push_back(it.first);
        }

        // See if display mode requested in config is supported and return it, or default to LDR
        m_CurrentDisplayMode = CheckAndGetDisplayModeRequested(pConfig->CurrentDisplayMode);

        auto found = modes.find(m_CurrentDisplayMode);

        CauldronAssert(ASSERT_CRITICAL, found != modes.end(), L"Unable to find a suitable swapchain format.");
        m_CurrentDisplayMode = found->first;
        m_SurfaceFormat      = found->second;

        CauldronAssert(ASSERT_WARNING,
                       overrideFormat == VK_FORMAT_UNDEFINED || m_SurfaceFormat.format == overrideFormat,
                       L"The requested swapchain format from the config file cannot be used for present/display. Override is ignored.");
        
        // Set format based on display mode
        m_SwapChainFormat = ConvertFormat(m_SurfaceFormat.format);
    }

    void SwapChainInternal::EnumerateHDRMetadata(VkPhysicalDevice                 PhysicalDevice,
                                                 VkPhysicalDeviceSurfaceInfo2KHR& physicalDeviceSurfaceInfo2,
                                                 VkSurfaceCapabilities2KHR&       capabilities2)
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();

        VkHdrMetadataEXT hdrMetadata = {};
        hdrMetadata.sType            = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
        hdrMetadata.pNext            = nullptr;

        VkDisplayNativeHdrSurfaceCapabilitiesAMD* displayNativeHdrSurfaceCapabilitiesAMD = reinterpret_cast<VkDisplayNativeHdrSurfaceCapabilitiesAMD*>(capabilities2.pNext);
        displayNativeHdrSurfaceCapabilitiesAMD->pNext = &hdrMetadata;

        // Must requry FS HDR display capabilities
        // After VkSwapchainDisplayNativeHdrCreateInfoAMD is set through swapchain creation when attached to swapchain pnext.
        // This fills in data into VkHdrMetadataEXT attached as pNext in VkSurfaceCapabilities2KHR->VkDisplayNativeHdrSurfaceCapabilitiesAMD
        pDevice->GetPhysicalDeviceSurfaceCapabilities2KHR()(PhysicalDevice, &physicalDeviceSurfaceInfo2, &capabilities2);

        m_HDRMetadata.RedPrimary[0]   = hdrMetadata.displayPrimaryRed.x;
        m_HDRMetadata.RedPrimary[1]   = hdrMetadata.displayPrimaryRed.y;
        m_HDRMetadata.GreenPrimary[0] = hdrMetadata.displayPrimaryGreen.x;
        m_HDRMetadata.GreenPrimary[1] = hdrMetadata.displayPrimaryGreen.y;
        m_HDRMetadata.BluePrimary[0]  = hdrMetadata.displayPrimaryBlue.x;
        m_HDRMetadata.BluePrimary[1]  = hdrMetadata.displayPrimaryBlue.y;
        m_HDRMetadata.WhitePoint[0]   = hdrMetadata.whitePoint.x;
        m_HDRMetadata.WhitePoint[1]   = hdrMetadata.whitePoint.y;
        m_HDRMetadata.MinLuminance    = hdrMetadata.minLuminance;
        m_HDRMetadata.MaxLuminance    = hdrMetadata.maxLuminance;
    }

    void SwapChainInternal::SetHDRMetadataAndColorspace()
    {
        VkHdrMetadataEXT hdrMetadata = {};
        hdrMetadata.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
        hdrMetadata.pNext = nullptr;

        hdrMetadata.displayPrimaryRed.x = m_HDRMetadata.RedPrimary[0];
        hdrMetadata.displayPrimaryRed.y = m_HDRMetadata.RedPrimary[1];
        hdrMetadata.displayPrimaryGreen.x = m_HDRMetadata.GreenPrimary[0];
        hdrMetadata.displayPrimaryGreen.y = m_HDRMetadata.GreenPrimary[1];
        hdrMetadata.displayPrimaryBlue.x = m_HDRMetadata.BluePrimary[0];
        hdrMetadata.displayPrimaryBlue.y = m_HDRMetadata.BluePrimary[1];
        hdrMetadata.whitePoint.x = m_HDRMetadata.WhitePoint[0];
        hdrMetadata.whitePoint.y = m_HDRMetadata.WhitePoint[1];

        hdrMetadata.maxLuminance = m_HDRMetadata.MaxLuminance;
        hdrMetadata.minLuminance = m_HDRMetadata.MinLuminance;

        hdrMetadata.maxContentLightLevel = m_HDRMetadata.MaxContentLightLevel;
        hdrMetadata.maxFrameAverageLightLevel = m_HDRMetadata.MaxFrameAverageLightLevel;

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        pDevice->GetSetHdrMetadata()(pDevice->VKDevice(), 1, &m_SwapChain, &hdrMetadata);
    }

    void SwapChainInternal::GetLastPresentCount(UINT* pLastPresentCount)
    {
        *pLastPresentCount = static_cast<UINT>(GetDevice()->GetImpl()->GetLastPresentCountFFX(m_SwapChain));
    }

    #include <dwmapi.h>
    #pragma comment(lib, "Dwmapi.lib")

    void SwapChainInternal::GetRefreshRate(double* outRefreshRate)
    {
        double dwsRate  = 1000.0;
        *outRefreshRate = 1000.0;

        bool bIsPotentialDirectFlip = false;
        bool isFullscreen = (GetFramework()->GetImpl()->GetPresentationMode() == PRESENTATIONMODE_BORDERLESS_FULLSCREEN);

        if (!isFullscreen)
        {
            DWM_TIMING_INFO compositionTimingInfo{};
            compositionTimingInfo.cbSize = sizeof(DWM_TIMING_INFO);
            double  monitorRefreshRate   = 0.0f;
            HRESULT hr                   = DwmGetCompositionTimingInfo(nullptr, &compositionTimingInfo);
            if (SUCCEEDED(hr))
            {
                dwsRate         = double(compositionTimingInfo.rateRefresh.uiNumerator) / compositionTimingInfo.rateRefresh.uiDenominator;
                *outRefreshRate = dwsRate;
            }
        }

        // if FS this should be the monitor used for FS, in windowed the window containing the main portion of the output
        {
            HMONITOR monitor = MonitorFromWindow(GetFramework()->GetImpl()->GetHWND(), MONITOR_DEFAULTTONEAREST);

            MONITORINFOEXW info{};
            info.cbSize = sizeof(info);
            if (GetMonitorInfoW(monitor, &info) != 0)
            {
                bIsPotentialDirectFlip = false;
                //isDirectFlip(swapChain, info.rcMonitor);  // TODO: check if the window is fully covering the monitor

                UINT32 numPathArrayElements, numModeInfoArrayElements;
                if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, &numModeInfoArrayElements) == ERROR_SUCCESS)
                {
                    DISPLAYCONFIG_PATH_INFO pathArray[8];
                    DISPLAYCONFIG_MODE_INFO modeInfoArray[32];

                    CauldronAssert(ASSERT_CRITICAL, _countof(pathArray) >= numPathArrayElements, L"Too many elements");
                    CauldronAssert(ASSERT_CRITICAL, _countof(modeInfoArray) >= numModeInfoArrayElements, L"Too many elements");

                    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, pathArray, &numModeInfoArrayElements, modeInfoArray, nullptr) ==
                        ERROR_SUCCESS)
                    {
                        bool rateFound = false;
                        // iterate through all the paths until find the exact source to match
                        for (size_t i = 0; i < numPathArrayElements && !rateFound; i++)
                        {
                            const DISPLAYCONFIG_PATH_INFO&   path = pathArray[i];
                            DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
                            sourceName.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(sourceName), path.sourceInfo.adapterId, path.sourceInfo.id};

                            if (DisplayConfigGetDeviceInfo(&sourceName.header) == ERROR_SUCCESS)
                            {
                                if (wcscmp(info.szDevice, sourceName.viewGdiDeviceName) == 0)
                                {
                                    // compute scanout rate using horizontal rate, as its fixed even in VRR
                                    auto& mode          = modeInfoArray[path.targetInfo.desktopModeInfoIdx];
                                    auto& signalInfo    = mode.targetMode.targetVideoSignalInfo;
                                    auto  hSyncFreq     = signalInfo.hSyncFreq;
                                    auto  scanlineCount = signalInfo.totalSize.cy;

                                    if (hSyncFreq.Denominator > 0 && scanlineCount > 0)
                                    {
                                        double refrate = (hSyncFreq.Numerator / (double)hSyncFreq.Denominator) / scanlineCount;

                                        *outRefreshRate = refrate;

                                        // we found a valid rate?
                                        rateFound = (refrate > 0.0);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!bIsPotentialDirectFlip)
            {
                *outRefreshRate = std::min(*outRefreshRate, dwsRate);
            }
        }
    }

} // namespace cauldron

#endif // #if defined(_VK)
