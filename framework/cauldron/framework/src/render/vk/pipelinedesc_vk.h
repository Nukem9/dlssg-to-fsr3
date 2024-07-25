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

#if defined(_VK)
#include "render/pipelinedesc.h"
#include "render/mesh.h"
#include <vulkan/vulkan.h>
#include <array>

#define BINDING_SHIFT(name, shift) \
    constexpr uint32_t name##_BINDING_SHIFT = shift;\
    constexpr wchar_t* name##_BINDING_SHIFT_STR = L#shift;

namespace cauldron
{
    // put it there for now
    BINDING_SHIFT(TEXTURE, 0);
    BINDING_SHIFT(SAMPLER, 1000);
    BINDING_SHIFT(UNORDERED_ACCESS_VIEW, 2000);
    BINDING_SHIFT(CONSTANT_BUFFER, 3000);

    VkCompareOp ConvertComparisonFunc(const ComparisonFunc func);

    struct PipelineDescInternal final
    {
        std::array<VkVertexInputBindingDescription, static_cast<uint32_t>(VertexAttributeType::Count)> m_BindingDescriptions;
        std::array<VkVertexInputAttributeDescription, static_cast<uint32_t>(VertexAttributeType::Count)> m_AttributeDescriptions;

        RootSignature* m_pRootSignature = nullptr;
        VkPipelineVertexInputStateCreateInfo   m_VertexInputInfo = {};
        VkPipelineRasterizationStateCreateInfo m_RasterizationState = {};
        VkPipelineMultisampleStateCreateInfo   m_MultisampleState = {};
        VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyState = {};
        VkPipelineDepthStencilStateCreateInfo  m_DepthStencilState = {};
        VkFormat                               m_DepthFormat = VK_FORMAT_UNDEFINED;
        uint32_t                               m_NumAttachments = 0;
        VkFormat                               m_ColorAttachmentFormats[PipelineDesc::s_MaxRenderTargets];
        VkPipelineColorBlendAttachmentState    m_BlendStates[PipelineDesc::s_MaxRenderTargets];

        // Functions
        void ResetBlendStates(size_t baseIndex)
        {
            for (size_t i = baseIndex; i < PipelineDesc::s_MaxRenderTargets; ++i)
            {
                VkPipelineColorBlendAttachmentState& blendAttachment = m_BlendStates[i];
                blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                blendAttachment.blendEnable = VK_FALSE;
                blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
                blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            }
        }
    };

} // namespace cauldron

#endif // #if defined(_VK)
