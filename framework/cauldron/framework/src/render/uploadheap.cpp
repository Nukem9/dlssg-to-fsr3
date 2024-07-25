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

#include "core/framework.h"
#include "render/gpuresource.h"
#include "render/uploadheap.h"

#include <chrono>
#include <thread>

namespace cauldron
{
    UploadHeap::~UploadHeap()
    {
        delete m_pResource;
    }

    void UploadHeap::InitAllocationBlocks()
    {
        // Only one block at the beginning
        AllocationBlock allocationInfo;
        allocationInfo.pDataBegin  = m_pDataBegin;
        allocationInfo.pDataEnd    = m_pDataEnd;
        allocationInfo.Size        = ((uint64_t)m_pDataEnd) - ((uint64_t)m_pDataBegin);

        m_AvailableAllocations.push_back(allocationInfo);
    }

    TransferInfo* UploadHeap::BeginResourceTransfer(size_t sliceSize, uint64_t sliceAlignment, uint32_t numSlices)
    {
        // Before we try to make any modifications, see how much mem we need and check if there is enough available
        size_t requiredSize = AlignUp(sliceSize, sliceAlignment) * numSlices;
        CauldronAssert(ASSERT_CRITICAL, requiredSize < m_Size, L"Resource will not fit into upload heap. Please make it bigger");

        // Spin here until we can get the size we need (might have to wait for other jobs to finish up)
        uint8_t* pDataBegin = nullptr;
        uint8_t* pDataEnd = nullptr;

        TransferInfo* pTransferInfo = nullptr;
        bool logged = false;

        while (!pTransferInfo)
        {
            // Go through the list of allocation blocks and find one big enough to accommodate us
            {
                std::unique_lock<std::mutex> lock(m_AllocationMutex);
                std::vector<AllocationBlock>::iterator iter = m_AvailableAllocations.begin();
                while (iter != m_AvailableAllocations.end())
                {
                    if ((*iter).Size > requiredSize)
                    {
                        // Figure out the begin, aligned begin, and end for the memory we want to use
                        pDataBegin = (*iter).pDataBegin;
                        uint8_t* pAlignedBegin = reinterpret_cast<uint8_t*>(AlignUp(reinterpret_cast<size_t>(pDataBegin), static_cast<size_t>(sliceAlignment)));
                        pDataEnd = reinterpret_cast<uint8_t*>(reinterpret_cast<size_t>(pAlignedBegin) + requiredSize);

                        // Modify the existing block
                        (*iter).pDataBegin = pDataEnd;
                        (*iter).Size = ((uint64_t)(*iter).pDataEnd) - ((uint64_t)(*iter).pDataBegin);

                        // Create our transfer block
                        m_ActiveTranfers.push_back(new TransferInfo());
                        pTransferInfo = m_ActiveTranfers.back();
                        break;
                    }
                    
                    ++iter;
                }

                // If we got here, couldn't find a block big enough. Sleep a bit and try again
                if (!pTransferInfo)
                {
                    if (!logged)
                    {
                        logged = true;  // Don't spam the output for the same resource
                        //CauldronWarning(L"Could not get a %zu size data block for resource data transfer. Waiting until more blocks are freed. Consider growing upload heap size.", requiredSize);
                    }                    
                    m_AllocationCV.wait(lock);
                }
            }                
        }

        // Got our memory, setup the transfer information, the slice pointers and fetch a command list to record the work
        pTransferInfo->AllocationInfo.pDataBegin    = pDataBegin;
        pTransferInfo->AllocationInfo.pDataEnd      = pDataEnd;
        pTransferInfo->AllocationInfo.Size          = ((uint64_t)pTransferInfo->AllocationInfo.pDataEnd) - ((uint64_t)pTransferInfo->AllocationInfo.pDataBegin);

        uint8_t* pSliceStart = pDataBegin;
        for (uint32_t i = 0; i < numSlices; ++i)
        {
            pSliceStart = reinterpret_cast<uint8_t*>(AlignUp(reinterpret_cast<size_t>(pSliceStart), static_cast<size_t>(sliceAlignment)));
            pTransferInfo->pSliceDataBegin.push_back(pSliceStart);
            pSliceStart += (sliceSize);
        }

        return pTransferInfo;
    }

    void UploadHeap::EndResourceTransfer(TransferInfo* pTransferBlock)
    {
        // Lock to avoid data collisions
        {
            std::unique_lock<std::mutex> lock(m_AllocationMutex);

            // Return the allocation block to the pool (will join memory to a an adjacent block where possible)
            std::vector<AllocationBlock>::iterator iter = m_AvailableAllocations.begin();

            // First find the right spot to insert it (ordered)
            for (; iter != m_AvailableAllocations.end(); ++iter)
            {
                if ((*iter).pDataBegin >= pTransferBlock->AllocationInfo.pDataEnd)
                    break;
            }
            iter = m_AvailableAllocations.insert(iter, pTransferBlock->AllocationInfo);

            // Now merge adjacent blocks together to free up larger blocks
            for (size_t index = m_AvailableAllocations.size() - 1; index >= 1; --index)
            {
                // Check if they are joinable
                if (m_AvailableAllocations[index].pDataBegin == m_AvailableAllocations[index - 1].pDataEnd)
                {
                    // Merge them 
                    m_AvailableAllocations[index - 1].pDataEnd = m_AvailableAllocations[index].pDataEnd;
                    m_AvailableAllocations[index - 1].Size += m_AvailableAllocations[index].Size;
                    
                    // Remove the previous element
                    m_AvailableAllocations.erase(std::next(m_AvailableAllocations.begin(), index));
                }
            }

            // Find and destroy this active transfer to clean everything up
            std::vector<TransferInfo*>::iterator transferIter = m_ActiveTranfers.begin();
            while (transferIter != m_ActiveTranfers.end())
            {
                if (*transferIter == pTransferBlock)
                {
                    delete pTransferBlock;
                    m_ActiveTranfers.erase(transferIter);
                    break;
                }

                ++transferIter;
            }

            // Signal any pending allocations
            m_AllocationCV.notify_one();
        }
    }

} // namespace cauldron
