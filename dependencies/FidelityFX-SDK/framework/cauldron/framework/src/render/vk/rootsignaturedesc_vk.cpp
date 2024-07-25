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
#include "render/vk/pipelinedesc_vk.h"
#include "render/vk/rootsignaturedesc_vk.h"
#include "render/vk/sampler_vk.h"

#include "misc/assert.h"

namespace cauldron
{
    VkShaderStageFlags ConvertShaderBindStages(ShaderBindStage bindStages)
    {
        VkShaderStageFlags stageFlags = 0;
        if (static_cast<bool>(bindStages & ShaderBindStage::Vertex))
            stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
        if (static_cast<bool>(bindStages & ShaderBindStage::Pixel))
            stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (static_cast<bool>(bindStages & ShaderBindStage::Compute))
            stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;

        return stageFlags;
    }

    RootSignatureDesc::RootSignatureDesc()
    {
        // Allocate implementation
        m_pSignatureDescImpl = new RootSignatureDescInternal;
    }

    RootSignatureDesc::~RootSignatureDesc()
    {
        // Delete allocated impl memory as it's no longer needed
        delete m_pSignatureDescImpl;
    }

    void RootSignatureDesc::AddTextureSRVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
        m_pSignatureDescImpl->AddBinding(BindingType::TextureSRV, bindingIndex, bindingIndex + TEXTURE_BINDING_SHIFT, bindStages, count);
        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddTextureUAVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
        m_pSignatureDescImpl->AddBinding(BindingType::TextureUAV, bindingIndex, bindingIndex + UNORDERED_ACCESS_VIEW_BINDING_SHIFT, bindStages, count);
        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddBufferSRVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
        m_pSignatureDescImpl->AddBinding(BindingType::BufferSRV, bindingIndex, bindingIndex + TEXTURE_BINDING_SHIFT, bindStages, count);
        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddBufferUAVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
        m_pSignatureDescImpl->AddBinding(BindingType::BufferUAV, bindingIndex, bindingIndex + UNORDERED_ACCESS_VIEW_BINDING_SHIFT, bindStages, count);
        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddRTAccelerationStructureSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
        m_pSignatureDescImpl->AddBinding(BindingType::AccelStructRT, bindingIndex, bindingIndex + TEXTURE_BINDING_SHIFT, bindStages, count);
        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddSamplerSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
        m_pSignatureDescImpl->AddBinding(BindingType::Sampler, bindingIndex, bindingIndex + SAMPLER_BINDING_SHIFT, bindStages, count);
        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddStaticSamplers(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count, const SamplerDesc* samplerDescList)
    {
        m_pSignatureDescImpl->AddStaticSamplerBinding(bindingIndex, bindingIndex + SAMPLER_BINDING_SHIFT, bindStages, count, samplerDescList);
        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddConstantBufferSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
        m_pSignatureDescImpl->AddBinding(BindingType::CBV, bindingIndex, bindingIndex + CONSTANT_BUFFER_BINDING_SHIFT, bindStages, count);
        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddConstantBufferView(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
        m_pSignatureDescImpl->AddBinding(BindingType::RootConstant, bindingIndex, bindingIndex + CONSTANT_BUFFER_BINDING_SHIFT, bindStages, count);
        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::Add32BitConstantBuffer(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
        PushConstantInfo info   = {};
        info.BaseShaderRegister = bindingIndex;
        info.Count              = count;
        info.StageFlags         = ConvertShaderBindStages(bindStages);

        for (auto& b : m_pSignatureDescImpl->m_PushConstantInfo)
        {
            CauldronAssert(ASSERT_CRITICAL,
                           (b.StageFlags & info.StageFlags) == 0,
                           L"There is already a 32Bit constant at the given shader stage. Vulkan only supports up to one push constant per stage.");
        }

        UpdatePipelineType(bindStages);

        m_pSignatureDescImpl->m_PushConstantInfo.push_back(info);
    }

    bool RootSignatureDescInternal::IsBindingUsed(BindingType type, uint32_t baseShaderRegister, uint32_t count)
    {
        // check that the binding index doesn't exist yet
        for (auto& b : m_Bindings)
        {
            if (b.Type == type && (b.BaseShaderRegister < (baseShaderRegister + count)) && (baseShaderRegister < (b.BaseShaderRegister + b.Count)))
                return true;
        }
        if (type == BindingType::Sampler)
        {
            for (auto& b : m_ImmutableSamplersBindings)
            {
                if (b.Type == type && (b.BaseShaderRegister < (baseShaderRegister + count)) && (baseShaderRegister < (b.BaseShaderRegister + b.Count)))
                    return true;
            }
        }
        return false;
    }

    void RootSignatureDescInternal::AddBinding(BindingType type, uint32_t baseShaderRegister, uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
        CauldronAssert(ASSERT_CRITICAL, !IsBindingUsed(type, bindingIndex, count), L"There is already a binding at index %d.", bindingIndex);

        BindingInfo info = {};
        info.Type = type;
        info.BaseShaderRegister = baseShaderRegister;
        info.BindingIndex = bindingIndex;
        info.Count = count;
        info.StageFlags = ConvertShaderBindStages(bindStages);

        // order by binding index. This is required for dynamic offsets (RootConstant) to be correctly ordered when binding the descriptor sets
        auto iter = m_Bindings.begin();
        for (; iter != m_Bindings.end(); ++iter)
        {
            if (bindingIndex < iter->BindingIndex)
                break;
        }
        m_Bindings.insert(iter, info);
    }

    void RootSignatureDescInternal::AddStaticSamplerBinding(uint32_t baseShaderRegister, uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count, const SamplerDesc* samplerDescList)
    {
        CauldronAssert(ASSERT_CRITICAL, !IsBindingUsed(BindingType::Sampler, bindingIndex, static_cast<uint32_t>(count)), L"There is already a binding at index %d.", bindingIndex);

        BindingInfo info = {};
        info.Type = BindingType::Sampler;
        info.BaseShaderRegister = baseShaderRegister;
        info.BindingIndex = bindingIndex;
        info.Count = count;
        info.StageFlags = ConvertShaderBindStages(bindStages);

        // Add the new samplers
        for (uint32_t i = 0; i < count; ++i)
            m_ImmutableSamplers.push_back(samplerDescList[i]);

        m_ImmutableSamplersBindings.push_back(info);
    }

} // namespace cauldron

#endif // #if defined(_VK)
