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

#include "core/framework.h"
#include "misc/assert.h"

#include "render/vk/device_vk.h"
#include "render/vk/gpuresource_vk.h"
#include "render/vk/uploadheap_vk.h"

#include "helpers.h"

namespace cauldron
{
    UploadHeap* UploadHeap::CreateUploadHeap()
    {
        return new UploadHeapInternal();
    }

    UploadHeapInternal::UploadHeapInternal() :
        UploadHeap()
    {
        m_Size = GetConfig()->UploadHeapSize;

        VkBufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = 0;
        info.size = static_cast<VkDeviceSize>(m_Size);
        info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;

        GPUResourceInitParams initParams = {};
        initParams.bufferInfo = info;
        initParams.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
        initParams.type = GPUResourceType::Buffer;

        m_pResource = GPUResource::CreateGPUResource(L"Cauldron Upload Heap", nullptr, ResourceState::GenericRead, &initParams);

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        VkResult res = vmaMapMemory(pDevice->GetVmaAllocator(), m_pResource->GetImpl()->VKAllocation(), reinterpret_cast<void**>(&m_pDataBegin));

        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Failed to map the upload heap");

        m_pDataEnd = m_pDataBegin + info.size;

        // Now that memory is mapped, initialize the allocation block scheme
        InitAllocationBlocks();
    }

    UploadHeapInternal::~UploadHeapInternal()
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        vmaUnmapMemory(pDevice->GetVmaAllocator(), m_pResource->GetImpl()->VKAllocation());
    }

} // namespace cauldron

#endif // #if defined(_VK)
