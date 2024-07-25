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

#if defined(_DX12)

#include "render/parameterset.h"

namespace cauldron
{
    class ParameterSetInternal final : public ParameterSet
    {
    public:
        virtual void SetRootConstantBufferResource(const GPUResource* pResource, const size_t size, uint32_t slotIndex) override;
        virtual void SetTextureSRV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex = 0, int32_t mip = -1, int32_t arraySize = -1, int32_t firstSlice = -1) override;
        virtual void SetTextureUAV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex = 0, int32_t mip = -1, int32_t arraySize = -1, int32_t firstSlice = -1) override;
        virtual void SetBufferSRV(const Buffer* pBuffer, uint32_t slotIndex = 0, uint32_t firstElement = -1, uint32_t numElements = -1) override;
        virtual void SetBufferUAV(const Buffer* pBuffer, uint32_t slotIndex = 0, uint32_t firstElement = -1, uint32_t numElements = -1) override;
        virtual void SetSampler(const Sampler* pSampler, uint32_t slotIndex = 0) override;
        virtual void SetAccelerationStructure(const TLAS* pTLAS, uint32_t slotIndex = 0) override;

        virtual void UpdateRootConstantBuffer(const BufferAddressInfo* pRootConstantBuffer, uint32_t rootBufferIndex) override;
        virtual void UpdateRoot32BitConstant(const uint32_t numEntries, const uint32_t* pConstData, uint32_t rootBufferIndex) override;
        virtual void Bind(CommandList* pCmdList, const PipelineObject* pPipeline) override;

    protected:
        int32_t GetResourceTableIndex(BindingType bindType, uint32_t slotIndex, const wchar_t* bindName) override;

    private:
        friend class ParameterSet;
        ParameterSetInternal(RootSignature* pRootSignature, ResourceView* pImmediateViews);
        virtual ~ParameterSetInternal() = default;

        void InitGraphics(CommandList* pCmdList);
        void InitCompute(CommandList* pCmdList);

    private:
        std::vector<BufferAddressInfo>  m_RootConstantBuffers = {};
        std::vector<int32_t>            m_ValidBindings = {};

        uint32_t m_Root32BitMem[MAX_PUSH_CONSTANTS_ENTRIES] = {0};
        uint32_t m_Current32BitMemOffset           = 0;
        std::vector<std::pair<uint32_t, uint32_t*>> m_Root32BitEntries = {};
    };

} // namespace cauldron

#endif // #if defined(_DX12)
