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

#include "render/dynamicbufferpool.h"
#include "render/vk/buffer_vk.h"
#include <vulkan/vulkan.h>
#include <mutex>

namespace cauldron
{
    class DynamicBufferPoolInternal final : public DynamicBufferPool
    {
    public:
        DynamicBufferPoolInternal();
        virtual ~DynamicBufferPoolInternal();

        virtual BufferAddressInfo AllocConstantBuffer(uint32_t size, const void* pInitData) override;
        virtual void BatchAllocateConstantBuffer(uint32_t size, uint32_t count, BufferAddressInfo* pBufferAddressInfos) override;
        virtual void InitializeConstantBuffer(const BufferAddressInfo& bufferAddressInfo, uint32_t size, const void* pInitData) override;

        virtual BufferAddressInfo AllocVertexBuffer(uint32_t vertexCount, uint32_t vertexStride, void** pBuffer) override;
        virtual BufferAddressInfo AllocIndexBuffer(uint32_t indexCount, uint32_t indexStride, void** pBuffer) override;

        virtual void EndFrame() override;

    private:
        bool InternalAlloc(uint32_t size, uint32_t* offset);

        struct MemoryPoolFrameInfo
        {
            uint64_t gpuSignal      = 0;
            uint32_t allocationSize = 0;
        };

    private:
        std::mutex                      m_Mutex;
        uint32_t                        m_Alignment            = 0;
        uint32_t                        m_AllocationTotal      = 0;
        std::queue<MemoryPoolFrameInfo> m_FrameAllocationQueue = {};
        uint32_t                        m_Tail                 = 0;
        uint32_t                        m_Head                 = 0;
    };

} // namespace cauldron

#endif // #if defined(_VK)
