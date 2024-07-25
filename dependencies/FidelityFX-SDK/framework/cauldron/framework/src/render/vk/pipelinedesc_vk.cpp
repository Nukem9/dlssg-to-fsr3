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

#if defined(_VK)
#include "render/vk/pipelinedesc_vk.h"

#include "core/framework.h"
#include "render/shaderbuilder.h"
#include "helpers.h"

#include "dxc/inc/dxcapi.h"

#include <algorithm>
#include <array>

namespace cauldron
{
    inline VkBool32 ConvertBool(const bool boolean)
    {
        return boolean ? VK_TRUE : VK_FALSE;
    }

    VkBlendFactor ConvertBlend(const Blend blend)
    {
        switch (blend)
        {
        case Blend::Zero:
            return VK_BLEND_FACTOR_ZERO;
        case Blend::One:
            return VK_BLEND_FACTOR_ONE;
        case Blend::SrcColor:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case Blend::DstColor:
            return VK_BLEND_FACTOR_DST_COLOR;
        case Blend::InvSrcColor:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case Blend::InvDstColor:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case Blend::SrcAlpha:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case Blend::DstAlpha:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case Blend::InvSrcAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case Blend::InvDstAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case Blend::SrcAlphaSat:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case Blend::BlendFactor:
            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case Blend::InvBlendFactor:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        default:
            return VK_BLEND_FACTOR_MAX_ENUM;
        }
    }

    VkBlendOp ConvertBlendOp(const BlendOp op)
    {
        switch (op)
        {
        case BlendOp::Add:
            return VK_BLEND_OP_ADD;
        case BlendOp::Subtract:
            return VK_BLEND_OP_SUBTRACT;
        case BlendOp::RevSubtract:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::Min:
            return VK_BLEND_OP_MIN;
        case BlendOp::Max:
            return VK_BLEND_OP_MAX;
        default:
            return VK_BLEND_OP_MAX_ENUM;
        }
    }

    VkCullModeFlags ConvertCullMode(const CullMode mode)
    {
        switch (mode)
        {
        case CullMode::None:
            return VK_CULL_MODE_NONE;
        case CullMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:
            return VK_CULL_MODE_BACK_BIT;
        default:
            return VK_CULL_MODE_NONE;
        }
    }

    VkStencilOp ConvertStencilOp(const StencilOp op)
    {
        switch (op)
        {
        case StencilOp::Zero:
            return VK_STENCIL_OP_ZERO;
        case StencilOp::Keep:
            return VK_STENCIL_OP_KEEP;
        case StencilOp::Replace:
            return VK_STENCIL_OP_REPLACE;
        case StencilOp::IncrementSat:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case StencilOp::DecrementSat:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case StencilOp::Invert:
            return VK_STENCIL_OP_INVERT;
        case StencilOp::Increment:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case StencilOp::Decrement:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default:
            return VK_STENCIL_OP_ZERO;
        }
    }

    VkPrimitiveTopology ConvertTopologyType(const PrimitiveTopologyType topology)
    {
        switch (topology)
        {
        case PrimitiveTopologyType::Point:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveTopologyType::Line:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopologyType::Triangle:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopologyType::Patch:
            return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        case PrimitiveTopologyType::Undefined:
        default:
            return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM; // TODO: what should it be?
        }
    }

    VkSampleCountFlagBits GetSampleCount(const uint32_t sampleCount)
    {
        if (sampleCount >= 64)
            return VK_SAMPLE_COUNT_64_BIT;
        else if (sampleCount >= 32)
            return VK_SAMPLE_COUNT_32_BIT;
        else if (sampleCount >= 16)
            return VK_SAMPLE_COUNT_16_BIT;
        else if (sampleCount >= 8)
            return VK_SAMPLE_COUNT_8_BIT;
        else if (sampleCount >= 4)
            return VK_SAMPLE_COUNT_4_BIT;
        else if (sampleCount >= 2)
            return VK_SAMPLE_COUNT_2_BIT;
        else
            return VK_SAMPLE_COUNT_1_BIT;
    }

    VkStencilOpState ConvertStencilDepth(const StencilDesc stencilDesc, const DepthDesc* pDepthDesc)
    {
        VkStencilOpState state = {};
        state.failOp = ConvertStencilOp(stencilDesc.StencilFailOp);
        state.passOp = ConvertStencilOp(stencilDesc.StencilPassOp);
        state.depthFailOp = ConvertStencilOp(stencilDesc.StencilDepthFailOp);
        state.compareOp = ConvertComparisonFunc(stencilDesc.StencilFunc);
        state.compareMask = static_cast<uint32_t>(pDepthDesc->StencilReadMask);
        state.writeMask = static_cast<uint32_t>(pDepthDesc->StencilWriteMask);
        state.reference = 0xffffffff;
        return state;
    }

    inline VkColorComponentFlagBits ConvertFlag(uint8_t flags, uint8_t mask, VkColorComponentFlagBits result)
    {
        if ((flags & mask) != 0)
            return result;
        return static_cast<VkColorComponentFlagBits>(0);
    }

    VkImageLayout FindDepthStencilLayout(const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState, VkFormat depthFormat)
    {
        auto stencilWriteFunction = [](const VkStencilOpState state)
        {
            return state.passOp != VK_STENCIL_OP_KEEP
                || state.failOp != VK_STENCIL_OP_KEEP
                || state.depthFailOp != VK_STENCIL_OP_KEEP;
        };

        bool hasDepth = depthFormat != VK_FORMAT_UNDEFINED;
        bool depthWrite = pDepthStencilState->depthWriteEnable == VK_TRUE;
        bool hasStencil = HasStencilComponent(depthFormat);
        bool stencilWrite = pDepthStencilState->stencilTestEnable == VK_TRUE
            && (stencilWriteFunction(pDepthStencilState->front) || stencilWriteFunction(pDepthStencilState->back));

        if (hasDepth)
        {
            if (hasStencil)
            {
                if (depthWrite)
                {
                    if (stencilWrite)
                        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    else
                        return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
                }
                else
                {
                    if (stencilWrite)
                        return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
                    else
                        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                }
            }
            else
            {
                if (depthWrite)
                    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                else
                    return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
            }
        }
        else if (hasStencil)
        {
            if (stencilWrite)
                return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            else
                return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
        }
        else
        {
            return VK_IMAGE_LAYOUT_UNDEFINED;
        }

    }

    VkCompareOp ConvertComparisonFunc(const ComparisonFunc func)
    {
        static bool invertedDepth = GetConfig()->InvertedDepth;

        switch (func)
        {
        case ComparisonFunc::Never:
            return VK_COMPARE_OP_NEVER;
        case ComparisonFunc::Less:
            return invertedDepth ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
        case ComparisonFunc::Equal:
            return VK_COMPARE_OP_EQUAL;
        case ComparisonFunc::LessEqual:
            return invertedDepth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
        case ComparisonFunc::Greater:
            return invertedDepth ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_GREATER;
        case ComparisonFunc::NotEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case ComparisonFunc::GreaterEqual:
            return invertedDepth ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;
        case ComparisonFunc::Always:
            return VK_COMPARE_OP_ALWAYS;
        default:
            return VK_COMPARE_OP_NEVER;
        }
    }

    PipelineDesc::PipelineDesc()
    {
        // Allocate implementation
        m_PipelineImpl = new PipelineDescInternal;

        // set the default values
        // vertex input
        m_PipelineImpl->m_VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        m_PipelineImpl->m_VertexInputInfo.vertexBindingDescriptionCount = 0;
        m_PipelineImpl->m_VertexInputInfo.pVertexBindingDescriptions = nullptr;
        m_PipelineImpl->m_VertexInputInfo.vertexAttributeDescriptionCount = 0;
        m_PipelineImpl->m_VertexInputInfo.pVertexAttributeDescriptions = nullptr;

        // rasterization state
        m_PipelineImpl->m_RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        m_PipelineImpl->m_RasterizationState.depthClampEnable = VK_FALSE;
        m_PipelineImpl->m_RasterizationState.rasterizerDiscardEnable = VK_FALSE;
        m_PipelineImpl->m_RasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        m_PipelineImpl->m_RasterizationState.lineWidth = 1.0f;
        m_PipelineImpl->m_RasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
        m_PipelineImpl->m_RasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
        m_PipelineImpl->m_RasterizationState.depthBiasEnable = VK_FALSE;
        m_PipelineImpl->m_RasterizationState.depthBiasConstantFactor = 0.0f;
        m_PipelineImpl->m_RasterizationState.depthBiasClamp = 0.0f;
        m_PipelineImpl->m_RasterizationState.depthBiasSlopeFactor = 0.0f;

        // multi-sample state
        m_PipelineImpl->m_MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        m_PipelineImpl->m_MultisampleState.sampleShadingEnable = VK_FALSE;
        m_PipelineImpl->m_MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        m_PipelineImpl->m_MultisampleState.minSampleShading = 1.0f; // Optional
        m_PipelineImpl->m_MultisampleState.pSampleMask = nullptr; // Optional
        m_PipelineImpl->m_MultisampleState.alphaToCoverageEnable = VK_FALSE;
        m_PipelineImpl->m_MultisampleState.alphaToOneEnable = VK_FALSE; // Optional

        // input assembly state
        m_PipelineImpl->m_InputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        m_PipelineImpl->m_InputAssemblyState.primitiveRestartEnable = VK_FALSE;

        // Depth-stencil state
        m_PipelineImpl->m_DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        m_PipelineImpl->m_DepthStencilState.depthTestEnable = VK_FALSE;
        m_PipelineImpl->m_DepthStencilState.depthWriteEnable = VK_FALSE;
        m_PipelineImpl->m_DepthStencilState.depthCompareOp = VK_COMPARE_OP_NEVER;
        m_PipelineImpl->m_DepthStencilState.depthBoundsTestEnable = VK_FALSE;
        m_PipelineImpl->m_DepthStencilState.minDepthBounds = 0.0f; // Optional
        m_PipelineImpl->m_DepthStencilState.maxDepthBounds = 1.0f; // Optional
        m_PipelineImpl->m_DepthStencilState.stencilTestEnable = VK_FALSE;

        auto initStencilState = [](VkStencilOpState& state) {
            state.failOp = VK_STENCIL_OP_KEEP;
            state.passOp = VK_STENCIL_OP_KEEP;
            state.depthFailOp = VK_STENCIL_OP_KEEP;
            state.compareOp = VK_COMPARE_OP_NEVER;
            state.compareMask = 0xffffffff;
            state.writeMask = 0xffffffff;
            state.reference = 0;
        };

        initStencilState(m_PipelineImpl->m_DepthStencilState.front);
        initStencilState(m_PipelineImpl->m_DepthStencilState.back);

        m_PipelineImpl->ResetBlendStates(0);
    }

    PipelineDesc::~PipelineDesc()
    {
        // Delete allocated impl memory as it's no longer needed
        delete m_PipelineImpl;
    }

    void PipelineDesc::SetRootSignature(RootSignature* pRootSignature)
    {
        m_PipelineImpl->m_pRootSignature = pRootSignature;
    }


    // Define an input layout for the pipeline object (with manual information)
    void PipelineDesc::AddInputLayout(std::vector<InputLayoutDesc>& inputLayouts)
    {
        // compute strides
        std::array<uint32_t, static_cast<uint32_t>(VertexAttributeType::Count)> bindingStrides;
        bindingStrides.fill(0);
        for (auto inputLayout : inputLayouts)
        {
            bindingStrides[inputLayout.AttributeInputSlot] += GetResourceFormatStride(inputLayout.AttributeFmt);
        }

        // set bindings
        std::array<bool, static_cast<uint32_t>(VertexAttributeType::Count)> setBindings;
        setBindings.fill(false);
        uint32_t numBindings = 0;
        for (auto inputLayout : inputLayouts)
        {
            if (setBindings[inputLayout.AttributeInputSlot])
                continue;

            m_PipelineImpl->m_BindingDescriptions[numBindings].binding = inputLayout.AttributeInputSlot;
            m_PipelineImpl->m_BindingDescriptions[numBindings].stride = bindingStrides[inputLayout.AttributeInputSlot];
            m_PipelineImpl->m_BindingDescriptions[numBindings].inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;

            ++numBindings;

            setBindings[inputLayout.AttributeInputSlot] = true;
        }

        // set attributes
        uint32_t numVertexAttributes = 0;
        for (auto inputLayout : inputLayouts)
        {
            m_PipelineImpl->m_AttributeDescriptions[numVertexAttributes].binding = inputLayout.AttributeInputSlot;
            m_PipelineImpl->m_AttributeDescriptions[numVertexAttributes].location = numVertexAttributes;
            m_PipelineImpl->m_AttributeDescriptions[numVertexAttributes].format = GetVkFormat(inputLayout.AttributeFmt);
            m_PipelineImpl->m_AttributeDescriptions[numVertexAttributes].offset = inputLayout.AttributeOffset;

            ++numVertexAttributes;
        }

        m_PipelineImpl->m_VertexInputInfo.vertexBindingDescriptionCount = numBindings;
        m_PipelineImpl->m_VertexInputInfo.pVertexBindingDescriptions = m_PipelineImpl->m_BindingDescriptions.data();
        m_PipelineImpl->m_VertexInputInfo.vertexAttributeDescriptionCount = numVertexAttributes;
        m_PipelineImpl->m_VertexInputInfo.pVertexAttributeDescriptions = m_PipelineImpl->m_AttributeDescriptions.data();
    }

    void PipelineDesc::AddRasterStateDescription(RasterDesc* pRasterDesc)
    {
        m_PipelineImpl->m_RasterizationState.depthClampEnable = ConvertBool(pRasterDesc->DepthClipEnable);
        m_PipelineImpl->m_RasterizationState.rasterizerDiscardEnable = VK_FALSE;
        m_PipelineImpl->m_RasterizationState.polygonMode = pRasterDesc->Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        m_PipelineImpl->m_RasterizationState.cullMode = ConvertCullMode(pRasterDesc->CullingMode);
        m_PipelineImpl->m_RasterizationState.frontFace = pRasterDesc->FrontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        m_PipelineImpl->m_RasterizationState.depthBiasConstantFactor = static_cast<float>(pRasterDesc->DepthBias);
        m_PipelineImpl->m_RasterizationState.depthBiasClamp = pRasterDesc->DepthBiasClamp;
        m_PipelineImpl->m_RasterizationState.depthBiasSlopeFactor = pRasterDesc->SlopeScaledDepthBias;

        m_PipelineImpl->m_MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        m_PipelineImpl->m_MultisampleState.sampleShadingEnable = ConvertBool(pRasterDesc->MultisampleEnable);
        if (pRasterDesc->MultisampleEnable)
            m_PipelineImpl->m_MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT; // hardcoded MSAA 4x
    }

    void PipelineDesc::AddBlendStates(const std::vector<BlendDesc>& blendDescs, bool alphaToCoverage, bool independentBlend)
    {
        m_PipelineImpl->m_MultisampleState.alphaToCoverageEnable = ConvertBool(alphaToCoverage);

        // independentBlend iis set at the device creation. It cannot be set per pipeline.

        CauldronAssert(ASSERT_WARNING, blendDescs.size() <= s_MaxRenderTargets, L"Cannot set more than %d blend states.", s_MaxRenderTargets);
        uint32_t maxBlendStates = static_cast<uint32_t>(blendDescs.size() < s_MaxRenderTargets ? blendDescs.size() : s_MaxRenderTargets);
        for (size_t i = 0; i < maxBlendStates; ++i)
        {
            const BlendDesc& desc = blendDescs[i];
            VkPipelineColorBlendAttachmentState& blendAttachment = m_PipelineImpl->m_BlendStates[i];

            blendAttachment.colorWriteMask = ConvertFlag(desc.RenderTargetWriteMask, 0x01, VK_COLOR_COMPONENT_R_BIT)
                | ConvertFlag(desc.RenderTargetWriteMask, 0x02, VK_COLOR_COMPONENT_G_BIT)
                | ConvertFlag(desc.RenderTargetWriteMask, 0x04, VK_COLOR_COMPONENT_B_BIT)
                | ConvertFlag(desc.RenderTargetWriteMask, 0x08, VK_COLOR_COMPONENT_A_BIT);
            blendAttachment.blendEnable = ConvertBool(desc.BlendEnabled);
            blendAttachment.srcColorBlendFactor = ConvertBlend(desc.SourceBlendColor);
            blendAttachment.dstColorBlendFactor = ConvertBlend(desc.DestBlendColor);
            blendAttachment.colorBlendOp = ConvertBlendOp(desc.ColorOp);
            blendAttachment.srcAlphaBlendFactor = ConvertBlend(desc.SourceBlendAlpha);
            blendAttachment.dstAlphaBlendFactor = ConvertBlend(desc.DestBlendAlpha);
            blendAttachment.alphaBlendOp = ConvertBlendOp(desc.AlphaOp);
        }

        m_PipelineImpl->ResetBlendStates(blendDescs.size());
    }

    void PipelineDesc::AddRenderTargetFormats(const uint32_t numColorFormats, const ResourceFormat* pColorFormats, const ResourceFormat depthStencilFormat)
    {
        m_PipelineImpl->m_DepthFormat = GetVkFormat(depthStencilFormat);

        m_PipelineImpl->m_NumAttachments = numColorFormats;
        CauldronAssert(ASSERT_ERROR, m_PipelineImpl->m_NumAttachments <= s_MaxRenderTargets, L"Cannot set more than %d render targets.", s_MaxRenderTargets);
        for (uint32_t i = 0; i < m_PipelineImpl->m_NumAttachments; ++i)
            m_PipelineImpl->m_ColorAttachmentFormats[i] = GetVkFormat(pColorFormats[i]);
        m_PipelineImpl->ResetBlendStates(m_PipelineImpl->m_NumAttachments);
    }

    void PipelineDesc::AddDepthState(const DepthDesc* pDepthDesc)
    {
        if (pDepthDesc != nullptr)
        {
            m_PipelineImpl->m_DepthStencilState.depthTestEnable = ConvertBool(pDepthDesc->DepthEnable);
            m_PipelineImpl->m_DepthStencilState.depthWriteEnable = ConvertBool(pDepthDesc->DepthWriteEnable);
            m_PipelineImpl->m_DepthStencilState.depthCompareOp = ConvertComparisonFunc(pDepthDesc->DepthFunc);
            m_PipelineImpl->m_DepthStencilState.stencilTestEnable = ConvertBool(pDepthDesc->StencilEnable);
            m_PipelineImpl->m_DepthStencilState.front = ConvertStencilDepth(pDepthDesc->FrontFace, pDepthDesc);
            m_PipelineImpl->m_DepthStencilState.back = ConvertStencilDepth(pDepthDesc->BackFace, pDepthDesc);
        }
    }

    void PipelineDesc::AddPrimitiveTopology(PrimitiveTopologyType topologyType)
    {
        // NOTE: framework only supports lists
        m_PipelineImpl->m_InputAssemblyState.topology = ConvertTopologyType(topologyType);
    }

} // namespace cauldron

#endif // #if defined(_VK)
