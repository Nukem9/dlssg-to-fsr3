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

#include "ffx_api.h"

#include <cstdint>
#include <type_traits>

namespace ffx
{

using Context = ffxContext;

enum class ReturnCode : uint32_t
{
    Ok = 0,
    Error = 1,
    ErrorUnknownDesctype = 2,
    ErrorRuntimeError = 3,
    ErrorNoProvider = 4,
    ErrorMemory = 5,
    ErrorParameter = 6,
};

static inline constexpr bool operator!(ReturnCode rc)
{
    return rc != ReturnCode::Ok;
}

namespace detail
{
    static inline ReturnCode ConvertReturnCode(ffxReturnCode_t rc)
    {
        return static_cast<ReturnCode>(rc);
    }
}

template<class First, class Second, class... Rest>
First* LinkHeaders(First& first, Second& second, Rest&... rest)
{
    first.pNext = &second;
    LinkHeaders(second, rest...);
    return &first;
}

template<class First, class Second>
First* LinkHeaders(First& first, Second& second)
{
    first.pNext = &second;
    second.pNext = nullptr;
    return &first;
}

template<class Header>
Header* LinkHeaders(Header& hdr)
{
    hdr.pNext = nullptr;
    return &hdr;
}

template<class... Desc>
inline ReturnCode CreateContext(Context& context, ffxAllocationCallbacks* memCb, Desc&... desc)
{
    auto header = LinkHeaders(desc.header...);
    return detail::ConvertReturnCode(ffxCreateContext(&context, header, memCb));
}

inline ReturnCode DestroyContext(Context& context, ffxAllocationCallbacks* memCb = nullptr)
{
    return detail::ConvertReturnCode(ffxDestroyContext(&context, memCb));
}

template<class... Desc>
inline ReturnCode Configure(Context &context, Desc&... desc)
{
    auto header = LinkHeaders(desc.header...);
    return detail::ConvertReturnCode(ffxConfigure(&context, header));
}

template<class... Desc>
inline ReturnCode Configure(Desc&... desc)
{
    auto header = LinkHeaders(desc.header...);
    return detail::ConvertReturnCode(ffxConfigure(nullptr, header));
}

template<class... Desc>
inline ReturnCode Query(Context &context, Desc&... desc)
{
    auto header = LinkHeaders(desc.header...);
    return detail::ConvertReturnCode(ffxQuery(&context, header));
}

template<class... Desc>
inline ReturnCode Query(Desc&... desc)
{
    auto header = LinkHeaders(desc.header...);
    return detail::ConvertReturnCode(ffxQuery(nullptr, header));
}

template<class... Desc>
inline ReturnCode Dispatch(Context &context, Desc&... desc)
{
    auto header = LinkHeaders(desc.header...);
    return detail::ConvertReturnCode(ffxDispatch(&context, header));
}

template<class T>
struct struct_type;

template<>
struct struct_type<ffxConfigureDescGlobalDebug1> : std::integral_constant<uint64_t, FFX_API_CONFIGURE_DESC_TYPE_GLOBALDEBUG1> {};

template<>
struct struct_type<ffxOverrideVersion> : std::integral_constant<uint64_t, FFX_API_DESC_TYPE_OVERRIDE_VERSION> {};

template<>
struct struct_type<ffxQueryDescGetVersions> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_GET_VERSIONS> {};

template <class Inner, uint64_t type = struct_type<Inner>::value>
struct InitHelper : public Inner
{
    InitHelper() : Inner()
    {
        this->header.pNext = nullptr;
        this->header.type = type;
    }
};

struct ConfigureDescGlobalDebug1 : public InitHelper<ffxConfigureDescGlobalDebug1>
{};

struct CreateContextDescOverrideVersion : public InitHelper<ffxOverrideVersion>
{};

struct QueryDescGetVersions : public InitHelper<ffxQueryDescGetVersions>
{};

template<class T>
const T* DynamicCast(const ffxApiHeader *hdr)
{
    if (hdr->type == struct_type<T>::value)
        return reinterpret_cast<const T*>(hdr);
    return nullptr;
}

template<class T>
T* DynamicCast(ffxApiHeader *hdr)
{
    if (hdr->type == struct_type<T>::value)
        return reinterpret_cast<T*>(hdr);
    return nullptr;
}

}
