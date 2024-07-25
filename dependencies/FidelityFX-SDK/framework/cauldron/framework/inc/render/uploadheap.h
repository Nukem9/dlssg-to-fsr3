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
#include "misc/sync.h"

#include <vector>

namespace cauldron
{
    // Defined in platform implementations
    class CommandList;
    class GPUResource;

    /// A structure representing an allocation block used to upload CPU-side memory to 
    /// a GPU resource.
    ///
    /// @ingroup CauldronRender
    struct AllocationBlock
    {
        uint8_t* pDataBegin = nullptr;      ///< The beginning of the allocation data.
        uint8_t* pDataEnd = nullptr;        ///< The end of the allocation data.
        size_t   Size = 0;                  ///< The size of the allocation.
    };

    /// A structure representing data transfer information. Is backed by an allocation block.
    ///
    /// @ingroup CauldronRender
    struct TransferInfo
    {
        uint8_t* DataPtr(uint32_t sliceID) { return pSliceDataBegin[sliceID]; }

    private:
        friend class UploadHeap;
        AllocationBlock         AllocationInfo;   // The backing allocation
        std::vector<uint8_t*>   pSliceDataBegin;  // The data pointer for each slice of data in the block
    };

    /// Per platform/API implementation of <c><i>UploadHeap</i></c>
    ///
    /// @ingroup CauldronRender
    class UploadHeapInternal;

    /**
     * @class UploadHeap
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of the upload heap.
     *
     * @ingroup CauldronRender
     */
    class UploadHeap
    {
    public:

        /**
         * @brief   UploadHeap instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static UploadHeap* CreateUploadHeap();

        /**
         * @brief   Destruction.
         */
        virtual ~UploadHeap();

        /**
         * @brief   Initializes the allocation blocks.
         */
        void InitAllocationBlocks();

        /**
         * @brief   Returns the UploadHeap's backing <c><i>GPUResource</i></c>.
         */
        const GPUResource* GetResource() const { return m_pResource; }
        GPUResource* GetResource() { return m_pResource; }

        /**
         * @brief   Returns the UploadHeap's base data pointer.
         */
        uint8_t* BasePtr() { return m_pDataBegin; }

        /**
         * @brief   Returns a <c><i>TransferInfo</i></c> instance setup to load a resource as requested.
         */
        TransferInfo* BeginResourceTransfer(size_t sliceSize, uint64_t sliceAlignment, uint32_t numSlices);

        /**
         * @brief   Ends the resource transfer associated with the <c><i>TransferInfo</i></c> pointer.
         */
        void EndResourceTransfer(TransferInfo* pTransferBlock);

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        virtual UploadHeapInternal* GetImpl() = 0;
        virtual const UploadHeapInternal* GetImpl() const = 0;

    private:
        // No copy, No move
        NO_COPY(UploadHeap)
        NO_MOVE(UploadHeap)

    protected:
        UploadHeap() = default;

        GPUResource*    m_pResource     = nullptr;
        size_t          m_Size          = 0;

        uint8_t*        m_pDataEnd      = nullptr; // Ending position of upload heap 
        uint8_t*        m_pDataBegin    = nullptr; // Starting position of upload heap

        std::vector<AllocationBlock>    m_AvailableAllocations;
        std::vector<TransferInfo*>      m_ActiveTranfers;
        std::mutex                      m_AllocationMutex;
        std::condition_variable         m_AllocationCV;
    };

} // namespace cauldron
