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
#include "render/dx12/pipelinedesc_dx12.h"
#include "render/dx12/pipelineobject_dx12.h"

#include "core/framework.h"
#include "misc/assert.h"

#include "dxheaders/include/directx/d3dx12.h"

namespace cauldron
{
    PipelineObject* PipelineObject::CreatePipelineObject(const wchar_t*      pipelineObjectName,
                                                         const PipelineDesc& Desc,
                                                         std::vector<const wchar_t*>* pAdditionalParameters/*=nullptr*/)
    {
        PipelineObjectInternal* pNewPipeline = new PipelineObjectInternal(pipelineObjectName);

        // Build in one step before returning
        pNewPipeline->Build(Desc, pAdditionalParameters);

        return pNewPipeline;
    }

    PipelineObjectInternal::PipelineObjectInternal(const wchar_t* pipelineObjectName) :
        PipelineObject(pipelineObjectName)
    {
    }

    // Most of the setup is in the desc class, so we just need to build the right type
    void PipelineObjectInternal::Build(const PipelineDesc& desc, std::vector<const wchar_t*>* pAdditionalParameters)
    {
        m_Desc = std::move(desc);
        m_Type = desc.GetPipelineType();

        // Start by doing all shader builds
        m_Desc.AddShaders(pAdditionalParameters);

        if (m_Type == PipelineType::Compute)
        {
            CauldronThrowOnFail(GetDevice()->GetImpl()->DX12Device()->CreateComputePipelineState(&m_Desc.GetImpl()->m_ComputePipelineDesc, IID_PPV_ARGS(&m_PipelineState)));
        }

        // Graphics
        else
        {
            CauldronThrowOnFail(GetDevice()->GetImpl()->DX12Device()->CreateGraphicsPipelineState(&m_Desc.GetImpl()->m_GraphicsPipelineDesc, IID_PPV_ARGS(&m_PipelineState)));
        }

        m_PipelineState->SetName(m_Name.c_str());

        // Release all shader binaries as they are no longer needed
        m_Desc.GetImpl()->m_ShaderBinaryStore.clear();
    }

} // namespace cauldron

#endif // #if defined(_DX12)
