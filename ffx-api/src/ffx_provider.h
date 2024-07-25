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
#include <ffx_api/ffx_api.hpp>
#include <ffx_api/ffx_api_types.h>
#include <FidelityFx/host/ffx_types.h>

#define VERIFY(_cond, _retcode) \
    if (!(_cond)) return _retcode

#define TRY(_expr) \
    if (ffxReturnCode_t _rc = (_expr); _rc != FFX_API_RETURN_OK) return _rc

#define TRY2(_expr) \
    if (FFX_OK != (_expr)) return FFX_API_RETURN_ERROR_RUNTIME_ERROR

struct Allocator
{
    const ffxAllocationCallbacks* cb;

    void* alloc(size_t sz)
    {
        if (cb)
            return cb->alloc(cb->pUserData, static_cast<uint64_t>(sz));
        else
            return malloc(sz);
    }

    void dealloc(void* ptr)
    {
        if (cb)
            cb->dealloc(cb->pUserData, ptr);
        else
            free(ptr);
    }

    template<typename T, typename... Args>
    T* construct(Args&&... args)
    {
        void* addr = alloc(sizeof(T));
        return ::new(addr) T(std::forward<Args>(args)...);
    }
};

class ffxProvider
{
public:
    virtual ~ffxProvider() = default;

    virtual bool CanProvide(uint64_t) const
    {
        return false;
    }

    virtual uint64_t GetId() const = 0;

    virtual const char* GetVersionName() const = 0;

    virtual ffxReturnCode_t CreateContext(ffxContext* context, ffxCreateContextDescHeader* desc, Allocator& alloc) const = 0;

    virtual ffxReturnCode_t DestroyContext(ffxContext* context, Allocator& alloc) const = 0;

    virtual ffxReturnCode_t Configure(ffxContext* context, const ffxConfigureDescHeader* desc) const = 0;

    virtual ffxReturnCode_t Query(ffxContext* context, ffxQueryDescHeader* desc) const = 0;

    virtual ffxReturnCode_t Dispatch(ffxContext* context, const ffxDispatchDescHeader* desc) const = 0;
};

const ffxProvider* GetffxProvider(ffxStructType_t descType, uint64_t overrideId, void* device);

const ffxProvider* GetAssociatedProvider(ffxContext* context);

uint64_t GetProviderCount(ffxStructType_t descType, void* device);

uint64_t GetProviderVersions(ffxStructType_t descType, void* device, uint64_t capacity, uint64_t* versionIds, const char** versionNames);

struct InternalContextHeader
{
    const ffxProvider* provider;
};

template<typename T>
static inline T ConvertEnum(uint32_t enumVal)
{
    return static_cast<T>(enumVal);
}

template<typename T>
static inline uint32_t ReverseConvertEnum(T enumVal)
{
    return static_cast<T>(enumVal);
}

static inline FfxResource Convert(const FfxApiResource& inRes)
{
    FfxResource outRes{};
    memset(outRes.name, 0, sizeof(outRes.name));
    outRes.resource = inRes.resource;
    outRes.state = ConvertEnum<FfxResourceStates>(inRes.state);
    outRes.description.type = ConvertEnum<FfxResourceType>(inRes.description.type);
    outRes.description.format = ConvertEnum<FfxSurfaceFormat>(inRes.description.format);
    outRes.description.width = inRes.description.width;
    outRes.description.height = inRes.description.height;
    outRes.description.depth = inRes.description.depth;
    outRes.description.mipCount = inRes.description.mipCount;
    outRes.description.flags = ConvertEnum<FfxResourceFlags>(inRes.description.flags);
    outRes.description.usage = ConvertEnum<FfxResourceUsage>(inRes.description.usage);
    return outRes;
}

static inline FfxApiResource Convert(const FfxResource& inRes)
{
    FfxApiResource outRes{};
    outRes.resource = inRes.resource;
    outRes.state = ReverseConvertEnum<FfxResourceStates>(inRes.state);
    outRes.description.type = ReverseConvertEnum<FfxResourceType>(inRes.description.type);
    outRes.description.format = ReverseConvertEnum<FfxSurfaceFormat>(inRes.description.format);
    outRes.description.width = inRes.description.width;
    outRes.description.height = inRes.description.height;
    outRes.description.depth = inRes.description.depth;
    outRes.description.mipCount = inRes.description.mipCount;
    outRes.description.flags = ReverseConvertEnum<FfxResourceFlags>(inRes.description.flags);
    outRes.description.usage = ReverseConvertEnum<FfxResourceUsage>(inRes.description.usage);
    return outRes;
}
