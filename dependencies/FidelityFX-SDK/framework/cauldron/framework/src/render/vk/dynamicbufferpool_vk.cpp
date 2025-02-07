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

#include "render/vk/buffer_vk.h"
#include "render/vk/device_vk.h"
#include "render/vk/dynamicbufferpool_vk.h"
#include "render/vk/gpuresource_vk.h"

#include "core/framework.h"
#include "misc/assert.h"

namespace cauldron
{
    DynamicBufferPool* DynamicBufferPool::CreateDynamicBufferPool()
    {
        return new DynamicBufferPoolInternal();
    }

    DynamicBufferPoolInternal::DynamicBufferPoolInternal() : 
        DynamicBufferPool()
    {
        // Init the vulkan buffer backing the dynamic buffer pool
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.pNext = nullptr;
        bufferInfo.flags = 0;
        bufferInfo.size = static_cast<VkDeviceSize>(m_TotalSize);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.queueFamilyIndexCount = 0;
        bufferInfo.pQueueFamilyIndices = nullptr;

        GPUResourceInitParams initParams = {};
        initParams.bufferInfo = bufferInfo;
        initParams.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        initParams.type = GPUResourceType::Buffer;

        m_pResource = GPUResource::CreateGPUResource(L"Cauldron dynamic buffer pool", nullptr, ResourceState::GenericRead, &initParams);

        // Map the memory
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        VkResult res = vmaMapMemory(pDevice->GetVmaAllocator(), m_pResource->GetImpl()->VKAllocation(), reinterpret_cast<void**>(&m_pData));
        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Unable to map dynamic buffer pool");

        // query the alignment for the buffers
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(pDevice->VKDevice(), m_pResource->GetImpl()->GetBuffer(), &requirements);
        m_Alignment = static_cast<uint32_t>(requirements.alignment);
    }

    DynamicBufferPoolInternal::~DynamicBufferPoolInternal()
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        vmaUnmapMemory(pDevice->GetVmaAllocator(), m_pResource->GetImpl()->VKAllocation());
    }

    bool DynamicBufferPoolInternal::InternalAlloc(uint32_t size, uint32_t* offset)
    {
        std::lock_guard<std::mutex> memlock(m_Mutex);

        // check if we can directly allocate from head
        if (m_Head >= m_Tail && m_Head + size <= m_TotalSize)
        {
            *offset = m_Head;
            m_Head += size;
            m_AllocationTotal += size;
            return true;
        }

        // check head if we've wrapped
        if (m_Head < m_Tail && m_Head + size < m_Tail)
        {
            *offset = m_Head;
            m_Head += size;
            m_AllocationTotal += size;
            return true;
        }

        // check if we need to wrap
        if (m_Head >= m_Tail && m_Head + size > m_TotalSize)
        {
            if (size < m_Tail)
            {
                *offset = 0;
                m_AllocationTotal += m_TotalSize - m_Head + size;
                m_Head = size;
                return true;
            }
        }

        return false;
    }

    BufferAddressInfo DynamicBufferPoolInternal::AllocConstantBuffer(uint32_t size, const void* pInitData)
    {
        uint32_t alignedSize = AlignUp(size, m_Alignment);

        uint32_t offset;
        CauldronAssert(ASSERT_CRITICAL, InternalAlloc(alignedSize, &offset), L"DynamicBufferPool has run out of memory. Please increase the allocation size.");
        
        // Copy the data in
        void* pBuffer = (void*)(m_pData + offset);
        memcpy(pBuffer, pInitData, size);

        BufferAddressInfo bufferInfo = {};
        BufferAddressInfoInternal* pInfo = (BufferAddressInfoInternal*)(&bufferInfo);
        pInfo->Buffer = m_pResource->GetImpl()->GetBuffer();
        pInfo->SizeInBytes = static_cast<VkDeviceSize>(alignedSize);
        pInfo->Offset = static_cast<VkDeviceSize>(offset);

        return bufferInfo;
    }

    void DynamicBufferPoolInternal::BatchAllocateConstantBuffer(uint32_t size, uint32_t count, BufferAddressInfo* pBufferAddressInfos)
    {
        uint32_t alignedSize = AlignUp(size, m_Alignment);
        uint32_t offset;
        CauldronAssert(ASSERT_CRITICAL, InternalAlloc(alignedSize * count, &offset), L"DynamicBufferPool has run out of memory. Please increase the allocation size.");
        for (uint32_t i = 0; i < count; i++)
        {
            BufferAddressInfoInternal* pInfo = (BufferAddressInfoInternal*)(&(pBufferAddressInfos[i]));
            pInfo->Buffer                    = m_pResource->GetImpl()->GetBuffer();
            pInfo->SizeInBytes               = static_cast<VkDeviceSize>(alignedSize);
            pInfo->Offset                    = static_cast<VkDeviceSize>(offset + (i * alignedSize));
        }
    }

    void DynamicBufferPoolInternal::InitializeConstantBuffer(const BufferAddressInfo& bufferAddressInfo, uint32_t size, const void* pInitData)
    {
        const BufferAddressInfoInternal* pInfo = bufferAddressInfo.GetImpl();
        CauldronAssert(ASSERT_CRITICAL, size <= pInfo->SizeInBytes, L"Constant buffer too small to inialize with provided data.");
        void* pBuffer = (void*)(m_pData + pInfo->Offset);
        memcpy(pBuffer, pInitData, size);
    }


    BufferAddressInfo DynamicBufferPoolInternal::AllocVertexBuffer(uint32_t vertexCount, uint32_t vertexStride, void** pBuffer)
    {
        uint32_t size = AlignUp(vertexCount * vertexStride, m_Alignment);

        uint32_t offset;
        CauldronAssert(ASSERT_CRITICAL, InternalAlloc(size, &offset), L"DynamicBufferPool has run out of memory. Please increase the allocation size.");

        // Set the buffer data pointer
        *pBuffer = (void*)(m_pData + offset);

        // Fill in the buffer address info struct
        BufferAddressInfo bufferInfo = {};
        BufferAddressInfoInternal* pInfo = (BufferAddressInfoInternal*)(&bufferInfo);
        pInfo->Buffer = m_pResource->GetImpl()->GetBuffer();
        pInfo->SizeInBytes = static_cast<VkDeviceSize>(size);
        pInfo->Offset = static_cast<VkDeviceSize>(offset);
        pInfo->StrideInBytes = static_cast<VkDeviceSize>(vertexStride);

        return bufferInfo;
    }

    BufferAddressInfo DynamicBufferPoolInternal::AllocIndexBuffer(uint32_t indexCount, uint32_t indexStride, void** pBuffer)
    {
        CauldronAssert(ASSERT_CRITICAL, indexStride == 2 || indexStride == 4, L"Requesting allocation of index buffer with an invalid index size.");
        uint32_t size = AlignUp(indexCount * indexStride, m_Alignment);

        uint32_t offset;
        CauldronAssert(ASSERT_CRITICAL, InternalAlloc(size, &offset), L"DynamicBufferPool has run out of memory. Please increase the allocation size.");

        // Set the buffer data pointer
        *pBuffer = (void*)(m_pData + offset);

        // Fill in the buffer address info struct
        BufferAddressInfo bufferInfo = {};
        BufferAddressInfoInternal* pInfo = (BufferAddressInfoInternal*)(&bufferInfo);
        pInfo->Buffer = m_pResource->GetImpl()->GetBuffer();
        pInfo->SizeInBytes = static_cast<VkDeviceSize>(size);
        pInfo->Offset = static_cast<VkDeviceSize>(offset);
        pInfo->IndexType = (indexStride == 4) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;

        return bufferInfo;
    }

    void DynamicBufferPoolInternal::EndFrame()
    {
        MemoryPoolFrameInfo poolInfo = {};

        // get the latests submitted value of the semaphore on the command queue to know when this memory has been processed so we can move up the tail
        poolInfo.gpuSignal = GetDevice()->GetImpl()->GetLatestSemaphoreValue(CommandQueue::Graphics);

        {
            std::lock_guard<std::mutex> memlock(m_Mutex);
            poolInfo.allocationSize = m_AllocationTotal;
            m_AllocationTotal       = 0;  // cleared as all allocations from here are "fresh"
        }

        // Enqueue the information
        m_FrameAllocationQueue.push(poolInfo);

        // Check past frame allocations to see if we can recoup them
        while (!m_FrameAllocationQueue.empty())
        {
            MemoryPoolFrameInfo& frameEntry = m_FrameAllocationQueue.front();
            if (frameEntry.gpuSignal > GetDevice()->QueryLastCompletedValue(CommandQueue::Graphics))
            {
                break;  // nothing else is done
            }

            // Lock the mutex, and move up the tail
            {
                std::lock_guard<std::mutex> memlock(m_Mutex);
                m_Tail = (m_Tail + frameEntry.allocationSize) % m_TotalSize;
            }
            m_FrameAllocationQueue.pop();
        }
    }

} // namespace cauldron

#endif // #if defined(_VK)
