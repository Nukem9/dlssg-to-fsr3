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
#include "core/loaders/textureloader.h"
#include "misc/assert.h"

#include "render/vk/commandlist_vk.h"
#include "render/vk/device_vk.h"
#include "render/vk/gpuresource_vk.h"
#include "render/vk/texture_vk.h"
#include "render/vk/uploadheap_vk.h"

#include "helpers.h"

namespace cauldron
{
    Texture::Texture(const TextureDesc* pDesc, ResourceState initialState, ResizeFunction fn) :
        m_TextureDesc(*pDesc),
        m_ResizeFn(fn)
    {
        VkImageCreateInfo imageInfo = ConvertTextureDesc(*pDesc);

        GPUResourceInitParams initParams = {};
        initParams.imageInfo = imageInfo;
        initParams.type = GPUResourceType::Texture;

        m_pResource = GPUResource::CreateGPUResource(pDesc->Name.c_str(), this, initialState, &initParams, fn != nullptr);
        CauldronAssert(ASSERT_ERROR, m_pResource != nullptr, L"Could not create GPU resource for texture %ls", pDesc->Name.c_str());

        // Update the texture desc after creation (as some parameters can auto-generate info (i.e. mip levels)
        m_TextureDesc.MipLevels = imageInfo.mipLevels;
    }

    Texture::Texture(const TextureDesc* pDesc, GPUResource* pResource) :
        m_TextureDesc(*pDesc),
        m_pResource(pResource),
        m_ResizeFn(nullptr)
    {
    }

    void Texture::CopyData(TextureDataBlock* pTextureDataBlock)
    {
        VkDeviceSize totalSize = GetTotalTextureSize(m_TextureDesc.Width, m_TextureDesc.Height, m_TextureDesc.Format, m_TextureDesc.MipLevels);
        UploadHeap* pUploadHeap = GetUploadHeap();
        DeviceInternal* pDevice = GetDevice()->GetImpl();

        // NOTES
        // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap7.html#synchronization-queue-transfers
        // We assume that all resources end up on the graphics queue. Copying data on vulkan requires some extra steps.
        //   - There is no need to perform a queue ownership transfer for the target texture since we don't care about its previous content.
        //   - Staging buffer is only accessed on the copy queue, so there is also no need to transfer queue ownership.
        //   - We assume that the texture is in a CopyDest state
        //   - We record all the copy commands on the copy queue.
        //   - If the copy queue and the graphics queue aren't the same family, we need to transfer the ownership of the texture from the copy queue to the graphics queue.
        //     This is done by issuing a image memory barrier on the copy queue then on the graphics queue, ensuring that the second barrier occurs after the first one.

        CommandList* pImmediateCopyCmdList = pDevice->CreateCommandList(L"ImmediateCopyCommandList", CommandQueue::Copy);
        
        // Get what we need to transfer data
        TransferInfo* pTransferInfo = pUploadHeap->BeginResourceTransfer(totalSize, 512, m_TextureDesc.DepthOrArraySize);

        uint32_t readOffset = 0;
        for (uint32_t a = 0; a < m_TextureDesc.DepthOrArraySize; ++a)
        {
            // Get the pointer for the entries in this slice (depth slice or array entry)
            uint8_t* pPixels = pTransferInfo->DataPtr(a);

            uint32_t width = m_TextureDesc.Width;
            uint32_t height = m_TextureDesc.Height;
            uint32_t offset = 0;

            // Copy all the mip slices into the offsets specified by the footprint structure
            for (uint32_t mip = 0; mip < m_TextureDesc.MipLevels; ++mip)
            {
                MipInformation info = GetMipInformation(width, height, m_TextureDesc.Format);

                pTextureDataBlock->CopyTextureData(pPixels + offset, static_cast<uint32_t>(info.Stride), static_cast<uint32_t>(info.Stride), info.Rows, readOffset);
                readOffset += static_cast<uint32_t>(info.Stride * info.Rows);
                
                VkDeviceSize bufferOffset = static_cast<VkDeviceSize>(offset) + static_cast<VkDeviceSize>(pPixels - pUploadHeap->BasePtr());

                TextureCopyDesc copyDesc;
                copyDesc.GetImpl()->IsSourceTexture = false;
                copyDesc.GetImpl()->DstImage = m_pResource->GetImpl()->GetImage();
                copyDesc.GetImpl()->SrcBuffer = pUploadHeap->GetResource()->GetImpl()->GetBuffer();
                copyDesc.GetImpl()->Region.bufferOffset = bufferOffset;
                copyDesc.GetImpl()->Region.bufferRowLength = 0; // tightly packed
                copyDesc.GetImpl()->Region.bufferImageHeight = 0; // tightly packed
                copyDesc.GetImpl()->Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyDesc.GetImpl()->Region.imageSubresource.mipLevel = mip;
                copyDesc.GetImpl()->Region.imageSubresource.baseArrayLayer = a;
                copyDesc.GetImpl()->Region.imageSubresource.layerCount = 1;
                copyDesc.GetImpl()->Region.imageOffset = { 0, 0, 0 };
                copyDesc.GetImpl()->Region.imageExtent = { width, height, 1 };

                copyDesc.GetImpl()->pDestResource = m_pResource;

                // record copy command
                CopyTextureRegion(pImmediateCopyCmdList, &copyDesc);
                
                offset += static_cast<uint32_t>(info.TotalSize);
                if (width > 1)
                    width >>= 1;
                if (height > 1)
                    height >>= 1;
            }
        }

        uint32_t             graphicsFamily              = pDevice->VKCmdQueueFamily(CommandQueue::Graphics);
        uint32_t             copyFamily                  = pDevice->VKCmdQueueFamily(CommandQueue::Copy);
        bool                 needsQueueOwnershipTransfer = (graphicsFamily != copyFamily);
        VkImageMemoryBarrier imageMemoryBarrier = {};
        if (needsQueueOwnershipTransfer)
        {
            // release ownership from the copy queue
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.pNext = nullptr;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.srcQueueFamilyIndex = copyFamily;
            imageMemoryBarrier.dstQueueFamilyIndex = graphicsFamily;
            imageMemoryBarrier.image = m_pResource->GetImpl()->GetImage();
            imageMemoryBarrier.subresourceRange.aspectMask = GetImageAspectMask(m_pResource->GetImpl()->GetImageCreateInfo().format);
            imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
            imageMemoryBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
            imageMemoryBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

            vkCmdPipelineBarrier(pImmediateCopyCmdList->GetImpl()->VKCmdBuffer(),
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr, 0, nullptr,
                1, &imageMemoryBarrier);
        }
        
        CloseCmdList(pImmediateCopyCmdList);

        std::vector<CommandList*> cmdLists;
        cmdLists.reserve(1);
        cmdLists.push_back(pImmediateCopyCmdList);
        // Copy all immediate
        pDevice->ExecuteCommandListsImmediate(cmdLists, CommandQueue::Copy);

        delete pImmediateCopyCmdList;

        // Kick off the resource transfer. When we get back from here the resource is ready to be used.
        pUploadHeap->EndResourceTransfer(pTransferInfo);

        if (needsQueueOwnershipTransfer)
        {
            // acquire ownership on the graphics queue
            CommandList* pImmediateGraphicsCmdList = pDevice->CreateCommandList(L"ImmediateGraphicsCommandList", CommandQueue::Graphics);

            vkCmdPipelineBarrier(pImmediateGraphicsCmdList->GetImpl()->VKCmdBuffer(),
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr, 0, nullptr,
                1, &imageMemoryBarrier);

            CloseCmdList(pImmediateGraphicsCmdList);

            cmdLists.clear();
            cmdLists.push_back(pImmediateGraphicsCmdList);
            pDevice->ExecuteCommandListsImmediate(cmdLists, CommandQueue::Graphics);

            delete pImmediateGraphicsCmdList;

            // TODO: a possible improvement would be to not wait that the copy finish before issuing the command on the graphics list.
            // If we can pass the wait semaphore to the ExecuteCommandListsImmediate, we only need to call ExecuteCommand on the copy queue.
            // In that case we can only delete the CommandLists and the pTransferInfo at the end of this method.
        }
    }

    void Texture::Recreate()
    {
        VkImageCreateInfo imageInfo = ConvertTextureDesc(m_TextureDesc);

        // recreate the resource
        m_pResource->GetImpl()->RecreateResource(imageInfo, m_pResource->GetImpl()->GetCurrentResourceState());
    }

    //////////////////////////////////////////////////////////////////////////
    // TextureCopyDesc

    TextureCopyDesc::TextureCopyDesc(const GPUResource* pSrc, const GPUResource* pDst, unsigned int arrayIndex, unsigned int mipLevel)
    {
        const GPUResourceInternal* pSrcResource = pSrc->GetImpl();
        const GPUResourceInternal* pDstResource = pDst->GetImpl();

        CauldronAssert(ASSERT_CRITICAL, pDstResource->GetResourceType() == ResourceType::Image, L"Destination should be an image.");

        if (pSrcResource->GetResourceType() == ResourceType::Image)
        {
            GetImpl()->IsSourceTexture = true;
            GetImpl()->SrcImage = pSrcResource->GetImage();
            GetImpl()->SrcImageFormat = pSrcResource->GetImageCreateInfo().format;
            GetImpl()->DstImage = pDstResource->GetImage();
            GetImpl()->DstImageFormat = pDstResource->GetImageCreateInfo().format;
            GetImpl()->pDestResource = const_cast<GPUResource*>(pDst);

            GetImpl()->ImageCopy = {};
            GetImpl()->ImageCopy.srcSubresource.aspectMask = GetImageAspectMask(pSrcResource->GetImageCreateInfo().format);
            GetImpl()->ImageCopy.srcSubresource.baseArrayLayer = arrayIndex;
            GetImpl()->ImageCopy.srcSubresource.layerCount = 1;
            GetImpl()->ImageCopy.srcSubresource.mipLevel = mipLevel;
            GetImpl()->ImageCopy.srcOffset.x = 0;
            GetImpl()->ImageCopy.srcOffset.y = 0;
            GetImpl()->ImageCopy.srcOffset.z = 0;
            GetImpl()->ImageCopy.dstSubresource.aspectMask = GetImageAspectMask(pDstResource->GetImageCreateInfo().format);
            GetImpl()->ImageCopy.dstSubresource.baseArrayLayer = arrayIndex;
            GetImpl()->ImageCopy.dstSubresource.layerCount = 1;
            GetImpl()->ImageCopy.dstSubresource.mipLevel = mipLevel;
            GetImpl()->ImageCopy.dstOffset.x = 0;
            GetImpl()->ImageCopy.dstOffset.y = 0;
            GetImpl()->ImageCopy.dstOffset.z = 0;

            VkExtent3D srcExtent = pSrcResource->GetImageCreateInfo().extent;
            GetImpl()->ImageCopy.extent = pDstResource->GetImageCreateInfo().extent;
            GetImpl()->ImageCopy.extent.width = std::min(CalculateSizeAtMipLevel(GetImpl()->ImageCopy.extent.width, mipLevel), srcExtent.width);
            GetImpl()->ImageCopy.extent.height = std::min(CalculateSizeAtMipLevel(GetImpl()->ImageCopy.extent.height, mipLevel), srcExtent.height);
            GetImpl()->ImageCopy.extent.depth = std::min(CalculateSizeAtMipLevel(GetImpl()->ImageCopy.extent.depth, mipLevel), srcExtent.depth);
        }
        else if (pSrcResource->GetResourceType() == ResourceType::Buffer)
        {
            GetImpl()->IsSourceTexture = false;
            GetImpl()->SrcBuffer = pSrcResource->GetBuffer();
            GetImpl()->DstImage = pDstResource->GetImage();
            GetImpl()->pDestResource = const_cast<GPUResource*>(pDst);

            GetImpl()->Region = {};
            GetImpl()->Region.bufferOffset = 0;
            GetImpl()->Region.bufferRowLength = 0; // tightly packed
            GetImpl()->Region.bufferImageHeight = 0; // tightly packed
            // we are assuming we are copying the whole texture
            // Copying a depth/stencil image will result in a validation error but we won't do that
            GetImpl()->Region.imageSubresource.aspectMask = GetImageAspectMask(pDstResource->GetImageCreateInfo().format);
            GetImpl()->Region.imageSubresource.baseArrayLayer = arrayIndex;
            GetImpl()->Region.imageSubresource.layerCount = pDstResource->GetImageCreateInfo().arrayLayers;
            GetImpl()->Region.imageSubresource.mipLevel = mipLevel;
            GetImpl()->Region.imageOffset.x = 0;
            GetImpl()->Region.imageOffset.y = 0;
            GetImpl()->Region.imageOffset.z = 0;
            GetImpl()->Region.imageExtent = pDstResource->GetImageCreateInfo().extent;
        }
    }

} // namespace cauldron

#endif // #if defined(_VK)
