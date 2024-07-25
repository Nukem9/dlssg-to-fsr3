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

#include "core/framework.h"
#include "render/parameterset.h"
#include "render/resourceviewallocator.h"
#include "render/rootsignature.h"
#include "render/texture.h"

#include <vector>

namespace cauldron
{
    ParameterSet::ParameterSet(RootSignature* pRootSignature, ResourceView* pImmediateViews, uint32_t numBufferedSets) :
        ResourceResizedListener(),
        m_pRootSignature(pRootSignature),
        m_Immediate(pImmediateViews != nullptr),
        m_pImmediateResourceViews(pImmediateViews),
        m_BufferedSetCount(numBufferedSets)
    {
        // If we are in immediate mode, there's nothing to set up as we will bind on the fly with dynamic views
        if (m_Immediate)
            return;

        uint32_t cbvCount = 0;
        uint32_t srvTextureCount = 0;
        uint32_t srvBufferCount = 0;
        uint32_t uavTextureCount = 0;
        uint32_t uavBufferCount = 0;
        uint32_t samplerCount = 0;

        for (auto b : m_pRootSignature->GetBindingDescriptions())
        {
            switch (b.Type)
            {
            case BindingType::TextureSRV:
                srvTextureCount = std::max(srvTextureCount, b.BaseShaderRegister + b.Count);
                break;
            case BindingType::BufferSRV:
            case BindingType::AccelStructRT:
                srvBufferCount = std::max(srvBufferCount, b.BaseShaderRegister + b.Count);
                break;
            case BindingType::TextureUAV:
                uavTextureCount = std::max(uavTextureCount, b.BaseShaderRegister + b.Count);
                break;
            case BindingType::BufferUAV:
                uavBufferCount = std::max(uavBufferCount, b.BaseShaderRegister + b.Count);
                break;
            case BindingType::CBV:
                cbvCount = std::max(cbvCount, b.BaseShaderRegister + b.Count);
                break;
            case BindingType::Sampler:
                samplerCount = std::max(samplerCount, b.BaseShaderRegister + b.Count);
                break;
            case BindingType::RootConstant:
            case BindingType::Root32BitConstant:
                break;
            default:
                CauldronCritical(L"Unsupported resource view type for ParameterSet");
                break;
            }
        }

        m_CBVCount        = cbvCount;
        m_TextureSRVCount = srvTextureCount;
        m_BufferSRVCount  = srvBufferCount;
        m_TextureUAVCount = uavTextureCount;
        m_BufferUAVCount  = uavBufferCount;
        m_SamplerCount    = samplerCount;

        cbvCount        *= m_BufferedSetCount;
        srvTextureCount *= m_BufferedSetCount;
        srvBufferCount  *= m_BufferedSetCount;
        uavTextureCount *= m_BufferedSetCount;
        uavBufferCount  *= m_BufferedSetCount;
        samplerCount    *= m_BufferedSetCount;

        if (srvTextureCount > 0)
        {
            GetResourceViewAllocator()->AllocateGPUResourceViews(&m_pTextureSRVResourceViews, srvTextureCount);
            m_BoundTextureSRVs.resize(srvTextureCount);
        }
        if (srvBufferCount > 0)
        {
            GetResourceViewAllocator()->AllocateGPUResourceViews(&m_pBufferSRVResourceViews, srvBufferCount);
            m_BoundBufferSRVs.resize(srvBufferCount);
        }
        if (uavTextureCount > 0)
        {
            GetResourceViewAllocator()->AllocateGPUResourceViews(&m_pTextureUAVResourceViews, uavTextureCount);
            m_BoundTextureUAVs.resize(uavTextureCount);
        }
        if (uavBufferCount > 0)
        {
            GetResourceViewAllocator()->AllocateGPUResourceViews(&m_pBufferUAVResourceViews, uavBufferCount);
            m_BoundBufferUAVs.resize(uavBufferCount);
        }
        if (cbvCount > 0)
        {
            GetResourceViewAllocator()->AllocateGPUResourceViews(&m_pCBVResourceViews, cbvCount);
            m_BoundCBVs.resize(cbvCount);
        }
        if (samplerCount > 0)
        {
            GetResourceViewAllocator()->AllocateGPUSamplerViews(&m_pSamplerResourceViews, samplerCount);
            m_BoundSamplers.resize(samplerCount);
        }
    }

    ParameterSet::~ParameterSet()
    {
        // This parameter set lives on the stack and is used for immediate setting. None of it's resources are allocated and must therefore not be released.
        if (m_Immediate)
            return;

        delete m_pTextureSRVResourceViews;
        delete m_pTextureUAVResourceViews;
        delete m_pBufferSRVResourceViews;
        delete m_pBufferUAVResourceViews;
        delete m_pCBVResourceViews;
        delete m_pSamplerResourceViews;
    }

    ResourceViewInfo ParameterSet::BindTextureSRV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t bufferedSetIndex)
    {
        // Bind to the correct position in the resource view
        int32_t descOffset = GetResourceTableIndex(BindingType::TextureSRV, slotIndex, L"TextureSRV");
        const BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[descOffset];

        CauldronAssert(ASSERT_CRITICAL, desc.Type == BindingType::TextureSRV, L"Incorrect type at the given root parameter index");
        slotIndex += bufferedSetIndex * m_TextureSRVCount;
        m_pTextureSRVResourceViews->BindTextureResource(pTexture->GetResource(), pTexture->GetDesc(), ResourceViewType::TextureSRV, dimension, mip, arraySize, firstSlice, slotIndex);

        // Update reference
        m_BoundTextureSRVs[slotIndex].pTexture = pTexture;
        m_BoundTextureSRVs[slotIndex].RootParameterIndex = descOffset;
        m_BoundTextureSRVs[slotIndex].ShaderRegister = slotIndex;
        m_BoundTextureSRVs[slotIndex].Dimension = dimension;
        m_BoundTextureSRVs[slotIndex].Mip = mip;
        m_BoundTextureSRVs[slotIndex].ArraySize = arraySize;
        m_BoundTextureSRVs[slotIndex].FirstSlice = firstSlice;

        // Check if the set has resizable resources
        CheckResizable();

        return m_pTextureSRVResourceViews->GetViewInfo(slotIndex);
    }

    ResourceViewInfo ParameterSet::BindTextureUAV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t bufferedSetIndex)
    {
        // Bind to the correct position in the resource view
        int32_t descOffset = GetResourceTableIndex(BindingType::TextureUAV, slotIndex, L"TextureUAV");
        const BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[descOffset];

        CauldronAssert(ASSERT_CRITICAL, desc.Type == BindingType::TextureUAV, L"Incorrect type at the given root parameter index");
        slotIndex += bufferedSetIndex * m_TextureUAVCount;
        m_pTextureUAVResourceViews->BindTextureResource(pTexture->GetResource(), pTexture->GetDesc(), ResourceViewType::TextureUAV, dimension, mip, arraySize, firstSlice, slotIndex);

        // Update reference
        m_BoundTextureUAVs[slotIndex].pTexture = pTexture;
        m_BoundTextureUAVs[slotIndex].RootParameterIndex = descOffset;
        m_BoundTextureUAVs[slotIndex].ShaderRegister = slotIndex;
        m_BoundTextureUAVs[slotIndex].Dimension = dimension;
        m_BoundTextureUAVs[slotIndex].Mip = mip;
        m_BoundTextureUAVs[slotIndex].ArraySize = arraySize;
        m_BoundTextureUAVs[slotIndex].FirstSlice = firstSlice;

        // Check if the set has resizable resources
        CheckResizable();

        return m_pTextureUAVResourceViews->GetViewInfo(slotIndex);
    }

    ResourceViewInfo ParameterSet::BindBufferUAV(const Buffer* pBuffer, uint32_t slotIndex, uint32_t firstElement, uint32_t numElements, uint32_t bufferedSetIndex)
    {
        // Bind to the correct position in the resource view
        int32_t descOffset = GetResourceTableIndex(BindingType::BufferUAV, slotIndex, L"BufferUAV");
        const BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[descOffset];

        CauldronAssert(ASSERT_CRITICAL, desc.Type == BindingType::BufferUAV, L"Incorrect type at the given root parameter index");
        slotIndex += bufferedSetIndex * m_BufferUAVCount;
        m_pBufferUAVResourceViews->BindBufferResource(pBuffer->GetResource(), pBuffer->GetDesc(), ResourceViewType::BufferUAV, firstElement, numElements, slotIndex);

        // Update reference
        m_BoundBufferUAVs[slotIndex].pBuffer = pBuffer;
        m_BoundBufferUAVs[slotIndex].RootParameterIndex = descOffset;
        m_BoundBufferUAVs[slotIndex].ShaderRegister = slotIndex;
        m_BoundBufferUAVs[slotIndex].Dimension = ViewDimension::Buffer;
        m_BoundBufferUAVs[slotIndex].FirstElement = firstElement;
        m_BoundBufferUAVs[slotIndex].NumElements = numElements;

        // Check if the set has resizable resources
        CheckResizable();

        return m_pBufferUAVResourceViews->GetViewInfo(slotIndex);
    }

    ResourceViewInfo ParameterSet::BindBufferSRV(const Buffer* pBuffer, uint32_t slotIndex, uint32_t firstElement, uint32_t numElements, uint32_t bufferedSetIndex)
    {
        // Bind to the correct position in the resource view
        int32_t descOffset = GetResourceTableIndex(BindingType::BufferSRV, slotIndex, L"BufferSRV");
        const BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[descOffset];

        CauldronAssert(ASSERT_CRITICAL, desc.Type == BindingType::BufferSRV, L"Incorrect type at the given root parameter index");
        slotIndex += bufferedSetIndex * m_BufferSRVCount;
        m_pBufferSRVResourceViews->BindBufferResource(pBuffer->GetResource(), pBuffer->GetDesc(), ResourceViewType::BufferSRV, firstElement, numElements, slotIndex);

        // Update reference
        m_BoundBufferSRVs[slotIndex].pBuffer = pBuffer;
        m_BoundBufferSRVs[slotIndex].RootParameterIndex = descOffset;
        m_BoundBufferSRVs[slotIndex].ShaderRegister = slotIndex;
        m_BoundBufferSRVs[slotIndex].Dimension = ViewDimension::Buffer;
        m_BoundBufferSRVs[slotIndex].FirstElement = firstElement;
        m_BoundBufferSRVs[slotIndex].NumElements = numElements;

        // Check if the set has resizable resources
        CheckResizable();

        return m_pBufferSRVResourceViews->GetViewInfo(slotIndex);
    }

    ResourceViewInfo ParameterSet::BindSampler(const Sampler* pSampler, uint32_t slotIndex, uint32_t bufferedSetIndex)
    {
        // Bind to the correct position in the resource view
        int32_t descOffset = GetResourceTableIndex(BindingType::Sampler, slotIndex, L"Sampler");
        const BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[descOffset];

        CauldronAssert(ASSERT_CRITICAL, desc.Type == BindingType::Sampler, L"Incorrect type at the given root parameter index");
        slotIndex += bufferedSetIndex * m_SamplerCount;
        m_pSamplerResourceViews->BindSamplerResource(pSampler, slotIndex);

        // Update reference
        m_BoundSamplers[slotIndex].pSampler = pSampler;
        m_BoundSamplers[slotIndex].RootParameterIndex = descOffset;
        m_BoundSamplers[slotIndex].ShaderRegister = slotIndex;

        return m_pSamplerResourceViews->GetViewInfo(slotIndex);
    }

    ResourceViewInfo ParameterSet::GetTextureSRV(uint32_t rootParameterIndex, uint32_t slotIndex)
    {
        BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[rootParameterIndex];
        CauldronAssert(ASSERT_CRITICAL, desc.Type == BindingType::TextureSRV, L"Incorrect type at the given root parameter index");

        return m_pTextureSRVResourceViews->GetViewInfo(slotIndex);
    }

    ResourceViewInfo ParameterSet::GetTextureUAV(uint32_t rootParameterIndex, uint32_t slotIndex)
    {
        BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[rootParameterIndex];
        CauldronAssert(ASSERT_CRITICAL, desc.Type == BindingType::TextureUAV, L"Incorrect type at the given root parameter index");
        uint32_t globalslotIndex = slotIndex;
        return m_pTextureUAVResourceViews->GetViewInfo(globalslotIndex);
    }

    ResourceViewInfo ParameterSet::GetBufferSRV(uint32_t rootParameterIndex, uint32_t slotIndex)
    {
        BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[rootParameterIndex];
        CauldronAssert(ASSERT_CRITICAL, desc.Type == BindingType::BufferSRV, L"Incorrect type at the given root parameter index");
        uint32_t globalslotIndex = slotIndex;
        return m_pBufferSRVResourceViews->GetViewInfo(globalslotIndex);
    }

    ResourceViewInfo ParameterSet::GetBufferUAV(uint32_t rootParameterIndex, uint32_t slotIndex)
    {
        BindingDesc desc = m_pRootSignature->GetBindingDescriptions()[rootParameterIndex];
        CauldronAssert(ASSERT_CRITICAL, desc.Type == BindingType::BufferUAV, L"Incorrect type at the given root parameter index");
        uint32_t globalslotIndex = slotIndex;
        return m_pBufferUAVResourceViews->GetViewInfo(globalslotIndex);
    }

    void ParameterSet::OnResourceResized()
    {
        // We assume that all the resource that are resizable have been resized
        for (uint32_t i = 0; i < m_BoundTextureSRVs.size(); ++i)
        {
            BoundResource b = m_BoundTextureSRVs[i];
            if (b.pTexture != nullptr && b.pTexture->GetResource()->IsResizable())
                m_pTextureSRVResourceViews->BindTextureResource(b.pTexture->GetResource(), b.pTexture->GetDesc(), ResourceViewType::TextureSRV, b.Dimension, b.Mip, b.ArraySize, b.FirstSlice, i);
        }

        for (uint32_t i = 0; i < m_BoundTextureUAVs.size(); ++i)
        {
            BoundResource b = m_BoundTextureUAVs[i];
            if (b.pTexture != nullptr && b.pTexture->GetResource()->IsResizable())
                m_pTextureUAVResourceViews->BindTextureResource(b.pTexture->GetResource(), b.pTexture->GetDesc(), ResourceViewType::TextureUAV, b.Dimension, b.Mip, b.ArraySize, b.FirstSlice, i);
        }

        for (uint32_t i = 0; i < m_BoundBufferSRVs.size(); ++i)
        {
            BoundResource b = m_BoundBufferSRVs[i];
            if (b.pBuffer != nullptr && b.pBuffer->GetResource()->IsResizable())
                m_pBufferSRVResourceViews->BindBufferResource(b.pBuffer->GetResource(), b.pBuffer->GetDesc(), ResourceViewType::BufferSRV, b.FirstElement, b.NumElements, i);
        }

        for (uint32_t i = 0; i < m_BoundBufferUAVs.size(); ++i)
        {
            BoundResource b = m_BoundBufferUAVs[i];
            if (b.pBuffer != nullptr && b.pBuffer->GetResource()->IsResizable())
                m_pBufferUAVResourceViews->BindBufferResource(b.pBuffer->GetResource(), b.pBuffer->GetDesc(), ResourceViewType::BufferUAV, b.FirstElement, b.NumElements, i);
        }

        // Add CBV support when necessary
    }

    void ParameterSet::CheckResizable()
    {

        for (auto b : m_BoundTextureSRVs)
        {
            if (b.pTexture != nullptr && b.pTexture->GetResource()->IsResizable())
            {
                MarkAsResizableResourceDependent();
                return;
            }
        }

        for (auto b : m_BoundTextureUAVs)
        {
            if (b.pTexture != nullptr && b.pTexture->GetResource()->IsResizable())
            {
                MarkAsResizableResourceDependent();
                return;
            }
        }

        for (auto b : m_BoundBufferSRVs)
        {
            if (b.pBuffer != nullptr && b.pBuffer->GetResource()->IsResizable())
            {
                MarkAsResizableResourceDependent();
                return;
            }
        }

        for (auto b : m_BoundBufferUAVs)
        {
            if (b.pBuffer != nullptr && b.pBuffer->GetResource()->IsResizable())
            {
                MarkAsResizableResourceDependent();
                return;
            }
        }

        // no resizable resource has been found
        MarkAsResizableResourceIndependent();
    }

} // namespace cauldron
