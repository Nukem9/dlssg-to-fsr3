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

#include "render/vk/buffer_vk.h"
#include "render/vk/commandlist_vk.h"
#include "render/vk/device_vk.h"
#include "render/vk/gpuresource_vk.h"
#include "render/vk/parameterset_vk.h"
#include "render/vk/resourceview_vk.h"
#include "render/vk/rootsignature_vk.h"
#include "render/vk/pipelineobject_vk.h"
#include "render/vk/rtresources_vk.h"
#include "render/vk/sampler_vk.h"

#include "render/texture.h"
#include "core/framework.h"
#include "misc/assert.h"

namespace cauldron
{
    ParameterSet* ParameterSet::CreateParameterSet(RootSignature* pRootSignature, ResourceView* pImmediateViews /* = nullptr */)
    {
        return new ParameterSetInternal(pRootSignature, pImmediateViews);
    }

    ParameterSetInternal::ParameterSetInternal(RootSignature* pRootSignature, ResourceView* pImmediateViews) : 
        ParameterSet(pRootSignature, pImmediateViews, pImmediateViews == nullptr ? cMaxDescriptorSets : 1)
    {
        m_DescriptorPool = pRootSignature->GetImpl()->GenerateDescriptorPool(m_BufferedSetCount);

        VkDescriptorSetLayout layout = pRootSignature->GetImpl()->VKDescriptorSetLayout();

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = m_DescriptorPool;
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = &layout;

        // allocate sets
        for (uint32_t i = 0; i < m_BufferedSetCount; ++i)
        {
            VkResult res = vkAllocateDescriptorSets(GetDevice()->GetImpl()->VKDevice(), &allocInfo, &(m_DescriptorSets[i]));
            CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Failed to allocate descriptor set");
        }

        // resize the root offsets
        uint32_t rootConstantBufferCount = 0;
        for (auto b : pRootSignature->GetBindingDescriptions())
        {
            if (b.Type == BindingType::RootConstant)
                rootConstantBufferCount += b.Count;
        }
        m_RootConstantBufferOffsets.resize(rootConstantBufferCount);
        for (uint32_t i = 0; i < rootConstantBufferCount; ++i)
            m_RootConstantBufferOffsets[i] = 0;
    }

    ParameterSetInternal::~ParameterSetInternal()
    {
        VkDevice device = GetDevice()->GetImpl()->VKDevice();
        vkFreeDescriptorSets(device, m_DescriptorPool, m_BufferedSetCount, m_DescriptorSets);
        vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
    }

    void ParameterSetInternal::UpdateDescriptorSetIndex()
    {
        // when using multiple descriptor sets, if the parameter set has been previously bound, go to the next descriptor set
        if (m_pImmediateResourceViews == nullptr && m_UsageState == UsageState::Bound)
        {
            m_CurrentSetIndex += 1;
            if (m_CurrentSetIndex >= m_BufferedSetCount)
                m_CurrentSetIndex = 0;
            m_UsageState = UsageState::Updating;
        }
    }

    void ParameterSetInternal::UpdateDescriptorSets(VkWriteDescriptorSet write)
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        if (m_pImmediateResourceViews == nullptr && m_UsageState == UsageState::Unbound)
        {
            // when using multiple descriptor sets and if the parameter set has never been bound, update all the descriptor sets
            for (uint32_t i = 0; i < m_BufferedSetCount; ++i)
            {
                write.dstSet = m_DescriptorSets[i];
                vkUpdateDescriptorSets(pDevice->VKDevice(), 1, &write, 0, nullptr);
            }
        }
        else
        {
            // for immediate mode and when a set has been bound, only use one set
            write.dstSet = m_DescriptorSets[m_CurrentSetIndex];
            vkUpdateDescriptorSets(pDevice->VKDevice(), 1, &write, 0, nullptr);
        }
    }

    void ParameterSetInternal::SetRootConstantBufferResource(const GPUResource* pResource, const size_t size, uint32_t slotIndex)
    {
        VulkanBinding vb = GetVulkanBinding(BindingType::RootConstant, slotIndex, L"RootConstant");

        UpdateDescriptorSetIndex();

        VkDescriptorBufferInfo info;
        info.buffer = pResource->GetImpl()->GetBuffer();
        info.offset = 0;
        info.range = static_cast<VkDeviceSize>(size);

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = nullptr;
        write.dstBinding = vb.BindingIndex;
        write.dstArrayElement = vb.DstArrayElement;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        write.pBufferInfo = &info;
        write.pImageInfo = nullptr;
        write.pTexelBufferView = nullptr;

        UpdateDescriptorSets(write);
    }

    void ParameterSetInternal::SetTextureSRV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex, int32_t mip, int32_t arraySize, int32_t firstSlice)
    {
        VulkanBinding vb = GetVulkanBinding(BindingType::TextureSRV, slotIndex, L"TextureSRV");

        UpdateDescriptorSetIndex();

        VkDescriptorImageInfo info;
        info.imageView = BindTextureSRV(pTexture, dimension, slotIndex, mip, arraySize, firstSlice, m_CurrentSetIndex).GetImpl()->Image.View;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.sampler = VK_NULL_HANDLE;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = nullptr;
        write.dstBinding = vb.BindingIndex;
        write.dstArrayElement = vb.DstArrayElement;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pBufferInfo = nullptr;
        write.pImageInfo = &info;
        write.pTexelBufferView = nullptr;

        UpdateDescriptorSets(write);
    }

    void ParameterSetInternal::SetTextureUAV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex, int32_t mip, int32_t arraySize, int32_t firstSlice)
    {
        VulkanBinding vb = GetVulkanBinding(BindingType::TextureUAV, slotIndex, L"TextureUAV");

        UpdateDescriptorSetIndex();

        VkDescriptorImageInfo info;
        info.imageView   = BindTextureUAV(pTexture, dimension, slotIndex, mip, arraySize, firstSlice, m_CurrentSetIndex).GetImpl()->Image.View;
        info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        info.sampler = VK_NULL_HANDLE;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = nullptr;
        write.dstBinding = vb.BindingIndex;
        write.dstArrayElement = vb.DstArrayElement;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pBufferInfo = nullptr;
        write.pImageInfo = &info;
        write.pTexelBufferView = nullptr;

        UpdateDescriptorSets(write);
    }

    void ParameterSetInternal::SetBufferSRV(const Buffer* pBuffer, uint32_t slotIndex, uint32_t firstElement, uint32_t numElements)
    {
        VulkanBinding vb = GetVulkanBinding(BindingType::BufferSRV, slotIndex, L"BufferSRV");

        UpdateDescriptorSetIndex();

        BindBufferSRV(pBuffer, slotIndex, firstElement, numElements, 0);

        VkDescriptorBufferInfo info;
        BufferAddressInfo addressInfo = pBuffer->GetAddressInfo();
        info.buffer = addressInfo.GetImpl()->Buffer;
        info.offset = addressInfo.GetImpl()->Offset;
        info.range  = addressInfo.GetImpl()->SizeInBytes;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = nullptr;
        write.dstBinding = vb.BindingIndex;
        write.dstArrayElement = vb.DstArrayElement;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &info;
        write.pImageInfo = nullptr;
        write.pTexelBufferView = nullptr;

        UpdateDescriptorSets(write);
    }

    void ParameterSetInternal::SetAccelerationStructure(const TLAS* pTLAS, uint32_t slotIndex)
    {
        VulkanBinding vb = GetVulkanBinding(BindingType::AccelStructRT, slotIndex, L"AccelStruct");

        UpdateDescriptorSetIndex();

        VkWriteDescriptorSetAccelerationStructureKHR info{};
        info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        info.accelerationStructureCount = 1;
        info.pAccelerationStructures    = &static_cast<const TLASInternal*>(pTLAS)->GetHandle();
        info.pNext = nullptr;

        VkWriteDescriptorSet write = {};
        write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext                = nullptr;
        write.dstBinding           = vb.BindingIndex;
        write.dstArrayElement      = vb.DstArrayElement;
        write.descriptorCount      = 1;
        write.descriptorType       = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        write.pBufferInfo          = nullptr;
        write.pImageInfo           = nullptr;
        write.pTexelBufferView     = nullptr;

        write.pNext = &info;
        write.descriptorCount = 1;

        UpdateDescriptorSets(write);
    }

    void ParameterSetInternal::SetBufferUAV(const Buffer* pBuffer, uint32_t slotIndex, uint32_t firstElement, uint32_t numElements)
    {
        VulkanBinding vb = GetVulkanBinding(BindingType::BufferUAV, slotIndex, L"BufferUAV");

        UpdateDescriptorSetIndex();

        BindBufferUAV(pBuffer, slotIndex, firstElement, numElements, 0);

        BufferAddressInfo addressInfo = pBuffer->GetAddressInfo();

        VkDescriptorBufferInfo info;
        info.buffer = addressInfo.GetImpl()->Buffer;
        info.offset = addressInfo.GetImpl()->Offset;
        info.range  = addressInfo.GetImpl()->SizeInBytes;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = nullptr;
        write.dstBinding = vb.BindingIndex;
        write.dstArrayElement = vb.DstArrayElement;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &info;
        write.pImageInfo = nullptr;
        write.pTexelBufferView = nullptr;

        UpdateDescriptorSets(write);
    }

    void ParameterSetInternal::SetSampler(const Sampler* pSampler, uint32_t slotIndex)
    {
        VulkanBinding vb = GetVulkanBinding(BindingType::Sampler, slotIndex, L"Sampler");

        UpdateDescriptorSetIndex();

        BindSampler(pSampler, slotIndex, 0);

        VkDescriptorImageInfo info;
        info.imageView = VK_NULL_HANDLE;
        info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.sampler = pSampler->GetImpl()->VKSampler();

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = nullptr;
        write.dstBinding = vb.BindingIndex;
        write.dstArrayElement = vb.DstArrayElement;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pBufferInfo = nullptr;
        write.pImageInfo = &info;
        write.pTexelBufferView = nullptr;

        UpdateDescriptorSets(write);
    }

    int32_t ParameterSetInternal::GetResourceTableIndex(BindingType bindType, uint32_t slotIndex, const wchar_t* bindName)
    {
        // Find the correct position in the binding descriptions for this entry's table
        const std::vector<BindingDesc>& descs = m_pRootSignature->GetBindingDescriptions();

        int32_t descOffset = m_pRootSignature->GetBindingDescOffset(bindType);
        CauldronAssert(ASSERT_CRITICAL, descOffset >= 0, L"No binding description found for %ls", bindName);

        // Find the correct binding desc
        for (descOffset; descOffset < descs.size(); ++descOffset)
        {
            CauldronAssert(ASSERT_CRITICAL, descs[descOffset].Type == bindType, L"No binding description found for %ls", bindName);

            if (descs[descOffset].BaseShaderRegister <= slotIndex && (descs[descOffset].BaseShaderRegister + descs[descOffset].Count) > slotIndex)
                return descOffset;
        }

        CauldronCritical(L"Could not find %ls table containing requested slotIndex", bindName);
        return -1;
    }

    ParameterSetInternal::VulkanBinding ParameterSetInternal::GetVulkanBinding(BindingType bindType, uint32_t slotIndex, const wchar_t* bindName)
    {
        int32_t descOffset = GetResourceTableIndex(bindType, slotIndex, bindName);

        const std::vector<BindingDesc>& descs = m_pRootSignature->GetBindingDescriptions();
        VulkanBinding vb = { descs[descOffset].BindingIndex, slotIndex - descs[descOffset].BaseShaderRegister };
        return vb;
    }

    void ParameterSetInternal::UpdateRootConstantBuffer(const BufferAddressInfo* pRootConstantBuffer, uint32_t rootBufferIndex)
    {
        // look for its index
        uint32_t offset = 0;
        for (auto b : m_pRootSignature->GetBindingDescriptions())
        {
            if (b.Type == BindingType::RootConstant)
            {
                if (b.BaseShaderRegister <= rootBufferIndex && b.BaseShaderRegister + b.Count > rootBufferIndex)
                {
                    m_RootConstantBufferOffsets[offset + (rootBufferIndex - b.BaseShaderRegister)] = static_cast<uint32_t>(pRootConstantBuffer->GetImpl()->Offset);
                    return;
                }
                else
                {
                    offset += b.Count;
                }
            }
        }
    }

    void ParameterSetInternal::UpdateRoot32BitConstant(const uint32_t numEntries, const uint32_t* pConstData, uint32_t rootBufferIndex)
    {
        // convert the rootBufferIndex into the index of the range
        uint32_t pushConstantRangeIndex = m_pRootSignature->GetImpl()->GetPushConstantIndex(rootBufferIndex);

        // Mash the number of entries and the root buffer index into 32 bit info key
        uint32_t key = (numEntries << 16) | (pushConstantRangeIndex);

        // Add an entry
        CauldronAssert(ASSERT_CRITICAL,
                       m_CurrentPushConstantMemOffset + numEntries < MAX_PUSH_CONSTANTS_ENTRIES,
                       L"Out of memory to store root 32-bit constants. Please grow MAX_PUSH_CONSTANTS_ENTRIES constant in parameterset_vk.h");
        // verify the size
        CauldronAssert(ASSERT_CRITICAL,
                       m_pRootSignature->GetImpl()->VKPushConstantRanges()[pushConstantRangeIndex].size > numEntries * sizeof(uint32_t),
                       L"Cannot set more data that the size of the push constant.");
        memcpy(&m_PushConstantsMem[m_CurrentPushConstantMemOffset], pConstData, sizeof(uint32_t) * numEntries);
        m_PushConstantEntries.push_back(std::make_pair(key, &m_PushConstantsMem[m_CurrentPushConstantMemOffset]));
        m_CurrentPushConstantMemOffset += numEntries;
    }

    void ParameterSetInternal::UpdateDescriptorSet(VkDescriptorImageInfo info, uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType)
    {
        VkWriteDescriptorSet write = {};
        write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext                = nullptr;
        write.dstSet               = m_DescriptorSets[m_CurrentSetIndex];
        write.dstBinding           = dstBinding;
        write.dstArrayElement      = dstArrayElement;
        write.descriptorCount      = 1;
        write.descriptorType       = descriptorType;
        write.pBufferInfo          = nullptr;
        write.pImageInfo           = &info;
        write.pTexelBufferView     = nullptr;

        vkUpdateDescriptorSets(GetDevice()->GetImpl()->VKDevice(), 1, &write, 0, nullptr);
    }

    void ParameterSetInternal::UpdateDescriptorSet(VkDescriptorBufferInfo info, uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType)
    {
        VkWriteDescriptorSet write = {};
        write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext                = nullptr;
        write.dstSet               = m_DescriptorSets[m_CurrentSetIndex];
        write.dstBinding           = dstBinding;
        write.dstArrayElement      = dstArrayElement;
        write.descriptorCount      = 1;
        write.descriptorType       = descriptorType;
        write.pBufferInfo          = &info;
        write.pImageInfo           = nullptr;
        write.pTexelBufferView     = nullptr;

        vkUpdateDescriptorSets(GetDevice()->GetImpl()->VKDevice(), 1, &write, 0, nullptr);
    }

    ResourceViewInfo ParameterSetInternal::GetImmediateResourceViewInfo(uint32_t shaderRegister, BindingType bindingType)
    {
        return m_pImmediateResourceViews->GetViewInfo(shaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(bindingType)]);
    }

    void ParameterSetInternal::Bind(CommandList* pCmdList, const PipelineObject* pPipeline)
    {
        if (m_pImmediateResourceViews != nullptr)
        {
            // we need to update the descriptor set
            for (auto desc : m_pRootSignature->GetBindingDescriptions())
            {
                switch (desc.Type)
                {
                case BindingType::RootConstant:
                case BindingType::Root32BitConstant:
                    // those don't need to update the descriptor set
                    continue;
                case BindingType::CBV:
                    CauldronError(L"CBV in ParameterSet not supported on Vulkan");
                    break;
                case BindingType::TextureSRV:
                {
                    for (uint32_t i = 0; i < desc.Count; ++i)
                    {
                        VkDescriptorImageInfo info;
                        info.imageView = GetImmediateResourceViewInfo(desc.BaseShaderRegister + i, BindingType::TextureSRV).GetImpl()->Image.View;
                        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        info.sampler     = VK_NULL_HANDLE;

                        if (info.imageView != VK_NULL_HANDLE)
                            UpdateDescriptorSet(info, desc.BindingIndex, i, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
                    }
                    break;
                }
                case BindingType::TextureUAV:
                {
                    for (uint32_t i = 0; i < desc.Count; ++i)
                    {
                        VkDescriptorImageInfo info;
                        info.imageView = GetImmediateResourceViewInfo(desc.BaseShaderRegister + i, BindingType::TextureUAV).GetImpl()->Image.View;
                        info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                        info.sampler     = VK_NULL_HANDLE;

                        if (info.imageView != VK_NULL_HANDLE)
                            UpdateDescriptorSet(info, desc.BindingIndex, i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
                    }
                    break;
                }
                case BindingType::BufferSRV:
                {
                    for (uint32_t i = 0; i < desc.Count; ++i)
                    {
                        ResourceViewInfo       viewInfo = GetImmediateResourceViewInfo(desc.BaseShaderRegister + i, BindingType::BufferSRV);
                        VkDescriptorBufferInfo info;
                        info.buffer = viewInfo.GetImpl()->Buffer.Buffer;
                        info.offset = (info.buffer == VK_NULL_HANDLE) ? 0 : viewInfo.GetImpl()->Buffer.Offset;
                        info.range  = (info.buffer == VK_NULL_HANDLE) ? VK_WHOLE_SIZE : viewInfo.GetImpl()->Buffer.Size;
                        
                        UpdateDescriptorSet(info, desc.BindingIndex, i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                    }
                    break;
                }
                case BindingType::BufferUAV:
                {
                    for (uint32_t i = 0; i < desc.Count; ++i)
                    {
                        ResourceViewInfo viewInfo = GetImmediateResourceViewInfo(desc.BaseShaderRegister + i, BindingType::BufferUAV);
                        VkDescriptorBufferInfo info;
                        info.buffer = viewInfo.GetImpl()->Buffer.Buffer;
                        info.offset = (info.buffer == VK_NULL_HANDLE) ? 0 : viewInfo.GetImpl()->Buffer.Offset;
                        info.range  = (info.buffer == VK_NULL_HANDLE) ? VK_WHOLE_SIZE : viewInfo.GetImpl()->Buffer.Size;

                        UpdateDescriptorSet(info, desc.BindingIndex, i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                    }
                    break;
                }
                case BindingType::Sampler:
                {
                    for (uint32_t i = 0; i < desc.Count; ++i)
                    {
                        VkDescriptorImageInfo info;
                        info.imageView   = VK_NULL_HANDLE;
                        info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                        info.sampler = GetImmediateResourceViewInfo(desc.BaseShaderRegister + i, BindingType::Sampler).GetImpl()->Sampler.Sampler;

                        if (info.sampler != VK_NULL_HANDLE)
                            UpdateDescriptorSet(info, desc.BindingIndex, i, VK_DESCRIPTOR_TYPE_SAMPLER);
                    }
                    break;
                }
                }
            }
        }

        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
        switch (m_pRootSignature->GetPipelineType())
        {
        case PipelineType::Graphics:
            bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            break;
        case PipelineType::Compute:
            bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
            break;
        default:
            CauldronCritical(L"Unknown pipeline type");
            break;
        }

        vkCmdBindDescriptorSets(pCmdList->GetImpl()->VKCmdBuffer(),
                                bindPoint,
                                pPipeline->GetImpl()->VKPipelineLayout(),
                                0,
                                1,
                                &(m_DescriptorSets[m_CurrentSetIndex]),
                                static_cast<uint32_t>(m_RootConstantBufferOffsets.size()),
                                m_RootConstantBufferOffsets.data());
        m_UsageState = UsageState::Bound;

        // Do push constants
        for (auto entry : m_PushConstantEntries)
        {
            uint32_t numEntries = (entry.first >> 16);
            uint32_t index      = (entry.first & 0x0000ffff);

            vkCmdPushConstants(
                pCmdList->GetImpl()->VKCmdBuffer(),
                pPipeline->GetImpl()->VKPipelineLayout(),
                m_pRootSignature->GetImpl()->VKPushConstantRanges()[index].stageFlags,
                0, // offset is always 0 for now
                sizeof(uint32_t) * numEntries,
                entry.second);
        }
    }

    void ParameterSetInternal::OnResourceResized()
    {
        ParameterSet::OnResourceResized();

        std::vector<VkDescriptorImageInfo> imageInfos;
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkWriteDescriptorSet> writes;

        imageInfos.reserve(m_BoundTextureSRVs.size() + m_BoundTextureUAVs.size());
        bufferInfos.reserve(m_BoundBufferSRVs.size() + m_BoundBufferUAVs.size());
        writes.reserve(imageInfos.size() + bufferInfos.size());

        for (auto b : m_BoundTextureSRVs)
        {
            if (b.pTexture != nullptr && b.pTexture->GetResource()->IsResizable())
            {
                BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[b.RootParameterIndex];

                VkDescriptorImageInfo info;
                info.imageView = GetTextureSRV(b.RootParameterIndex, b.ShaderRegister).GetImpl()->Image.View; // NOTE: b.ShaderRegister is incorrect if we adjust the size of the resource view to the necessary number of views
                info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                info.sampler = VK_NULL_HANDLE;
                imageInfos.push_back(info);

                VkWriteDescriptorSet write = {};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.pNext = nullptr;
                write.dstBinding = desc.BindingIndex;
                write.dstArrayElement = (b.ShaderRegister % m_TextureSRVCount) - desc.BaseShaderRegister;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                write.pBufferInfo = nullptr;
                write.pImageInfo = &imageInfos.back();
                write.pTexelBufferView = nullptr;
                writes.push_back(write);
            }
        }

        for (auto b : m_BoundTextureUAVs)
        {
            if (b.pTexture != nullptr && b.pTexture->GetResource()->IsResizable())
            {
                BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[b.RootParameterIndex];

                VkDescriptorImageInfo info;
                info.imageView = GetTextureUAV(b.RootParameterIndex, b.ShaderRegister).GetImpl()->Image.View; // NOTE: b.ShaderRegister is incorrect if we adjust the size of the resource view to the necessary number of views
                info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                info.sampler = VK_NULL_HANDLE;
                imageInfos.push_back(info);

                VkWriteDescriptorSet write = {};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.pNext = nullptr;
                write.dstBinding = desc.BindingIndex;
                write.dstArrayElement = (b.ShaderRegister % m_TextureUAVCount) - desc.BaseShaderRegister;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                write.pBufferInfo = nullptr;
                write.pImageInfo = &imageInfos.back();
                write.pTexelBufferView = nullptr;
                writes.push_back(write);
            }
        }

        for (auto b : m_BoundBufferSRVs)
        {
            if (b.pBuffer != nullptr && b.pBuffer->GetResource()->IsResizable())
            {
                BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[b.RootParameterIndex];

                BufferAddressInfo addressInfo = b.pBuffer->GetAddressInfo();

                VkDescriptorBufferInfo info;
                info.buffer = addressInfo.GetImpl()->Buffer;
                info.offset = addressInfo.GetImpl()->Offset;
                info.range  = addressInfo.GetImpl()->SizeInBytes;
                bufferInfos.push_back(info);

                VkWriteDescriptorSet write = {};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.pNext = nullptr;
                write.dstBinding = desc.BindingIndex;
                write.dstArrayElement = (b.ShaderRegister % m_BufferSRVCount) - desc.BaseShaderRegister;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.pBufferInfo = &bufferInfos.back();
                write.pImageInfo = nullptr;
                write.pTexelBufferView = nullptr;
                writes.push_back(write);
            }
        }

        for (auto b : m_BoundBufferUAVs)
        {
            if (b.pBuffer != nullptr && b.pBuffer->GetResource()->IsResizable())
            {
                BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[b.RootParameterIndex];

                BufferAddressInfo addressInfo = b.pBuffer->GetAddressInfo();

                VkDescriptorBufferInfo info;
                info.buffer = addressInfo.GetImpl()->Buffer;
                info.offset = addressInfo.GetImpl()->Offset;
                info.range  = addressInfo.GetImpl()->SizeInBytes;
                bufferInfos.push_back(info);

                VkWriteDescriptorSet write = {};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.pNext = nullptr;
                write.dstBinding = desc.BindingIndex;
                write.dstArrayElement = (b.ShaderRegister % m_BufferUAVCount) - desc.BaseShaderRegister;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.pBufferInfo = &bufferInfos.back();
                write.pImageInfo = nullptr;
                write.pTexelBufferView = nullptr;
                writes.push_back(write);
            }
        }

        // Sampler aren't resized

        for (uint32_t i = 0; i < m_BufferedSetCount; ++i)
        {
            for (auto it = writes.begin(); it != writes.end(); ++it)
                it->dstSet = m_DescriptorSets[i];

            vkUpdateDescriptorSets(GetDevice()->GetImpl()->VKDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }
}  // namespace cauldron

#endif // #if defined(_VK)
