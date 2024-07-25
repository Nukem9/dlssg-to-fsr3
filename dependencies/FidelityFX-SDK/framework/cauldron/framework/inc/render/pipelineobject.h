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
#include "render/pipelinedesc.h"

#include <xstring>

namespace cauldron
{
    /// Per platform/API implementation of <c><i>PipelineObject</i></c>
    ///
    /// @ingroup CauldronRender
    class PipelineObjectInternal;

    /**
     * @class PipelineObject
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> pipeline object instance used to execute GPU workloads.
     *
     * @ingroup CauldronRender
     */
    class PipelineObject
    {
    public:

        /**
         * @brief   PipelineObject instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static PipelineObject* CreatePipelineObject(const wchar_t* pipelineObjectName, const PipelineDesc& Desc, std::vector<const wchar_t*>* pAdditionalParameters = nullptr);

        /**
         * @brief   Destruction with default behavior.
         */
        virtual ~PipelineObject() = default;

        /**
         * @brief   Returns the <c><i>PipelineType</i></c>. Graphics or Compute.
         */
        PipelineType GetPipelineType() const { return m_Type; }

        /**
         * @brief   Returns the <c><i>PipelineDesc</i></c> description used to create the pipeline.
         */
        const PipelineDesc& GetDesc() { return m_Desc; }

        /**
         * @brief   Returns the pipeline object's name.
         */
        const wchar_t* GetName() const { return m_Name.c_str(); }

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        virtual PipelineObjectInternal* GetImpl() = 0;
        virtual const PipelineObjectInternal* GetImpl() const = 0;

    private:
        // No copy, No move
        NO_COPY(PipelineObject)
        NO_MOVE(PipelineObject)

        virtual void Build(const PipelineDesc& desc, std::vector<const wchar_t*>* pAdditionalParameters) = 0;

    protected:
        PipelineObject(const wchar_t* pipelineObjectName) : m_Name(pipelineObjectName) {}
        PipelineObject() = delete;

        std::wstring m_Name = L"";
        PipelineType m_Type = PipelineType::Undefined;
        PipelineDesc m_Desc;
    };

} // namespace cauldron
