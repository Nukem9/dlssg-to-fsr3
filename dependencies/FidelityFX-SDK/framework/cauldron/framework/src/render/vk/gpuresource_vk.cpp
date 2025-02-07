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

#include "render/vk/gpuresource_vk.h"   // This needs to come first or we get a linker error (that is very weird)
#include "render/vk/device_vk.h"

#include "render/buffer.h"
#include "render/texture.h"

#include "core/framework.h"
#include "misc/assert.h"
#include "helpers.h"
#include "FidelityFX/host/ffx_types.h"

namespace cauldron
{
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(pDevice->VKPhysicalDevice(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        CauldronWarning(L"failed to find suitable memory type!");
        return -1;
    }

    GPUResource* GPUResource::CreateGPUResource(const wchar_t* resourceName, void* pOwner, ResourceState initialState, void* pInitParams, bool resizable /* = false */)
    {
        GPUResourceInitParams* pParams = (GPUResourceInitParams*)pInitParams;
        switch (pParams->type)
        {
        case GPUResourceType::Texture:
            return new GPUResourceInternal(pParams->imageInfo, initialState, resourceName, pOwner, resizable);
        case GPUResourceType::Buffer:
            return new GPUResourceInternal(pParams->bufferInfo, pParams->memoryUsage, resourceName, pOwner,
                                            initialState, resizable, pParams->alignment);
        case GPUResourceType::BufferBreadcrumbs:
            return new GPUResourceInternal(pParams->bufferInfo.size, pOwner, initialState, resourceName);
        case GPUResourceType::Swapchain:
            return new GPUResourceInternal(pParams->image, pParams->imageInfo, resourceName, initialState, resizable);
        default:
            CauldronCritical(L"Unsupported GPUResourceType creation requested");
            break;
        }

        return nullptr;
    }


    
VkFormat GetVkFormatFromSurfaceFormat(FfxSurfaceFormat fmt)
    {
        switch (fmt)
        {
        case (FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS):
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case (FFX_SURFACE_FORMAT_R32G32B32A32_UINT):
            return VK_FORMAT_R32G32B32A32_UINT;
        case (FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT):
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case (FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT):
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case (FFX_SURFACE_FORMAT_R32G32_FLOAT):
            return VK_FORMAT_R32G32_SFLOAT;
        case (FFX_SURFACE_FORMAT_R32_UINT):
            return VK_FORMAT_R32_UINT;
        case (FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS):
            return VK_FORMAT_R8G8B8A8_UNORM;
        case (FFX_SURFACE_FORMAT_R8G8B8A8_UNORM):
            return VK_FORMAT_R8G8B8A8_UNORM;
        case (FFX_SURFACE_FORMAT_R8G8B8A8_SNORM):
            return VK_FORMAT_R8G8B8A8_SNORM;
        case (FFX_SURFACE_FORMAT_R8G8B8A8_SRGB):
            return VK_FORMAT_R8G8B8A8_SRGB;
        case (FFX_SURFACE_FORMAT_B8G8R8A8_SRGB):
            return VK_FORMAT_B8G8R8A8_SRGB;
        case (FFX_SURFACE_FORMAT_R11G11B10_FLOAT):
            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case (FFX_SURFACE_FORMAT_R10G10B10A2_UNORM):
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case (FFX_SURFACE_FORMAT_R16G16_FLOAT):
            return VK_FORMAT_R16G16_SFLOAT;
        case (FFX_SURFACE_FORMAT_R16G16_UINT):
            return VK_FORMAT_R16G16_UINT;
        case (FFX_SURFACE_FORMAT_R16G16_SINT):
            return VK_FORMAT_R16G16_SINT;
        case (FFX_SURFACE_FORMAT_R16_FLOAT):
            return VK_FORMAT_R16_SFLOAT;
        case (FFX_SURFACE_FORMAT_R16_UINT):
            return VK_FORMAT_R16_UINT;
        case (FFX_SURFACE_FORMAT_R16_UNORM):
            return VK_FORMAT_R16_UNORM;
        case (FFX_SURFACE_FORMAT_R16_SNORM):
            return VK_FORMAT_R16_SNORM;
        case (FFX_SURFACE_FORMAT_R8_UNORM):
            return VK_FORMAT_R8_UNORM;
        case (FFX_SURFACE_FORMAT_R8_UINT):
            return VK_FORMAT_R8_UINT;
        case (FFX_SURFACE_FORMAT_R8G8_UNORM):
            return VK_FORMAT_R8G8_UNORM;
        case (FFX_SURFACE_FORMAT_R8G8_UINT):
            return VK_FORMAT_R8G8_UINT;
        case (FFX_SURFACE_FORMAT_R32_FLOAT):
            return VK_FORMAT_R32_SFLOAT;
        case (FFX_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP):
            return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
        case (FFX_SURFACE_FORMAT_UNKNOWN):
            return VK_FORMAT_UNDEFINED;

        default:
            CauldronCritical(L"Format not yet supported");
            return VK_FORMAT_UNDEFINED;
        }
    }

    VkImageUsageFlags GetVkImageUsageFlagsFromResourceUsage(FfxResourceUsage usage)
    {
        VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // default
        if ((usage & FFX_RESOURCE_USAGE_RENDERTARGET) != 0)
            imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if ((usage & FFX_RESOURCE_USAGE_UAV) != 0)
            imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
        if ((usage & FFX_RESOURCE_USAGE_DEPTHTARGET) != 0)
            imageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        return imageUsage;
    }

    VkImageType GetVkImageTypeFromResourceType(FfxResourceType type)
    {
        switch (type)
        {
        case FFX_RESOURCE_TYPE_TEXTURE1D:
            return VK_IMAGE_TYPE_1D;
        case FFX_RESOURCE_TYPE_TEXTURE2D:
            return VK_IMAGE_TYPE_2D;
        case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
            return VK_IMAGE_TYPE_2D;
        case FFX_RESOURCE_TYPE_TEXTURE3D:
            return VK_IMAGE_TYPE_3D;
        default:
            CauldronCritical(L"Type not yet supported");
            return VK_IMAGE_TYPE_2D;
        }
    }

    GPUResource* GPUResource::GetWrappedResourceFromSDK(const wchar_t* name, void* pSDKResource, const TextureDesc* pDesc, ResourceState initialState)
    {
        VkImageCreateInfo imageCreateInfo = ConvertTextureDesc(*pDesc);
        return new GPUResourceInternal(static_cast<VkImage>(pSDKResource), imageCreateInfo, name, initialState, false);
    }

    GPUResource* GPUResource::GetWrappedResourceFromSDK(const wchar_t* name, void* pSDKResource, const BufferDesc* pDesc, ResourceState initialState)
    {
        VkBufferCreateInfo bufferCreateInfo = ConvertBufferDesc(*pDesc);
        return new GPUResourceInternal(static_cast<VkBuffer>(pSDKResource), bufferCreateInfo, name, initialState, false);
    }

    void GPUResource::ReleaseWrappedResource(GPUResource* pResource)
    {
        delete pResource;
    }

    GPUResourceInternal::GPUResourceInternal(VkImageCreateInfo info, ResourceState initialState, const wchar_t* resourceName, void* pOwner, bool resizable) :
        GPUResource(resourceName, pOwner, g_UndefinedState, resizable),
        m_ImageCreateInfo(info)
    {
        m_Type = ResourceType::Image;
        m_OwnerType = OwnerType::Texture;
        CreateImage(initialState);

        // Setup sub-resource states
        InitSubResourceCount(m_ImageCreateInfo.arrayLayers * m_ImageCreateInfo.mipLevels);
    }

    GPUResourceInternal::GPUResourceInternal(VkImage image, VkImageCreateInfo info, const wchar_t* resourceName, ResourceState initialState, bool resizable) :
        GPUResource(resourceName, nullptr, initialState, resizable),
        m_ImageCreateInfo(info)
    {
        m_Type = ResourceType::Image;
        m_Image = image;
        m_External = true;

        // Setup sub-resource states
        InitSubResourceCount(m_ImageCreateInfo.arrayLayers * m_ImageCreateInfo.mipLevels);
    }

    GPUResourceInternal::GPUResourceInternal(VkBufferCreateInfo info,
                             VmaMemoryUsage     memoryUsage,
                             const wchar_t*     resourceName,
                             void*              pOwner,
                             ResourceState      initialState,
                             bool               resizable,
                             size_t             alignment /*=0*/) : 
        GPUResource(resourceName, pOwner, initialState, resizable),
        m_BufferCreateInfo(info),
        m_MemoryUsage(memoryUsage)
    {
        m_Type = ResourceType::Buffer;
        if (m_pOwner)
        {
            if (memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU)
                m_OwnerType = OwnerType::Memory;
            else
                m_OwnerType = OwnerType::Buffer;
        }

        CreateBuffer(alignment);
    }

    GPUResourceInternal::GPUResourceInternal(VkBuffer buffer,
                                             VkBufferCreateInfo info,
                                             const wchar_t* resourceName,
                                             ResourceState initialState,
                                             bool resizable,
                                             size_t alignment /*=0*/) :
        GPUResource(resourceName, nullptr, initialState, resizable),
        m_BufferCreateInfo(info)
    {
        m_Type     = ResourceType::Buffer;
        m_Buffer   = buffer;
        m_External = true;
    }

    GPUResourceInternal::GPUResourceInternal(uint64_t blockBytes, void* pExternalOwner, ResourceState initialState, const wchar_t* resourceName) :
        GPUResource(resourceName, nullptr, initialState, false)
    {
        m_OwnerType = OwnerType::BufferBreadcrumbs;
        m_Type = ResourceType::Buffer;
        FfxBreadcrumbsBlockData* blockData = (FfxBreadcrumbsBlockData*)pExternalOwner;

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        VkDevice dev = pDevice->VKDevice();
        
        m_BufferCreateInfo = {};
        m_BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        m_BufferCreateInfo.pNext = nullptr;
        m_BufferCreateInfo.flags = 0;
        m_BufferCreateInfo.size = blockBytes;
        m_BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        m_BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        m_BufferCreateInfo.queueFamilyIndexCount = 0;
        m_BufferCreateInfo.pQueueFamilyIndices = nullptr;

        VkResult res = pDevice->GetCreateBuffer()(dev, &m_BufferCreateInfo, nullptr, &m_Buffer);
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS && m_Buffer != VK_NULL_HANDLE, L"Failed to create a breadcrumbs buffer");

        VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
        allocInfo.allocationSize = blockBytes;
        allocInfo.memoryTypeIndex = pDevice->GetBreadcrumbsMemoryIndex();

        VkMemoryDedicatedAllocateInfo dedicatedAlloc = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, nullptr };
        if (pDevice->BreadcrumbsDedicatedAllocRequired())
        {
            dedicatedAlloc.image = VK_NULL_HANDLE;
            dedicatedAlloc.buffer = m_Buffer;
            dedicatedAlloc.pNext = allocInfo.pNext;
            allocInfo.pNext = &dedicatedAlloc;
        }

        VkDeviceMemory memory = VK_NULL_HANDLE;
        res = pDevice->GetAllocateMemory()(dev, &allocInfo, nullptr, &memory);
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS && memory != VK_NULL_HANDLE, L"Failed to create a memory for breadcrumbs buffer");

        res = pDevice->GetBindBufferMemory()(dev, m_Buffer, memory, 0);
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS, L"Failed to bind memory to breadcrumbs buffer");

        res = pDevice->GetMapMemory()(dev, memory, 0, blockBytes, 0, &blockData->memory);
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS, L"Failed to map memory of breadcrumbs buffer");

        blockData->heap = (void*)memory;
        blockData->buffer = (void*)m_Buffer;
        blockData->baseAddress = 0;

        pDevice->SetResourceName(VK_OBJECT_TYPE_BUFFER, (uint64_t)m_Buffer, "Buffer for Breadcrumbs");
    }

    GPUResourceInternal::~GPUResourceInternal()
    {
        ClearResource();
    }

    void GPUResourceInternal::SetAllocationName()
    {
        std::string name = WStringToString(m_Name);
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        vmaSetAllocationName(pDevice->GetVmaAllocator(), m_Allocation, name.c_str());
    }

    void GPUResourceInternal::SetOwner(void* pOwner)
    {
        m_pOwner = pOwner;

        // What type of resource is this?
        if (m_pOwner && m_OwnerType != OwnerType::BufferBreadcrumbs)
        {
            if (m_Type == ResourceType::Buffer)
                m_OwnerType = OwnerType::Buffer;

            else if (m_Type == ResourceType::Image)
                m_OwnerType = OwnerType::Texture;

            else
                m_OwnerType = OwnerType::Memory;
        }
    }

    void GPUResourceInternal::ClearResource()
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        if (m_OwnerType == OwnerType::BufferBreadcrumbs && m_pOwner)
        {
            FfxBreadcrumbsBlockData* blockData = (FfxBreadcrumbsBlockData*)m_pOwner;
            if (blockData->memory && blockData->baseAddress == 0)
            {
                pDevice->GetUnmapMemory()(pDevice->VKDevice(), (VkDeviceMemory)blockData->heap);
                blockData->memory = nullptr;
            }
            if (blockData->buffer)
            {
                pDevice->GetDestroyBuffer()(pDevice->VKDevice(), (VkBuffer)blockData->buffer, nullptr);
                blockData->buffer = nullptr;
                m_Buffer = VK_NULL_HANDLE;
            }
            if (blockData->heap)
            {
                pDevice->GetFreeMemory()(pDevice->VKDevice(), (VkDeviceMemory)blockData->heap, nullptr);
                blockData->heap = nullptr;
            }
        }
        else if (!m_External)
        {
            switch (m_Type)
            {
            case ResourceType::Image:
                vmaDestroyImage(pDevice->GetVmaAllocator(), m_Image, m_Allocation);
                m_Image = VK_NULL_HANDLE;
                m_Allocation = VK_NULL_HANDLE;
                break;
            case ResourceType::Buffer:
                vmaDestroyBuffer(pDevice->GetVmaAllocator(), m_Buffer, m_Allocation);
                m_Buffer = VK_NULL_HANDLE;
                m_Allocation = VK_NULL_HANDLE;
                break;
            default:
                break;
            }
        }
    }

    void GPUResourceInternal::RecreateResource(VkImageCreateInfo info, ResourceState initialState)
    {
        CauldronAssert(ASSERT_CRITICAL, m_Type == ResourceType::Image, L"Cannot recreate non-image resource");
        CauldronAssert(ASSERT_CRITICAL, m_Resizable, L"Cannot recreate non-resizable resource");
        m_ImageCreateInfo = info;
        m_CurrentStates.front() = g_UndefinedState;
        InitSubResourceCount(m_ImageCreateInfo.arrayLayers * m_ImageCreateInfo.mipLevels);
        CreateImage(initialState);
    }

    void GPUResourceInternal::RecreateResource(VkBufferCreateInfo info, ResourceState initialState)
    {
        CauldronAssert(ASSERT_CRITICAL, m_Type == ResourceType::Buffer, L"Cannot recreate non-buffer resource");
        CauldronAssert(ASSERT_CRITICAL, m_Resizable, L"Cannot recreate non-resizable resource");
        m_BufferCreateInfo = info;
        m_CurrentStates.front() = initialState; // buffer have no state, we can safely set this state internally
        InitSubResourceCount(1);
        CreateBuffer();
    }

    VkImage GPUResourceInternal::GetImage() const
    {
        CauldronAssert(ASSERT_ERROR, m_Type == ResourceType::Image, L"GPUResource type isn't Image");
        return m_Image;
    }

    VkBuffer GPUResourceInternal::GetBuffer() const
    {
        CauldronAssert(ASSERT_ERROR, m_Type == ResourceType::Buffer, L"GPUResource type isn't Buffer");
        return m_Buffer;
    }

    VkDeviceAddress GPUResourceInternal::GetDeviceAddress() const
    {
        CauldronAssert(ASSERT_ERROR, m_Type == ResourceType::Buffer, L"GPUResource type isn't Buffer");
        return m_DeviceAddress;
    }

    VkImageCreateInfo GPUResourceInternal::GetImageCreateInfo() const
    {
        CauldronAssert(ASSERT_ERROR, m_Type == ResourceType::Image, L"GPUResource type isn't Image");
        return m_ImageCreateInfo;
    }

    VkBufferCreateInfo GPUResourceInternal::GetBufferCreateInfo() const
    {
        CauldronAssert(ASSERT_ERROR, m_Type == ResourceType::Buffer, L"GPUResource type isn't Buffer");
        return m_BufferCreateInfo;
    }

    void GPUResourceInternal::CreateImage(ResourceState initialState)
    {
        CauldronAssert(ASSERT_ERROR, m_Type == ResourceType::Image, L"GPUResource type isn't Image");
        ClearResource();

        DeviceInternal* pDevice = GetDevice()->GetImpl();

        m_MemoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = m_MemoryUsage;
        VkResult res = vmaCreateImage(pDevice->GetVmaAllocator(), &m_ImageCreateInfo, &allocInfo, &m_Image, &m_Allocation, nullptr);
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS && m_Image != VK_NULL_HANDLE, L"Failed to create an image");

        // reset the pointer to the format info structure
        m_ImageCreateInfo.pNext = nullptr;

        pDevice->SetResourceName(VK_OBJECT_TYPE_IMAGE, (uint64_t)m_Image, m_Name.c_str());
        SetAllocationName();

        // In vulkan, after creation the image is in an undefined state. Transition it to the desired state
        if (initialState != g_UndefinedState)
        {
            switch (initialState)
            {
            case ResourceState::PixelShaderResource:
            case ResourceState::NonPixelShaderResource:
            case ResourceState::ShaderResource:
            case ResourceState::RenderTargetResource:
            case ResourceState::DepthRead:
            {
                Barrier barrier;
                barrier.Type = BarrierType::Transition;
                barrier.pResource = this;
                barrier.SourceState = g_UndefinedState;
                barrier.DestState = initialState;
                pDevice->ExecuteResourceTransitionImmediate(CommandQueue::Graphics, 1, &barrier);
                break;
            }
            case ResourceState::UnorderedAccess:
            {
                // we need to transition to our final state. This should be done on the graphics queue.
                // only render targets should call for an inital state like those ones.
                CauldronAssert(ASSERT_CRITICAL,
                               (m_ImageCreateInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0,
                               L"A non-UAV texture has an unexpected initial state. Please review and implement if necessary");
                Barrier barrier;
                barrier.Type        = BarrierType::Transition;
                barrier.pResource   = this;
                barrier.SourceState = g_UndefinedState;
                barrier.DestState   = initialState;
                pDevice->ExecuteResourceTransitionImmediate(CommandQueue::Graphics, 1, &barrier);
                break;
            }
            case ResourceState::CopyDest:
            {
                // transition on the copy queue as it will be used to load data
                Barrier barrier;
                barrier.Type = BarrierType::Transition;
                barrier.pResource = this;
                barrier.SourceState = g_UndefinedState;
                barrier.DestState = initialState;
                pDevice->ExecuteResourceTransitionImmediate(CommandQueue::Copy, 1, &barrier);
                break;
            }
            case ResourceState::ShadingRateSource:
            {
                // transition on the graphics queue as it will be used to load data
                CauldronAssert(ASSERT_CRITICAL,
                               (m_ImageCreateInfo.usage & VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR) != 0,
                               L"Cannot transition to initial state ShadingRateSource because the texture doesn't support this usage.");
                Barrier barrier;
                barrier.Type        = BarrierType::Transition;
                barrier.pResource   = this;
                barrier.SourceState = g_UndefinedState;
                barrier.DestState   = initialState;
                pDevice->ExecuteResourceTransitionImmediate(CommandQueue::Graphics, 1, &barrier);
                break;
            }
            default:
                CauldronCritical(L"Unsupported initial resource state (%d). Please implement the correct transition.", static_cast<uint32_t>(initialState));
                break;
            }
        }
    }

    void GPUResourceInternal::CreateBuffer(size_t alignment /*=0*/)
    {
        ClearResource();

        DeviceInternal* pDevice = GetDevice()->GetImpl();

        m_Buffer = VK_NULL_HANDLE;
        m_Allocation = VK_NULL_HANDLE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = m_MemoryUsage;

        // Create with alignment
        VkResult res;
        if (alignment != 0)
        {
            res = vmaCreateBufferWithAlignment(pDevice->GetVmaAllocator(), &m_BufferCreateInfo, &allocInfo, (VkDeviceSize)alignment, &m_Buffer, &m_Allocation, nullptr);
        }
        else
        {
            res = vmaCreateBuffer(pDevice->GetVmaAllocator(), &m_BufferCreateInfo, &allocInfo, &m_Buffer, &m_Allocation, nullptr);
        }
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS && m_Buffer != VK_NULL_HANDLE, L"Failed to create a buffers");

        pDevice->SetResourceName(VK_OBJECT_TYPE_BUFFER, (uint64_t)m_Buffer, m_Name.c_str());
        SetAllocationName();

        if ((m_BufferCreateInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0)
        {
            VkBufferDeviceAddressInfo bufferAddressInfo = {};
            bufferAddressInfo.sType                     = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferAddressInfo.pNext                     = nullptr;
            bufferAddressInfo.buffer                    = m_Buffer;
            m_DeviceAddress                             = vkGetBufferDeviceAddress(pDevice->VKDevice(), &bufferAddressInfo);
        }

    }
} // namespace cauldron

#endif // #if defined(_VK)
