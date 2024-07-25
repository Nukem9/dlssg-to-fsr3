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

#if defined(_DX12)

#include "render/dx12/rootsignaturedesc_dx12.h"
#include "render/dx12/sampler_dx12.h"

#include "core/framework.h"
#include "misc/assert.h"

namespace cauldron
{
    D3D12_SHADER_VISIBILITY Convert(ShaderBindStage stages)
    {
        if (static_cast<bool>(stages & ShaderBindStage::Compute))
        {
            return D3D12_SHADER_VISIBILITY_ALL;
        }

        else if (static_cast<bool>(stages & ShaderBindStage::Vertex))
        {
            if (static_cast<bool>(stages & ShaderBindStage::Pixel))
            {
                return D3D12_SHADER_VISIBILITY_ALL;
            }

            return D3D12_SHADER_VISIBILITY_VERTEX;
        }

        else if (static_cast<bool>(stages & ShaderBindStage::Pixel))
        {
            return D3D12_SHADER_VISIBILITY_PIXEL;
        }

        return D3D12_SHADER_VISIBILITY_ALL;
    }

    #if _DEBUG
    void TestRange(uint32_t start1, uint32_t count1, uint32_t start2, uint32_t count2)
    {
        uint32_t min = std::min(start1, start2);
        uint32_t max = std::max(start1 + count1, start2 + count2);
        CauldronAssert(ASSERT_CRITICAL, count1 + count2 <= max - min, L"Overlapping resources of same type registered to root signature");
    }
    #endif // _DEBUG

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
    #if _DEBUG
        // Make sure this doesn't overlap an existing range of entries
        for (auto range : m_pSignatureDescImpl->m_DescRanges)
        {
            if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                TestRange(bindingIndex, count, range.BaseShaderRegister, range.NumDescriptors);
        }
    #endif // _DEBUG

        CD3DX12_DESCRIPTOR_RANGE    DescRange = {};
        DescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, count, bindingIndex, 0);
        m_pSignatureDescImpl->m_DescRanges.push_back(DescRange);

        RootParameter RTParam;
        RTParam.Type = BindingType::TextureSRV;
        RTParam.Visibility = Convert(bindStages);
        RTParam.DescRangeIndex = static_cast<uint32_t>(m_pSignatureDescImpl->m_DescRanges.size() - 1);
        m_pSignatureDescImpl->m_RootParameters.push_back(RTParam);

        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddTextureUAVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
#if _DEBUG
        // Make sure this doesn't overlap an existing range of entries
        for (auto range : m_pSignatureDescImpl->m_DescRanges)
        {
            if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                TestRange(bindingIndex, count, range.BaseShaderRegister, range.NumDescriptors);
        }
#endif  // _DEBUG

        CD3DX12_DESCRIPTOR_RANGE    descRange = {};
        descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, count, bindingIndex, 0);
        m_pSignatureDescImpl->m_DescRanges.push_back(descRange);

        RootParameter rtParam;
        rtParam.Type = BindingType::TextureUAV;
        rtParam.Visibility = Convert(bindStages);
        rtParam.DescRangeIndex = static_cast<uint32_t>(m_pSignatureDescImpl->m_DescRanges.size() - 1);
        m_pSignatureDescImpl->m_RootParameters.push_back(rtParam);

        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddBufferSRVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
#if _DEBUG
        // Make sure this doesn't overlap an existing range of entries
        for (auto range : m_pSignatureDescImpl->m_DescRanges)
        {
            if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                TestRange(bindingIndex, count, range.BaseShaderRegister, range.NumDescriptors);
        }
#endif  // _DEBUG

        CD3DX12_DESCRIPTOR_RANGE    DescRange = {};
        DescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, count, bindingIndex, 0);
        m_pSignatureDescImpl->m_DescRanges.push_back(DescRange);

        RootParameter RTParam;
        RTParam.Type = BindingType::BufferSRV;
        RTParam.Visibility = Convert(bindStages);
        RTParam.DescRangeIndex = static_cast<uint32_t>(m_pSignatureDescImpl->m_DescRanges.size() - 1);
        m_pSignatureDescImpl->m_RootParameters.push_back(RTParam);

        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddBufferUAVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
#if _DEBUG
        // Make sure this doesn't overlap an existing range of entries
        for (auto range : m_pSignatureDescImpl->m_DescRanges)
        {
            if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                TestRange(bindingIndex, count, range.BaseShaderRegister, range.NumDescriptors);
        }
#endif  // _DEBUG

        CD3DX12_DESCRIPTOR_RANGE    descRange = {};
        descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, count, bindingIndex, 0);
        m_pSignatureDescImpl->m_DescRanges.push_back(descRange);

        RootParameter rtParam;
        rtParam.Type = BindingType::BufferUAV;
        rtParam.Visibility = Convert(bindStages);
        rtParam.DescRangeIndex = static_cast<uint32_t>(m_pSignatureDescImpl->m_DescRanges.size() - 1);
        m_pSignatureDescImpl->m_RootParameters.push_back(rtParam);

        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddRTAccelerationStructureSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
#if _DEBUG
        // Make sure this doesn't overlap an existing range of entries
        for (auto range : m_pSignatureDescImpl->m_DescRanges)
        {
            if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                TestRange(bindingIndex, count, range.BaseShaderRegister, range.NumDescriptors);
        }
#endif  // _DEBUG

        CD3DX12_DESCRIPTOR_RANGE descRange = {};
        descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, count, bindingIndex, 0);
        m_pSignatureDescImpl->m_DescRanges.push_back(descRange);

        RootParameter rtParam;
        rtParam.Type           = BindingType::AccelStructRT;
        rtParam.Visibility     = Convert(bindStages);
        rtParam.DescRangeIndex = static_cast<uint32_t>(m_pSignatureDescImpl->m_DescRanges.size() - 1);
        m_pSignatureDescImpl->m_RootParameters.push_back(rtParam);

        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddSamplerSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
#if _DEBUG
        // Make sure this doesn't overlap an existing range of entries
        for (auto range : m_pSignatureDescImpl->m_DescRanges)
        {
            if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
                TestRange(bindingIndex, count, range.BaseShaderRegister, range.NumDescriptors);
        }
        // Need to check static ones too
        for (auto sampler : m_pSignatureDescImpl->m_StaticSamplers)
            TestRange(bindingIndex, count, sampler.RegisterSpace, 1);
#endif  // _DEBUG

        CD3DX12_DESCRIPTOR_RANGE    DescRange = {};
        DescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, count, bindingIndex, 0);
        m_pSignatureDescImpl->m_DescRanges.push_back(DescRange);

        RootParameter RTParam;
        RTParam.Type = BindingType::Sampler;
        RTParam.Visibility = Convert(bindStages);
        RTParam.DescRangeIndex = static_cast<uint32_t>(m_pSignatureDescImpl->m_DescRanges.size() - 1);
        m_pSignatureDescImpl->m_RootParameters.push_back(RTParam);

        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddStaticSamplers(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count, const SamplerDesc* samplerDescList)
    {
#if _DEBUG
        // Make sure this doesn't overlap an existing range of entries
        for (auto range : m_pSignatureDescImpl->m_DescRanges)
        {
            if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
                TestRange(bindingIndex, count, range.BaseShaderRegister, range.NumDescriptors);
        }
#endif  // _DEBUG

        CauldronAssert(ASSERT_CRITICAL, samplerDescList, L"AddSamplerSet called with no sampler descriptions");
        for (uint32_t i = 0; i < count; ++i)
        {
            // Set the binding index and stage
            D3D12_STATIC_SAMPLER_DESC desc = DX12StaticSamplerDesc(samplerDescList[i]);
            desc.ShaderRegister = bindingIndex++;   // Increment the binding index for each sampler
            desc.ShaderVisibility = Convert(bindStages);
            m_pSignatureDescImpl->m_StaticSamplers.push_back(desc);
        }

        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddConstantBufferSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
#if _DEBUG
        // Make sure this doesn't overlap an existing range of entries
        for (auto range : m_pSignatureDescImpl->m_DescRanges)
        {
            if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
                TestRange(bindingIndex, count, range.BaseShaderRegister, range.NumDescriptors);
        }
#endif  // _DEBUG

        CD3DX12_DESCRIPTOR_RANGE    descRange = {};
        descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, count, bindingIndex, 0);
        m_pSignatureDescImpl->m_DescRanges.push_back(descRange);

        RootParameter rtParam;
        rtParam.Type = BindingType::CBV;
        rtParam.Visibility = Convert(bindStages);
        rtParam.DescRangeIndex = static_cast<uint32_t>(m_pSignatureDescImpl->m_DescRanges.size() - 1);
        m_pSignatureDescImpl->m_RootParameters.push_back(rtParam);

        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::AddConstantBufferView(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
#if _DEBUG
        // Make sure this doesn't overlap an existing range of entries
        for (auto range : m_pSignatureDescImpl->m_DescRanges)
        {
            if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
                TestRange(bindingIndex, count, range.BaseShaderRegister, range.NumDescriptors);
        }
#endif  // _DEBUG

        for (uint32_t i = 0; i < count; ++i)
        {
            RootParameter rtParam;
            rtParam.Type = BindingType::RootConstant;
            rtParam.Visibility = Convert(bindStages);
            rtParam.BindingIndex = bindingIndex + i;
            m_pSignatureDescImpl->m_RootParameters.push_back(rtParam);
        }

        UpdatePipelineType(bindStages);
    }

    void RootSignatureDesc::Add32BitConstantBuffer(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count)
    {
#if _DEBUG
        // Make sure this doesn't overlap an existing range of entries
        for (auto range : m_pSignatureDescImpl->m_DescRanges)
        {
            if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
                TestRange(bindingIndex, 1, range.BaseShaderRegister, range.NumDescriptors);
        }
#endif  // _DEBUG

        RootParameter rtParam;
        rtParam.Type = BindingType::Root32BitConstant;
        rtParam.Visibility   = Convert(bindStages);
        rtParam.BindingIndex = bindingIndex;
        rtParam.Size         = count;

        m_pSignatureDescImpl->m_RootParameters.push_back(rtParam);
    }

} // namespace cauldron

#endif // #if defined(_DX12)
