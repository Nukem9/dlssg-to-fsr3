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
#include "render/rootsignaturedesc.h"

namespace cauldron
{
    /// Structure representing a binding description for resource binding.
    ///
    /// @ingroup CauldronRender
    struct BindingDesc
    {
        BindingType Type = BindingType::Invalid;    ///< The binding type.
        uint32_t BaseShaderRegister = 0;            ///< Shader register to bind to.
        uint32_t BindingIndex = 0;                  ///< Shader space to bind to.
        uint32_t Count = 0;                         ///< Number of bound resources.
    };

    /// Per platform/API implementation of <c><i>RootSignature</i></c>
    ///
    /// @ingroup CauldronRender
    class RootSignatureInternal;

    /**
     * @class RootSignature
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of the root signature.
     *
     * @ingroup CauldronRender
     */
    class RootSignature
    {
    public:
        /**
         * @brief   RootSignature instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static RootSignature* CreateRootSignature(const wchar_t* name, const RootSignatureDesc& desc);

        /**
         * @brief   Destruction with default behavior.
         */
        virtual ~RootSignature() = default;

        /**
         * @brief   Returns the <c><i>PipelineType</i></c> associated with this root signature.
         */
        PipelineType GetPipelineType() const { return m_PipelineType; }

        /**
         * @brief   Returns the root signature's name.
         */
        const wchar_t* GetName() const { return m_Name.c_str(); }

        /**
         * @brief   Returns the vector of binding descriptions.
         */
        const std::vector<BindingDesc>& GetBindingDescriptions() const { return m_BindingDescriptions; }

        /**
         * @brief   Returns the binding description offset by binding type.
         */
        const int32_t GetBindingDescOffset(BindingType bindType) const { return m_BindingDescOffsets[static_cast<uint32_t>(bindType)]; }

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        virtual RootSignatureInternal* GetImpl() = 0;
        virtual const RootSignatureInternal* GetImpl() const = 0;

    private:
        // No copy, No move
        NO_COPY(RootSignature)
        NO_MOVE(RootSignature)

        virtual void Build(const RootSignatureDesc& desc) = 0;

    protected:
        RootSignature(const wchar_t* name) : m_Name(name) {}
        RootSignature() = delete;

    protected:
        PipelineType m_PipelineType = PipelineType::Undefined;
        std::wstring m_Name = L"";
        std::vector<BindingDesc> m_BindingDescriptions;
        int32_t m_BindingDescOffsets[static_cast<uint32_t>(BindingType::Count)] = { -1 };
    };

} // namespace cauldron
