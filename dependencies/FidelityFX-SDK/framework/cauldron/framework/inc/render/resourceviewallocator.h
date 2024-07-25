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

#include "misc/helpers.h"
#include "render/resourceview.h"

namespace cauldron
{
    /// Per platform/API implementation of <c><i>ResourceViewAllocator</i></c>
    ///
    /// @ingroup CauldronRender
    class ResourceViewAllocatorInternal;

    /**
     * @class ResourceViewAllocator
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of the resource view allocator.
     *
     * @ingroup CauldronRender
     */
    class ResourceViewAllocator
    {
    public:

        /**
         * @brief   ResourceViewAllocator instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static ResourceViewAllocator* CreateResourceViewAllocator();

        /**
         * @brief   Destruction with default behavior.
         */
        virtual ~ResourceViewAllocator() = default;

        /**
         * @brief   Allocates CPU resource views.
         */
        virtual void AllocateCPUResourceViews(ResourceView** ppResourceView, uint32_t count = 1) = 0;

        /**
         * @brief   Allocates GPU resource views.
         */
        virtual void AllocateGPUResourceViews(ResourceView** ppResourceView, uint32_t count = 1) = 0;

        /**
         * @brief   Allocates GPU sampler views.
         */
        virtual void AllocateGPUSamplerViews(ResourceView** ppResourceView, uint32_t count = 1) = 0;

        /**
         * @brief   Allocates CPU render views.
         */
        virtual void AllocateCPURenderViews(ResourceView** ppResourceView, uint32_t count = 1) = 0;

        /**
         * @brief   Allocates CPU depth views.
         */
        virtual void AllocateCPUDepthViews(ResourceView** ppResourceView, uint32_t count = 1) = 0;

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        virtual ResourceViewAllocatorInternal* GetImpl() = 0;
        virtual const ResourceViewAllocatorInternal* GetImpl() const = 0;

    private:
        // No copy, No move
        NO_COPY(ResourceViewAllocator)
        NO_MOVE(ResourceViewAllocator)

    protected:
        ResourceViewAllocator();
        
        uint32_t m_NumViews[static_cast<uint32_t>(ResourceViewHeapType::Count)];
    };

} // namespace cauldron
