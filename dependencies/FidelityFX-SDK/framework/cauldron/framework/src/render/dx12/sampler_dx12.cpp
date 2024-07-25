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

#if defined(_DX12)

#include "render/dx12/sampler_dx12.h"
#include "misc/assert.h"

namespace cauldron
{
    D3D12_FILTER Convert(FilterFunc filter)
    {
        switch (filter)
        {
        case FilterFunc::MinMagMipPoint:
            return D3D12_FILTER_MIN_MAG_MIP_POINT;
        case FilterFunc::MinMagPointMipLinear:
            return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        case FilterFunc::MinPointMagLinearMipPoint:
            return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        case FilterFunc::MinPointMagMipLinear:
            return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        case FilterFunc::MinLinearMagMipPoint:
            return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        case FilterFunc::MinLinearMagPointMipLinear:
            return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        case FilterFunc::MinMagLinearMipPoint:
            return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        case FilterFunc::MinMagMipLinear:
            return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        case FilterFunc::Anisotropic:
            return D3D12_FILTER_ANISOTROPIC;
        case FilterFunc::ComparisonMinMagMipPoint:
            return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
        case FilterFunc::ComparisonMinMagPointMipLinear:
            return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
        case FilterFunc::ComparisonMinPointMagLinearMipPoint:
            return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
        case FilterFunc::ComparisonMinPointMagMipLinear:
            return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
        case FilterFunc::ComparisonMinLinearMagMipPoint:
            return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
        case FilterFunc::ComparisonMinLinearMagPointMipLinear:
            return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        case FilterFunc::ComparisonMinMagLinearMipPoint:
            return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        case FilterFunc::ComparisonMinMagMipLinear:
            return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        case FilterFunc::ComparisonAnisotropic:
            return D3D12_FILTER_COMPARISON_ANISOTROPIC;
        default:
            CauldronWarning(L"Unknown sampler filter func requested. Returning min mag mip point");
            return D3D12_FILTER_MIN_MAG_MIP_POINT;
        }
    }

    D3D12_TEXTURE_ADDRESS_MODE Convert(AddressMode address)
    {
        switch (address)
        {
        case AddressMode::Wrap:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case AddressMode::Mirror:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case AddressMode::Clamp:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case AddressMode::Border:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case AddressMode::MirrorOnce:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        default:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        }
    }

    D3D12_COMPARISON_FUNC Convert(const ComparisonFunc func)
    {
        switch (func)
        {
        case ComparisonFunc::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case ComparisonFunc::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case ComparisonFunc::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case ComparisonFunc::LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case ComparisonFunc::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case ComparisonFunc::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case ComparisonFunc::GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case ComparisonFunc::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            return D3D12_COMPARISON_FUNC_NEVER;
        }
    }

    D3D12_STATIC_SAMPLER_DESC DX12StaticSamplerDesc(const SamplerDesc& desc)
    {
        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};

        samplerDesc.Filter = Convert(desc.Filter);
        samplerDesc.AddressU = Convert(desc.AddressU);
        samplerDesc.AddressV = Convert(desc.AddressV);
        samplerDesc.AddressW = Convert(desc.AddressW);
        samplerDesc.ComparisonFunc = Convert(desc.Comparison);
        samplerDesc.MinLOD = desc.MinLOD;
        samplerDesc.MaxLOD = desc.MaxLOD;
        samplerDesc.MipLODBias = desc.MipLODBias;
        samplerDesc.MaxAnisotropy = desc.MaxAnisotropy;

        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        samplerDesc.ShaderRegister = 0;                             // will be set by the root signature
        samplerDesc.RegisterSpace = 0;                              // will be set by the root signature
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // will be set by the root signature

        return samplerDesc;
    }

    D3D12_SAMPLER_DESC DX12SamplerDesc(const SamplerDesc& desc)
    {
        D3D12_SAMPLER_DESC samplerDesc = {};

        samplerDesc.Filter = Convert(desc.Filter);
        samplerDesc.AddressU = Convert(desc.AddressU);
        samplerDesc.AddressV = Convert(desc.AddressV);
        samplerDesc.AddressW = Convert(desc.AddressW);
        samplerDesc.ComparisonFunc = Convert(desc.Comparison);
        samplerDesc.MinLOD = desc.MinLOD;
        samplerDesc.MaxLOD = desc.MaxLOD;
        samplerDesc.MipLODBias = desc.MipLODBias;
        samplerDesc.MaxAnisotropy = desc.MaxAnisotropy;

        // Add support for border color if needed
        samplerDesc.BorderColor[0] = samplerDesc.BorderColor[1] = samplerDesc.BorderColor[2] = 0.f;
        samplerDesc.BorderColor[3] = 1.f;

        return samplerDesc;
    }

    Sampler* Sampler::CreateSampler(const wchar_t* name, const SamplerDesc& desc)
    {
        return new SamplerInternal(name, desc);
    }

    SamplerInternal::SamplerInternal(const wchar_t* name, const SamplerDesc& desc) :
        Sampler(name, desc)
    {

    }
    
    D3D12_SAMPLER_DESC SamplerInternal::DX12Desc() const 
    { 
        return DX12SamplerDesc(m_SamplerDesc); 
    }
  
} // namespace cauldron

#endif // #if defined(_DX12)
