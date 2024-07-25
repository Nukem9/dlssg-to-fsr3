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

#include "misc/assert.h"
#include "misc/helpers.h"
#include "render/renderdefines.h"

#include <vector>

namespace cauldron
{
    class GPUResource;
    class Texture;
    class Buffer;
    class Sampler;

    /// Per platform/API implementation of <c><i>ResourceViewInfo</i></c>
    ///
    /// @ingroup CauldronRender
    struct ResourceViewInfoInternal;

    /// A structure representing resource view information used to bind resources
    /// to the GPU. Private implementations can be found under each API/Platform folder
    ///
    /// @ingroup CauldronRender
    struct ResourceViewInfo
    {
        uint64_t resourceViewSize[6];            // Memory placeholder
        const ResourceViewInfoInternal* GetImpl() const { return (const ResourceViewInfoInternal*)resourceViewSize; }
    };
    
    struct TextureDesc;
    struct BufferDesc;

    /**
     * @class ResourceView
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of the resource view.
     *
     * @ingroup CauldronRender
     */
    class ResourceView
    {
    public:

        /**
         * @brief   ResourceView instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static ResourceView* CreateResourceView(ResourceViewHeapType type, uint32_t count, void* pInitParams);

        /**
         * @brief   Destruction with default behavior.
         */
        virtual ~ResourceView() = default;

        /**
         * @brief   Returns the number of entries in the resource view.
         */
        const uint32_t GetCount() const { return m_Count; }

        /**
         * @brief   Returns the resource view type.
         */
        const ResourceViewHeapType GetType() const { return m_Type; }

        /**
         * @brief   Returns the <c><i>ResourceViewInfo</i></c>.
         */
        virtual const ResourceViewInfo GetViewInfo(uint32_t index = 0) const = 0;
        
        /**
         * @brief   Binds a texture resource view.
         */
        virtual void BindTextureResource(const GPUResource* pResource, const TextureDesc& texDesc, ResourceViewType type, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstArraySlice, uint32_t index = 0) = 0;

        /**
         * @brief   Binds a buffer resource view.
         */
        virtual void BindBufferResource(const GPUResource* pResource, const BufferDesc& bufferDesc, ResourceViewType type, uint32_t firstElement, uint32_t numElements, uint32_t index = 0) = 0;

        /**
         * @brief   Binds a sampler resource view.
         */
        virtual void BindSamplerResource(const Sampler* pSampler, uint32_t index = 0) = 0;

    private:
        // No copy, No move
        NO_COPY(ResourceView)
        NO_MOVE(ResourceView)

    protected:
        ResourceView(ResourceViewHeapType type, uint32_t count);
        ResourceView() = delete;

        ResourceViewHeapType m_Type  = ResourceViewHeapType::GPUResourceView;
        uint32_t             m_Count = 0;
    };
    
} // namespace cauldron
