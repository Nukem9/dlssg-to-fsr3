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

#include "ffx_breadcrumbs_list.h"

void* ffxBreadcrumbsAppendList(void* src, size_t currentCount, size_t elementSize, size_t appendCount, FfxAllocationCallbacks* callbacks)
{
    FFX_ASSERT(src ? currentCount > 0 : currentCount == 0);

    void* dst = callbacks->fpRealloc(src, elementSize * (currentCount + appendCount));
    FFX_ASSERT(dst);

    return dst;
}

void* ffxBreadcrumbsPopList(void* src, size_t newCount, size_t elementSize, FfxAllocationCallbacks* callbacks)
{
    FFX_ASSERT(src);

    void* dst = nullptr;
    if (newCount > 0)
    {
        dst = callbacks->fpRealloc(src, elementSize * newCount);
        FFX_ASSERT(dst);
    }
    else
        callbacks->fpFree(src);

    return dst;
}
