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
#include "render/vk/device_vk.h"
#include "render/vk/gpuresource_vk.h"
#include "render/vk/resourceview_vk.h"
#include "render/vk/sampler_vk.h"

#include "helpers.h"

#include "core/framework.h"
#include "misc/assert.h"

namespace cauldron
{
    VkImageViewType GetViewType(ViewDimension dimension)
    {
        switch (dimension)
        {
        case ViewDimension::Texture1D:
            return VK_IMAGE_VIEW_TYPE_1D;
        case ViewDimension::Texture1DArray:
            return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case ViewDimension::Texture2D:
            return VK_IMAGE_VIEW_TYPE_2D;
        case ViewDimension::Texture2DArray:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case ViewDimension::Texture2DMS:
            return VK_IMAGE_VIEW_TYPE_2D;
        case ViewDimension::Texture2DMSArray:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case ViewDimension::Texture3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case ViewDimension::TextureCube:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        case ViewDimension::TextureCubeArray:
            return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        case ViewDimension::Unknown:
        case ViewDimension::Buffer:
        case ViewDimension::RTAccelerationStruct:
        default:
            CauldronError(L"Unsupported image view type");
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        };
    }

    ResourceView* ResourceView::CreateResourceView(ResourceViewHeapType type, uint32_t count, void* pInitParams)
    {
        return new ResourceViewInternal(type, count);
    }

    ResourceViewInternal::ResourceViewInternal(ResourceViewHeapType type, uint32_t count) :
        ResourceView(type, count) // resource type is useless in vulkan but we keep it for consistency with DX12
    {
        m_Views.resize(count);
    }

    ResourceViewInternal::~ResourceViewInternal()
    {
        for (uint32_t i = 0; i < m_Count; ++i)
            DestroyView(i);
    }

    const ResourceViewInfo ResourceViewInternal::GetViewInfo(uint32_t index) const
    {
        CauldronAssert(ASSERT_CRITICAL, index < m_Count, L"Accessing view out of the bounds");
        return m_Views[index];
    }

    void ResourceViewInternal::DestroyView(uint32_t index)
    {
        VkDevice device = GetDevice()->GetImpl()->VKDevice();
        ResourceViewInfoInternal* pView = (ResourceViewInfoInternal*)&m_Views[index];
        switch (pView->Type)
        {
        case ResourceViewType::RTV:
        case ResourceViewType::DSV:
        case ResourceViewType::TextureSRV:
        case ResourceViewType::TextureUAV:
            if (pView->Image.View != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device, pView->Image.View, nullptr);
                pView->Image.View = VK_NULL_HANDLE;
            }
            break;
        case ResourceViewType::CBV:
            if (pView->Buffer.View != VK_NULL_HANDLE)
            {
                vkDestroyBufferView(device, pView->Buffer.View, nullptr);
                pView->Buffer.View = VK_NULL_HANDLE;
            }
            break;
        case ResourceViewType::BufferSRV:
        case ResourceViewType::BufferUAV:
            // no view needed for buffer
            pView->Buffer.Buffer = VK_NULL_HANDLE;
            break;
        case ResourceViewType::Sampler:
            // no view needed for sampler
            pView->Sampler.Sampler = VK_NULL_HANDLE;
            break;
        }
    }

    void ResourceViewInternal::BindTextureResource(const GPUResource* pResource, const TextureDesc& textureDesc, ResourceViewType type, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstArraySlice, uint32_t index)
    {
        // Create the view
        CauldronAssert(ASSERT_ERROR,
                       m_Type == ResourceViewHeapType::GPUResourceView || m_Type == ResourceViewHeapType::CPUResourceView ||
                           m_Type == ResourceViewHeapType::CPURenderView || m_Type == ResourceViewHeapType::CPUDepthView,
                       L"Invalid view type for the heap type.");
        CauldronAssert(ASSERT_CRITICAL, index < m_Count, L"Binding resource out of the view bounds");

        DestroyView(index);

        ResourceViewInfoInternal* pView = (ResourceViewInfoInternal*)&m_Views[index];
        pView->Type = type;

        CauldronAssert(ASSERT_CRITICAL, type == ResourceViewType::RTV || type == ResourceViewType::DSV || type == ResourceViewType::TextureSRV || type == ResourceViewType::TextureUAV, L"Unsupported texture resource binding requested");

        // Create the view
        VkImageCreateInfo imageInfo = pResource->GetImpl()->GetImageCreateInfo();

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.pNext = nullptr;
        viewInfo.viewType = GetViewType(dimension);
        viewInfo.image = pResource->GetImpl()->GetImage();
        viewInfo.format = imageInfo.format;
        if (IsDepthFormat(imageInfo.format) && HasStencilComponent(imageInfo.format) && type != ResourceViewType::DSV)
        {
            // a SRV/UAV cannot have both depth and stencil aspect
            // by default, we choose the depth aspect as cauldron doesn't made use of stencil SRV/UAV.
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        else
        {
            viewInfo.subresourceRange.aspectMask = GetImageAspectMask(imageInfo.format);
        }

        if (mip == -1)
        {
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        }
        else
        {
            viewInfo.subresourceRange.baseMipLevel = static_cast<uint32_t>(mip);
            viewInfo.subresourceRange.levelCount = 1;
        }

        viewInfo.subresourceRange.baseArrayLayer = (firstArraySlice == -1) ? 0 : static_cast<uint32_t>(firstArraySlice);

        if (arraySize == -1)
        {
            switch (dimension)
            {
            case ViewDimension::Texture1D:
            case ViewDimension::Texture2D:
            case ViewDimension::Texture2DMS:
            case ViewDimension::Texture3D:
                viewInfo.subresourceRange.layerCount = 1;
                break;
            case ViewDimension::Texture1DArray:
            case ViewDimension::Texture2DArray:
            case ViewDimension::Texture2DMSArray:
            case ViewDimension::TextureCube:
            case ViewDimension::TextureCubeArray:
                viewInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
                break;
            default:
                CauldronCritical(L"Unsupported view dimension");
                break;
            }
        }
        else
        {
            CauldronAssert(ASSERT_ERROR, viewInfo.subresourceRange.baseArrayLayer + arraySize <= imageInfo.arrayLayers, L"The number of requested layers exceeds the number of available layers.");
            viewInfo.subresourceRange.layerCount = static_cast<uint32_t>(arraySize);
        }

        VkImageViewUsageCreateInfo usageInfo = {};
        if (type != ResourceViewType::TextureUAV && (imageInfo.flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) != 0)
        {
            // mutable is only for sRGB textures that will need a storage
            usageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
            usageInfo.pNext = nullptr;
            usageInfo.usage = imageInfo.usage & (~VK_IMAGE_USAGE_STORAGE_BIT); // remove the storage flag

            viewInfo.format = VKToGamma(viewInfo.format);
            viewInfo.pNext = &usageInfo;
        }

        // TODO: We always want SRGB versions of render targets to stay linear
        // TODO: Support specific array size/slice/mip mapping
        // TODO: Support multi-sampling

        VkResult res = vkCreateImageView(GetDevice()->GetImpl()->VKDevice(), &viewInfo, nullptr, &pView->Image.View);
        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS && pView->Image.View != VK_NULL_HANDLE, L"failed to create texture image view!");

        pView->Image.Width = imageInfo.extent.width;
        pView->Image.Height = imageInfo.extent.height;
        pView->Image.Image = pResource->GetImpl()->GetImage();
        pView->Image.Format = viewInfo.format;
    }

    void ResourceViewInternal::BindBufferResource(const GPUResource* pResource, const BufferDesc& bufferDesc, ResourceViewType type, uint32_t firstElement, uint32_t numElements, uint32_t index)
    {
        CauldronAssert(ASSERT_ERROR,
                       m_Type == ResourceViewHeapType::GPUResourceView || m_Type == ResourceViewHeapType::CPUResourceView,
                       L"Invalid view type for the heap type.");
        CauldronAssert(ASSERT_CRITICAL, index < m_Count, L"Binding resource out of the view bounds");

        DestroyView(index);

        ResourceViewInfoInternal* pView = (ResourceViewInfoInternal*)&m_Views[index];
        pView->Type             = type;
        pView->Buffer.Buffer    = pResource->GetImpl()->GetBuffer();
        pView->Buffer.View      = VK_NULL_HANDLE;
        pView->Buffer.Size      = (numElements == -1) ? VK_WHOLE_SIZE : static_cast<VkDeviceSize>(numElements * bufferDesc.Stride);
        pView->Buffer.Offset    = (firstElement == -1) ? 0 : static_cast<VkDeviceSize>(firstElement * bufferDesc.Stride);

        // for now, don't create views
        return;

        // Create the view
        switch (type)
        {
        case ResourceViewType::CBV:
            // there is no implementaion for that....
            CauldronCritical(L"Not yet implemented.");
            break;
        case ResourceViewType::BufferSRV:
        case ResourceViewType::BufferUAV:
        {
            // TODO: srv are images too
            VkBufferViewCreateInfo viewInfo{};
            viewInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
            viewInfo.pNext  = nullptr;
            viewInfo.flags  = 0;
            viewInfo.buffer = pResource->GetImpl()->GetBuffer();
            viewInfo.format = VK_FORMAT_UNDEFINED;
            viewInfo.offset = pView->Buffer.Offset;
            viewInfo.range  = pView->Buffer.Size;

            VkResult res = vkCreateBufferView(GetDevice()->GetImpl()->VKDevice(), &viewInfo, nullptr, &pView->Buffer.View);
            CauldronAssert(ASSERT_ERROR, res != VK_SUCCESS && pView->Buffer.View != VK_NULL_HANDLE, L"failed to create buffer image view!");
            break;
        }
        default:
            CauldronCritical(L"Unsupported buffer resource binding requested");
            break;
        }
    }

    void ResourceViewInternal::BindSamplerResource(const Sampler* pSampler, uint32_t index/*=0*/)
    {
        // There's no sampler view in  Vulkan, just copy the handle
        CauldronAssert(ASSERT_ERROR, m_Type == ResourceViewHeapType::GPUSamplerView, L"Invalid view type for the heap type.");
        CauldronAssert(ASSERT_ERROR, index < m_Count, L"Buffer index out of ResourceView bounds.");
        ResourceViewInfoInternal* pView = (ResourceViewInfoInternal*)&m_Views[index];
        pView->Type              = ResourceViewType::Sampler;
        pView->Sampler.Sampler   = pSampler->GetImpl()->VKSampler();
    }

} // namespace cauldron

#endif // #if defined(_VK)
