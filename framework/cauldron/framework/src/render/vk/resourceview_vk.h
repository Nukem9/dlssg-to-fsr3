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

#include "render/renderdefines.h"
#include "render/resourceview.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace cauldron
{
    struct ResourceViewInfoInternal final
    {
        ResourceViewType Type;
        union
        {
            struct
            {
                VkImage Image;
                VkImageView View;
                VkFormat Format;
                uint32_t Width;
                uint32_t Height;
            } Image;
            struct {
                VkBuffer Buffer;
                VkBufferView View;
                VkDeviceSize Offset;
                VkDeviceSize Size;
            } Buffer;
            struct {
                VkSampler Sampler;
            } Sampler;
        };

        ResourceViewInfoInternal()
            : Type{ ResourceViewType::Invalid }
        {
            Image.Image = VK_NULL_HANDLE;
            Image.View = VK_NULL_HANDLE;
            Image.Format = VK_FORMAT_UNDEFINED;
            Image.Width = 0;
            Image.Height = 0;
        }
    };
    static_assert(sizeof(ResourceViewInfo::resourceViewSize) >= sizeof(ResourceViewInfoInternal), "ResourceViewInfo is not large enough to hold all implementation details. Please grow.");

    struct ResourceViewInitParams
    {
    };

    class ResourceViewInternal final : public ResourceView
    {
    public:
        const ResourceViewInfo GetViewInfo(uint32_t index = 0) const override;

        void BindTextureResource(const GPUResource* pResource, const TextureDesc& textureDesc, ResourceViewType type, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstArraySlice, uint32_t index = 0) override;
        void BindBufferResource(const GPUResource* pResource, const BufferDesc& bufferDesc, ResourceViewType type, uint32_t firstElement, uint32_t numElements, uint32_t index = 0) override;
        void BindSamplerResource(const Sampler* pSampler, uint32_t index = 0) override;

    private:
        friend class ResourceView;
        ResourceViewInternal(ResourceViewHeapType type, uint32_t count);
        virtual ~ResourceViewInternal();

        void DestroyView(uint32_t index);

        // Internal members
        std::vector<ResourceViewInfo> m_Views;
    };

} // namespace cauldron

#endif // #if defined(_VK)
