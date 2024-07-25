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

#include "render/pipelineobject.h"
#include "render/dx12/defines_dx12.h"

#include "dxc/inc/dxcapi.h"

namespace cauldron
{
    class PipelineObjectInternal final : public PipelineObject
    {
    public:
        const ID3D12PipelineState* DX12PipelineState() const { return m_PipelineState.Get(); }
        ID3D12PipelineState* DX12PipelineState() { return m_PipelineState.Get(); }

        PipelineObjectInternal* GetImpl() override { return this; }
        const PipelineObjectInternal* GetImpl() const override { return this; }

    private:
        friend class PipelineObject;
        PipelineObjectInternal(const wchar_t* pipelineObjectName);
        virtual ~PipelineObjectInternal() = default;

        void Build(const PipelineDesc& Desc, std::vector<const wchar_t*>* pAdditionalParameters = nullptr) override;

    private:
        // Internal members
        MSComPtr<ID3D12PipelineState> m_PipelineState = nullptr;
    };

} // namespace cauldron

#endif // #if defined(_DX12)
