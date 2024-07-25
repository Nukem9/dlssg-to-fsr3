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
#ifdef FFX_BACKEND_DX12
#include "../ffx_provider.h"

class ffxProvider_FrameGenerationSwapChain_DX12: public ffxProvider
{
public:
    ffxProvider_FrameGenerationSwapChain_DX12() = default;
    virtual ~ffxProvider_FrameGenerationSwapChain_DX12() = default;

    virtual bool CanProvide(uint64_t type) const override;

    virtual uint64_t GetId() const override;

    virtual const char* GetVersionName() const override;

    virtual ffxReturnCode_t CreateContext(ffxContext* context, ffxCreateContextDescHeader* desc, Allocator& alloc) const override;

    virtual ffxReturnCode_t DestroyContext(ffxContext* context, Allocator& alloc) const override;

    virtual ffxReturnCode_t Configure(ffxContext* context, const ffxConfigureDescHeader* desc) const override;

    virtual ffxReturnCode_t Query(ffxContext* context, ffxQueryDescHeader* desc) const override;

    virtual ffxReturnCode_t Dispatch(ffxContext* context, const ffxDispatchDescHeader* desc) const override;

    static ffxProvider_FrameGenerationSwapChain_DX12 Instance;
};
#endif // FFX_BACKEND_DX12
