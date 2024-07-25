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

#include "render/rootsignature.h"
#include "render/shaderbuilder.h"
#include "render/renderdefines.h"

#include <utility>

namespace cauldron
{
    class Surface;

    /// Structure representing the blend description needed for a pipeline object.
    ///
    /// @ingroup CauldronRender
    struct BlendDesc
    {
        bool    BlendEnabled = false;                                                   ///< True if blending enabled.
        //bool    LogicOpEnabled = false; // Support when needed
        Blend   SourceBlendColor = Blend::One;                                          ///< Source blend operator (defaults to Blend::One).
        Blend   DestBlendColor = Blend::Zero;                                           ///< Destination blend operator (defaults to Blend::Zero).
        BlendOp ColorOp = BlendOp::Add;                                                 ///< Color blend operation (defaults to BlendOp::Add).
        Blend   SourceBlendAlpha = Blend::One;                                          ///< Source alpha blend operator (defaults to Blend::One).
        Blend   DestBlendAlpha = Blend::Zero;                                           ///< Destination alpha blend operator (defaults to Blend::Zero).
        BlendOp AlphaOp = BlendOp::Add;                                                 ///< Alpha blend operation (defaults to BlendOp::Add).
        uint32_t RenderTargetWriteMask = static_cast<uint32_t>(ColorWriteMask::All);    ///< Controls what channels are written to (defaults to ColorWriteMask::All).
    };

    /// Structure representing the rasterization description needed for a pipeline object.
    ///
    /// @ingroup CauldronRender
    struct RasterDesc
    {
        bool     Wireframe = false;                 ///< True if wireframe rendering is desired.
        CullMode CullingMode = CullMode::Front;     ///< The culling mode to apply (defaults to CullMode::Front).
        bool     FrontCounterClockwise = false;     ///< Indicates if front-facing direction is counter-clockwise winding order (defaults to false).
        int32_t  DepthBias = 0;                     ///< Depth bias to apply (defaults to 0).
        float    DepthBiasClamp = 0.f;              ///< Depth bias clamping to apply (defaults to 0.f).
        float    SlopeScaledDepthBias = 0.f;        ///< Sloped scaled depth bias to apply (defaults to 0.f).
        bool     DepthClipEnable = true;            ///< True to enable depth clip (defaults to true).
        bool     MultisampleEnable = false;         ///< True to enable multisample rasterization (defaults to false and currently unsupported).
    };

    /// Structure representing the stencil description needed for a pipeline object.
    ///
    /// @ingroup CauldronRender
    struct StencilDesc
    {
        StencilOp      StencilFailOp      = StencilOp::Keep;            ///< Stencil fail operation (defaults to StencilOp::Keep).
        StencilOp      StencilDepthFailOp = StencilOp::Keep;            ///< Stencil depth fail operation (defaults to StencilOp::Keep).
        StencilOp      StencilPassOp      = StencilOp::Keep;            ///< Stencil pass operation (defaults to StencilOp::Keep).
        ComparisonFunc StencilFunc        = ComparisonFunc::Always;     ///< Stenicl comparison function (defaults to ComparisonFunc::Always).
    };

    /// Structure representing the depth description needed for a pipeline object.
    ///
    /// @ingroup CauldronRender
    struct DepthDesc
    {
        bool           DepthEnable      = false;                    ///< True to enable depth testing (defaults to false).
        bool           DepthWriteEnable = false;                    ///< True to enable depth writes (defaults to false).
        ComparisonFunc DepthFunc        = ComparisonFunc::Always;   ///< Depth test comparison function (defaults to ComparisonFunc::Always).
        bool           StencilEnable    = false;                    ///< True to enable stencil testing (defaults to false).
        uint8_t        StencilReadMask  = 0xff;                     ///< The stencil read mask (defaults to 0xff).
        uint8_t        StencilWriteMask = 0x00;                     ///< The stencil write mask (defaults to 0x00).
        StencilDesc    FrontFace        = {};                       ///< The <c><i>StencilDesc</i></c> description to use for the front face stencil test.
        StencilDesc    BackFace         = {};                       ///< The <c><i>StencilDesc</i></c> description to use for the front face stencil test.
    };

    /// Structure representing the input layout description for a single vertex attribute. Needed for a pipeline object.
    ///
    /// @ingroup CauldronRender
    struct InputLayoutDesc
    {
        VertexAttributeType AttributeType = VertexAttributeType::Position;  ///< Input layout attribute type (defaults to VertexAttributeType::Position).
        ResourceFormat      AttributeFmt = ResourceFormat::RGB32_FLOAT;     ///< Input layout attribute format (defaults to ResourceFormat::RGB32_FLOAT).
        uint32_t            AttributeInputSlot = 0;                         ///< Input layout attribute binding slot.
        uint32_t            AttributeOffset = 0;                            ///< INput layout attribute data offset.

        InputLayoutDesc(VertexAttributeType type, ResourceFormat format, uint32_t inputSlot, uint32_t offset) :
            AttributeType(type), AttributeFmt(format), AttributeInputSlot(inputSlot), AttributeOffset(offset) {}
        InputLayoutDesc() = delete;
    };

    /// Per platform/API implementation of <c><i>PipelineDesc</i></c>
    ///
    /// @ingroup CauldronRender
    struct PipelineDescInternal;



    /**
     * @struct PipelineDesc
     *
     * The description used to build a pipeline object.
     * The minimal requirements for a PipelineDesc is a RootSignature and a Shader (for compute)
     * By default, the graphics pipeline desc will build with the following attributes:
     * - No Depth Target
     * - No Input Layout
     * - Solid Fill and BackFace culling
     * - No multisampling and 0 depth bias values
     * - No blending
     * - Depth disabled
     * - Triangle topology
     *
     * @ingroup CauldronRender
     */
    struct PipelineDesc
    {
        /// Limit the number of render targets to 8
        ///
        static constexpr uint32_t s_MaxRenderTargets = 8;

        /**
         * @brief   Add a shader to the pipeline description.
         */
        void AddShaderDesc(ShaderBuildDesc& shaderDesc);

        /**
         * @brief   Add a shader blob to the pipeline description.
         */
        void AddShaderBlobDesc(ShaderBlobDesc& shaderBlobDesc);

        /**
         * @brief   Add the format of the render targets.
         */
        void AddRasterFormats(const ResourceFormat& rtFormat, const ResourceFormat depthFormat = ResourceFormat::Unknown);
        void AddRasterFormats(const std::vector<ResourceFormat>& rtFormats, const ResourceFormat depthFormat = ResourceFormat::Unknown);

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        PipelineDescInternal* GetImpl() { return m_PipelineImpl; }
        const PipelineDescInternal* GetImpl() const { return m_PipelineImpl; }

        /**
         * @brief   Construction, implemented per api/platform.
         */
        PipelineDesc();

        /**
         * @brief   Destruction, implemented per api/platform.
         */
        virtual ~PipelineDesc();

        PipelineDesc(PipelineDesc&&) = delete;
        PipelineDesc(const PipelineDesc&&) = delete;

        /**
         * @brief   Move assignment overload, implemented per api/platform.
         */
        PipelineDesc& operator=(PipelineDesc&&) noexcept;
        PipelineDesc& operator=(const PipelineDesc&&) noexcept;

        /**
         * @brief   Set the root signature for the pipeline.
         */
        void SetRootSignature(RootSignature* pRootSignature);

        /**
         * @brief   Add shaders (and build them) when ready.
         */
        void AddShaders(std::vector<const wchar_t*>* pAdditionalParameters = nullptr);

        /**
         * @brief   Define an input layout for the pipeline object (with manual information).
         */
        void AddInputLayout(std::vector<InputLayoutDesc>& inputLayouts);

        /**
         * @brief   Add Rasterization state information (For Graphics Pipeline Objects).
         */
        void AddRasterStateDescription(RasterDesc* pRasterDesc);

        /**
         * @brief   Add the format of the render targets.
         */
        void AddRenderTargetFormats(const uint32_t numColorFormats, const ResourceFormat* pColorFormats, const ResourceFormat depthStencilFormat);

        /**
         * @brief   Adds the blend states of the render targets.
         */
        void AddBlendStates(const std::vector<BlendDesc>& blendDescs, bool alphaToCoverage, bool independentBlend);

        /**
         * @brief   Adds the depth state.
         */
        void AddDepthState(const DepthDesc* pDepthDesc);

        /**
         * @brief   Add primitive topology information (For Graphics Pipeline Objects).
         */
        void AddPrimitiveTopology(PrimitiveTopologyType topologyType);

        /**
         * @brief   Set Wave64 for this pipeline
         */
        void SetWave64(bool isWave64);

        // TODO: Add support for SampleDesc, Flags, and Node information when it's needed

        PipelineType GetPipelineType() const { return m_PipelineType; }

        std::vector<ShaderBuildDesc>    m_ShaderDescriptions = {};      ///< Shader build descriptions (builds shaders from string or file source).
        std::vector<ShaderBlobDesc>     m_ShaderBlobDescriptions = {};  ///< Shader build descriptions (builds shaders from shader binary blob).

        bool                            m_IsWave64     = false;                     ///< Sets this pipeline to operate with Wave64 if the shader blob doesn't have this information yet
        PipelineType                    m_PipelineType = PipelineType::Undefined;   ///< The pipeline type (compute or graphics).
        PipelineDescInternal*           m_PipelineImpl = nullptr;                   ///< Internal implementation details set per api/platform.

    private:
        NO_COPY(PipelineDesc)
    };

} // namespace cauldron
