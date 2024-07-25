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
#include "render/vk/sampler_vk.h"

#include "core/framework.h"

namespace cauldron
{
    void SetFilter(FilterFunc filter, VkSamplerCreateInfo& info)
    {
        switch (filter)
        {
        case cauldron::FilterFunc::MinMagMipPoint:
        case cauldron::FilterFunc::ComparisonMinMagMipPoint:
            info.minFilter = VK_FILTER_NEAREST;
            info.magFilter = VK_FILTER_NEAREST;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case cauldron::FilterFunc::MinMagPointMipLinear:
        case cauldron::FilterFunc::ComparisonMinMagPointMipLinear:
            info.minFilter = VK_FILTER_NEAREST;
            info.magFilter = VK_FILTER_NEAREST;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case cauldron::FilterFunc::MinPointMagLinearMipPoint:
        case cauldron::FilterFunc::ComparisonMinPointMagLinearMipPoint:
            info.minFilter = VK_FILTER_NEAREST;
            info.magFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case cauldron::FilterFunc::MinPointMagMipLinear:
        case cauldron::FilterFunc::ComparisonMinPointMagMipLinear:
            info.minFilter = VK_FILTER_NEAREST;
            info.magFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case cauldron::FilterFunc::MinLinearMagMipPoint:
        case cauldron::FilterFunc::ComparisonMinLinearMagMipPoint:
            info.minFilter = VK_FILTER_LINEAR;
            info.magFilter = VK_FILTER_NEAREST;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case cauldron::FilterFunc::MinLinearMagPointMipLinear:
        case cauldron::FilterFunc::ComparisonMinLinearMagPointMipLinear:
            info.minFilter = VK_FILTER_LINEAR;
            info.magFilter = VK_FILTER_NEAREST;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case cauldron::FilterFunc::MinMagLinearMipPoint:
        case cauldron::FilterFunc::ComparisonMinMagLinearMipPoint:
            info.minFilter = VK_FILTER_LINEAR;
            info.magFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case cauldron::FilterFunc::MinMagMipLinear:
        case cauldron::FilterFunc::ComparisonMinMagMipLinear:
        case cauldron::FilterFunc::Anisotropic:
        case cauldron::FilterFunc::ComparisonAnisotropic:
            info.minFilter = VK_FILTER_LINEAR;
            info.magFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        default:
            CauldronWarning(L"Unknown sampler filter func requested. Returning min mag mip nearest");
            info.minFilter = VK_FILTER_NEAREST;
            info.magFilter = VK_FILTER_NEAREST;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        }
    }

    bool GetCompareEnable(FilterFunc filter)
    {
        switch (filter)
        {
        case cauldron::FilterFunc::MinMagMipPoint:
        case cauldron::FilterFunc::MinMagPointMipLinear:
        case cauldron::FilterFunc::MinPointMagLinearMipPoint:
        case cauldron::FilterFunc::MinPointMagMipLinear:
        case cauldron::FilterFunc::MinLinearMagMipPoint:
        case cauldron::FilterFunc::MinLinearMagPointMipLinear:
        case cauldron::FilterFunc::MinMagLinearMipPoint:
        case cauldron::FilterFunc::MinMagMipLinear:
        case cauldron::FilterFunc::Anisotropic:
            return false;
        case cauldron::FilterFunc::ComparisonMinMagMipPoint:
        case cauldron::FilterFunc::ComparisonMinMagPointMipLinear:
        case cauldron::FilterFunc::ComparisonMinPointMagLinearMipPoint:
        case cauldron::FilterFunc::ComparisonMinPointMagMipLinear:
        case cauldron::FilterFunc::ComparisonMinLinearMagMipPoint:
        case cauldron::FilterFunc::ComparisonMinLinearMagPointMipLinear:
        case cauldron::FilterFunc::ComparisonMinMagLinearMipPoint:
        case cauldron::FilterFunc::ComparisonMinMagMipLinear:
        case cauldron::FilterFunc::ComparisonAnisotropic:
            return true;
        default:
            return false;
        }
    }

    VkSamplerAddressMode Convert(AddressMode address)
    {
        switch (address)
        {
        case AddressMode::Wrap:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case AddressMode::Mirror:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case AddressMode::Clamp:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case AddressMode::Border:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case AddressMode::MirrorOnce:
            return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        }
    }

    VkCompareOp Convert(const ComparisonFunc func)
    {
        switch (func)
        {
        case ComparisonFunc::Never:
            return VK_COMPARE_OP_NEVER;
        case ComparisonFunc::Less:
            return VK_COMPARE_OP_LESS;
        case ComparisonFunc::Equal:
            return VK_COMPARE_OP_EQUAL;
        case ComparisonFunc::LessEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case ComparisonFunc::Greater:
            return VK_COMPARE_OP_GREATER;
        case ComparisonFunc::NotEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case ComparisonFunc::GreaterEqual:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case ComparisonFunc::Always:
            return VK_COMPARE_OP_ALWAYS;
        default:
            return VK_COMPARE_OP_NEVER;
        }
    }

    VkSamplerCreateInfo Convert(const SamplerDesc& desc)
    {
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = 0;
        SetFilter(desc.Filter, info);
        info.addressModeU = Convert(desc.AddressU);
        info.addressModeV = Convert(desc.AddressV);
        info.addressModeW = Convert(desc.AddressW);
        info.minLod = desc.MinLOD;
        info.maxLod = desc.MaxLOD;
        info.anisotropyEnable = desc.Filter == FilterFunc::Anisotropic;
        info.maxAnisotropy = static_cast<float>(desc.MaxAnisotropy);

        info.compareEnable = GetCompareEnable(desc.Filter);
        info.compareOp = Convert(desc.Comparison);

        info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        info.unnormalizedCoordinates = VK_FALSE;;

        return info;
    }

    VkSampler VKStaticSampler(const SamplerDesc& desc)
    {
        VkSamplerCreateInfo info = Convert(desc);
        VkSampler sampler;
        VkResult res = vkCreateSampler(GetDevice()->GetImpl()->VKDevice(), &info, nullptr, &sampler);

        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS && sampler != VK_NULL_HANDLE, L"Unable to create the sampler");
        return sampler;
    }

    Sampler* Sampler::CreateSampler(const wchar_t* name, const SamplerDesc& desc)
    {
        return new SamplerInternal(name, desc);
    }

    SamplerInternal::SamplerInternal(const wchar_t* name, const SamplerDesc& desc) :
        Sampler(name, desc)
    {
        m_Sampler = VKStaticSampler(desc);
    }

    SamplerInternal::~SamplerInternal()
    {
        vkDestroySampler(GetDevice()->GetImpl()->VKDevice(), m_Sampler, nullptr);
    }

} // namespace cauldron

#endif // #if defined(_VK)
