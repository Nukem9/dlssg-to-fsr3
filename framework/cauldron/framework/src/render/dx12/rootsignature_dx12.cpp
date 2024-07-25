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
#include "render/dx12/device_dx12.h"
#include "render/dx12/rootsignaturedesc_dx12.h"
#include "render/dx12/rootsignature_dx12.h"

#include "core/framework.h"
#include "misc/assert.h"

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

    void RootSignatureInternal::Build(const RootSignatureDesc& desc)
    {
        m_PipelineType = desc.GetPipelineType();

        // build the root parameters
        const std::vector<RootParameter>& parameters = desc.m_pSignatureDescImpl->m_RootParameters;
        const std::vector<CD3DX12_DESCRIPTOR_RANGE>& ranges = desc.m_pSignatureDescImpl->m_DescRanges;

        // We are going to remap the order of parameters to what as defined to make it easier to find later
        // This means we need to remap initial values to new ones
        std::vector<BindingDesc> texSRVBufferDescs  = {};
        std::vector<uint32_t> texSRVRemapIndexes    = {};
        std::vector<BindingDesc> texUAVBufferDescs  = {};
        std::vector<uint32_t> texUAVRemapIndexes    = {};
        std::vector<BindingDesc> bufSRVBufferDescs  = {};
        std::vector<uint32_t> bufSRVRemapIndexes    = {};
        std::vector<BindingDesc> bufUAVBufferDescs  = {};
        std::vector<uint32_t> bufUAVRemapIndexes    = {};
        std::vector<BindingDesc> cbvBufferDescs     = {};
        std::vector<uint32_t> cbvRemapIndexes       = {};
        std::vector<BindingDesc> samplerBufferDescs = {};
        std::vector<uint32_t> samplerRemapIndexes   = {};

        uint32_t bindingIndex = 0;
        std::vector<uint32_t> paramRemapIndices     = {};

        for (uint32_t i = 0; i < parameters.size(); ++i)
        {
            BindingDesc desc;
            desc.BindingIndex = 0; // unused on DX12
            RootParameter param = parameters[i];
            switch (param.Type)
            {
            case BindingType::TextureSRV:
                desc.Type = BindingType::TextureSRV;
                desc.Count = ranges[param.DescRangeIndex].NumDescriptors;
                desc.BaseShaderRegister = ranges[param.DescRangeIndex].BaseShaderRegister;
                texSRVBufferDescs.push_back(desc);
                texSRVRemapIndexes.push_back(i);
                break;
            case BindingType::TextureUAV:
                desc.Type = BindingType::TextureUAV;
                desc.Count = ranges[param.DescRangeIndex].NumDescriptors;
                desc.BaseShaderRegister = ranges[param.DescRangeIndex].BaseShaderRegister;
                texUAVBufferDescs.push_back(desc);
                texUAVRemapIndexes.push_back(i);
                break;
            case BindingType::BufferSRV:
            case BindingType::AccelStructRT:
                desc.Type = BindingType::BufferSRV;
                desc.Count = ranges[param.DescRangeIndex].NumDescriptors;
                desc.BaseShaderRegister = ranges[param.DescRangeIndex].BaseShaderRegister;
                bufSRVBufferDescs.push_back(desc);
                bufSRVRemapIndexes.push_back(i);
                break;
            case BindingType::BufferUAV:
                desc.Type = BindingType::BufferUAV;
                desc.Count = ranges[param.DescRangeIndex].NumDescriptors;
                desc.BaseShaderRegister = ranges[param.DescRangeIndex].BaseShaderRegister;
                bufUAVBufferDescs.push_back(desc);
                bufUAVRemapIndexes.push_back(i);
                break;
            case BindingType::Sampler:
                desc.Type = BindingType::Sampler;
                desc.Count = ranges[param.DescRangeIndex].NumDescriptors;
                desc.BaseShaderRegister = ranges[param.DescRangeIndex].BaseShaderRegister;
                samplerBufferDescs.push_back(desc);
                samplerRemapIndexes.push_back(i);
                break;
            case BindingType::CBV:
                desc.Type = BindingType::CBV;
                desc.Count = ranges[param.DescRangeIndex].NumDescriptors;
                desc.BaseShaderRegister = ranges[param.DescRangeIndex].BaseShaderRegister;
                cbvBufferDescs.push_back(desc);
                cbvRemapIndexes.push_back(i);
                break;
            case BindingType::RootConstant:
                desc.Type = BindingType::RootConstant;
                desc.Count = 1;
                desc.BindingIndex = bindingIndex++;
                desc.BaseShaderRegister = param.BindingIndex;
                m_BindingDescriptions.push_back(desc);
                paramRemapIndices.push_back(i);
                break;
            case BindingType::Root32BitConstant:
                desc.Type = BindingType::Root32BitConstant;
                desc.Count = param.Size;
                desc.BindingIndex = bindingIndex++;
                desc.BaseShaderRegister = param.BindingIndex;
                m_BindingDescriptions.push_back(desc);
                paramRemapIndices.push_back(i);
                break;
            default:
                CauldronCritical(L"Unknown or unsuported root parameter type");;
            }

            // NOTE: static samplers don't count in the root parameter indices
            // No binding description is created for static samplers.
        }

        // Insert all of our binding descriptions in the order we want to track them later

        // Texture SRV
        if (texSRVBufferDescs.size())
        {
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::TextureSRV)] = static_cast<uint32_t>(m_BindingDescriptions.size());
            // Set the right binding index for each entry
            for (auto& texSRVBufferDesc : texSRVBufferDescs)
                texSRVBufferDesc.BindingIndex = bindingIndex++;
            // And add everything in the binding description list
            m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(texSRVBufferDescs.begin()), std::make_move_iterator(texSRVBufferDescs.end()));
            paramRemapIndices.insert(paramRemapIndices.end(), std::make_move_iterator(texSRVRemapIndexes.begin()), std::make_move_iterator(texSRVRemapIndexes.end()));
        }

        // Texture UAV
        if (texUAVBufferDescs.size())
        {
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::TextureUAV)] = static_cast<uint32_t>(m_BindingDescriptions.size());

            // Set the right binding index for each entry
            for (auto& texUAVBufferDesc : texUAVBufferDescs)
                texUAVBufferDesc.BindingIndex = bindingIndex++;
            // And add everything in the binding description list
            m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(texUAVBufferDescs.begin()), std::make_move_iterator(texUAVBufferDescs.end()));
            paramRemapIndices.insert(paramRemapIndices.end(), std::make_move_iterator(texUAVRemapIndexes.begin()), std::make_move_iterator(texUAVRemapIndexes.end()));
        }

        // Buffer SRV
        if (bufSRVBufferDescs.size())
        {
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::BufferSRV)] = static_cast<uint32_t>(m_BindingDescriptions.size());

            // Set the right binding index for each entry
            for (auto& bufSRVBufferDesc : bufSRVBufferDescs)
                bufSRVBufferDesc.BindingIndex = bindingIndex++;
            // And add everything in the binding description list
            m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(bufSRVBufferDescs.begin()), std::make_move_iterator(bufSRVBufferDescs.end()));
            paramRemapIndices.insert(paramRemapIndices.end(), std::make_move_iterator(bufSRVRemapIndexes.begin()), std::make_move_iterator(bufSRVRemapIndexes.end()));
        }

        // Buffer UAV
        if (bufUAVBufferDescs.size())
        {
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::BufferUAV)] = static_cast<uint32_t>(m_BindingDescriptions.size());

            // Set the right binding index for each entry
            for (auto& bufUAVBufferDesc : bufUAVBufferDescs)
                bufUAVBufferDesc.BindingIndex = bindingIndex++;
            // And add everything in the binding description list
            m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(bufUAVBufferDescs.begin()), std::make_move_iterator(bufUAVBufferDescs.end()));
            paramRemapIndices.insert(paramRemapIndices.end(), std::make_move_iterator(bufUAVRemapIndexes.begin()), std::make_move_iterator(bufUAVRemapIndexes.end()));
        }

        // CBV sets
        if (cbvBufferDescs.size())
        {
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::CBV)] = static_cast<uint32_t>(m_BindingDescriptions.size());

            // Set the right binding index for each entry
            for (auto& cbvBufferDesc : cbvBufferDescs)
                cbvBufferDesc.BindingIndex = bindingIndex++;
            // And add everything in the binding description list
            m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(cbvBufferDescs.begin()), std::make_move_iterator(cbvBufferDescs.end()));
            paramRemapIndices.insert(paramRemapIndices.end(), std::make_move_iterator(cbvRemapIndexes.begin()), std::make_move_iterator(cbvRemapIndexes.end()));
        }

        // Sampler sets
        if (samplerBufferDescs.size())
        {
            m_BindingDescOffsets[static_cast<uint32_t>(BindingType::Sampler)] = static_cast<uint32_t>(m_BindingDescriptions.size());

            // Set the right binding index for each entry
            for (auto& samplerBufferDesc : samplerBufferDescs)
                samplerBufferDesc.BindingIndex = bindingIndex++;
            // And add everything in the binding description list
            m_BindingDescriptions.insert(m_BindingDescriptions.end(), std::make_move_iterator(samplerBufferDescs.begin()), std::make_move_iterator(samplerBufferDescs.end()));
            paramRemapIndices.insert(paramRemapIndices.end(), std::make_move_iterator(samplerRemapIndexes.begin()), std::make_move_iterator(samplerRemapIndexes.end()));
        }

        CauldronAssert(ASSERT_CRITICAL, parameters.size() == paramRemapIndices.size(), L"Critical error remapping parameter indices");

        std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;
        rootParameters.resize(paramRemapIndices.size());
        for (size_t i = 0; i < paramRemapIndices.size(); ++i)
        {
            RootParameter param = parameters[paramRemapIndices[i]];
            switch (param.Type)
            {
            case BindingType::TextureSRV:
            case BindingType::TextureUAV:
            case BindingType::BufferSRV:
            case BindingType::AccelStructRT:
            case BindingType::BufferUAV:
            case BindingType::Sampler:
            case BindingType::CBV:
                rootParameters[i].InitAsDescriptorTable(1, &ranges[param.DescRangeIndex], param.Visibility);
                break;
            case BindingType::RootConstant:
                rootParameters[i].InitAsConstantBufferView(param.BindingIndex, 0, param.Visibility);
                break;
            case BindingType::Root32BitConstant:
                rootParameters[i].InitAsConstants(param.Size, param.BindingIndex, 0, param.Visibility);
                break;
            default:
                CauldronCritical(L"Unknown or unsuported root parameter type");
            }
        }

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = CD3DX12_ROOT_SIGNATURE_DESC();
        rootSignatureDesc.NumParameters = static_cast<UINT>(rootParameters.size());
        rootSignatureDesc.pParameters = rootParameters.data();
        rootSignatureDesc.NumStaticSamplers = static_cast<UINT>(desc.m_pSignatureDescImpl->m_StaticSamplers.size());
        rootSignatureDesc.pStaticSamplers = desc.m_pSignatureDescImpl->m_StaticSamplers.data();

        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE
            | D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        MSComPtr<ID3DBlob> pOutBlob = nullptr, pErrorBlob = nullptr;
        HRESULT hResult = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob);

        // If the shader failed to compile it should have written something to the error message.
        if (FAILED(hResult))
        {
            if (pErrorBlob)
            {
                std::string error = (char*)pErrorBlob->GetBufferPointer();
                CauldronCritical(StringToWString(error).c_str());
            }
            else
                CauldronCritical(L"Failed to serialize root signature.");
        }

        CauldronThrowOnFail(GetDevice()->GetImpl()->DX12Device()->CreateRootSignature(0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature)));
    }

} // namespace cauldron

#endif // #if defined(_DX12)
