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

#include "render/rootsignature.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace cauldron
{
    class RootSignatureInternal final : public RootSignature
    {
    public:
        uint32_t GetPushConstantIndex(uint32_t slotIndex) const;

        const std::vector<VkPushConstantRange>& VKPushConstantRanges() const { return m_PushConstantRanges; }
        const VkDescriptorSetLayout VKDescriptorSetLayout() const { return m_DescriptorSetLayout; }
        VkDescriptorSetLayout VKDescriptorSetLayout() { return m_DescriptorSetLayout; }
        VkDescriptorPool GenerateDescriptorPool(uint32_t numSets = 1) const;

        RootSignatureInternal* GetImpl() override { return this; }
        const RootSignatureInternal* GetImpl() const override { return this; }

    private:
        friend class RootSignature;
        RootSignatureInternal(const wchar_t* name);
        virtual ~RootSignatureInternal();

        void Build(const RootSignatureDesc& desc) override;

        void DestroyImmutableSamplers();

    private:

        // Internal members
        std::vector<VkSampler> m_ImmutableSamplers;  // we need to keep a list of them as they are allocated on the device and need to be destroyed later
        std::vector<uint32_t>  m_PushConstantRegisters;
        std::vector<VkPushConstantRange> m_PushConstantRanges;
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    };

} // namespace cauldron

#endif // #if defined(_VK)
