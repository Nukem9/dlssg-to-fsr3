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

#include <stdint.h>
#include "ffx_provider.h"

typedef uint32_t (*pfnCanProvide)(uint64_t typeId);
typedef uint32_t (*pfnCreateContext)(void* context, void* desc, const void* allocator);
typedef uint32_t (*pfnDestroyContext)(void* context, const void* allocator);
typedef uint32_t (*pfnConfigure)(void* context, const void* desc);
typedef uint32_t (*pfnQuery)(void* context, void* desc);
typedef uint32_t (*pfnDispatch)(void* context, const void* desc);

struct ffxProviderInterface
{
    uint64_t versionId;
    const char* versionName;
    pfnCanProvide canProvide;
    pfnCreateContext createContext;
    pfnDestroyContext destroyContext;
    pfnConfigure configure;
    pfnQuery query;
    pfnDispatch dispatch;
};

class ffxProviderExternal final : public ffxProvider {
public:
    ffxProviderInterface data;

    ffxProviderExternal(ffxProviderInterface data) : data(data) {}

    bool CanProvide(uint64_t type) const override
    {
        return data.canProvide(type) != 0;
    }

    uint64_t GetId() const override
    {
        return data.versionId;
    }

    const char* GetVersionName() const override
    {
        return data.versionName;
    }

    ffxReturnCode_t CreateContext(ffxContext* context, ffxCreateContextDescHeader* desc, Allocator& alloc) const override
    {
        TRY(data.createContext(context, desc, alloc.cb));
        // Ensure the provider is set to this external wrapper class.
        reinterpret_cast<InternalContextHeader*>(*context)->provider = this;
        return FFX_API_RETURN_OK;
    }

    ffxReturnCode_t DestroyContext(ffxContext* context, Allocator& alloc) const override
    {
        return data.destroyContext(context, alloc.cb);
    }

    ffxReturnCode_t Configure(ffxContext* context, const ffxConfigureDescHeader* desc) const override
    {
        return data.configure(context, desc);
    }

    ffxReturnCode_t Query(ffxContext* context, ffxQueryDescHeader* desc) const override
    {
        return data.query(context, desc);
    }

    ffxReturnCode_t Dispatch(ffxContext* context, const ffxDispatchDescHeader* desc) const override
    {
        return data.dispatch(context, desc);
    }
};
