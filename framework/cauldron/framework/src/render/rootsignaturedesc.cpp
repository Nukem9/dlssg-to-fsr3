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

#include "render/rootsignaturedesc.h"
#include "misc/assert.h"

namespace cauldron
{
    RootSignatureDesc& RootSignatureDesc::operator=(RootSignatureDesc&& right) noexcept
    {
        m_PipelineType              = right.m_PipelineType;
        m_pSignatureDescImpl        = right.m_pSignatureDescImpl;
        right.m_pSignatureDescImpl  = nullptr; // Prevent multiple deletes
        return *this;
    }

    RootSignatureDesc& RootSignatureDesc::operator=(const RootSignatureDesc&& right) noexcept
    {
        return this->operator=(const_cast<RootSignatureDesc&&>(right));
    }

    void RootSignatureDesc::UpdatePipelineType(ShaderBindStage bindStages)
    {
        if (static_cast<bool>(bindStages & ShaderBindStage::Vertex) || 
            static_cast<bool>(bindStages & ShaderBindStage::Pixel) || 
            static_cast<bool>(bindStages & ShaderBindStage::VertexAndPixel) )
        {
            CauldronAssert(ASSERT_CRITICAL, m_PipelineType == PipelineType::Graphics || m_PipelineType == PipelineType::Undefined, L"Root signature is already set for another pipeline than graphics.");
            m_PipelineType = PipelineType::Graphics;
        }

        if (static_cast<bool>(bindStages & ShaderBindStage::Compute))
        {
            CauldronAssert(ASSERT_CRITICAL, m_PipelineType == PipelineType::Compute || m_PipelineType == PipelineType::Undefined, L"Root signature is already set for another pipeline than compute.");
            m_PipelineType = PipelineType::Compute;
        }
    }

} // namespace cauldron
