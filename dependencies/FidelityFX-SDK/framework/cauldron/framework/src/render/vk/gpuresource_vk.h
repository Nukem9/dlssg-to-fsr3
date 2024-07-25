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

#if defined(_VK)
#include "memoryallocator/memoryallocator.h"
#include "render/renderdefines.h"
#include "render/gpuresource.h"

//#include <vulkan/vulkan.h>

namespace cauldron
{
    // NOTE: to indicate an unknown state
    constexpr ResourceState g_UndefinedState = static_cast<ResourceState>(-1);

    enum class ResourceType
    {
        Unknown,
        Image,
        Buffer,
    };

    struct GPUResourceInitParams
    {
        union {
            VkImageCreateInfo   imageInfo;
            VkBufferCreateInfo  bufferInfo;
        };
        VkImage                 image = VK_NULL_HANDLE;
        VmaMemoryUsage          memoryUsage;
        size_t                  alignment = 0;
        GPUResourceType         type = GPUResourceType::Texture;
    };

    class GPUResourceInternal final : public GPUResource
    {
    public:
        virtual ~GPUResourceInternal();

        const VmaAllocation VKAllocation() const { return m_Allocation; }

        void RecreateResource(VkImageCreateInfo info, ResourceState initialState);
        void RecreateResource(VkBufferCreateInfo info, ResourceState initialState);
        void SetOwner(void* pOwner) override;

        ResourceType GetResourceType() const { return m_Type; }
        VkImage GetImage() const;
        VkBuffer GetBuffer() const;
        VkDeviceAddress GetDeviceAddress() const;
        VkImageCreateInfo GetImageCreateInfo() const;
        VkBufferCreateInfo GetBufferCreateInfo() const;

        virtual const GPUResourceInternal* GetImpl() const override { return this; }
        virtual GPUResourceInternal* GetImpl() override { return this; }

    private:
        friend class GPUResource;
        GPUResourceInternal() = delete;
        GPUResourceInternal(VkImageCreateInfo info, ResourceState initialState, const wchar_t* resourceName, void* pOwner, bool resizable);
        GPUResourceInternal(VkImage image, VkImageCreateInfo info, const wchar_t* resourceName, ResourceState initialState, bool resizable);  // for swapchain
        GPUResourceInternal(VkBufferCreateInfo info, VmaMemoryUsage memoryUsage, const wchar_t* resourceName, void* pOwner, ResourceState initialState, bool resizable, size_t alignment = 0);
        GPUResourceInternal(VkBuffer buffer, VkBufferCreateInfo info, const wchar_t* resourceName, ResourceState initialState, bool resizable, size_t alignment = 0);
        GPUResourceInternal(uint64_t blockBytes, void* pExternalOwner, ResourceState initialState, const wchar_t* resourceName);

        void SetAllocationName();
        void ClearResource();

        void CreateImage(ResourceState initialState);
        void CreateBuffer(size_t alignment = 0);

        // Internal members
        VmaAllocation m_Allocation = VK_NULL_HANDLE;

        ResourceType m_Type     = ResourceType::Unknown;
        bool         m_External = false; // flag to indicate that the VkImage or VkBuffer lifetime is managed outside of this instance
        union
        {
            VkImage  m_Image = VK_NULL_HANDLE;
            struct
            {
                VkBuffer m_Buffer;
                VkDeviceAddress m_DeviceAddress;
            };
        };
        union
        {
            VkImageCreateInfo  m_ImageCreateInfo;
            VkBufferCreateInfo m_BufferCreateInfo;
        };
        VmaMemoryUsage m_MemoryUsage;
    };

} // namespace cauldron

#endif // #if defined(_VK)
