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

#include "render/commandlist.h"
#include "render/dx12/defines_dx12.h"

#include <agilitysdk/include/d3d12.h>

namespace cauldron
{
    struct CommandListInitParams
    {
        MSComPtr<ID3D12GraphicsCommandList2>           pCmdList;
        MSComPtr<ID3D12CommandAllocator>     pCmdAllocator;
    };

    class CommandListInternal final : public CommandList
    {
    public:
        virtual ~CommandListInternal();

        ID3D12GraphicsCommandList2* DX12CmdList() { return m_pCommandList.Get(); }
        const ID3D12GraphicsCommandList* DX12CmdList() const { return m_pCommandList.Get(); }

        MSComPtr<ID3D12CommandAllocator> DX12ComAllocator() { return m_pCmdAllocator; };

        virtual const CommandListInternal* GetImpl() const override { return this; }
        virtual CommandListInternal* GetImpl() override { return this; }
    
    private:
        friend class CommandList;
        CommandListInternal(const wchar_t* Name, MSComPtr<ID3D12GraphicsCommandList2> pCmdList, MSComPtr<ID3D12CommandAllocator> pCmdAllocator, CommandQueue queueType);
        CommandListInternal() = delete;

        MSComPtr<ID3D12GraphicsCommandList2>  m_pCommandList = nullptr;
        MSComPtr<ID3D12CommandAllocator>      m_pCmdAllocator = nullptr;
    };

    class UploadContextInternal final : public UploadContext
    {
    public:
        virtual ~UploadContextInternal();

        void Execute() override;

        CommandList* GetCopyCmdList() { return m_pCopyCmdList; }
        CommandList* GetTransitionCmdList() { return m_pTransitionCmdList; }

        const UploadContextInternal* GetImpl() const override { return this; }
        UploadContextInternal* GetImpl() override { return this; }

    private:
        friend class UploadContext;
        UploadContextInternal();

        // Internal members
        CommandList* m_pCopyCmdList = nullptr;
        CommandList* m_pTransitionCmdList = nullptr;
    };

} // namespace cauldron

#endif // #if defined(_DX12)
