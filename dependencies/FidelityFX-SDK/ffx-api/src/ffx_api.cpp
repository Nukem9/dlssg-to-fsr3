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

#include <FidelityFX/host/ffx_types.h>
#include <FidelityFX/host/ffx_error.h>
#include <ffx_api/ffx_api.h>
#include "ffx_provider.h"
#include "backends.h"

static uint64_t GetVersionOverride(const ffxApiHeader* header)
{
    for (auto it = header; it; it = it->pNext)
    {
        if (auto versionDesc = ffx::DynamicCast<ffxOverrideVersion>(it))
        {
            return versionDesc->versionId;
        }
    }
    return 0;
}

FFX_API_ENTRY ffxReturnCode_t ffxCreateContext(ffxContext* context, ffxCreateContextDescHeader* desc, const ffxAllocationCallbacks* memCb)
{
    VERIFY(desc != nullptr, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(context != nullptr, FFX_API_RETURN_ERROR_PARAMETER);

    *context = nullptr;

    const ffxProvider* provider = GetffxProvider(desc->type, GetVersionOverride(desc), GetDevice(desc));
    VERIFY(provider != nullptr, FFX_API_RETURN_NO_PROVIDER);
    
    Allocator alloc{memCb};
    return provider->CreateContext(context, desc, alloc);
}

FFX_API_ENTRY ffxReturnCode_t ffxDestroyContext(ffxContext* context, const ffxAllocationCallbacks* memCb)
{
    VERIFY(context != nullptr, FFX_API_RETURN_ERROR_PARAMETER);

    Allocator alloc{memCb};
    return GetAssociatedProvider(context)->DestroyContext(context, alloc);
}

FFX_API_ENTRY ffxReturnCode_t ffxConfigure(ffxContext* context, const ffxConfigureDescHeader* desc)
{
    VERIFY(desc != nullptr, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(context != nullptr, FFX_API_RETURN_ERROR_PARAMETER);

    return GetAssociatedProvider(context)->Configure(context, desc);
}

FFX_API_ENTRY ffxReturnCode_t ffxQuery(ffxContext* context, ffxQueryDescHeader* header)
{
    VERIFY(header != nullptr, FFX_API_RETURN_ERROR_PARAMETER);

    if (context == nullptr)
    {
        if (auto desc = ffx::DynamicCast<ffxQueryDescGetVersions>(header))
        {
            // if output count is zero or no other pointer passed, count providers only
            if (desc->outputCount && (*desc->outputCount == 0 || (!desc->versionIds && !desc->versionNames)))
            {
                *desc->outputCount = GetProviderCount(desc->createDescType, desc->device);
            }
            else if (desc->outputCount && *desc->outputCount > 0)
            {
                uint64_t capacity = *desc->outputCount;
                *desc->outputCount = GetProviderVersions(desc->createDescType, desc->device, capacity, desc->versionIds, desc->versionNames);
            }
            return FFX_API_RETURN_OK;
        }
        else if (auto provider = GetffxProvider(header->type, GetVersionOverride(header), GetDevice(header)))
        {
            return provider->Query(nullptr, header);
        }
        else
        {
            return FFX_API_RETURN_NO_PROVIDER;
        }
    }

    return GetAssociatedProvider(context)->Query(context, header);
}

FFX_API_ENTRY ffxReturnCode_t ffxDispatch(ffxContext* context, const ffxDispatchDescHeader* desc)
{
    VERIFY(desc != nullptr, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(context != nullptr, FFX_API_RETURN_ERROR_PARAMETER);

    return GetAssociatedProvider(context)->Dispatch(context, desc);
}
