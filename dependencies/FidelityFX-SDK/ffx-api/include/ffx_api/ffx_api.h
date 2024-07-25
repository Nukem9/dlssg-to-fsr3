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

#if defined(__cplusplus)
extern "C" {
#endif  // #if defined(__cplusplus)

#define FFX_API_ENTRY __declspec(dllexport)
#include <stdint.h>

enum FfxApiReturnCodes
{
    FFX_API_RETURN_OK                     = 0, ///< The oparation was successful.
    FFX_API_RETURN_ERROR                  = 1, ///< An error occurred that is not further specified.
    FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE = 2, ///< The structure type given was not recognized for the function or context with which it was used. This is likely a programming error.
    FFX_API_RETURN_ERROR_RUNTIME_ERROR    = 3, ///< The underlying runtime (e.g. D3D12, Vulkan) or effect returned an error code.
    FFX_API_RETURN_NO_PROVIDER            = 4, ///< No provider was found for the given structure type. This is likely a programming error.
    FFX_API_RETURN_ERROR_MEMORY           = 5, ///< A memory allocation failed.
    FFX_API_RETURN_ERROR_PARAMETER        = 6, ///< A parameter was invalid, e.g. a null pointer, empty resource or out-of-bounds enum value.
};

typedef void* ffxContext;
typedef uint32_t ffxReturnCode_t;

#define FFX_API_EFFECT_MASK 0xffff0000u
#define FFX_API_EFFECT_ID_GENERAL 0x00000000u

// Base Descriptor types
typedef uint64_t ffxStructType_t;
typedef struct ffxApiHeader
{
    ffxStructType_t      type;  ///< The structure type. Must always be set to the corresponding value for any structure (found nearby with a similar name).
    struct ffxApiHeader* pNext; ///< Pointer to next structure, used for optional parameters and extensions. Can be null.
} ffxApiHeader;

typedef ffxApiHeader ffxCreateContextDescHeader;
typedef ffxApiHeader ffxConfigureDescHeader;
typedef ffxApiHeader ffxQueryDescHeader;
typedef ffxApiHeader ffxDispatchDescHeader;

// Extensions for global debug
#define FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_SILENCE  0x0000000u
#define FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_ERRORS   0x0000001u
#define FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_WARNINGS 0x0000002u
#define FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_VERBOSE  0xfffffffu

enum FfxApiMsgType
{
    FFX_API_MESSAGE_TYPE_ERROR   = 0,
    FFX_API_MESSAGE_TYPE_WARNING = 1,
    FFX_API_MESSAGE_TYPE_COUNT
};

typedef void (*ffxApiMessage)(uint32_t type, const wchar_t* message);

#define FFX_API_CONFIGURE_DESC_TYPE_GLOBALDEBUG1 0x0000001u
struct ffxConfigureDescGlobalDebug1
{
    ffxConfigureDescHeader header;
    ffxApiMessage          fpMessage;
    uint32_t               debugLevel;
};

#define FFX_API_QUERY_DESC_TYPE_GET_VERSIONS 4u
struct ffxQueryDescGetVersions
{
    ffxQueryDescHeader header;
    uint64_t createDescType;    ///< Create description for the effect whose versions should be enumerated.
    void* device;               ///< For DX12: pointer to ID3D12Device.
    uint64_t *outputCount;      ///< Input capacity of id and name arrays. Output number of returned versions. If initially zero, output is number of available versions.
    uint64_t *versionIds;       ///< Output array of version ids to be used as version overrides. If null, only names and count are returned.
    const char** versionNames;  ///< Output array of version names for display. If null, only ids and count are returned. If both this and versionIds are null, only count is returned.
};

#define FFX_API_DESC_TYPE_OVERRIDE_VERSION 5u
struct ffxOverrideVersion
{
    ffxApiHeader header;
    uint64_t versionId;  ///< Id of version to use. Must be a value returned from a query in ffxQueryDescGetVersions.versionIds array.
};

// Memory allocation function. Must return a valid pointer to at least size bytes of memory aligned to hold any type.
// May return null to indicate failure. Standard library malloc fulfills this requirement.
typedef void* (*ffxAlloc)(void* pUserData, uint64_t size);

// Memory deallocation function. May be called with null pointer as second argument.
typedef void (*ffxDealloc)(void* pUserData, void* pMem);

typedef struct ffxAllocationCallbacks
{
    void* pUserData;
    ffxAlloc alloc;
    ffxDealloc dealloc;
} ffxAllocationCallbacks;

// Creates a FFX object context.
// Depending on the desc structures provided to this function, the context will be created with the desired version and attributes.
// Non-zero return indicates error code.
// Pointers passed in desc must remain live until ffxDestroyContext is called on the context.
// MemCb may be null; the system allocator (malloc/free) will be used in this case.
FFX_API_ENTRY ffxReturnCode_t ffxCreateContext(ffxContext* context, ffxCreateContextDescHeader* desc, const ffxAllocationCallbacks* memCb);
typedef ffxReturnCode_t (*PfnFfxCreateContext)(ffxContext* context, ffxCreateContextDescHeader* desc, const ffxAllocationCallbacks* memCb);

// Destroys an FFX object context.
// Non-zero return indicates error code.
// MemCb must be compatible with the callbacks passed into ffxCreateContext.
FFX_API_ENTRY ffxReturnCode_t ffxDestroyContext(ffxContext* context, const ffxAllocationCallbacks* memCb);
typedef ffxReturnCode_t (*PfnFfxDestroyContext)(ffxContext* context, const ffxAllocationCallbacks* memCb);

// Configures the provided FFX object context.
// If context is null, configure operates on any global state.
// Non-zero return indicates error code.
FFX_API_ENTRY ffxReturnCode_t ffxConfigure(ffxContext* context, const ffxConfigureDescHeader* desc);
typedef ffxReturnCode_t (*PfnFfxConfigure)(ffxContext* context, const ffxConfigureDescHeader* desc);

// Queries the provided FFX object context.
// If context is null, query operates on any global state.
// Non-zero return indicates error code.
FFX_API_ENTRY ffxReturnCode_t ffxQuery(ffxContext* context, ffxQueryDescHeader* desc);
typedef ffxReturnCode_t (*PfnFfxQuery)(ffxContext* context, ffxQueryDescHeader* desc);

// Dispatches work on the given FFX object context defined by the dispatch descriptor.
// Non-zero return indicates error code.
FFX_API_ENTRY ffxReturnCode_t ffxDispatch(ffxContext* context, const ffxDispatchDescHeader* desc);
typedef ffxReturnCode_t (*PfnFfxDispatch)(ffxContext* context, const ffxDispatchDescHeader* desc);

#if defined(__cplusplus)
}
#endif  // #if defined(__cplusplus)
