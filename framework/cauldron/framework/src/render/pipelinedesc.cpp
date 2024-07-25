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

#include "render/pipelinedesc.h"

#include "core/framework.h"
#include "misc/assert.h"

namespace cauldron
{
    // Move operators
    PipelineDesc& PipelineDesc::operator=(PipelineDesc&& right) noexcept
    {
        m_ShaderDescriptions = right.m_ShaderDescriptions;
        m_ShaderBlobDescriptions = right.m_ShaderBlobDescriptions;

        m_PipelineType = right.m_PipelineType;
        m_PipelineImpl = right.m_PipelineImpl;
        right.m_PipelineImpl = nullptr; // Prevent multiple deletes
        return *this;
    }

    PipelineDesc& PipelineDesc::operator=(const PipelineDesc&& right) noexcept
    {
        return this->operator=(const_cast<PipelineDesc&&>(right));
    }

    void PipelineDesc::AddShaderDesc(ShaderBuildDesc& shaderDesc)
    {
        if (shaderDesc.Stage == ShaderStage::Compute)
        {
            CauldronAssert(ASSERT_CRITICAL, m_PipelineType == PipelineType::Compute || m_PipelineType == PipelineType::Undefined, L"Compute shader has been added a pipeline description that isn't a compute one. Terminating due to invalid behavior");
            m_PipelineType = PipelineType::Compute;
        }
        else
        {
            CauldronAssert(ASSERT_CRITICAL, m_PipelineType == PipelineType::Graphics || m_PipelineType == PipelineType::Undefined, L"Graphics shader has been added a pipeline description that isn't a graphics one. Terminating due to invalid behavior");
            m_PipelineType = PipelineType::Graphics;
        }

        // Append defines for near/far depth
        static bool s_InvertedDepth = GetConfig()->InvertedDepth;
        if (s_InvertedDepth)
        {
            shaderDesc.Defines.insert(std::make_pair(L"FAR_DEPTH", L"0.0"));
            shaderDesc.Defines.insert(std::make_pair(L"NEAR_DEPTH", L"1.0"));
        }
        else
        {
            shaderDesc.Defines.insert(std::make_pair(L"FAR_DEPTH", L"1.0"));
            shaderDesc.Defines.insert(std::make_pair(L"NEAR_DEPTH", L"0.0"));
        }

        m_ShaderDescriptions.push_back(shaderDesc);
    }

    void PipelineDesc::AddShaderBlobDesc(ShaderBlobDesc& shaderBlobDesc)
    {
        if (shaderBlobDesc.Stage == ShaderStage::Compute)
        {
            CauldronAssert(ASSERT_CRITICAL,
                           m_PipelineType == PipelineType::Compute || m_PipelineType == PipelineType::Undefined,
                           L"Compute shader has been added a pipeline description that isn't a compute one. Terminating due to invalid behavior");
            m_PipelineType = PipelineType::Compute;
        }
        else
        {
            CauldronAssert(ASSERT_CRITICAL,
                           m_PipelineType == PipelineType::Graphics || m_PipelineType == PipelineType::Undefined,
                           L"Graphics shader has been added a pipeline description that isn't a graphics one. Terminating due to invalid behavior");
            m_PipelineType = PipelineType::Graphics;
        }

        m_ShaderBlobDescriptions.push_back(shaderBlobDesc);
    }

    void PipelineDesc::AddRasterFormats(const ResourceFormat& rtFormat, const ResourceFormat depthFormat /*= ResourceFormat::Unknown*/)
    {
        CauldronAssert(ASSERT_CRITICAL, rtFormat != ResourceFormat::Unknown || depthFormat != ResourceFormat::Unknown, L"There are no formats to pass to the pipeline description.");
        AddRenderTargetFormats(1, &rtFormat, depthFormat);
    }

    void PipelineDesc::AddRasterFormats(const std::vector<ResourceFormat>& rtFormats, const ResourceFormat depthFormat /*= ResourceFormat::Unknown*/)
    {
        CauldronAssert(ASSERT_CRITICAL, rtFormats.size() || depthFormat != ResourceFormat::Unknown, L"There are no formats to pass to the pipeline description.");
        AddRenderTargetFormats((uint32_t)rtFormats.size(), rtFormats.data(), depthFormat);
    }

    void PipelineDesc::SetWave64(bool isWave64)
    {
        m_IsWave64 = isWave64;
    }

} // namespace cauldron
