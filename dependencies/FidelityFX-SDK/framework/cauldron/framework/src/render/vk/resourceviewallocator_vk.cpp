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

#include "misc/assert.h"

#include "render/vk/resourceviewallocator_vk.h"

namespace cauldron
{
    ResourceViewAllocator* ResourceViewAllocator::CreateResourceViewAllocator()
    {
        return new ResourceViewAllocatorInternal();
    }

    ResourceViewAllocatorInternal::ResourceViewAllocatorInternal() : 
        ResourceViewAllocator()
    {
        // TODO: do we reserve memory for views?
    }

    ResourceView* ResourceViewAllocatorInternal::AllocateViews(ResourceViewHeapType type, uint32_t count)
    {
        {
            std::lock_guard<std::mutex> lock(m_CriticalSection);
            uint32_t                    heapID = static_cast<uint32_t>(type);
            // this is to have the same behavior between DX12 and Vulkan
            CauldronAssert(
                ASSERT_CRITICAL, m_NumDescriptors[heapID] + count <= m_NumViews[heapID], L"Resource view allocator has run out of memory, please increase its size.");
            m_NumDescriptors[heapID] += count;
        }

        ResourceView* pView = ResourceView::CreateResourceView(type, count, nullptr);
        CauldronAssert(ASSERT_ERROR, pView != nullptr, L"Could not allocate ResourceView");
        return pView;
    }

    void ResourceViewAllocatorInternal::AllocateCPUResourceViews(ResourceView** ppResourceView, uint32_t count /*=1*/)
    {
        *ppResourceView = AllocateViews(ResourceViewHeapType::CPUResourceView, count);
    }

    void ResourceViewAllocatorInternal::AllocateGPUResourceViews(ResourceView** ppResourceView, uint32_t count /*=1*/)
    {
        *ppResourceView = AllocateViews(ResourceViewHeapType::GPUResourceView, count);
    }

    void ResourceViewAllocatorInternal::AllocateGPUSamplerViews(ResourceView** ppResourceView, uint32_t count /*=1*/)
    {
        *ppResourceView = AllocateViews(ResourceViewHeapType::GPUSamplerView, count);
    }

    void ResourceViewAllocatorInternal::AllocateCPURenderViews(ResourceView** ppResourceView, uint32_t count /*=1*/)
    {
        *ppResourceView = AllocateViews(ResourceViewHeapType::CPURenderView, count);
    }

    void ResourceViewAllocatorInternal::AllocateCPUDepthViews(ResourceView** ppResourceView, uint32_t count /*=1*/)
    {
        *ppResourceView = AllocateViews(ResourceViewHeapType::CPUDepthView, count);
    }

} // namespace cauldron

#endif // #if defined(_VK)
