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
#include "render/vk/rootsignaturedesc_vk.h"
#include "render/vk/rootsignature_vk.h"
#include "render/vk/sampler_vk.h"

#include "core/framework.h"
#include "misc/assert.h"
#include "helpers.h"

namespace cauldron
{
    RootSignature* RootSignature::CreateRootSignature(const wchar_t* name, const RootSignatureDesc& desc)
    {
        RootSignatureInternal* pNewSignature = new RootSignatureInternal(name);

        // Build in one step before returning
        pNewSignature->Build(desc);

        return pNewSignature;
    }

    RootSignatureInternal::RootSignatureInternal(const wchar_t* name) :
        RootSignature(name)
    {

    }

    RootSignatureInternal::~RootSignatureInternal()
    {
        DestroyImmutableSamplers();
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        vkDestroyDescriptorSetLayout(pDevice->VKDevice(), m_DescriptorSetLayout, nullptr);
    }

    void RootSignatureInternal::DestroyImmutableSamplers()
    {
        VkDevice device = GetDevice()->GetImpl()->VKDevice();
        for (auto it = m_ImmutableSamplers.begin(); it != m_ImmutableSamplers.end(); ++it)
            vkDestroySampler(device, *it, nullptr);
        m_ImmutableSamplers.clear();
    }

    uint32_t RootSignatureInternal::GetPushConstantIndex(uint32_t slotIndex) const
    {
        for (uint32_t i = 0; i < m_PushConstantRegisters.size(); ++i)
        {
            if (slotIndex == m_PushConstantRegisters[i])
                return i;
        }

        CauldronCritical(L"Push constant register not found.");
        return 0;
    }

    void RootSignatureInternal::Build(const RootSignatureDesc& desc)
    {
        m_PipelineType = desc.GetPipelineType();

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings;
        descriptorSetLayoutBindings.resize(desc.m_pSignatureDescImpl->m_Bindings.size() + desc.m_pSignatureDescImpl->m_ImmutableSamplersBindings.size());
        m_BindingDescriptions.reserve(desc.m_pSignatureDescImpl->m_Bindings.size());

        std::vector<VkDescriptorBindingFlags> bindingFlags;
        bindingFlags.resize(desc.m_pSignatureDescImpl->m_Bindings.size() + desc.m_pSignatureDescImpl->m_ImmutableSamplersBindings.size());

        // destroy the old samplers
        DestroyImmutableSamplers();
        // create the samplers
        m_ImmutableSamplers.resize(desc.m_pSignatureDescImpl->m_ImmutableSamplers.size());
        uint32_t immutableSamplerOffset = 0;
        for (auto s : desc.m_pSignatureDescImpl->m_ImmutableSamplers)
            m_ImmutableSamplers[immutableSamplerOffset++] = VKStaticSampler(s);

        // create the descriptor set layout binding and compute the offset in the view
        uint32_t i = 0;

        std::vector<BindingDesc> texSRVBufferDescs  = {};
        std::vector<BindingDesc> texUAVBufferDescs  = {};
        std::vector<BindingDesc> bufSRVBufferDescs  = {};
        std::vector<BindingDesc> rtasBufferDescs    = {};
        std::vector<BindingDesc> bufUAVBufferDescs  = {};
        std::vector<BindingDesc> cbvBufferDescs     = {};
        std::vector<BindingDesc> samplerBufferDescs = {};
        std::vector<BindingDesc> rootConstantDescs  = {};

        // TODO: reserve vectors?

        for (auto b : desc.m_pSignatureDescImpl->m_Bindings)
        {
            BindingDesc bindingDesc;
            bindingDesc.Type = b.Type;
            bindingDesc.BaseShaderRegister = b.BaseShaderRegister;
            bindingDesc.BindingIndex = b.BindingIndex;
            bindingDesc.Count = b.Count;
            switch (b.Type)
            {
            case BindingType::TextureSRV:
                texSRVBufferDescs.push_back(bindingDesc);
                break;
            case BindingType::TextureUAV:
                texUAVBufferDescs.push_back(bindingDesc);
                break;
            case BindingType::BufferSRV:
                bufSRVBufferDescs.push_back(bindingDesc);
                break;
            case BindingType::AccelStructRT:
                rtasBufferDescs.push_back(bindingDesc);
                break;
            case BindingType::BufferUAV:
                bufUAVBufferDescs.push_back(bindingDesc);
                break;
            case BindingType::CBV:
                cbvBufferDescs.push_back(bindingDesc);
                break;
            case BindingType::Sampler:
                samplerBufferDescs.push_back(bindingDesc);
                break;
            case BindingType::RootConstant:
                rootConstantDescs.push_back(bindingDesc);
                break;
            case BindingType::Root32BitConstant:
            default:
                CauldronCritical(L"Unsupported binding type");
                break;
            }

            descriptorSetLayoutBindings[i].binding = b.BindingIndex;
            descriptorSetLayoutBindings[i].descriptorType = ConvertToDescriptorType(b.Type);
            descriptorSetLayoutBindings[i].descriptorCount = b.Count;
            descriptorSetLayoutBindings[i].stageFlags = b.StageFlags;
            descriptorSetLayoutBindings[i].pImmutableSamplers = nullptr;

            bindingFlags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT; 
            ++i;
        }

        immutableSamplerOffset = 0; // reset offset
        for (auto b : desc.m_pSignatureDescImpl->m_ImmutableSamplersBindings)
        {
            descriptorSetLayoutBindings[i].binding = b.BindingIndex;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            descriptorSetLayoutBindings[i].descriptorCount = b.Count;
            descriptorSetLayoutBindings[i].stageFlags = b.StageFlags;
            descriptorSetLayoutBindings[i].pImmutableSamplers = &m_ImmutableSamplers[immutableSamplerOffset];
            bindingFlags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            ++i;
            immutableSamplerOffset += b.Count;
        }

        // allocate the constant ranges
        m_PushConstantRanges.clear();
        m_PushConstantRanges.resize(desc.m_pSignatureDescImpl->m_PushConstantInfo.size());
        m_PushConstantRegisters.clear();
        m_PushConstantRegisters.resize(desc.m_pSignatureDescImpl->m_PushConstantInfo.size());
        uint32_t pushConstantRangeIndex = 0;
        for (auto b : desc.m_pSignatureDescImpl->m_PushConstantInfo)
        {
            m_PushConstantRanges[pushConstantRangeIndex].stageFlags = b.StageFlags;
            m_PushConstantRanges[pushConstantRangeIndex].offset     = 0;
            m_PushConstantRanges[pushConstantRangeIndex].size       = sizeof(uint32_t) * b.Count;

            m_PushConstantRegisters[pushConstantRangeIndex] = b.BaseShaderRegister;

            ++pushConstantRangeIndex;
        }

        // create the descriptor set layout
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
        bindingFlagsInfo.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &bindingFlagsInfo;
        layoutInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
        layoutInfo.pBindings = descriptorSetLayoutBindings.data();

        VkResult res = vkCreateDescriptorSetLayout(GetDevice()->GetImpl()->VKDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout);
        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Failed to create the descriptor set layout");

        // Insert all of our binding descriptions in the order we want to track them later

        // Texture SRV
        if (texSRVBufferDescs.size())
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::TextureSRV)] = static_cast<uint32_t>(m_BindingDescriptions.size());
        m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(texSRVBufferDescs.begin()), std::make_move_iterator(texSRVBufferDescs.end()));

        // Texture UAV
        if (texUAVBufferDescs.size())
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::TextureUAV)] = static_cast<uint32_t>(m_BindingDescriptions.size());
        m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(texUAVBufferDescs.begin()), std::make_move_iterator(texUAVBufferDescs.end()));

        // Buffer SRV
        if (bufSRVBufferDescs.size())
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::BufferSRV)] = static_cast<uint32_t>(m_BindingDescriptions.size());
        m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(bufSRVBufferDescs.begin()), std::make_move_iterator(bufSRVBufferDescs.end()));

        // RTAccelStruct
        if (rtasBufferDescs.size())
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::AccelStructRT)] = static_cast<uint32_t>(m_BindingDescriptions.size());
        m_BindingDescriptions.insert(
            m_BindingDescriptions.end(), std::make_move_iterator(rtasBufferDescs.begin()), std::make_move_iterator(rtasBufferDescs.end()));

        // Buffer UAV
        if (bufUAVBufferDescs.size())
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::BufferUAV)] = static_cast<uint32_t>(m_BindingDescriptions.size());
        m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(bufUAVBufferDescs.begin()), std::make_move_iterator(bufUAVBufferDescs.end()));

        // CBV sets
        if (cbvBufferDescs.size())
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::CBV)] = static_cast<uint32_t>(m_BindingDescriptions.size());
        m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(cbvBufferDescs.begin()), std::make_move_iterator(cbvBufferDescs.end()));

        // Sampler sets
        if (samplerBufferDescs.size())
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::Sampler)] = static_cast<uint32_t>(m_BindingDescriptions.size());
        m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(samplerBufferDescs.begin()), std::make_move_iterator(samplerBufferDescs.end()));

        // Root constant sets
        if (rootConstantDescs.size())
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::RootConstant)] = static_cast<uint32_t>(m_BindingDescriptions.size());
        m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(rootConstantDescs.begin()), std::make_move_iterator(rootConstantDescs.end()));
    }

    void AddDescriptorTypeToPool(std::vector<VkDescriptorPoolSize>& poolSizes, VkDescriptorType type, uint32_t count)
    {
        auto it = poolSizes.begin();
        for (; it != poolSizes.end(); ++it)
        {
            if (type == it->type)
            {
                it->descriptorCount += count;
                return;
            }
        }

        if (it == poolSizes.end())
        {
            VkDescriptorPoolSize poolSize;
            poolSize.type = type;
            poolSize.descriptorCount = count;
            poolSizes.push_back(poolSize);
        }
    }

    VkDescriptorPool RootSignatureInternal::GenerateDescriptorPool(uint32_t numSets) const
    {
        std::vector<VkDescriptorPoolSize> poolSizes;
        for (auto b : m_BindingDescriptions)
            AddDescriptorTypeToPool(poolSizes, ConvertToDescriptorType(b.Type), numSets * b.Count);

        // add immutable samplers
        if (m_ImmutableSamplers.size() > 0)
            AddDescriptorTypeToPool(poolSizes, VK_DESCRIPTOR_TYPE_SAMPLER, numSets * static_cast<uint32_t>(m_ImmutableSamplers.size()));

        // no need to add push constants

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.pNext = nullptr;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = numSets;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkResult res = vkCreateDescriptorPool(GetDevice()->GetImpl()->VKDevice(), &poolInfo, nullptr, &descriptorPool);
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS, L"failed to create descriptor pool");

        return descriptorPool;
    }

} // namespace cauldron

#endif // #if defined(_VK)
