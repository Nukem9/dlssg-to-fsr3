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

#include <cstdint>
#include <cstdio>      // sprintf_s
#include <FidelityFX/host/ffx_assert.h>

#define FFX_BREADCRUMBS_APPEND_STRING(buff, count, str)                                  \
    do                                                                                   \
    {                                                                                    \
        buff = (char*)ffxBreadcrumbsAppendList(buff, count, 1, sizeof(str) - 1, allocs); \
        memcpy(buff + count, str, sizeof(str) - 1);                                      \
        count += sizeof(str) - 1;                                                        \
    } while (false)

#define FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(buff, count, src)                  \
    do                                                                           \
    {                                                                            \
        const char* _str = src;                                                  \
        const size_t _length = strlen(_str);                                     \
        buff = (char*)ffxBreadcrumbsAppendList(buff, count, 1, _length, allocs); \
        memcpy(buff + count, _str, _length);                                     \
        count += _length;                                                        \
    } while (false)

#define FFX_BREADCRUMBS_APPEND_NUMBER(buff, count, number, maxLength, format)    \
    do                                                                           \
    {                                                                            \
        char _numberStr[maxLength];                                              \
        const size_t _length = sprintf_s(_numberStr, maxLength, format, number); \
        buff = (char*)ffxBreadcrumbsAppendList(buff, count, 1, _length, allocs); \
        memcpy(buff + count, _numberStr, _length);                               \
        count += _length;                                                        \
    } while (false)

#define FFX_BREADCRUMBS_APPEND_UINT(buff, count, number) \
    FFX_BREADCRUMBS_APPEND_NUMBER(buff, count, number, 11, "%u")

#define FFX_BREADCRUMBS_APPEND_UINT64(buff, count, number) \
    FFX_BREADCRUMBS_APPEND_NUMBER(buff, count, number, 21, "%zu")

#define FFX_BREADCRUMBS_APPEND_FLOAT(buff, count, number) \
    FFX_BREADCRUMBS_APPEND_NUMBER(buff, count, number, 18, "%.3f")

#define FFX_BREADCRUMBS_PRINTING_INDENT "    "

#define FFX_BREADCRUMBS_PRINT_STRING(buff, count, baseStruct, member)                             \
    do                                                                                            \
    {                                                                                             \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, FFX_BREADCRUMBS_PRINTING_INDENT #member ": "); \
        FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(buff, count, baseStruct.member);                    \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, "\n");                                         \
    } while (false)

#define FFX_BREADCRUMBS_PRINT_UINT(buff, count, baseStruct, member)                               \
    do                                                                                            \
    {                                                                                             \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, FFX_BREADCRUMBS_PRINTING_INDENT #member ": "); \
        FFX_BREADCRUMBS_APPEND_UINT(buff, count, baseStruct.member);                              \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, "\n");                                         \
    } while (false)

#define FFX_BREADCRUMBS_PRINT_UINT64(buff, count, baseStruct, member)                             \
    do                                                                                            \
    {                                                                                             \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, FFX_BREADCRUMBS_PRINTING_INDENT #member ": "); \
        FFX_BREADCRUMBS_APPEND_UINT64(buff, count, baseStruct.member);                            \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, "\n");                                         \
    } while (false)

#define FFX_BREADCRUMBS_PRINT_FLOAT(buff, count, baseStruct, member)                              \
    do                                                                                            \
    {                                                                                             \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, FFX_BREADCRUMBS_PRINTING_INDENT #member ": "); \
        FFX_BREADCRUMBS_APPEND_FLOAT(buff, count, baseStruct.member);                             \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, "\n");                                         \
    } while (false)

#define FFX_BREADCRUMBS_PRINT_BOOL(buff, count, baseStruct, member)                               \
    do                                                                                            \
    {                                                                                             \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, FFX_BREADCRUMBS_PRINTING_INDENT #member ": "); \
        if (baseStruct.member)                                                                    \
        {                                                                                         \
            FFX_BREADCRUMBS_APPEND_STRING(buff, count, "True\n");                                 \
        }                                                                                         \
        else                                                                                      \
        {                                                                                         \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, "False\n");                                    \
        }                                                                                         \
    } while (false)

#define FFX_BREADCRUMBS_PRINT_HEX_NUMBER(buff, count, baseStruct, member, maxLength, format)        \
    do                                                                                              \
    {                                                                                               \
        FFX_BREADCRUMBS_APPEND_STRING(buff, count, FFX_BREADCRUMBS_PRINTING_INDENT #member ": 0x"); \
        char _hexStr[maxLength];                                                                    \
        const size_t _length = sprintf_s(_hexStr, maxLength, format, baseStruct.member);            \
        buff = (char*)ffxBreadcrumbsAppendList(buff, count, 1, _length + 1, allocs);                \
        memcpy(buff + count, _hexStr, _length);                                                     \
        count += _length;                                                                           \
        buff[count++] = '\n';                                                                       \
    } while (false)

#define FFX_BREADCRUMBS_PRINT_HEX32(buff, count, baseStruct, member) \
    FFX_BREADCRUMBS_PRINT_HEX_NUMBER(buff, count, baseStruct, member, 9, "%X")

#define FFX_BREADCRUMBS_PRINT_HEX64(buff, count, baseStruct, member) \
    FFX_BREADCRUMBS_PRINT_HEX_NUMBER(buff, count, baseStruct, member, 17, "%zX")

#define FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, count, val)                         \
    do                                                                           \
    {                                                                            \
        buff = (char*)ffxBreadcrumbsAppendList(buff, count, 1, 2, allocs);       \
        buff[count++] = (char)((val >> 4) + ((val >> 4) >= 10 ? '7' : '0'));     \
        buff[count++] = (char)((val & 0x0F) + ((val & 0x0F) >= 10 ? '7' : '0')); \
    } while (false)

#if defined(__cplusplus)
extern "C" {
#endif // #if defined(__cplusplus)

    FFX_API void* ffxBreadcrumbsAppendList(void* src, size_t currentCount, size_t elementSize, size_t appendCount, FfxAllocationCallbacks* callbacks);

    FFX_API void* ffxBreadcrumbsPopList(void* src, size_t newCount, size_t elementSize, FfxAllocationCallbacks* callbacks);

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
