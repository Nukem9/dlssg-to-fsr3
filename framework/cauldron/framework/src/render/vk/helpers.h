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

#include "render/renderdefines.h"
#include "render/buffer.h"
#include "render/texture.h"
#include <vulkan/vulkan.h>

namespace cauldron
{
    VkFormat GetVkFormat(ResourceFormat format);
    VkDeviceSize GetBlockSize(ResourceFormat format);
    VkFormat VKToGamma(VkFormat format);
    VkFormat VKFromGamma(VkFormat format);

    bool IsDepthFormat(VkFormat format);
    bool HasStencilComponent(VkFormat format);
    VkImageAspectFlags GetImageAspectMask(VkFormat format);

    VkDescriptorType ConvertToDescriptorType(BindingType type);

    struct MipInformation
    {
        VkDeviceSize TotalSize;
        VkDeviceSize Stride;
        uint32_t     Rows;
    };

    VkDeviceSize      GetTotalTextureSize(uint32_t width, uint32_t height, ResourceFormat format, uint32_t mipCount);
    MipInformation    GetMipInformation(uint32_t width, uint32_t height, ResourceFormat format);
    uint32_t          CalculateMipLevels(const TextureDesc& desc);
    uint32_t          CalculateSizeAtMipLevel(const uint32_t size, const uint32_t mipLevel);
    VkImageCreateInfo ConvertTextureDesc(const TextureDesc& desc);

    VkBufferCreateInfo ConvertBufferDesc(const BufferDesc& desc);
}
