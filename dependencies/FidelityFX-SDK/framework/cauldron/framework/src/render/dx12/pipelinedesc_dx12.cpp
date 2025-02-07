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
#include "render/dx12/gpuresource_dx12.h"
#include "render/dx12/pipelinedesc_dx12.h"
#include "render/dx12/rootsignature_dx12.h"

#include "render/shaderbuilder.h"
#include "core/framework.h"
#include "misc/assert.h"

#include "dxheaders/include/directx/d3dx12.h"

namespace cauldron
{
    D3D12_CULL_MODE Convert(const CullMode mode)
    {
        switch (mode)
        {
        case CullMode::None:
            return D3D12_CULL_MODE_NONE;
        case CullMode::Front:
            return D3D12_CULL_MODE_FRONT;
        case CullMode::Back:
            return D3D12_CULL_MODE_BACK;
        default:
            return D3D12_CULL_MODE_NONE;
        }
    }

    D3D12_COMPARISON_FUNC ConvertComparisonFunc(const ComparisonFunc func)
    {
        static bool s_InvertedDepth = GetConfig()->InvertedDepth;

        switch (func)
        {
        case ComparisonFunc::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case ComparisonFunc::Less:
            return s_InvertedDepth ? D3D12_COMPARISON_FUNC_GREATER : D3D12_COMPARISON_FUNC_LESS;
        case ComparisonFunc::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case ComparisonFunc::LessEqual:
            return s_InvertedDepth ? D3D12_COMPARISON_FUNC_GREATER_EQUAL : D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case ComparisonFunc::Greater:
            return s_InvertedDepth ? D3D12_COMPARISON_FUNC_LESS : D3D12_COMPARISON_FUNC_GREATER;
        case ComparisonFunc::GreaterEqual:
            return s_InvertedDepth ? D3D12_COMPARISON_FUNC_LESS_EQUAL : D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case ComparisonFunc::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case ComparisonFunc::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            return D3D12_COMPARISON_FUNC_NEVER;
        }
    }

    D3D12_STENCIL_OP Convert(const StencilOp op)
    {
        switch (op)
        {
        case StencilOp::Zero:
            return D3D12_STENCIL_OP_ZERO;
        case StencilOp::Keep:
            return D3D12_STENCIL_OP_KEEP;
        case StencilOp::Replace:
            return D3D12_STENCIL_OP_REPLACE;
        case StencilOp::IncrementSat:
            return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOp::DecrementSat:
            return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOp::Invert:
            return D3D12_STENCIL_OP_INVERT;
        case StencilOp::Increment:
            return D3D12_STENCIL_OP_INCR;
        case StencilOp::Decrement:
            return D3D12_STENCIL_OP_DECR;
        default:
            return D3D12_STENCIL_OP_ZERO;
        }
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE Convert(const PrimitiveTopologyType topology)
    {
        switch (topology)
        {
        case PrimitiveTopologyType::Undefined:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        case PrimitiveTopologyType::Point:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case PrimitiveTopologyType::Line:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveTopologyType::Triangle:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case PrimitiveTopologyType::Patch:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        default:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        }
    }

    D3D12_DEPTH_STENCILOP_DESC Convert(const StencilDesc stencilDesc)
    {
        D3D12_DEPTH_STENCILOP_DESC desc;
        desc.StencilFailOp = Convert(stencilDesc.StencilFailOp);
        desc.StencilDepthFailOp = Convert(stencilDesc.StencilDepthFailOp);
        desc.StencilPassOp = Convert(stencilDesc.StencilPassOp);
        desc.StencilFunc = ConvertComparisonFunc(stencilDesc.StencilFunc);
        return desc;
    }

    inline BOOL Convert(bool value)
    {
        return value ? TRUE : FALSE;
    }

    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ConvertIndexStringCutValue(uint32_t index)
    {
        if (index == 0)
            return D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        if (index == 0xffff)
            return D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
        if (index == 0xffffffff)
            return D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF;
        return D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    }

    D3D12_BLEND Convert(const Blend blend)
    {
        switch (blend)
        {
        case Blend::Zero:
            return D3D12_BLEND_ZERO;
        case Blend::One:
            return D3D12_BLEND_ONE;
        case Blend::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case Blend::DstColor:
            return D3D12_BLEND_DEST_COLOR;
        case Blend::InvSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case Blend::InvDstColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case Blend::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case Blend::DstAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case Blend::InvSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case Blend::InvDstAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case Blend::SrcAlphaSat:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case Blend::BlendFactor:
            return D3D12_BLEND_BLEND_FACTOR;
        case Blend::InvBlendFactor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        default:
            return D3D12_BLEND_ZERO;
        }
    }

    D3D12_BLEND_OP Convert(const BlendOp op)
    {
        switch (op)
        {
        case BlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case BlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case BlendOp::RevSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendOp::Min:
            return D3D12_BLEND_OP_MIN;
        case BlendOp::Max:
            return D3D12_BLEND_OP_MAX;
        default:
            return D3D12_BLEND_OP_ADD;
        }
    }

    UINT8 Convert(const uint32_t mask)
    {
        UINT8 colorMask = 0;
        colorMask |= (mask & static_cast<uint32_t>(ColorWriteMask::Red)) ? D3D12_COLOR_WRITE_ENABLE_RED : 0;
        colorMask |= (mask & static_cast<uint32_t>(ColorWriteMask::Green)) ? D3D12_COLOR_WRITE_ENABLE_GREEN: 0;
        colorMask |= (mask & static_cast<uint32_t>(ColorWriteMask::Blue)) ? D3D12_COLOR_WRITE_ENABLE_BLUE : 0;
        colorMask |= (mask & static_cast<uint32_t>(ColorWriteMask::Alpha)) ? D3D12_COLOR_WRITE_ENABLE_ALPHA : 0;
        return colorMask;
    }

    PipelineDesc::PipelineDesc()
    {
        // Allocate implementation
        m_PipelineImpl = new PipelineDescInternal;

        // Default graphics pipeline desc
        m_PipelineImpl->m_GraphicsPipelineDesc = {};
        m_PipelineImpl->m_GraphicsPipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        m_PipelineImpl->m_GraphicsPipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        m_PipelineImpl->m_GraphicsPipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        m_PipelineImpl->m_GraphicsPipelineDesc.DepthStencilState.DepthEnable = FALSE;
        m_PipelineImpl->m_GraphicsPipelineDesc.SampleMask = UINT_MAX;
        m_PipelineImpl->m_GraphicsPipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        m_PipelineImpl->m_GraphicsPipelineDesc.SampleDesc = { 1, 0 };

        // Default compute pipeline desc
        m_PipelineImpl->m_ComputePipelineDesc = {};
    }

    PipelineDesc::~PipelineDesc()
    {
        // Delete allocated impl memory as it's no longer needed
        delete m_PipelineImpl;
    }

    // Set the root signature for the pipeline
    void PipelineDesc::SetRootSignature(RootSignature* pRootSignature)
    {
        m_PipelineImpl->m_ComputePipelineDesc.pRootSignature = m_PipelineImpl->m_GraphicsPipelineDesc.pRootSignature = pRootSignature->GetImpl()->DX12RootSignature();
    }

    void PipelineDesc::AddShaders(std::vector<const wchar_t*>* pAdditionalParameters)
    {
        // Go through each shader desc and build it
        for (size_t i = 0; i < m_ShaderDescriptions.size(); ++i)
        {
            // Add defines for the platform
            m_ShaderDescriptions[i].Defines[L"_DX12"] = L"";
            m_ShaderDescriptions[i].Defines[L"_HLSL"] = L"";

            // Compile the shader
            MSComPtr<IDxcBlob> pShaderBlob;
            pShaderBlob.Attach(reinterpret_cast<IDxcBlob*>(CompileShaderToByteCode(m_ShaderDescriptions[i], pAdditionalParameters)));
            m_PipelineImpl->m_ShaderBinaryStore.push_back(pShaderBlob);

            // Fill in the right stage
            switch (m_ShaderDescriptions[i].Stage)
            {
            case ShaderStage::Compute:
                m_PipelineImpl->m_ComputePipelineDesc.CS.BytecodeLength = pShaderBlob->GetBufferSize();
                m_PipelineImpl->m_ComputePipelineDesc.CS.pShaderBytecode = pShaderBlob->GetBufferPointer();
                break;
            case ShaderStage::Vertex:
                m_PipelineImpl->m_GraphicsPipelineDesc.VS.BytecodeLength = pShaderBlob->GetBufferSize();
                m_PipelineImpl->m_GraphicsPipelineDesc.VS.pShaderBytecode = pShaderBlob->GetBufferPointer();
                break;
            case ShaderStage::Pixel:
                m_PipelineImpl->m_GraphicsPipelineDesc.PS.BytecodeLength = pShaderBlob->GetBufferSize();
                m_PipelineImpl->m_GraphicsPipelineDesc.PS.pShaderBytecode = pShaderBlob->GetBufferPointer();
                break;
            case ShaderStage::Domain:
                m_PipelineImpl->m_GraphicsPipelineDesc.DS.BytecodeLength = pShaderBlob->GetBufferSize();
                m_PipelineImpl->m_GraphicsPipelineDesc.DS.pShaderBytecode = pShaderBlob->GetBufferPointer();
                break;
            case ShaderStage::Geometry:
                m_PipelineImpl->m_GraphicsPipelineDesc.GS.BytecodeLength = pShaderBlob->GetBufferSize();
                m_PipelineImpl->m_GraphicsPipelineDesc.GS.pShaderBytecode = pShaderBlob->GetBufferPointer();
                break;
            case ShaderStage::Hull:
                m_PipelineImpl->m_GraphicsPipelineDesc.HS.BytecodeLength = pShaderBlob->GetBufferSize();
                m_PipelineImpl->m_GraphicsPipelineDesc.HS.pShaderBytecode = pShaderBlob->GetBufferPointer();
            default:
                CauldronCritical(L"Invalid shader stage requested");
                break;
            }
        }

        // Also go through shaber blob descs in case we are creating directly from a blob
        for ( ShaderBlobDesc& blobDesc : m_ShaderBlobDescriptions )
        {
            switch (blobDesc.Stage)
            {
            case ShaderStage::Vertex:
                m_PipelineImpl->m_GraphicsPipelineDesc.VS.BytecodeLength  = blobDesc.DataSize;
                m_PipelineImpl->m_GraphicsPipelineDesc.VS.pShaderBytecode = blobDesc.pData;
                break;
            case ShaderStage::Pixel:
                m_PipelineImpl->m_GraphicsPipelineDesc.PS.BytecodeLength  = blobDesc.DataSize;
                m_PipelineImpl->m_GraphicsPipelineDesc.PS.pShaderBytecode = blobDesc.pData;
                break;
            case ShaderStage::Hull:
                m_PipelineImpl->m_GraphicsPipelineDesc.HS.BytecodeLength  = blobDesc.DataSize;
                m_PipelineImpl->m_GraphicsPipelineDesc.HS.pShaderBytecode = blobDesc.pData;
                break;
            case ShaderStage::Domain:
                m_PipelineImpl->m_GraphicsPipelineDesc.DS.BytecodeLength  = blobDesc.DataSize;
                m_PipelineImpl->m_GraphicsPipelineDesc.DS.pShaderBytecode = blobDesc.pData;
                break;
            case ShaderStage::Geometry:
                m_PipelineImpl->m_GraphicsPipelineDesc.GS.BytecodeLength  = blobDesc.DataSize;
                m_PipelineImpl->m_GraphicsPipelineDesc.GS.pShaderBytecode = blobDesc.pData;
                break;
            case ShaderStage::Compute:
                m_PipelineImpl->m_ComputePipelineDesc.CS.BytecodeLength  = blobDesc.DataSize;
                m_PipelineImpl->m_ComputePipelineDesc.CS.pShaderBytecode = blobDesc.pData;
                break;
            default:
                CauldronCritical(L"Invalid shader stage requested");
                break;
            }
        }
    }

    // Define an input layout for the pipeline object (with manual information)
    void PipelineDesc::AddInputLayout(std::vector<InputLayoutDesc>& inputLayouts)
    {
        m_PipelineImpl->m_NumVertexAttributes = 0;

        static_assert(static_cast<uint32_t>(VertexAttributeType::Count) == 12, L"Number of vertex attributes has changed, fix up semantic names and index accordingly");
        static const char* s_semanticNames[] =    { "POSITION", "NORMAL", "TANGENT", "TEXCOORD", "TEXCOORD", "COLOR", "COLOR", "WEIGHTS", "WEIGHTS", "JOINTS", "JOINTS", "PREVIOUSPOSITION"};
        static const uint32_t s_semanticIndex[] = { 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0 };

        for (auto layoutIter = inputLayouts.begin(); layoutIter < inputLayouts.end(); ++layoutIter)
        {
            D3D12_INPUT_ELEMENT_DESC& desc = m_PipelineImpl->m_InputElementDescriptions[m_PipelineImpl->m_NumVertexAttributes];
            desc.SemanticIndex = s_semanticIndex[static_cast<uint32_t>(layoutIter->AttributeType)];
            desc.SemanticName = s_semanticNames[static_cast<uint32_t>(layoutIter->AttributeType)];
            desc.InputSlot = layoutIter->AttributeInputSlot;
            desc.AlignedByteOffset = layoutIter->AttributeOffset;
            desc.InstanceDataStepRate = 0;
            desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            desc.Format = GetDXGIFormat(layoutIter->AttributeFmt);

            ++m_PipelineImpl->m_NumVertexAttributes;
        }

        m_PipelineImpl->m_GraphicsPipelineDesc.InputLayout.NumElements = m_PipelineImpl->m_NumVertexAttributes;
        m_PipelineImpl->m_GraphicsPipelineDesc.InputLayout.pInputElementDescs = m_PipelineImpl->m_InputElementDescriptions.data();
    }

    // Add Rasterization state information (For Graphics Pipeline Objects)
    void PipelineDesc::AddRasterStateDescription(RasterDesc* pRasterDesc)
    {
        m_PipelineImpl->m_GraphicsPipelineDesc.RasterizerState.CullMode = Convert(pRasterDesc->CullingMode);
        m_PipelineImpl->m_GraphicsPipelineDesc.RasterizerState.FillMode = pRasterDesc->Wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
        m_PipelineImpl->m_GraphicsPipelineDesc.RasterizerState.FrontCounterClockwise = pRasterDesc->FrontCounterClockwise ? TRUE : FALSE;
        m_PipelineImpl->m_GraphicsPipelineDesc.RasterizerState.DepthBias = static_cast<INT>(pRasterDesc->DepthBias);
        m_PipelineImpl->m_GraphicsPipelineDesc.RasterizerState.DepthBiasClamp = static_cast<FLOAT>(pRasterDesc->DepthBiasClamp);
        m_PipelineImpl->m_GraphicsPipelineDesc.RasterizerState.SlopeScaledDepthBias = static_cast<FLOAT>(pRasterDesc->SlopeScaledDepthBias);
        m_PipelineImpl->m_GraphicsPipelineDesc.RasterizerState.DepthClipEnable = pRasterDesc->DepthClipEnable ? TRUE : FALSE;
        m_PipelineImpl->m_GraphicsPipelineDesc.RasterizerState.MultisampleEnable = pRasterDesc->MultisampleEnable ? TRUE : FALSE;
        //BOOL AntialiasedLineEnable;
        //UINT ForcedSampleCount;
        //D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
    }

    // Add the format of the render targets
    void PipelineDesc::AddRenderTargetFormats(const uint32_t numColorFormats, const ResourceFormat* pColorFormats, const ResourceFormat depthStencilFormat)
    {
        m_PipelineImpl->m_GraphicsPipelineDesc.NumRenderTargets = static_cast<UINT>(numColorFormats);
        for (uint32_t i = 0; i < numColorFormats; ++i)
            m_PipelineImpl->m_GraphicsPipelineDesc.RTVFormats[i] = ConvertTypelessDXGIFormat(GetDXGIFormat(pColorFormats[i]));

        m_PipelineImpl->m_GraphicsPipelineDesc.DSVFormat = GetDXGIFormat(depthStencilFormat);
    }

    // Adds the blend states of the render targets
    void PipelineDesc::AddBlendStates(const std::vector<BlendDesc>& blendDescs, bool alphaToCoverage, bool independentBlend)
    {
        m_PipelineImpl->m_GraphicsPipelineDesc.BlendState.AlphaToCoverageEnable = alphaToCoverage;
        m_PipelineImpl->m_GraphicsPipelineDesc.BlendState.IndependentBlendEnable = independentBlend;

        uint32_t rtID(0);
        for (auto blendIter = blendDescs.begin(); blendIter != blendDescs.end(); ++blendIter, ++rtID)
        {
            m_PipelineImpl->m_GraphicsPipelineDesc.BlendState.RenderTarget[rtID].BlendEnable = blendIter->BlendEnabled;
            m_PipelineImpl->m_GraphicsPipelineDesc.BlendState.RenderTarget[rtID].SrcBlend = Convert(blendIter->SourceBlendColor);
            m_PipelineImpl->m_GraphicsPipelineDesc.BlendState.RenderTarget[rtID].DestBlend = Convert(blendIter->DestBlendColor);
            m_PipelineImpl->m_GraphicsPipelineDesc.BlendState.RenderTarget[rtID].BlendOp = Convert(blendIter->ColorOp);
            m_PipelineImpl->m_GraphicsPipelineDesc.BlendState.RenderTarget[rtID].SrcBlendAlpha = Convert(blendIter->SourceBlendAlpha);
            m_PipelineImpl->m_GraphicsPipelineDesc.BlendState.RenderTarget[rtID].DestBlendAlpha = Convert(blendIter->DestBlendAlpha);
            m_PipelineImpl->m_GraphicsPipelineDesc.BlendState.RenderTarget[rtID].BlendOpAlpha = Convert(blendIter->AlphaOp);
            m_PipelineImpl->m_GraphicsPipelineDesc.BlendState.RenderTarget[rtID].RenderTargetWriteMask = Convert(blendIter->RenderTargetWriteMask);
        }
    }

    // Adds the depth state
    void PipelineDesc::AddDepthState(const DepthDesc* pDepthDesc)
    {
        m_PipelineImpl->m_GraphicsPipelineDesc.DepthStencilState.DepthEnable = pDepthDesc->DepthEnable ? TRUE : FALSE;
        m_PipelineImpl->m_GraphicsPipelineDesc.DepthStencilState.DepthWriteMask = pDepthDesc->DepthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL :
        D3D12_DEPTH_WRITE_MASK_ZERO;
        m_PipelineImpl->m_GraphicsPipelineDesc.DepthStencilState.DepthFunc = ConvertComparisonFunc(pDepthDesc->DepthFunc);
        m_PipelineImpl->m_GraphicsPipelineDesc.DepthStencilState.StencilEnable = pDepthDesc->StencilEnable ? TRUE : FALSE;
        m_PipelineImpl->m_GraphicsPipelineDesc.DepthStencilState.StencilReadMask = static_cast<UINT8>(pDepthDesc->StencilReadMask);
        m_PipelineImpl->m_GraphicsPipelineDesc.DepthStencilState.StencilWriteMask = static_cast<UINT8>(pDepthDesc->StencilWriteMask);
        m_PipelineImpl->m_GraphicsPipelineDesc.DepthStencilState.FrontFace = Convert(pDepthDesc->FrontFace);
        m_PipelineImpl->m_GraphicsPipelineDesc.DepthStencilState.BackFace = Convert(pDepthDesc->BackFace);
    }

    // Add primitive topology information (For Graphics Pipeline Objects)
    void PipelineDesc::AddPrimitiveTopology(PrimitiveTopologyType topologyType)
    {
        m_PipelineImpl->m_GraphicsPipelineDesc.PrimitiveTopologyType = Convert(topologyType);
    }

} // namespace cauldron

#endif // #if defined(_DX12)
