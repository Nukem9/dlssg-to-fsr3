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

#include "render/rootsignaturedesc.h"

#include "agilitysdk/include/d3d12.h"
#include "dxheaders/include/directx/d3dx12.h"

#include <vector>

namespace cauldron
{
    struct RootParameter
    {
        BindingType Type = BindingType::TextureSRV;
        D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL;
        union
        {
            uint32_t DescRangeIndex = 0;
            uint32_t BindingIndex;
        };
        uint32_t Size = 0;  // Used for select binding types (i.e. 32Bit root constants)
    };

    struct RootSignatureDescInternal final
    {
        std::vector<RootParameter> m_RootParameters = {};
        std::vector<CD3DX12_DESCRIPTOR_RANGE> m_DescRanges = {};
        std::vector<D3D12_STATIC_SAMPLER_DESC> m_StaticSamplers = {};
    };

} // namespace cauldron

#endif // #if defined(_DX12)
