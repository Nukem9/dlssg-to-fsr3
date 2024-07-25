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
#include "render/vk/copyresource_vk.h"
#include "render/vk/device_vk.h"
#include "render/vk/gpuresource_vk.h"

#include "core/framework.h"
#include "misc/assert.h"


namespace cauldron
{
    CopyResource* CopyResource::CreateCopyResource(const GPUResource* pDest, const SourceData* pSrc, ResourceState initialState)
    {
        return new CopyResourceInternal(pDest, pSrc, initialState);
    }

    CopyResourceInternal::CopyResourceInternal(const GPUResource* pDest, const SourceData* pSrc, ResourceState initialState):
        CopyResource(pSrc)
    {
        VkBufferCreateInfo info    = {};
        info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.pNext                 = nullptr;
        info.flags                 = 0;
        info.size                  = static_cast<VkDeviceSize>(pSrc->size);
        info.usage                 = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices   = nullptr;

        // create staging resource
        std::wstring name = pDest->GetName();
        name += L"_CopyResource";

        GPUResourceInitParams initParams = {};
        initParams.bufferInfo = info;
        initParams.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        initParams.type = GPUResourceType::Buffer;

        m_pResource = GPUResource::CreateGPUResource(name.c_str(), this, initialState, &initParams);
        CauldronAssert(ASSERT_ERROR, m_pResource != nullptr, L"Could not create copy resource for resource %ls", pDest->GetName());

        // Copy the data in
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        VmaAllocator  allocator  = pDevice->GetVmaAllocator();
        VmaAllocation allocation = m_pResource->GetImpl()->VKAllocation();
        void*         pDestData  = nullptr;
        VkResult      res        = vmaMapMemory(allocator, allocation, &pDestData);
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS, L"Cannot map staging buffer");

        const uint8_t* src = static_cast<uint8_t*>(pSrc->buffer);
        uint8_t*       dst = static_cast<uint8_t*>(pDestData);
        if (pSrc->type == SourceData::Type::BUFFER)
            memcpy(dst, src, static_cast<size_t>(info.size));
        else if (pSrc->type == SourceData::Type::VALUE)
            memset(dst, pSrc->value, static_cast<size_t>(info.size));
        else
        {
            CauldronCritical(L"Invalid type of source data");
        }

        vmaFlushAllocation(allocator, allocation, 0, info.size);

        vmaUnmapMemory(allocator, allocation);
    }

} // namespace cauldron

#endif // #if defined(_VK)
