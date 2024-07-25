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

namespace cauldron
{
    /// A structure representing a sampler description.
    ///
    /// @ingroup CauldronRender
    struct SamplerDesc
    {
        FilterFunc  Filter = FilterFunc::MinMagMipLinear;       ///< Sampler filter function (defaults to linear).
        AddressMode AddressU = AddressMode::Clamp;              ///< Sampler U addressing mode (defaults to clamp).
        AddressMode AddressV = AddressMode::Clamp;              ///< Sampler V addressing mode (defaults to clamp).
        AddressMode AddressW = AddressMode::Clamp;              ///< Sampler W addressing mode (defaults to clamp).
        ComparisonFunc Comparison = ComparisonFunc::Never;      ///< Sampler comparison function (defaults to never).
        float MinLOD = 0.0f;                                    ///< Sampler minimum LOD clamp (defaults to 0.f).
        float MaxLOD = std::numeric_limits<float>::max();       ///< Sampler maximum LOD clamp (defaults to FLOAT_MAX).
        float MipLODBias = 0.0f;                                ///< Sampler mip LOD bias (defaults to 0.f).
        uint32_t MaxAnisotropy = 16;                            ///< Sampler maximum anisotropy clamp (defaults to 16).

        ///< Sampler equals operator for runtime comparisons.
        bool operator==(const SamplerDesc& rhs) const
        {
            return !std::memcmp(this, &rhs, sizeof(SamplerDesc));
        }
    };

    /// Per platform/API implementation of <c><i>Sampler</i></c>
    ///
    /// @ingroup CauldronRender
    class SamplerInternal;

    /**
     * @class Sampler
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of the sampler resource.
     *
     * @ingroup CauldronRender
     */
    class Sampler
    {
    public:

        /**
         * @brief   Sampler instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static Sampler* CreateSampler(const wchar_t* name, const SamplerDesc& desc);

        /**
         * @brief   Destruction with default behavior.
         */
        virtual ~Sampler() = default;

        /**
         * @brief   Returns the <c><i>SamplerDesc</i></c> used to construct the sampler.
         */
        const SamplerDesc& GetDesc() const { return m_SamplerDesc; }

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        virtual SamplerInternal* GetImpl() = 0;
        virtual const SamplerInternal* GetImpl() const = 0;

    private:
        // No copy, No move
        NO_COPY(Sampler)
        NO_MOVE(Sampler)

    protected:

        Sampler(const wchar_t* name, const SamplerDesc& desc) : 
            m_Name(name),
            m_SamplerDesc(desc) {}
        Sampler() = delete;

    protected:

        std::wstring    m_Name = L"";
        SamplerDesc     m_SamplerDesc = {};
    };

} // namespace cauldron

