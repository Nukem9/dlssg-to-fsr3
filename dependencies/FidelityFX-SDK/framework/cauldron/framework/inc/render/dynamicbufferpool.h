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

#include "misc/helpers.h"

#include <mutex>

namespace cauldron
{
    struct BufferAddressInfo;
    class GPUResource;
    
    /**
     * @class Buffer
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of a dynamic buffer pool.
     *
     * @ingroup CauldronRender
     */
    class DynamicBufferPool
    {
    public:

        /**
         * @brief   DynamicBufferPool instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static DynamicBufferPool* CreateDynamicBufferPool();

        /**
         * @brief   Destruction.
         */
        virtual ~DynamicBufferPool();

        /**
         * @brief   Allocates a temporary constant buffer and initializes it with the provided memory.
         *          Returns the BufferAddressInfo for the buffer.
         */
        virtual BufferAddressInfo AllocConstantBuffer(uint32_t size, const void* pInitData) = 0;

        /**
         * @brief   Allocates a batch of temporary constant buffers without initializing.
         *          Sets the BufferAddressInfo for the buffers at the location specified in pBufferAddressInfos.
         */
        virtual void BatchAllocateConstantBuffer(uint32_t size, uint32_t count, BufferAddressInfo* pBufferAddressInfos) = 0;

        /*
         * @brief   Initializes the provided temporary constant buffer with the provided data.
         */
        virtual void InitializeConstantBuffer(const BufferAddressInfo& bufferAddressInfo, uint32_t size, const void* pInitData) = 0;


        /**
         * @brief   Allocates a temporary vertex buffer and maps the provided pointer to the backing memory.
         *          Returns the BufferAddressInfo for the buffer.
         */
        virtual BufferAddressInfo AllocVertexBuffer(uint32_t vertexCount, uint32_t vertexStride, void** pBuffer) = 0;

        /**
         * @brief   Allocates a temporary index buffer and maps the provided pointer to the backing memory.
         *          Returns the BufferAddressInfo for the buffer.
         */
        virtual BufferAddressInfo AllocIndexBuffer(uint32_t indexCount, uint32_t indexStride, void** pBuffer) = 0;

        /**
         * @brief   Gets a constant pointer to the buffer pool's underlaying <c><i>GPUResource</i></c>.
         */
        const GPUResource* GetResource() const { return m_pResource; }

        /**
         * @brief   Cycles used memory thus far in preparation for next batch of use
         */
        virtual void EndFrame() = 0;


    private:
        // No copy, No move
        NO_COPY(DynamicBufferPool)
        NO_MOVE(DynamicBufferPool)

    protected:

        // Special case for swap chains
        DynamicBufferPool();

        uint32_t     m_TotalSize  = 0;
        uint8_t*     m_pData      = nullptr;

        // Backing resource
        GPUResource* m_pResource = nullptr;
    };

} // namespace cauldron
