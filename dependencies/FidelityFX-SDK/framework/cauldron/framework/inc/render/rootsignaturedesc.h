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

#include "misc/helpers.h"
#include "render/renderdefines.h"

#include <vector>

namespace cauldron
{
    struct SamplerDesc;
    struct RootSignatureDescInternal;

    /**
     * @struct RootSignatureDesc
     *
     * The description structure used to construct <c><i>FidelityFX Cauldron Framework</i></c>'s <c><i>RootSignature</i></c>
     *
     * @ingroup CauldronRender
     */
    struct RootSignatureDesc
    {
        /**
         * @brief   Gets the pipeline type for the root signature to create.
         */
        PipelineType GetPipelineType() const { return m_PipelineType; }

        /**
         * @brief   Construction. Implemented at api/platform level.
         */
        RootSignatureDesc();

        /**
         * @brief   Destruction. implemented at api/platform level.
         */
        virtual ~RootSignatureDesc();

        RootSignatureDesc(RootSignatureDesc&&) = delete;
        RootSignatureDesc(const RootSignatureDesc&&) = delete;

        /**
         * @brief   Move assignement operator. Handles impl memory in a custom fashion.
         */
        RootSignatureDesc& operator=(RootSignatureDesc&&) noexcept;
        RootSignatureDesc& operator=(const RootSignatureDesc&&) noexcept;

        /**
         * @brief   Add a texture srv set to the signature description.
         */
        void AddTextureSRVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count);

        /**
         * @brief   Add a texture uav set to the signature description.
         */
        void AddTextureUAVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count);

        /**
         * @brief   Add a buffer srv set to the signature description.
         */
        void AddBufferSRVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count);

        /**
         * @brief   Add a buffer uav set to the signature description.
         */
        void AddBufferUAVSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count);

        /**
         * @brief   Add an rt acceleration structure set to the signature description.
         */
        void AddRTAccelerationStructureSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count);

        /**
         * @brief   Add a sampler set to the signature description.
         */
        void AddSamplerSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count);

        /**
         * @brief   Add a static sampler set to the signature description.
         */
        void AddStaticSamplers(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count, const SamplerDesc* samplerDescList);

        /**
         * @brief   Add a constant buffer set to the signature description.
         */
        void AddConstantBufferSet(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count);

        /**
         * @brief   Add a constant buffer view to the signature description.
         */
        void AddConstantBufferView(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count);

        /**
         * @brief   Add a 32-bit push constant buffer to the signature description.
         */
        void Add32BitConstantBuffer(uint32_t bindingIndex, ShaderBindStage bindStages, uint32_t count);

        /**
         * @brief   Sanity check to ensure root signature description is capable of being created.
         */
        void UpdatePipelineType(ShaderBindStage bindStages);

        PipelineType                m_PipelineType = PipelineType::Undefined;   ///< The pipeline type for the root signature.
        RootSignatureDescInternal*  m_pSignatureDescImpl = nullptr;             ///< The api/platform specific implementation pointer.

    private:
        NO_COPY(RootSignatureDesc)
    };

} // namespace cauldron
