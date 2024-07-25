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

#if defined(_VK)

#include "render/buffer.h"
#include "render/renderdefines.h"
#include <vulkan/vulkan.h>

namespace cauldron
{
    // Defined per platform
    struct BufferAddressInfoInternal final 
    {
        VkBuffer     Buffer = VK_NULL_HANDLE;
        VkDeviceSize SizeInBytes = 0;
        VkDeviceSize Offset = 0;
        union
        {
            VkDeviceSize StrideInBytes = 0;
            VkIndexType IndexType;
        };
    };
    static_assert(sizeof(BufferAddressInfo::addressInfoSize) >= sizeof(BufferAddressInfoInternal), "BufferAddressInfo is not large enough to hold all implementation details. Please grow.");

    struct BufferCopyDescInternal
    {
        VkBuffer     SrcBuffer;
        VkBuffer     DstBuffer;
        VkBufferCopy Region;
    };
    static_assert(sizeof(BufferCopyDesc::bufferCopyDescMem) >= sizeof(BufferCopyDescInternal), "BufferCopyDesc is not large enough to hold all implementation details. Please grow.");

    class UploadContext;

    class BufferInternal final : public Buffer
    {
    public:
        virtual ~BufferInternal() = default;

        virtual void CopyData(const void* pData, size_t size) override;
        virtual void CopyData(const void* pData, size_t size, UploadContext* pUploadCtx, ResourceState postCopyState) override;
        virtual BufferAddressInfo GetAddressInfo()const override;

    private:

        BufferInternal() = delete;
        BufferInternal(const BufferDesc* pDesc, ResourceState initialState, ResizeFunction fn, void* customOwner);
        virtual void Recreate() override;

        // only those this class can call the Buffer constructor
        friend class Buffer;
    };

} // namespace cauldron

#endif // #if defined(_VK)
