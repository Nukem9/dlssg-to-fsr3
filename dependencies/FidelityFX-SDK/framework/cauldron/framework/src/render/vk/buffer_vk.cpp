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

#if defined(_VK)

#include "core/framework.h"
#include "misc/assert.h"

#include "render/vk/buffer_vk.h"
#include "render/vk/commandlist_vk.h"
#include "render/vk/device_vk.h"
#include "render/vk/gpuresource_vk.h"
#include "render/vk/uploadheap_vk.h"

#include "helpers.h"

namespace cauldron
{
    BufferCopyDesc::BufferCopyDesc(const GPUResource* pSrc, const GPUResource* pDst)
    {
        const GPUResourceInternal* pSrcResource = pSrc->GetImpl();
        const GPUResourceInternal* pDstResource = pDst->GetImpl();

        CauldronAssert(ASSERT_CRITICAL, pSrcResource->GetResourceType() == ResourceType::Buffer, L"Source should be an buffer.");
        CauldronAssert(ASSERT_CRITICAL, pDstResource->GetResourceType() == ResourceType::Buffer, L"Destination should be an buffer.");

        GetImpl()->SrcBuffer = pSrcResource->GetBuffer();
        GetImpl()->DstBuffer = pDstResource->GetBuffer();

        GetImpl()->Region.srcOffset = 0;
        GetImpl()->Region.dstOffset = 0;
        GetImpl()->Region.size = pSrcResource->GetBufferCreateInfo().size; // assume we want to copy the whole buffer
    }

    Buffer* Buffer::CreateBufferResource(const BufferDesc* pDesc, ResourceState initialState, ResizeFunction fn/*= nullptr*/, void* customOwner/*= nullptr*/)
    {
        return new BufferInternal(pDesc, initialState, fn, customOwner);
    }

    BufferInternal::BufferInternal(const BufferDesc* pDesc, ResourceState initialState, ResizeFunction fn, void* customOwner) :
        Buffer(pDesc, fn)
    {
        VkBufferCreateInfo bufferInfo = ConvertBufferDesc(*pDesc);
        
        GPUResourceInitParams initParams = {};
        initParams.bufferInfo = bufferInfo;
        initParams.alignment = pDesc->Alignment;

        if (static_cast<bool>(pDesc->Flags & ResourceFlags::BreadcrumbsBuffer))
            initParams.type = GPUResourceType::BufferBreadcrumbs;
        else
        {
            initParams.type = GPUResourceType::Buffer;
            initParams.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            customOwner = this;
        }

        m_pResource = GPUResource::CreateGPUResource(m_BufferDesc.Name.c_str(), customOwner, initialState, &initParams, m_ResizeFn != nullptr);
        CauldronAssert(ASSERT_ERROR, m_pResource != nullptr, L"Could not create GPU resource for buffer %ls", m_BufferDesc.Name);
    }

    void BufferInternal::CopyData(const void* pData, size_t size)
    {
        UploadHeap* pUploadHeap = GetUploadHeap();
        DeviceInternal* pDevice = GetDevice()->GetImpl();

        // NOTES
        // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap7.html#synchronization-queue-transfers
        // We assume that all resources end up on the graphics queue. Copying data on vulkan requires some extra steps.
        //   - There is no need to perform a queue ownership transfer for the target buffer since we don't care about its previous content.
        //   - Staging buffer is only accessed on the copy queue, so there is also no need to transfer queue ownership.
        //   - We assume that if a barrier was necessary to copy into this buffer, it has been handled by the caller of this method
        //   - We record all the copy commands on the copy queue.
        //   - If the copy queue and the graphics queue aren't the same family, we need to transfer the ownership of the buffer from the copy queue to the graphics queue.
        //     This is done by issuing a buffer memory barrier on the copy queue then on the graphics queue, ensuring that the second barrier occurs after the first one.

        CommandList* pImmediateCopyCmdList = pDevice->CreateCommandList(L"ImmediateCopyCommandList", CommandQueue::Copy);

        // Get what we need to transfer data
        TransferInfo* pTransferInfo = pUploadHeap->BeginResourceTransfer(size, 256, 1);

        uint8_t* pMapped = pTransferInfo->DataPtr(0);
        memcpy(pMapped, pData, size);

        VkDeviceSize bufferOffset = static_cast<VkDeviceSize>(pMapped - pUploadHeap->BasePtr());

        BufferCopyDesc desc;
        desc.GetImpl()->SrcBuffer = pUploadHeap->GetResource()->GetImpl()->GetBuffer();
        desc.GetImpl()->DstBuffer = m_pResource->GetImpl()->GetBuffer();
        desc.GetImpl()->Region.srcOffset = bufferOffset;
        desc.GetImpl()->Region.dstOffset = 0;
        desc.GetImpl()->Region.size = static_cast<VkDeviceSize>(size);

        CopyBufferRegion(pImmediateCopyCmdList, &desc);

        uint32_t              graphicsFamily              = pDevice->VKCmdQueueFamily(CommandQueue::Graphics);
        uint32_t              copyFamily                  = pDevice->VKCmdQueueFamily(CommandQueue::Copy);
        bool                  needsQueueOwnershipTransfer = (graphicsFamily != copyFamily);
        VkBufferMemoryBarrier bufferMemoryBarrier = {};
        if (needsQueueOwnershipTransfer)
        {
            // release ownership from the copy queue
            bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferMemoryBarrier.pNext = nullptr;
            bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufferMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufferMemoryBarrier.srcQueueFamilyIndex = copyFamily;
            bufferMemoryBarrier.dstQueueFamilyIndex = graphicsFamily;
            bufferMemoryBarrier.buffer = m_pResource->GetImpl()->GetBuffer();
            bufferMemoryBarrier.offset = 0;
            bufferMemoryBarrier.size = static_cast<VkDeviceSize>(size);

            vkCmdPipelineBarrier(pImmediateCopyCmdList->GetImpl()->VKCmdBuffer(),
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                0, nullptr,
                1, &bufferMemoryBarrier,
                0, nullptr);
        }

        CloseCmdList(pImmediateCopyCmdList);

        std::vector<CommandList*> cmdLists;
        cmdLists.reserve(1);
        cmdLists.push_back(pImmediateCopyCmdList);
        
        if (needsQueueOwnershipTransfer)
        {
           VkSemaphore signaledSemaphore = pDevice->ExecuteCommandListsWithSignalSemaphore(cmdLists, CommandQueue::Copy);

            // acquire ownership on the graphics queue
            CommandListInternal* pImmediateGraphicsCmdList = (CommandListInternal*)pDevice->CreateCommandList(L"ImmediateGraphicsCommandList", CommandQueue::Graphics);

            vkCmdPipelineBarrier(pImmediateGraphicsCmdList->VKCmdBuffer(),
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                0, nullptr,
                1, &bufferMemoryBarrier,
                0, nullptr);

            CloseCmdList(pImmediateGraphicsCmdList);

            cmdLists.clear();
            cmdLists.push_back(pImmediateGraphicsCmdList);
            pDevice->ExecuteCommandListsImmediate(cmdLists, CommandQueue::Graphics, signaledSemaphore, CommandQueue::Copy);

            delete pImmediateGraphicsCmdList;
        }
        else
        {
            // Copy all immediate
            pDevice->ExecuteCommandListsImmediate(cmdLists, CommandQueue::Copy);
        }

        delete pImmediateCopyCmdList;

        pUploadHeap->EndResourceTransfer(pTransferInfo);
    }

    void BufferInternal::CopyData(const void* pData, size_t size, UploadContext* pUploadContext, ResourceState postCopyState)
    {
        UploadHeap*     pUploadHeap = GetUploadHeap();
        DeviceInternal* pDevice     = GetDevice()->GetImpl();

        // NOTES
        // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap7.html#synchronization-queue-transfers
        // We assume that all resources end up on the graphics queue. Copying data on vulkan requires some extra steps.
        //   - There is no need to perform a queue ownership transfer for the target buffer since we don't care about its previous content.
        //   - Staging buffer is only accessed on the copy queue, so there is also no need to transfer queue ownership.
        //   - We assume that if a barrier was necessary to copy into this buffer, it has been handled by the caller of this method
        //   - We record all the copy commands on the copy queue.
        //   - If the copy queue and the graphics queue aren't the same family, we need to transfer the ownership of the buffer from the copy queue to the graphics queue.
        //     This is done by issuing a buffer memory barrier on the copy queue then on the graphics queue, ensuring that the second barrier occurs after the first one.

        CauldronAssert(ASSERT_CRITICAL, pUploadContext != nullptr, L"null upload context");
        
        // Get what we need to transfer data
        TransferInfo* pTransferInfo = pUploadHeap->BeginResourceTransfer(size, 256, 1);

        uint8_t* pMapped = pTransferInfo->DataPtr(0);
        memcpy(pMapped, pData, size);

        VkDeviceSize bufferOffset = static_cast<VkDeviceSize>(pMapped - pUploadHeap->BasePtr());

        BufferCopyDesc desc;
        desc.GetImpl()->SrcBuffer        = pUploadHeap->GetResource()->GetImpl()->GetBuffer();
        desc.GetImpl()->DstBuffer        = m_pResource->GetImpl()->GetBuffer();
        desc.GetImpl()->Region.srcOffset = bufferOffset;
        desc.GetImpl()->Region.dstOffset = 0;
        desc.GetImpl()->Region.size      = static_cast<VkDeviceSize>(size);

        CopyBufferRegion(pUploadContext->GetImpl()->GetCopyCmdList(), &desc);

        uint32_t              graphicsFamily              = pDevice->VKCmdQueueFamily(CommandQueue::Graphics);
        uint32_t              copyFamily                  = pDevice->VKCmdQueueFamily(CommandQueue::Copy);
        bool                  needsQueueOwnershipTransfer = (graphicsFamily != copyFamily);
        VkBufferMemoryBarrier bufferMemoryBarrier         = {};
        if (needsQueueOwnershipTransfer)
        {
            // release ownership from the copy queue
            bufferMemoryBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferMemoryBarrier.pNext               = nullptr;
            bufferMemoryBarrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufferMemoryBarrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufferMemoryBarrier.srcQueueFamilyIndex = copyFamily;
            bufferMemoryBarrier.dstQueueFamilyIndex = graphicsFamily;
            bufferMemoryBarrier.buffer              = m_pResource->GetImpl()->GetBuffer();
            bufferMemoryBarrier.offset              = 0;
            bufferMemoryBarrier.size                = static_cast<VkDeviceSize>(size);

            vkCmdPipelineBarrier(pUploadContext->GetImpl()->GetCopyCmdList()->GetImpl()->VKCmdBuffer(),
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &bufferMemoryBarrier,
                                 0,
                                 nullptr);
        }

        if (needsQueueOwnershipTransfer)
        {
            pUploadContext->GetImpl()->HasGraphicsCmdList() = true;
            vkCmdPipelineBarrier(pUploadContext->GetImpl()->GetGraphicsCmdList()->GetImpl()->VKCmdBuffer(),
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &bufferMemoryBarrier,
                                 0,
                                 nullptr);
        }

        pUploadContext->AppendTransferInfo(pTransferInfo);

        Barrier bufferTransition = Barrier::Transition(GetResource(), ResourceState::CopyDest, postCopyState);
        ResourceBarrier(pUploadContext->GetImpl()->GetGraphicsCmdList(), 1, &bufferTransition);
        pUploadContext->GetImpl()->HasGraphicsCmdList() = true;
    }

    BufferAddressInfo BufferInternal::GetAddressInfo() const
    {
        BufferAddressInfo addressInfo;
        BufferAddressInfoInternal* pInfo = (BufferAddressInfoInternal*)(&addressInfo);

        pInfo->Buffer = m_pResource->GetImpl()->GetBuffer();
        pInfo->SizeInBytes = static_cast<VkDeviceSize>(m_BufferDesc.Size);
        pInfo->Offset = 0;

        switch (m_BufferDesc.Type)
        {
        case BufferType::Vertex:
            pInfo->StrideInBytes = static_cast<VkDeviceSize>(m_BufferDesc.Stride);
            break;
        case BufferType::Index:
            if (m_BufferDesc.Format == ResourceFormat::R16_UINT)
                pInfo->IndexType = VK_INDEX_TYPE_UINT16;
            else if (m_BufferDesc.Format == ResourceFormat::R32_UINT)
                pInfo->IndexType = VK_INDEX_TYPE_UINT32;
            break;
        case BufferType::Data:
            pInfo->StrideInBytes = static_cast<VkDeviceSize>(m_BufferDesc.Stride);
            break;
        case BufferType::AccelerationStructure:
            pInfo->StrideInBytes = 1;
            break;
        default:
            CauldronCritical(L"Unknown buffer type");
            break;
        }

        return addressInfo;
    }

    void BufferInternal::Recreate()
    {
        // Create an updated Resource Description with updated buffer desc
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.pNext = nullptr;
        bufferInfo.flags = 0;
        bufferInfo.size = static_cast<VkDeviceSize>(m_BufferDesc.Size);
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        switch (m_BufferDesc.Type)
        {
        case BufferType::Vertex:
            bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        //case BufferType::ConstantBuffer:
        //    bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        //    break;
        case BufferType::Index:
            bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        case BufferType::Data:
            bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;
        default:
            CauldronError(L"Unsupported buffer type.");
            break;
        }
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.queueFamilyIndexCount = 0;
        bufferInfo.pQueueFamilyIndices = nullptr;

        // recreate the resource
        m_pResource->GetImpl()->RecreateResource(bufferInfo, m_pResource->GetCurrentResourceState());
    }

} // namespace cauldron

#endif // #if defined(_VK)
