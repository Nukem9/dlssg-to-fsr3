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

#include "render/parameterset.h"
#include <vulkan/vulkan.h>

namespace cauldron
{
    class ParameterSetInternal final : public ParameterSet
    {
    public:
        void SetRootConstantBufferResource(const GPUResource* pResource, const size_t size, uint32_t rootParameterIndex) override;
        void SetTextureSRV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex = 0, int32_t mip = -1, int32_t arraySize = -1, int32_t firstSlice = -1) override;
        void SetTextureUAV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex = 0, int32_t mip = -1, int32_t arraySize = -1, int32_t firstSlice = -1) override;
        void SetBufferSRV(const Buffer* pBuffer, uint32_t slotIndex = 0, uint32_t firstElement = -1, uint32_t numElements = -1) override;
        void SetBufferUAV(const Buffer* pBuffer, uint32_t slotIndex = 0, uint32_t firstElement = -1, uint32_t numElements = -1) override;
        void SetSampler(const Sampler* pSampler, uint32_t slotIndex = 0) override;
        void SetAccelerationStructure(const TLAS* pTLAS, uint32_t slotIndex = 0) override;

        void UpdateRootConstantBuffer(const BufferAddressInfo* pRootConstantBuffer, uint32_t rootBufferIndex) override;
        void UpdateRoot32BitConstant(const uint32_t numEntries, const uint32_t* pConstData, uint32_t rootBufferIndex) override;
        void Bind(CommandList* pCmdList, const PipelineObject* pPipeline) override;

        virtual void OnResourceResized() override;

    private:
        friend class ParameterSet;
        ParameterSetInternal(RootSignature* pRootSignature, ResourceView* pImmediateViews);
        virtual ~ParameterSetInternal();

        // Returns the right resource table index in the list of binding descriptions
        int32_t GetResourceTableIndex(BindingType bindType, uint32_t slotIndex, const wchar_t* bindName) override;

        struct VulkanBinding
        {
            uint32_t BindingIndex;
            uint32_t DstArrayElement;
        };
        VulkanBinding GetVulkanBinding(BindingType bindType, uint32_t slotIndex, const wchar_t* bindName);

        ResourceViewInfo GetImmediateResourceViewInfo(uint32_t shaderRegister, BindingType bindingType);
        void UpdateDescriptorSet(VkDescriptorImageInfo info, uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType);
        void UpdateDescriptorSet(VkDescriptorBufferInfo info, uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType);

        void UpdateDescriptorSetIndex();
        void UpdateDescriptorSets(VkWriteDescriptorSet write);

    private:
        static constexpr uint32_t cMaxDescriptorSets = 3;

        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        std::vector<uint32_t> m_RootConstantBufferOffsets;

        enum class UsageState
        {
            Unbound, // This state is only available when the parameter set has been vreated but has never been bound once. Every Set*** will modify all the descriptor sets
            Bound, // when bound, any Set*** will automatically apply to the next descriptor set
            Updating // in this state, the descriptor set index won't be modified
        };
        VkDescriptorSet                             m_DescriptorSets[cMaxDescriptorSets]           = {VK_NULL_HANDLE};
        uint32_t                                    m_CurrentSetIndex                              = 0;
        UsageState                                  m_UsageState                                   = UsageState::Unbound;

        uint32_t                                    m_PushConstantsMem[MAX_PUSH_CONSTANTS_ENTRIES] = {0};
        uint32_t                                    m_CurrentPushConstantMemOffset                 = 0;
        std::vector<std::pair<uint32_t, uint32_t*>> m_PushConstantEntries                          = {};
    };

} // namespace cauldron

#endif // #if defined(_VK)
