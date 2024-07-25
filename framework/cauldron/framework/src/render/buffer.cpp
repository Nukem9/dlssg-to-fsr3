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

#include "render/buffer.h"
#include "core/framework.h"
#include "misc/assert.h"

namespace cauldron
{
    Buffer::Buffer(const BufferDesc* pDesc, ResizeFunction fn) :
        m_BufferDesc(*pDesc),
        m_ResizeFn(fn)
    {
    }

    Buffer::~Buffer()
    {
        delete m_pResource;
    }

    void Buffer::OnRenderingResolutionResize(uint32_t outputWidth, uint32_t outputHeight, uint32_t renderingWidth, uint32_t renderingHeight)
    {
        CauldronAssert(ASSERT_CRITICAL, m_ResizeFn != nullptr, L"There no method to resize the buffer");

        // get the new information
        m_ResizeFn(m_BufferDesc, outputWidth, outputHeight, renderingWidth, renderingHeight);

        // recreate resource
        Recreate();
    }

} // namespace cauldron
