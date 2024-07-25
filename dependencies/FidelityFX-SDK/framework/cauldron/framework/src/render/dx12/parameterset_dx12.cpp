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

#include "render/dx12/commandlist_dx12.h"
#include "render/dx12/buffer_dx12.h"
#include "render/dx12/parameterset_dx12.h"
#include "render/dx12/resourceview_dx12.h"
#include "render/dx12/rootsignature_dx12.h"
#include "render/dx12/texture_dx12.h"

#include "core/framework.h"
#include "render/rtresources.h"

namespace cauldron
{
    //////////////////////////////////////////////////////////////////////////
    // Helpers

    void SetGraphicsRootSignature(CommandList* pCmdList, RootSignature* pRootSignature)
    {
        pCmdList->GetImpl()->DX12CmdList()->SetGraphicsRootSignature(pRootSignature->GetImpl()->DX12RootSignature());
    }

    void SetComputeRootSignature(CommandList* pCmdList, RootSignature* pRootSignature)
    {
        pCmdList->GetImpl()->DX12CmdList()->SetComputeRootSignature(pRootSignature->GetImpl()->DX12RootSignature());
    }

    void SetGraphicsRootConstantBuffer(CommandList* pCmdList, uint32_t rootParameterIndex, const BufferAddressInfo* pBufferAddressInfo)
    {
        pCmdList->GetImpl()->DX12CmdList()->SetGraphicsRootConstantBufferView(rootParameterIndex, pBufferAddressInfo->GetImpl()->GPUBufferView);
    }

    void SetComputeRootConstantBuffer(CommandList* pCmdList, uint32_t rootParameterIndex, const BufferAddressInfo* pBufferAddressInfo)
    {
        pCmdList->GetImpl()->DX12CmdList()->SetComputeRootConstantBufferView(rootParameterIndex, pBufferAddressInfo->GetImpl()->GPUBufferView);
    }

    void SetGraphicsRoot32BitConstants(CommandList* pCmdList, uint32_t rootParameterIndex, uint32_t numEntries, void* pData)
    {
        pCmdList->GetImpl()->DX12CmdList()->SetGraphicsRoot32BitConstants(rootParameterIndex, numEntries, pData, 0);
    }

    void SetComputeRoot32BitConstants(CommandList* pCmdList, uint32_t rootParameterIndex, uint32_t numEntries, void* pData)
    {
        pCmdList->GetImpl()->DX12CmdList()->SetComputeRoot32BitConstants(rootParameterIndex, numEntries, pData, 0);
    }

    void SetGraphicsRootResourceView(CommandList* pCmdList, uint32_t rootParameterIndex, const ResourceViewInfo* pResourceViewInfo)
    {
        pCmdList->GetImpl()->DX12CmdList()->SetGraphicsRootDescriptorTable(rootParameterIndex, pResourceViewInfo->GetImpl()->hGPUHandle);
    }

    void SetComputeRootResourceView(CommandList* pCmdList, uint32_t rootParameterIndex, const ResourceViewInfo* pResourceViewInfo)
    {
        pCmdList->GetImpl()->DX12CmdList()->SetComputeRootDescriptorTable(rootParameterIndex, pResourceViewInfo->GetImpl()->hGPUHandle);
    }

    //////////////////////////////////////////////////////////////////////////
    // ParameterSet

    ParameterSet* ParameterSet::CreateParameterSet(RootSignature* pRootSignature, ResourceView* pImmediateViews /* = nullptr */)
    {
        return new ParameterSetInternal(pRootSignature, pImmediateViews);
    }

    ParameterSetInternal::ParameterSetInternal(RootSignature* pRootSignature, ResourceView* pImmediateViews /*=nullptr*/) :
        ParameterSet(pRootSignature, pImmediateViews, 1)
    {
    }

    // Returns the right resource table index in the list of binding descriptions
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

    void ParameterSetInternal::SetRootConstantBufferResource(const GPUResource* pResource, size_t size, uint32_t slotIndex)
    {
        // Reserve space for constant buffer entry
        if (static_cast<int32_t>(slotIndex) > static_cast<int32_t>(m_RootConstantBuffers.size() - 1))
            m_RootConstantBuffers.resize(slotIndex + 1);

        m_ValidBindings.push_back(slotIndex);
    }

    void ParameterSetInternal::SetTextureSRV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex, int32_t mip, int32_t arraySize, int32_t firstSlice)
    {
        BindTextureSRV(pTexture, dimension, slotIndex, mip, arraySize, firstSlice, 0);
    }

    void ParameterSetInternal::SetTextureUAV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex/*=0*/, int32_t mip/*=-1*/, int32_t arraySize/*=-1*/, int32_t firstSlice/*=-1*/)
    {
        BindTextureUAV(pTexture, dimension, slotIndex, mip, arraySize, firstSlice, 0);
    }

    void ParameterSetInternal::SetBufferSRV(const Buffer* pBuffer, uint32_t slotIndex/*=0*/, uint32_t firstElement/*=-1*/, uint32_t numElements/*=-1*/)
    {
        BindBufferSRV(pBuffer, slotIndex, firstElement, numElements, 0);
    }

    void ParameterSetInternal::SetAccelerationStructure(const TLAS* pTLAS, uint32_t slotIndex/*=0*/)
    {
        BindBufferSRV(pTLAS->GetBuffer(), slotIndex, -1, -1, 0);
    }

    void ParameterSetInternal::SetBufferUAV(const Buffer* pBuffer, uint32_t slotIndex/*=0*/, uint32_t firstElement/*=-1*/, uint32_t numElements/*=-1*/)
    {
        BindBufferUAV(pBuffer, slotIndex, firstElement, numElements, 0);
    }

    void ParameterSetInternal::SetSampler(const Sampler* pSampler, uint32_t slotIndex/*=0*/)
    {
        BindSampler(pSampler, slotIndex, 0);
    }

    void ParameterSetInternal::UpdateRootConstantBuffer(const BufferAddressInfo* pRootConstantBuffer, uint32_t rootBufferIndex)
    {
        m_RootConstantBuffers[rootBufferIndex]  = *pRootConstantBuffer;
    }

    void ParameterSetInternal::UpdateRoot32BitConstant(const uint32_t numEntries, const uint32_t* pConstData, uint32_t rootBufferIndex)
    {
        // Mash the number of entries and the root buffer index into 32 bit info key
        uint32_t key = (numEntries << 16) | (rootBufferIndex);

        // Add an entry
        CauldronAssert(ASSERT_CRITICAL, m_Current32BitMemOffset + numEntries < MAX_PUSH_CONSTANTS_ENTRIES, L"Out of memory to store root 32-bit constants. Please grow MAX_32BIT_ENTRIES constant in parameterset_dx12.h");
        memcpy(&m_Root32BitMem[m_Current32BitMemOffset], pConstData, sizeof(uint32_t) * numEntries);
        m_Root32BitEntries.push_back(std::make_pair(key, &m_Root32BitMem[m_Current32BitMemOffset]));
        m_Current32BitMemOffset += numEntries;
    }

    void ParameterSetInternal::Bind(CommandList* pCmdList, const PipelineObject* pPipeline)
    {

        if (m_pRootSignature->GetPipelineType() == PipelineType::Graphics)
            InitGraphics(pCmdList);
        else
            InitCompute(pCmdList);

        // Do root constant buffer views
        for (auto index : m_ValidBindings)
        {
            if (m_pRootSignature->GetPipelineType() == PipelineType::Graphics)
                SetGraphicsRootConstantBuffer(pCmdList, index, &m_RootConstantBuffers[index]);
            else
                SetComputeRootConstantBuffer(pCmdList, index, &m_RootConstantBuffers[index]);
        }

        // Do root 32-bit constants
        for (auto entry : m_Root32BitEntries)
        {
            uint32_t numEntries = (entry.first >> 16);
            uint32_t index      = (entry.first & 0x0000ffff);

            if (m_pRootSignature->GetPipelineType() == PipelineType::Graphics)
                SetGraphicsRoot32BitConstants(pCmdList, index, numEntries, entry.second);
            else
                SetComputeRoot32BitConstants(pCmdList, index, numEntries, entry.second);
        }
    }


    void ParameterSetInternal::InitGraphics(CommandList* pCmdList)
    {
        SetGraphicsRootSignature(pCmdList, m_pRootSignature);

        for (auto desc : m_pRootSignature->GetBindingDescriptions())
        {
            switch (desc.Type)
            {
            case BindingType::RootConstant:
            case BindingType::Root32BitConstant:
                continue;
            case BindingType::CBV:
                if (m_pImmediateResourceViews)
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::CBV)]));
                else
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pCBVResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            case BindingType::TextureSRV:
                if (m_pImmediateResourceViews)
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::TextureSRV)]));
                else
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pTextureSRVResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            case BindingType::TextureUAV:
                if (m_pImmediateResourceViews)
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::TextureUAV)]));
                else
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pTextureUAVResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            case BindingType::BufferSRV:
                if (m_pImmediateResourceViews)
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::BufferSRV)]));
                else
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pBufferSRVResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            case BindingType::BufferUAV:
                if (m_pImmediateResourceViews)
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::BufferUAV)]));
                else
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pBufferUAVResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            case BindingType::Sampler:
                if (m_pImmediateResourceViews)
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::Sampler)]));
                else
                    SetGraphicsRootResourceView(pCmdList, desc.BindingIndex, &m_pSamplerResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            }
        }
    }

    void ParameterSetInternal::InitCompute(CommandList* pCmdList)
    {
        SetComputeRootSignature(pCmdList, m_pRootSignature);

        for (auto desc : m_pRootSignature->GetBindingDescriptions())
        {
            switch (desc.Type)
            {
            case BindingType::RootConstant:
            case BindingType::Root32BitConstant:
                continue;
            case BindingType::CBV:
                if (m_pImmediateResourceViews)
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::CBV)]));
                else
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pCBVResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            case BindingType::TextureSRV:
                if (m_pImmediateResourceViews)
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::TextureSRV)]));
                else
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pTextureSRVResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            case BindingType::TextureUAV:
                if (m_pImmediateResourceViews)
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::TextureUAV)]));
                else
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pTextureUAVResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            case BindingType::BufferSRV:
                if (m_pImmediateResourceViews)
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::BufferSRV)]));
                else
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pBufferSRVResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            case BindingType::BufferUAV:
                if (m_pImmediateResourceViews)
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::BufferUAV)]));
                else
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pBufferUAVResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            case BindingType::Sampler:
                if (m_pImmediateResourceViews)
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pImmediateResourceViews->GetViewInfo(desc.BaseShaderRegister + m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::Sampler)]));
                else
                    SetComputeRootResourceView(pCmdList, desc.BindingIndex, &m_pSamplerResourceViews->GetViewInfo(desc.BaseShaderRegister));
                break;
            }
        }
    }

} // namespace cauldron

#endif // #if defined(_DX12)
