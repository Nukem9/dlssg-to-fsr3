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

#include "render/dx12/indirectworkload_dx12.h"
#include "render/dx12/device_dx12.h"

#include "core/framework.h"

#include "dxheaders/include/directx/d3dx12.h"

namespace cauldron
{
    IndirectWorkload* IndirectWorkload::CreateIndirectWorkload(const IndirectCommandType& type)
    {
        return new IndirectWorkloadInternal(type);
    }

    IndirectWorkloadInternal::IndirectWorkloadInternal(const IndirectCommandType& type)
        : m_type(type)
    {
        D3D12_INDIRECT_ARGUMENT_DESC argumentDescs = {};

        switch (m_type)
        {
        case IndirectCommandType::Draw:
            argumentDescs.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
            m_stride           = sizeof(D3D12_DRAW_ARGUMENTS);
            break;
        case IndirectCommandType::DrawIndexed:
            argumentDescs.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
            m_stride           = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
            break;
        case IndirectCommandType::Dispatch:
            argumentDescs.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
            m_stride           = sizeof(D3D12_DISPATCH_ARGUMENTS);
            break;
        default:
            CauldronWarning(L"Unsupported command type for indirect workload.");
            return;
        }

        D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
        commandSignatureDesc.pArgumentDescs               = &argumentDescs;
        commandSignatureDesc.NumArgumentDescs             = 1;
        commandSignatureDesc.ByteStride                   = m_stride;

        CauldronThrowOnFail(GetDevice()->GetImpl()->DX12Device()->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&m_pCommandSignature)));
    }

}  // namespace cauldron

#endif // #if defined(_DX12)
