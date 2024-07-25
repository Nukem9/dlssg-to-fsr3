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
#include "render/sampler.h"

#include <vulkan/vulkan.h>

namespace cauldron
{
    void SetFilter(FilterFunc filter, VkSamplerCreateInfo& info);
    bool GetCompareEnable(FilterFunc filter);
    VkSamplerAddressMode Convert(AddressMode address);
    VkCompareOp Convert(const ComparisonFunc func);
    VkSamplerCreateInfo Convert(const SamplerDesc& desc);
    VkSampler VKStaticSampler(const SamplerDesc& desc);

    class SamplerInternal final : public Sampler
    {
    public:
        VkSampler VKSampler() const { return m_Sampler; }

        SamplerInternal* GetImpl() override { return this; }
        const SamplerInternal* GetImpl() const override { return this; }

    private:
        friend class Sampler;
        SamplerInternal(const wchar_t* name, const SamplerDesc& desc);
        virtual ~SamplerInternal();

    private:
        VkSampler m_Sampler = VK_NULL_HANDLE;
    };

} // namespace cauldron

#endif // #if defined(_VK)
