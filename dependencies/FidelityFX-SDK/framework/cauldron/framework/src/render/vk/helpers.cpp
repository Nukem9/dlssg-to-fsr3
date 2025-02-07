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

#include "helpers.h"
#include "misc/assert.h"

namespace cauldron
{
    VkFormat GetVkFormat(ResourceFormat format)
    {
        switch (format)
        {
        case cauldron::ResourceFormat::Unknown:
            return VK_FORMAT_UNDEFINED;

            // 8-bit
        case ResourceFormat::R8_SINT:
            return VK_FORMAT_R8_SINT;
        case ResourceFormat::R8_UINT:
            return VK_FORMAT_R8_UINT;
        case ResourceFormat::R8_UNORM:
            return VK_FORMAT_R8_UNORM;

            // 16-bit
        case ResourceFormat::R16_SINT:
            return VK_FORMAT_R16_SINT;
        case ResourceFormat::R16_UINT:
            return VK_FORMAT_R16_UINT;
        case ResourceFormat::R16_FLOAT:
            return VK_FORMAT_R16_SFLOAT;
        case ResourceFormat::R16_UNORM:
            return VK_FORMAT_R16_UNORM;
        case ResourceFormat::R16_SNORM:
            return VK_FORMAT_R16_SNORM;
        case ResourceFormat::RG8_SINT:
            return VK_FORMAT_R8G8_SINT;
        case ResourceFormat::RG8_UINT:
            return VK_FORMAT_R8G8_UINT;
        case ResourceFormat::RG8_UNORM:
            return VK_FORMAT_R8G8_UNORM;

            // 32-bit
        case ResourceFormat::R32_SINT:
            return VK_FORMAT_R32_SINT;
        case ResourceFormat::R32_UINT:
            return VK_FORMAT_R32_UINT;
        case ResourceFormat::RGBA8_SINT:
            return VK_FORMAT_R8G8B8A8_SINT;
        case ResourceFormat::BGRA8_SINT:
            return VK_FORMAT_B8G8R8A8_SINT;
        case ResourceFormat::RGBA8_UINT:
            return VK_FORMAT_R8G8B8A8_UINT;
        case ResourceFormat::BGRA8_UINT:
            return VK_FORMAT_B8G8R8A8_UINT;
        case ResourceFormat::RGBA8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case ResourceFormat::RGBA8_SNORM:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case ResourceFormat::BGRA8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case ResourceFormat::RGBA8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case ResourceFormat::BGRA8_SRGB:
            return VK_FORMAT_B8G8R8A8_SRGB;
        case ResourceFormat::RGBA8_TYPELESS:
            CauldronError(L"Implement typeless texture format support");
            return VK_FORMAT_R8G8B8A8_USCALED;
        case ResourceFormat::BGRA8_TYPELESS:
            CauldronError(L"Implement typeless texture format support");
            return VK_FORMAT_B8G8R8A8_USCALED;
        case ResourceFormat::RGB10A2_UNORM:
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case ResourceFormat::RG11B10_FLOAT:
            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case ResourceFormat::RGB9E5_SHAREDEXP:
            return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
        case ResourceFormat::RG16_SINT:
            return VK_FORMAT_R16G16_SINT;
        case ResourceFormat::RG16_UINT:
            return VK_FORMAT_R16G16_UINT;
        case ResourceFormat::RG16_FLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case ResourceFormat::R32_FLOAT:
            return VK_FORMAT_R32_SFLOAT;

            // 64-bit
        case ResourceFormat::RGBA16_SINT:
            return VK_FORMAT_R16G16B16A16_SINT;
        case ResourceFormat::RGBA16_UINT:
            return VK_FORMAT_R16G16B16A16_UINT;
        case ResourceFormat::RGBA16_UNORM:
            return VK_FORMAT_R16G16B16A16_UNORM;
        case ResourceFormat::RGBA16_SNORM:
            return VK_FORMAT_R16G16B16A16_SNORM;
        case ResourceFormat::RGBA16_FLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case ResourceFormat::RG32_SINT:
            return VK_FORMAT_R32G32_SINT;
        case ResourceFormat::RG32_UINT:
            return VK_FORMAT_R32G32_UINT;
        case cauldron::ResourceFormat::RG32_FLOAT:
            return VK_FORMAT_R32G32_SFLOAT;

            // 96-bit
        case ResourceFormat::RGB32_SINT:
            return VK_FORMAT_R32G32B32_SINT;
        case ResourceFormat::RGB32_UINT:
            return VK_FORMAT_R32G32B32_UINT;
        case ResourceFormat::RGB32_FLOAT:
            return VK_FORMAT_R32G32B32_SFLOAT;

            // 128-bit
        case ResourceFormat::RGBA32_SINT:
            return VK_FORMAT_R32G32B32A32_SINT;
        case ResourceFormat::RGBA32_UINT:
            return VK_FORMAT_R32G32B32A32_UINT;
        case ResourceFormat::RGBA32_FLOAT:
        case ResourceFormat::RGBA32_TYPELESS:
            return VK_FORMAT_R32G32B32A32_SFLOAT;

            // Depth
        case ResourceFormat::D16_UNORM:
            return VK_FORMAT_D16_UNORM;
        case ResourceFormat::D32_FLOAT:
            return VK_FORMAT_D32_SFLOAT;

            // compressed
        case ResourceFormat::BC1_UNORM:
            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case ResourceFormat::BC1_SRGB:
            return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case ResourceFormat::BC2_UNORM:
            return VK_FORMAT_BC2_UNORM_BLOCK;
        case ResourceFormat::BC2_SRGB:
            return VK_FORMAT_BC2_SRGB_BLOCK;
        case ResourceFormat::BC3_UNORM:
            return VK_FORMAT_BC3_UNORM_BLOCK;
        case ResourceFormat::BC3_SRGB:
            return VK_FORMAT_BC3_SRGB_BLOCK;
        case ResourceFormat::BC4_UNORM:
            return VK_FORMAT_BC4_UNORM_BLOCK;
        case ResourceFormat::BC4_SNORM:
            return VK_FORMAT_BC4_SNORM_BLOCK;
        case ResourceFormat::BC5_UNORM:
            return VK_FORMAT_BC5_UNORM_BLOCK;
        case ResourceFormat::BC5_SNORM:
            return VK_FORMAT_BC5_SNORM_BLOCK;
        case ResourceFormat::BC6_UNSIGNED:
            return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case ResourceFormat::BC6_SIGNED:
            return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case ResourceFormat::BC7_UNORM:
            return VK_FORMAT_BC7_UNORM_BLOCK;
        case ResourceFormat::BC7_SRGB:
            return VK_FORMAT_BC7_SRGB_BLOCK;

        default:
            CauldronError(L"Cannot convert unknown format.");
            return VK_FORMAT_UNDEFINED;
        };
    }

    VkDeviceSize GetBlockSize(ResourceFormat format)
    {
        switch (format)
        {
        case ResourceFormat::Unknown:
            return 0;

            // 16-bit
        case ResourceFormat::R16_FLOAT:
            return 2;

            // 32-bit
        case ResourceFormat::RGBA8_UNORM:
        case ResourceFormat::RGBA8_SRGB:
        case ResourceFormat::RGB10A2_UNORM:
        case ResourceFormat::RG16_FLOAT:
        case ResourceFormat::R32_FLOAT:
        case ResourceFormat::RG11B10_FLOAT:
        case ResourceFormat::RGB9E5_SHAREDEXP:
            return 4;

            // 64-bit
        case ResourceFormat::RGBA16_UNORM:
        case ResourceFormat::RGBA16_SNORM:
        case ResourceFormat::RGBA16_FLOAT:
        case ResourceFormat::RG32_FLOAT:
            return 8;

            // 128-bit
        case ResourceFormat::RGBA32_FLOAT:
        case ResourceFormat::RGBA32_SINT:
        case ResourceFormat::RGBA32_UINT:
        case ResourceFormat::RGBA32_TYPELESS:
            return 16;

            // Depth
        case ResourceFormat::D16_UNORM:
            return 16;
        case ResourceFormat::D32_FLOAT:
            return 32;

            // compressed
        case ResourceFormat::BC1_UNORM:
        case ResourceFormat::BC1_SRGB:
            return 8;
        case ResourceFormat::BC2_UNORM:
        case ResourceFormat::BC2_SRGB:
        case ResourceFormat::BC3_UNORM:
        case ResourceFormat::BC3_SRGB:
            return 16;
        case ResourceFormat::BC4_UNORM:
        case ResourceFormat::BC4_SNORM:
            return 8;
        case ResourceFormat::BC5_UNORM:
        case ResourceFormat::BC5_SNORM:
            return 16;
        case ResourceFormat::BC7_UNORM:
        case ResourceFormat::BC7_SRGB:
            return 16;

        default:
            CauldronError(L"Cannot calculate block size of unknown format.");
            return 0;
        };
    }

    VkFormat VKToGamma(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_R8G8B8A8_UNORM:       return VK_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:       return VK_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:  return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case VK_FORMAT_BC2_UNORM_BLOCK:      return VK_FORMAT_BC2_SRGB_BLOCK;
        case VK_FORMAT_BC3_UNORM_BLOCK:      return VK_FORMAT_BC3_SRGB_BLOCK;
        case VK_FORMAT_BC7_UNORM_BLOCK:      return VK_FORMAT_BC7_SRGB_BLOCK;
        }

        return format;
    }

    VkFormat VKFromGamma(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_R8G8B8A8_SRGB:       return VK_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:       return VK_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:  return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case VK_FORMAT_BC2_SRGB_BLOCK:      return VK_FORMAT_BC2_UNORM_BLOCK;
        case VK_FORMAT_BC3_SRGB_BLOCK:      return VK_FORMAT_BC3_UNORM_BLOCK;
        case VK_FORMAT_BC7_SRGB_BLOCK:      return VK_FORMAT_BC7_UNORM_BLOCK;
        }

        return format;
    }

    bool IsDepthFormat(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;

        default:
            return false;
        }
    }

    bool HasStencilComponent(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_S8_UINT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;

        default:
            return false;
        }
    }

    VkImageAspectFlags GetImageAspectMask(VkFormat format)
    {
        if (IsDepthFormat(format))
        {
            if (HasStencilComponent(format))
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            else
                return VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        else
        {
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    VkDescriptorType ConvertToDescriptorType(BindingType type)
    {
        switch (type)
        {
        case BindingType::TextureSRV:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case BindingType::TextureUAV:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case BindingType::BufferSRV:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case BindingType::BufferUAV:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case BindingType::CBV:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case BindingType::RootConstant:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case BindingType::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case BindingType::AccelStructRT:
            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

        default:
            CauldronCritical(L"Unsupported binding type");
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    bool IsBCFormat(ResourceFormat format)
    {
        switch (format)
        {
        case ResourceFormat::BC1_UNORM:
        case ResourceFormat::BC1_SRGB:
        case ResourceFormat::BC2_UNORM:
        case ResourceFormat::BC2_SRGB:
        case ResourceFormat::BC3_UNORM:
        case ResourceFormat::BC3_SRGB:
        case ResourceFormat::BC4_UNORM:
        case ResourceFormat::BC4_SNORM:
        case ResourceFormat::BC5_UNORM:
        case ResourceFormat::BC5_SNORM:
        case ResourceFormat::BC6_UNSIGNED:
        case ResourceFormat::BC6_SIGNED:
        case ResourceFormat::BC7_UNORM:
        case ResourceFormat::BC7_SRGB:
            return true;

        default:
            return false;
        };
    }

    VkImageType GetImageType(const TextureDesc& desc)
    {
        switch (desc.Dimension)
        {
        case TextureDimension::Texture1D:
            return VK_IMAGE_TYPE_1D;
        case TextureDimension::Texture2D:
        case TextureDimension::CubeMap:
            return VK_IMAGE_TYPE_2D;
        case TextureDimension::Texture3D:
            return VK_IMAGE_TYPE_3D;
        default:
            CauldronError(L"Incorrect texture dimension");
            return VK_IMAGE_TYPE_MAX_ENUM;
        }
    }

    VkImageUsageFlags GetUsage(ResourceFlags cauldronFlags)
    {
        uint32_t flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        if (static_cast<bool>(cauldronFlags & ResourceFlags::AllowRenderTarget))
        {
            // add color attachment flag
            flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            // keep transfer dest flag to clear
        }
        if (static_cast<bool>(cauldronFlags & ResourceFlags::AllowDepthStencil))
        {
            // add depth stencil attachment flag
            flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            // keep transfer dest flag to clear
        }
        if (static_cast<bool>(cauldronFlags & ResourceFlags::AllowUnorderedAccess))
        {
            flags |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
        if (static_cast<bool>(cauldronFlags & ResourceFlags::DenyShaderResource))
        {
            flags &= ~(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        }
        if (static_cast<bool>(cauldronFlags & ResourceFlags::AllowShadingRate))
        {
            flags |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
        }

        return static_cast<VkImageUsageFlags>(flags);
    }

    MipInformation GetMipInformation(uint32_t width, uint32_t height, ResourceFormat format)
    {
        MipInformation info;
        VkDeviceSize   blockSize = GetBlockSize(format);
        VkDeviceSize   lineSize  = 0;
        VkDeviceSize   totalSize = 0;
        if (IsBCFormat(format))
        {
            info.Stride    = blockSize * DivideRoundingUp(width, 4);
            info.Rows      = DivideRoundingUp(height, 4);
            info.TotalSize = info.Stride * info.Rows;
        }
        else
        {
            info.Stride    = blockSize * width;
            info.Rows      = height;
            info.TotalSize = info.Stride * info.Rows;
        }
        return info;
    }

    VkDeviceSize GetTotalTextureSize(uint32_t width, uint32_t height, ResourceFormat format, uint32_t mipCount)
    {
        VkDeviceSize totalSize = 0;
        for (uint32_t i = 0; i < mipCount; ++i)
        {
            totalSize += GetMipInformation(width, height, format).TotalSize;
            width /= 2;
            height /= 2;
        }
        return totalSize;
    }

    uint32_t CalculateMipLevels(const TextureDesc& desc)
    {
        if (desc.MipLevels == 0)
        {
            if (desc.Dimension == TextureDimension::Texture1D)
            {
                uint32_t w        = desc.Width;
                uint32_t mipCount = 1;
                while (w > 1)
                {
                    w >>= 1;
                    ++mipCount;
                }
                return mipCount;
            }
            else if (desc.Dimension == TextureDimension::Texture2D || desc.Dimension == TextureDimension::CubeMap)
            {
                uint32_t w        = desc.Width;
                uint32_t h        = desc.Height;
                uint32_t mipCount = 1;
                while (w > 1 || h > 1)
                {
                    w >>= 1;
                    h >>= 1;
                    ++mipCount;
                }
                return mipCount;
            }
            else if (desc.Dimension == TextureDimension::Texture3D)
            {
                uint32_t w        = desc.Width;
                uint32_t h        = desc.Height;
                uint32_t d        = desc.DepthOrArraySize;
                uint32_t mipCount = 1;
                while (w > 1 || h > 1 || d > 1)
                {
                    w >>= 1;
                    h >>= 1;
                    d >>= 1;
                    ++mipCount;
                }
                return mipCount;
            }
            else
            {
                CauldronCritical(L"Cannot calculate mip count for unknown texture dimension");
                return 0;
            }
        }
        else
        {
            return desc.MipLevels;
        }
    }

    uint32_t CalculateSizeAtMipLevel(const uint32_t size, const uint32_t mipLevel)
    {
        return std::max(size >> mipLevel, 1u);
    }

    VkImageCreateInfo ConvertTextureDesc(const TextureDesc& desc)
    {
        VkImageType imageType = GetImageType(desc);

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType         = imageType;
        imageInfo.extent.width      = desc.Width;
        imageInfo.extent.height     = desc.Height;
        imageInfo.extent.depth      = imageType == VK_IMAGE_TYPE_3D ? desc.DepthOrArraySize : 1;
        imageInfo.mipLevels         = CalculateMipLevels(desc);
        imageInfo.arrayLayers       = imageType == VK_IMAGE_TYPE_3D ? 1 : desc.DepthOrArraySize;
        imageInfo.format            = GetVkFormat(desc.Format);
        imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage             = GetUsage(desc.Flags);
        imageInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags             = 0;

        if (desc.Dimension == TextureDimension::CubeMap)
        {
            CauldronAssert(ASSERT_CRITICAL, desc.DepthOrArraySize % 6 == 0, L"The number of slices of the cubemap texture isn't a mutiple of 6");
            imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        // if a sRGB texture will have a UAV view on it, we need to:
        //  - use a non sRGB format for it
        //  - use this format for UAV/storage view
        //  - use sRGB view for SRV and RTV
        if (static_cast<bool>(desc.Flags & ResourceFlags::AllowUnorderedAccess) && IsSRGB(desc.Format))
        {
            imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            imageInfo.format = VKFromGamma(imageInfo.format);
        }

        return imageInfo;
    }

    VkBufferCreateInfo ConvertBufferDesc(const BufferDesc& desc)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.pNext              = nullptr;
        bufferInfo.flags              = 0;
        bufferInfo.size               = static_cast<VkDeviceSize>(desc.Size);
        bufferInfo.usage              = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        switch (desc.Type)
        {
        case BufferType::Vertex:
            bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            break;
        case BufferType::Index:
            bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            break;
        case BufferType::Data:
            bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            break;
        case BufferType::AccelerationStructure:
            bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            break;
        case BufferType::Constant:
            // Will support when needed
            //    bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            //    break;
        default:
            CauldronError(L"Unsupported buffer type.");
            break;
        }

        // Check if this buffer was flagged for indirect argument usage
        if (static_cast<bool>(desc.Flags & ResourceFlags::AllowIndirect))
        {
            bufferInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        }
        if (static_cast<bool>(desc.Flags & ResourceFlags::AllowConstantBuffer))
        {
            bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }

        bufferInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.queueFamilyIndexCount = 0;
        bufferInfo.pQueueFamilyIndices   = nullptr;

        return bufferInfo;
    }
} // namespace cauldron
