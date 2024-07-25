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

#include "render/resourceviewallocator.h"

#include <vulkan/vulkan.h>
#include <atomic>

namespace cauldron
{
    class ResourceViewAllocatorInternal final : public ResourceViewAllocator
    {
    public:
        void AllocateCPUResourceViews(ResourceView** ppResourceView, uint32_t count = 1) override;
        void AllocateGPUResourceViews(ResourceView** ppResourceView, uint32_t count = 1) override;
        void AllocateGPUSamplerViews(ResourceView** ppResourceView, uint32_t count = 1) override;
        void AllocateCPURenderViews(ResourceView** ppResourceView, uint32_t count = 1) override;
        void AllocateCPUDepthViews(ResourceView** ppResourceView, uint32_t count = 1) override;

        ResourceViewAllocatorInternal* GetImpl() override { return this; }
        const ResourceViewAllocatorInternal* GetImpl() const override { return this; }

    private:
        friend class ResourceViewAllocator;
        ResourceViewAllocatorInternal();
        virtual ~ResourceViewAllocatorInternal() = default;

        ResourceView* AllocateViews(ResourceViewHeapType type, uint32_t count);
    
    private:
        uint32_t   m_NumDescriptors[static_cast<uint32_t>(ResourceViewHeapType::Count)] = {0};
        std::mutex m_CriticalSection;
    };

} // namespace cauldron

#endif // #if defined(_VK)
