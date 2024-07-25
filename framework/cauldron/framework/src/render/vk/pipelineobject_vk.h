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
#include "render/pipelineobject.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace cauldron
{
    class PipelineObjectInternal final : public PipelineObject
    {
    public:
        const VkPipeline VKPipeline() const { return m_Pipeline; }
        const VkPipelineLayout VKPipelineLayout() const { return m_PipelineLayout; }

        PipelineObjectInternal* GetImpl() override { return this; }
        const PipelineObjectInternal* GetImpl() const override { return this; }

    private:
        friend class PipelineObject;
        PipelineObjectInternal(const wchar_t* pipelineObjectName);
        virtual ~PipelineObjectInternal();

        void Build(const PipelineDesc& desc, std::vector<const wchar_t*>* pAdditionalParameters = nullptr) override;

        void BuildGraphicsPipeline(PipelineDesc& pipelineDesc);
        void BuildComputePipeline(PipelineDesc& pipelineDesc, std::vector<const wchar_t*>* pAdditionalParameters = nullptr);

    private:
        // Internal members
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline       m_Pipeline       = VK_NULL_HANDLE;
    };

} // namespace cauldron

#endif // #if defined(_VK)
