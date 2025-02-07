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
#include "render/vk/indirectworkload_vk.h"
#include "render/vk/resourceview_vk.h"
#include "render/vk/pipelineobject_vk.h"
#include "render/vk/texture_vk.h"
#include "render/vk/uploadheap_vk.h"
#include "render/rasterview.h"

#include "helpers.h"

namespace cauldron
{
    VkImageLayout ConvertToLayout(ResourceState state)
    {
        switch (state)
        {
            case ResourceState::CommonResource:
                return VK_IMAGE_LAYOUT_GENERAL;
            case ResourceState::RenderTargetResource:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case ResourceState::UnorderedAccess:
                return VK_IMAGE_LAYOUT_GENERAL;
            case ResourceState::DepthWrite:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case ResourceState::DepthRead:
            case ResourceState::DepthShaderResource:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ResourceState::NonPixelShaderResource:
            case ResourceState::PixelShaderResource:
            case ResourceState::ShaderResource:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ResourceState::CopyDest:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case ResourceState::CopySource:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case ResourceState::GenericRead:
                return VK_IMAGE_LAYOUT_GENERAL;
            case ResourceState::Present:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            case ResourceState::ShadingRateSource:
                return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
            case g_UndefinedState:
                return VK_IMAGE_LAYOUT_UNDEFINED;
            // Unsupported states
            case ResourceState::VertexBufferResource:
            case ResourceState::ConstantBufferResource:
            case ResourceState::IndexBufferResource:
            case ResourceState::IndirectArgument:
            case ResourceState::ResolveDest:
            case ResourceState::ResolveSource:
            case ResourceState::RTAccelerationStruct:
            default:
                CauldronCritical(L"Unsupported resource state for layout.");
                return VK_IMAGE_LAYOUT_UNDEFINED;
        };
    }

    #define HAS_FLAG(flag) ((state & flag) != 0)
    VkAccessFlags ConvertToAccessMask(ResourceState state)
    {
        switch (state)
        {
        case ResourceState::CommonResource:
            return 0; // VK_ACCESS_NONE_KHR;
        case ResourceState::VertexBufferResource:
            return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        case ResourceState::ConstantBufferResource:
            return VK_ACCESS_UNIFORM_READ_BIT;
        case ResourceState::IndexBufferResource:
            return VK_ACCESS_INDEX_READ_BIT;
        case ResourceState::RenderTargetResource:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case ResourceState::UnorderedAccess:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case ResourceState::DepthWrite:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case ResourceState::DepthRead:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        case ResourceState::DepthShaderResource:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
        case ResourceState::NonPixelShaderResource:
        case ResourceState::PixelShaderResource:
        case ResourceState::ShaderResource:
            return VK_ACCESS_SHADER_READ_BIT;
        case ResourceState::IndirectArgument:
            return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        case ResourceState::CopyDest:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case ResourceState::CopySource:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case ResourceState::ResolveDest:
            break;
        case ResourceState::ResolveSource:
            break;
        case ResourceState::RTAccelerationStruct:
            return VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            //return VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        case ResourceState::ShadingRateSource:
            return VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
        case ResourceState::GenericRead:
            break;
        case ResourceState::Present:
            return 0; // VK_ACCESS_NONE_KHR;
        case g_UndefinedState:
            return 0;  // VK_ACCESS_NONE_KHR;
        };

        CauldronCritical(L"Unsupported resource state for access mask.");
        return 0; //VK_ACCESS_NONE_KHR;
    }

    VkPrimitiveTopology ConvertTopology(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::PointList:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveTopology::LineList:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::TriangleList:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        default:
            return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
        }
    }

    CommandList* CommandList::CreateCommandList(const wchar_t* name, CommandQueue queueType, void* pInitParams)
    {
        CommandListInitParams* pParams = (CommandListInitParams*)pInitParams;
        return new CommandListInternal(pParams->pDevice, queueType, pParams->pool, name);
    }

    CommandList* CommandList::GetWrappedCmdListFromSDK(const wchar_t* name, CommandQueue queueType, void* pSDKCmdList)
    {
        return new CommandListInternal(queueType, (VkCommandBuffer)pSDKCmdList, name);
    }

    void CommandList::ReleaseWrappedCmdList(CommandList* pCmdList)
    {
        delete pCmdList;
    }

    CommandListInternal::CommandListInternal(Device* pDevice, CommandQueue queueType, VkCommandPool pool, const wchar_t* name) :
        CommandList(queueType),
        m_Device(pDevice->GetImpl()->VKDevice()),
        m_CommandBuffer(VK_NULL_HANDLE),
        m_CommandPool(pool)
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.commandBufferCount = 1;

        VkResult res = vkAllocateCommandBuffers(m_Device, &allocInfo, &m_CommandBuffer);
        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Failed to allocate a command buffer");

        if (name != nullptr)
            pDevice->GetImpl()->SetResourceName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)m_CommandBuffer, name);

        // begin recording into the command buffer immediately
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        res = vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Failed to begin recording into a command buffer");
    }

    CommandListInternal::CommandListInternal(CommandQueue queueType, VkCommandBuffer cmdBuffer, const wchar_t* name)
        : CommandList(queueType)
        , m_Device(VK_NULL_HANDLE)
        , m_CommandBuffer(cmdBuffer)
        , m_CommandPool(VK_NULL_HANDLE)
    {

    }

    CommandListInternal::~CommandListInternal()
    {
        if (m_CommandBuffer != VK_NULL_HANDLE && m_CommandPool != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &m_CommandBuffer);
        }
        if (m_CommandPool != VK_NULL_HANDLE)
            GetDevice()->GetImpl()->ReleaseCommandPool(this);
    }

    UploadContext* UploadContext::CreateUploadContext()
    {
        return new UploadContextInternal();
    }

    UploadContextInternal::UploadContextInternal() :
        UploadContext()
    {
        m_pCopyCommandList     = GetDevice()->CreateCommandList(L"ImmediateCopyCommandList", CommandQueue::Copy);
        m_pGraphicsCommandList = GetDevice()->CreateCommandList(L"ImmediateGraphicsCommandList", CommandQueue::Graphics);
        m_HasGraphicsCommands  = false;
    }

    UploadContextInternal::~UploadContextInternal()
    {
        delete m_pCopyCommandList;
        delete m_pGraphicsCommandList;
    }

    void UploadContextInternal::Execute()
    {
        CloseCmdList(m_pCopyCommandList);

        std::vector<CommandList*> cmdLists;
        cmdLists.reserve(1);
        cmdLists.push_back(m_pCopyCommandList);

        if (m_HasGraphicsCommands)
        {
            VkSemaphore signaledSemaphore = GetDevice()->GetImpl()->ExecuteCommandListsWithSignalSemaphore(cmdLists, CommandQueue::Copy);

            CloseCmdList(m_pGraphicsCommandList);

            cmdLists.clear();
            cmdLists.push_back(m_pGraphicsCommandList);
            GetDevice()->GetImpl()->ExecuteCommandListsImmediate(cmdLists, CommandQueue::Graphics, signaledSemaphore, CommandQueue::Copy);
        }
        else
        {
            // Copy all immediate
            GetDevice()->ExecuteCommandListsImmediate(cmdLists, CommandQueue::Copy);
        }
    }

    void SetAllResourceViewHeaps(CommandList* pCmdList, ResourceViewAllocator* pAllocator /*=nullptr*/)
    {
        // Does nothing on Vulkan
    }

    void CloseCmdList(CommandList* pCmdList)
    {
        VkResult res = vkEndCommandBuffer(pCmdList->GetImpl()->VKCmdBuffer());
        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Failed to end recording into a command buffer");
    }

    void SetSubResourceRange(const GPUResource* pResource, VkImageMemoryBarrier& imageBarrier, uint32_t subResource)
    {
        CauldronAssert(ASSERT_CRITICAL, pResource->GetImpl()->GetResourceType() == ResourceType::Image, L"Only images support subresource.");
        imageBarrier.subresourceRange.aspectMask = GetImageAspectMask(pResource->GetImpl()->GetImageCreateInfo().format);
        if (subResource == 0xffffffff)
        {
            imageBarrier.subresourceRange.baseMipLevel   = 0;
            imageBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
            imageBarrier.subresourceRange.baseArrayLayer = 0;
            imageBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
        }
        else
        {
            VkImageCreateInfo createInfo = pResource->GetImpl()->GetImageCreateInfo();
            // For types that have both depth and stencil, we need to correct the aspect mask and re-index the sub resource.
            if (imageBarrier.subresourceRange.aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))  // depth + stencil
            {
                uint32_t numDepthSubResources = createInfo.mipLevels * createInfo.arrayLayers;
                if (subResource >= numDepthSubResources)  // stencil
                {
                    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
                    subResource -= numDepthSubResources;
                }
                else  // depth
                {
                    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                }
            }

            imageBarrier.subresourceRange.baseMipLevel   = subResource % createInfo.mipLevels;
            imageBarrier.subresourceRange.levelCount     = 1;
            imageBarrier.subresourceRange.baseArrayLayer = subResource / createInfo.mipLevels;
            imageBarrier.subresourceRange.layerCount     = 1;

            CauldronAssert(
                ASSERT_CRITICAL, imageBarrier.subresourceRange.baseMipLevel < createInfo.mipLevels, L"Subresource range is outside of the image range.");
            CauldronAssert(
                ASSERT_CRITICAL, imageBarrier.subresourceRange.baseArrayLayer < createInfo.arrayLayers, L"Subresource range is outside of the image range.");
        }
    }

    void ResourceBarrier(CommandList* pCmdList, uint32_t barrierCount, const Barrier* pBarriers)
    {
        std::vector<VkImageMemoryBarrier> imageBarriers;
        std::vector<VkBufferMemoryBarrier> bufferBarriers;

        Device* pDevice = GetDevice();

        for (uint32_t i = 0; i < barrierCount; ++i)
        {
            const Barrier barrier = pBarriers[i];

            if (barrier.Type == BarrierType::Transition)
            {
                CauldronAssert(ASSERT_CRITICAL, barrier.SourceState == barrier.pResource->GetCurrentResourceState(barrier.SubResource), L"ResourceBarrier::Error : ResourceState and Barrier.SourceState do not match.");
                if (barrier.pResource->GetImpl()->GetResourceType() == ResourceType::Buffer)
                {
                    VkBufferMemoryBarrier bufferBarrier;
                    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    bufferBarrier.pNext = nullptr;
                    bufferBarrier.srcAccessMask = ConvertToAccessMask(barrier.SourceState);
                    bufferBarrier.dstAccessMask = ConvertToAccessMask(barrier.DestState);
                    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    bufferBarrier.buffer = barrier.pResource->GetImpl()->GetBuffer();
                    bufferBarrier.offset = 0;
                    bufferBarrier.size = VK_WHOLE_SIZE;

                    bufferBarriers.push_back(bufferBarrier);
                }
                else
                {
                    if ((barrier.SourceState == ResourceState::Present || barrier.SourceState == g_UndefinedState)
                        && (barrier.DestState == ResourceState::PixelShaderResource
                            || barrier.DestState == ResourceState::NonPixelShaderResource
                            || barrier.DestState == (ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource)))
                    {
                        // Add an intermediate transition to get rid of the validation warning:
                        // we are transitioning from undefined state (which means the content of the texture is undefined) to a read state.
                        // Vulkan triggers a warning in this case.
                        // More case might exist. We will add them when we meet them.

                        VkImageMemoryBarrier imageBarrier;
                        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                        imageBarrier.pNext = nullptr;
                        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        SetSubResourceRange(barrier.pResource, imageBarrier, barrier.SubResource);
                        imageBarrier.image = barrier.pResource->GetImpl()->GetImage();

                        imageBarrier.srcAccessMask = 0;
                        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

                        VkImageUsageFlags usage = barrier.pResource->GetImpl()->GetImageCreateInfo().usage;
                        if ((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
                        {
                            imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                            imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        }
                        else if ((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
                        {
                            imageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                            imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                        }
                        else if ((usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0)
                        {
                            imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                            imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                        }
                        else if ((usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
                        {
                            imageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                            imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                        }
                        else
                        {
                            CauldronWarning(L"Unable to find an appropriate intermediate transition. Please support this case.");
                        }

                        // push intermediate barrier
                        imageBarriers.push_back(imageBarrier);

                        // push final barrier
                        imageBarrier.srcAccessMask = imageBarrier.dstAccessMask;
                        imageBarrier.dstAccessMask = ConvertToAccessMask(barrier.DestState);
                        imageBarrier.oldLayout = imageBarrier.newLayout;
                        imageBarrier.newLayout = ConvertToLayout(barrier.DestState);
                        imageBarriers.push_back(imageBarrier);
                    }
                    else
                    {
                        VkImageMemoryBarrier imageBarrier = {};
                        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                        imageBarrier.pNext = nullptr;
                        imageBarrier.srcAccessMask = ConvertToAccessMask(barrier.SourceState);
                        imageBarrier.dstAccessMask = ConvertToAccessMask(barrier.DestState);
                        imageBarrier.oldLayout = barrier.SourceState == ResourceState::Present ? VK_IMAGE_LAYOUT_UNDEFINED : ConvertToLayout(barrier.SourceState);
                        imageBarrier.newLayout = ConvertToLayout(barrier.DestState);
                        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        imageBarrier.subresourceRange.aspectMask = GetImageAspectMask(barrier.pResource->GetImpl()->GetImageCreateInfo().format);
                        SetSubResourceRange(barrier.pResource, imageBarrier, barrier.SubResource);
                        imageBarrier.image = barrier.pResource->GetImpl()->GetImage();

                        imageBarriers.push_back(imageBarrier);
                    }
                }

                // Set the new internal state (this is largely used for debugging
                const_cast<GPUResource*>(barrier.pResource)->SetCurrentResourceState(barrier.DestState, barrier.SubResource);
            }
            else  if (barrier.Type == BarrierType::UAV)
            {
                // Resource is expected to be in UAV state
                CauldronAssert(ASSERT_CRITICAL,
                               ResourceState::UnorderedAccess == barrier.pResource->GetCurrentResourceState(barrier.SubResource) ||
                                   ResourceState::RTAccelerationStruct == barrier.pResource->GetCurrentResourceState(barrier.SubResource),
                               L"ResourceBarrier::Error : ResourceState isn't UnorderedAccess or RTAccelerationStruct.");

                if (barrier.pResource->GetImpl()->GetResourceType() == ResourceType::Image)
                {
                    VkFormat imageFormat = barrier.pResource->GetImpl()->GetImageCreateInfo().format;

                    VkImageMemoryBarrier imageBarrier = {};
                    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    imageBarrier.pNext = nullptr;
                    imageBarrier.srcAccessMask = ConvertToAccessMask(barrier.SourceState); // Is this really needed for a UAV barrier? Remove if it's ignored
                    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                    imageBarrier.oldLayout = barrier.SourceState == ResourceState::Present ? VK_IMAGE_LAYOUT_UNDEFINED : ConvertToLayout(barrier.SourceState);
                    imageBarrier.newLayout = ConvertToLayout(barrier.DestState);
                    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    imageBarrier.subresourceRange.aspectMask = GetImageAspectMask(imageFormat);
                    SetSubResourceRange(barrier.pResource, imageBarrier, barrier.SubResource);
                    imageBarrier.image = barrier.pResource->GetImpl()->GetImage();

                    imageBarriers.push_back(imageBarrier);
                }
                else if (barrier.pResource->GetImpl()->GetResourceType() == ResourceType::Buffer)
                {
                    VkBufferMemoryBarrier bufferBarrier;
                    bufferBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    bufferBarrier.pNext               = nullptr;
                    bufferBarrier.srcAccessMask       = ConvertToAccessMask(barrier.SourceState);  // Is this really needed for a UAV barrier? Remove if it's ignored
                    bufferBarrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    bufferBarrier.buffer              = barrier.pResource->GetImpl()->GetBuffer();
                    bufferBarrier.offset              = 0;
                    bufferBarrier.size                = VK_WHOLE_SIZE;

                    bufferBarriers.push_back(bufferBarrier);
                }
            }
            else
            {
                CauldronError(L"Unsupported barrier");
            }
        }

        if (bufferBarriers.size() > 0 || imageBarriers.size() > 0)
        {
            uint32_t srcStageMask = VK_PIPELINE_STAGE_NONE;
            uint32_t dstStageMask = VK_PIPELINE_STAGE_NONE;
            switch (pCmdList->GetQueueType())
            {
            case CommandQueue::Graphics:
                srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
                dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
                break;
            case CommandQueue::Compute:
                srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                break;
            case CommandQueue::Copy:
                srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;
            }
            vkCmdPipelineBarrier(
                pCmdList->GetImpl()->VKCmdBuffer(),
                srcStageMask, dstStageMask, 0,
                0, nullptr,
                static_cast<uint32_t>(bufferBarriers.size()),
                bufferBarriers.data(),
                static_cast<uint32_t>(imageBarriers.size()),
                imageBarriers.data());
        }
    }

    void CopyTextureRegion(CommandList* pCmdList, const TextureCopyDesc* pCopyDesc)
    {
        if (pCopyDesc->GetImpl()->IsSourceTexture)
        {
            if (pCopyDesc->GetImpl()->ImageCopy.srcSubresource.aspectMask == pCopyDesc->GetImpl()->ImageCopy.dstSubresource.aspectMask)
            {
                // NOTE: we don't handle multiplanar formats
                vkCmdCopyImage(pCmdList->GetImpl()->VKCmdBuffer(),
                               pCopyDesc->GetImpl()->SrcImage,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               pCopyDesc->GetImpl()->DstImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &(pCopyDesc->GetImpl()->ImageCopy));
            }
            else
            {
                CauldronAssert(ASSERT_CRITICAL,
                               pCopyDesc->GetImpl()->ImageCopy.srcSubresource.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT,
                               L"Unsupported texture copy type across 2 aspects. Source image should be VK_IMAGE_ASPECT_DEPTH_BIT");
                CauldronAssert(ASSERT_CRITICAL,
                               pCopyDesc->GetImpl()->SrcImageFormat == VK_FORMAT_D32_SFLOAT,
                               L"Unsupported texture copy type across 2 aspects. Source image should be VK_FORMAT_D32_SFLOAT");
                CauldronAssert(ASSERT_CRITICAL,
                               pCopyDesc->GetImpl()->ImageCopy.dstSubresource.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT,
                               L"Unsupported texture copy type across 2 aspects. Destination image should be VK_IMAGE_ASPECT_COLOR_BIT");
                CauldronAssert(ASSERT_CRITICAL,
                               pCopyDesc->GetImpl()->DstImageFormat == VK_FORMAT_R32_SFLOAT,
                               L"Unsupported texture copy type across 2 aspects. Destination image should be VK_FORMAT_R32_SFLOAT");

                VkDeviceSize      totalSize      = 4 * pCopyDesc->GetImpl()->ImageCopy.extent.width * pCopyDesc->GetImpl()->ImageCopy.extent.height;
                BufferAddressInfo copyBufferInfo = GetDevice()->GetImpl()->GetDepthToColorCopyBuffer(totalSize);

                VkBufferImageCopy bufferImageCopy;
                bufferImageCopy.bufferOffset      = copyBufferInfo.GetImpl()->Offset;
                bufferImageCopy.bufferRowLength   = 0;
                bufferImageCopy.bufferImageHeight = 0;
                bufferImageCopy.imageSubresource  = pCopyDesc->GetImpl()->ImageCopy.srcSubresource;
                bufferImageCopy.imageOffset       = pCopyDesc->GetImpl()->ImageCopy.srcOffset;
                bufferImageCopy.imageExtent       = pCopyDesc->GetImpl()->ImageCopy.extent;

                vkCmdCopyImageToBuffer(pCmdList->GetImpl()->VKCmdBuffer(),
                                       pCopyDesc->GetImpl()->SrcImage,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       copyBufferInfo.GetImpl()->Buffer,
                                       1,
                                       &bufferImageCopy);

                VkBufferMemoryBarrier bufferBarrier = {};
                bufferBarrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                bufferBarrier.pNext                 = nullptr;
                bufferBarrier.srcAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
                bufferBarrier.dstAccessMask         = VK_ACCESS_TRANSFER_READ_BIT;
                bufferBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.buffer                = copyBufferInfo.GetImpl()->Buffer;
                bufferBarrier.offset                = 0;
                bufferBarrier.size                  = copyBufferInfo.GetImpl()->SizeInBytes;
                vkCmdPipelineBarrier(pCmdList->GetImpl()->VKCmdBuffer(),
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     1,
                                     &bufferBarrier,
                                     0,
                                     nullptr);

                bufferImageCopy.imageSubresource = pCopyDesc->GetImpl()->ImageCopy.dstSubresource;
                bufferImageCopy.imageOffset      = pCopyDesc->GetImpl()->ImageCopy.dstOffset;
                bufferImageCopy.imageExtent      = pCopyDesc->GetImpl()->ImageCopy.extent;

                vkCmdCopyBufferToImage(pCmdList->GetImpl()->VKCmdBuffer(),
                                       copyBufferInfo.GetImpl()->Buffer,
                                       pCopyDesc->GetImpl()->DstImage,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       1,
                                       &bufferImageCopy);
            }
        }
        else
        {
            vkCmdCopyBufferToImage(pCmdList->GetImpl()->VKCmdBuffer(),
                                   pCopyDesc->GetImpl()->SrcBuffer,
                                   pCopyDesc->GetImpl()->DstImage,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1, &pCopyDesc->GetImpl()->Region);
        }
    }

    void CopyBufferRegion(CommandList* pCmdList, const BufferCopyDesc* pCopyDesc)
    {
        vkCmdCopyBuffer(pCmdList->GetImpl()->VKCmdBuffer(),
                        pCopyDesc->GetImpl()->SrcBuffer,
                        pCopyDesc->GetImpl()->DstBuffer,
                        1, &pCopyDesc->GetImpl()->Region);
    }

    void ClearRenderTarget(CommandList* pCmdList, const ResourceViewInfo* pRendertargetView, const float clearColor[4])
    {
        #pragma message("transition the view into general state for now. Assume this is in a render target state.")

        VkImageMemoryBarrier imageBarrier = {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.pNext = nullptr;
        imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange.baseMipLevel = 0;
        imageBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        imageBarrier.image = pRendertargetView->GetImpl()->Image.Image;

        vkCmdPipelineBarrier(
            pCmdList->GetImpl()->VKCmdBuffer(),
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &imageBarrier);


        VkClearColorValue clearValue = {};
        clearValue.float32[0] = clearColor[0];
        clearValue.float32[1] = clearColor[1];
        clearValue.float32[2] = clearColor[2];
        clearValue.float32[3] = clearColor[3];

        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        vkCmdClearColorImage(pCmdList->GetImpl()->VKCmdBuffer(), pRendertargetView->GetImpl()->Image.Image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &range);

        imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        vkCmdPipelineBarrier(
            pCmdList->GetImpl()->VKCmdBuffer(),
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &imageBarrier);
    }

    void ClearDepthStencil(CommandList* pCmdList, const ResourceViewInfo* pDepthStencilView, uint8_t stencilValue)
    {
        #pragma message("transition the view into general state for now. Assume this is in a depth write state.")

        VkImageMemoryBarrier imageBarrier = {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.pNext = nullptr;
        imageBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.subresourceRange.aspectMask = GetImageAspectMask(pDepthStencilView->GetImpl()->Image.Format);
        imageBarrier.subresourceRange.baseMipLevel = 0;
        imageBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        imageBarrier.image = pDepthStencilView->GetImpl()->Image.Image;

        vkCmdPipelineBarrier(
            pCmdList->GetImpl()->VKCmdBuffer(),
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &imageBarrier);


        static bool s_InvertedDepth = GetConfig()->InvertedDepth;
        VkClearDepthStencilValue clearValue = {};
        clearValue.depth = s_InvertedDepth ? 0.0f : 1.0f;
        clearValue.stencil = static_cast<uint32_t>(stencilValue);

        VkImageSubresourceRange range = {};
        range.aspectMask = imageBarrier.subresourceRange.aspectMask;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        vkCmdClearDepthStencilImage(pCmdList->GetImpl()->VKCmdBuffer(), pDepthStencilView->GetImpl()->Image.Image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &range);

        imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        vkCmdPipelineBarrier(
            pCmdList->GetImpl()->VKCmdBuffer(),
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &imageBarrier);
    }

    void ClearUAVFloat(CommandList* pCmdList, const GPUResource* pResource, const ResourceViewInfo* pGPUView, const ResourceViewInfo* pCPUView, float clearColor[4])
    {
        CauldronAssert(ASSERT_CRITICAL, pResource->IsTexture(), L"ClearUAVFloat can only be called on textures");
        VkFormat format = pResource->GetImpl()->GetImageCreateInfo().format;
        CauldronAssert(ASSERT_CRITICAL, !(IsDepthFormat(format) || HasStencilComponent(format)), L"Clear of depth/stencil texture UAV not supported");
        if (pGPUView->GetImpl()->Type == ResourceViewType::TextureUAV)
        {
            VkImageSubresourceRange range;
            range.aspectMask     = GetImageAspectMask(pResource->GetImpl()->GetImageCreateInfo().format);
            range.baseArrayLayer = 0;
            range.baseMipLevel   = 0;
            range.layerCount     = pResource->GetImpl()->GetImageCreateInfo().arrayLayers;
            range.levelCount     = pResource->GetImpl()->GetImageCreateInfo().mipLevels;
            // Assume the image is in the correct layout
            VkClearColorValue clearColorValue;
            memcpy(clearColorValue.float32, clearColor, 4 * sizeof(float));
            vkCmdClearColorImage(pCmdList->GetImpl()->VKCmdBuffer(), pResource->GetImpl()->GetImage(), VK_IMAGE_LAYOUT_GENERAL, &clearColorValue, 1, &range);
        }
        else if (pResource->IsBuffer())
        {
            CauldronAssert(
                ASSERT_CRITICAL,
                (pResource->GetImpl()->GetBufferCreateInfo().usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0,
                L"Cannot call ClearUAVFloat on a buffer that doesn't have VK_BUFFER_USAGE_TRANSFER_DST_BIT flag");
            vkCmdFillBuffer(pCmdList->GetImpl()->VKCmdBuffer(), pResource->GetImpl()->GetBuffer(), 0, VK_WHOLE_SIZE, static_cast<uint32_t>(clearColor[0]));
        }
        else
        {
            CauldronCritical(L"Cannot call ClearUAVFloat on this type of resource or resource view");
        }
    }

    void ClearUAVUInt(CommandList* pCmdList, const GPUResource* pResource, const ResourceViewInfo* pGPUView, const ResourceViewInfo* pCPUView, uint32_t clearColor[4])
    {
        CauldronCritical(L"Not yet implemented");
    }

    void SetPipelineState(CommandList* pCmdList, PipelineObject* pPipeline)
    {
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
        switch (pPipeline->GetPipelineType())
        {
        case PipelineType::Graphics:
            bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            break;
        case PipelineType::Compute:
            bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
            break;
        default:
            CauldronError(L"Unknown pipeline type");
            break;
        }

        vkCmdBindPipeline(pCmdList->GetImpl()->VKCmdBuffer(), bindPoint, pPipeline->GetImpl()->VKPipeline());
    }

    void SetPrimitiveTopology(CommandList* pCmdList, PrimitiveTopology topology)
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        pDevice->GetCmdSetPrimitiveTopology()(pCmdList->GetImpl()->VKCmdBuffer(), ConvertTopology(topology));
    }

    void SetVertexBuffers(CommandList* pCmdList, uint32_t startSlot, uint32_t numBuffers, BufferAddressInfo* pVertexBufferView)
    {
        // vkCmdBindVertexBuffers shouldn't be called if there is no buffer
        if (numBuffers == 0)
            return;

        VkBuffer buffers[8];
        VkDeviceSize offsets[8];
        for (uint32_t i = 0; i < numBuffers; ++i)
        {
            buffers[i] = pVertexBufferView[i].GetImpl()->Buffer;
            offsets[i] = pVertexBufferView[i].GetImpl()->Offset;
        }
        vkCmdBindVertexBuffers(pCmdList->GetImpl()->VKCmdBuffer(), startSlot, numBuffers, buffers, offsets);
    }

    void SetIndexBuffer(CommandList* pCmdList, BufferAddressInfo* pIndexBufferView)
    {
        vkCmdBindIndexBuffer(pCmdList->GetImpl()->VKCmdBuffer(), pIndexBufferView->GetImpl()->Buffer, pIndexBufferView->GetImpl()->Offset, pIndexBufferView->GetImpl()->IndexType);
    }

    void SetRenderTargets(CommandList* pCmdList, uint32_t numRasterViews, const ResourceViewInfo* pRasterViews, const ResourceViewInfo* pDepthView /*= nullptr*/)
    {
        CauldronCritical(L"Not yet implemented");
    }

    void BeginRaster(CommandList*                   pCmdList,
                     uint32_t                       numRasterViews,
                     const RasterView**             pRasterViews,
                     const RasterView*              pDepthView,
                     const VariableShadingRateInfo* pVrsInfo)
    {
        constexpr uint32_t cMaxViews = 8;
        CauldronAssert(ASSERT_WARNING, numRasterViews <= cMaxViews, L"Cannot set more than %d render targets.", cMaxViews);

        ResourceViewInfo colorViews[cMaxViews];
        for (uint32_t i = 0; i < numRasterViews; ++i)
            colorViews[i] = pRasterViews[i]->GetResourceView();

        ResourceViewInfo depthView;
        if (pDepthView != nullptr)
            depthView = pDepthView->GetResourceView();

        BeginRaster(pCmdList, numRasterViews, colorViews, pDepthView == nullptr ? nullptr : &depthView, pVrsInfo);
    }

    void BeginRaster(CommandList*                   pCmdList,
                     uint32_t                       numColorViews,
                     const ResourceViewInfo*        pColorViews,
                     const ResourceViewInfo*        pDepthView,
                     const VariableShadingRateInfo* pVrsInfo)
    {
        CauldronAssert(ASSERT_WARNING, !pCmdList->GetRastering(), L"Calling BeginRaster before previous EndRaster. Strangeness or crashes may occur.");
        CauldronAssert(ASSERT_WARNING, numColorViews <= 8, L"Cannot set more than 8 render targets.");

        if (pVrsInfo != nullptr)
        {
            pCmdList->BeginVRSRendering(pVrsInfo);
        }
        else
        {
            ShadingRateCombiner combiner[] = {ShadingRateCombiner::ShadingRateCombiner_Passthrough, ShadingRateCombiner::ShadingRateCombiner_Passthrough};
            SetShadingRate(pCmdList, ShadingRate::ShadingRate_1X1, combiner);
        }

        VkRenderingInfo renderingInfo     = {};
        renderingInfo.sType               = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.pNext               = nullptr;
        renderingInfo.flags               = 0;
        renderingInfo.renderArea.offset.x = 0;
        renderingInfo.renderArea.offset.y = 0;

        // renderArea must be smaller than the smallest RT
        uint32_t min_width  = 10000000;
        uint32_t min_height = 10000000;
        if (pDepthView != nullptr)
        {
            min_width  = pDepthView->GetImpl()->Image.Width;
            min_height = pDepthView->GetImpl()->Image.Height;
        }
        for (uint32_t i = 0; i < numColorViews; ++i)
        {
            uint32_t rt_width  = pColorViews[0].GetImpl()->Image.Width;
            uint32_t rt_height = pColorViews[0].GetImpl()->Image.Height;
            if (rt_width < min_width)
                min_width = rt_width;
            if (rt_height < min_height)
                min_height = rt_height;
        }
        renderingInfo.renderArea.extent.width  = min_width;
        renderingInfo.renderArea.extent.height = min_height;
        renderingInfo.layerCount               = 1;
        renderingInfo.viewMask                 = 0;
        renderingInfo.colorAttachmentCount     = numColorViews;

        VkRenderingAttachmentInfo colorAttachments[8];
        for (uint32_t i = 0; i < numColorViews; ++i)
        {
            VkRenderingAttachmentInfo& attachment = colorAttachments[i];
            attachment.sType                      = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            attachment.pNext                      = nullptr;
            attachment.imageView                  = pColorViews[i].GetImpl()->Image.View;
            attachment.imageLayout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.resolveMode                = VK_RESOLVE_MODE_NONE;
            attachment.resolveImageView           = VK_NULL_HANDLE;
            attachment.resolveImageLayout         = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.loadOp                     = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp                    = VK_ATTACHMENT_STORE_OP_STORE;
            //attachment.clearValue = ;
        }
        renderingInfo.pColorAttachments = colorAttachments;

        VkRenderingAttachmentInfo depthStencilAttachment = {};
        bool                      hasStencil             = false;
        if (pDepthView != nullptr)
        {
            hasStencil = HasStencilComponent(pDepthView->GetImpl()->Image.Format);

            depthStencilAttachment.sType     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthStencilAttachment.pNext     = nullptr;
            depthStencilAttachment.imageView = pDepthView->GetImpl()->Image.View;

            depthStencilAttachment.resolveMode      = VK_RESOLVE_MODE_NONE;
            depthStencilAttachment.resolveImageView = VK_NULL_HANDLE;
            depthStencilAttachment.loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
            depthStencilAttachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
            //depthStencilAttachment.clearValue = ;

            if (hasStencil)
            {
                depthStencilAttachment.imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depthStencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                renderingInfo.pStencilAttachment          = &depthStencilAttachment;
            }
            else
            {
                depthStencilAttachment.imageLayout        = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                depthStencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            }

            renderingInfo.pDepthAttachment = &depthStencilAttachment;
        }

        if (pVrsInfo != nullptr && pVrsInfo->VariableShadingMode > VariableShadingMode::VariableShadingMode_Per_Draw)
        {
            VkRenderingFragmentShadingRateAttachmentInfoKHR shadingRateInfo = {};

            const RasterView* shadingRateImageView         = GetRasterViewAllocator()->RequestRasterView(pVrsInfo->pShadingRateImage, ViewDimension::Texture2D);
            shadingRateInfo.sType                          = VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR;
            shadingRateInfo.pNext                          = nullptr;
            shadingRateInfo.imageLayout                    = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
            shadingRateInfo.imageView                      = shadingRateImageView->GetResourceView().GetImpl()->Image.View;
            shadingRateInfo.shadingRateAttachmentTexelSize = {pVrsInfo->ShadingRateTileWidth, pVrsInfo->ShadingRateTileHeight};

            renderingInfo.pNext = &shadingRateInfo;
        }

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        pDevice->GetCmdBeginRenderingKHR()(pCmdList->GetImpl()->VKCmdBuffer(), &renderingInfo);

        // Flag that we are currently doing raster ops
        pCmdList->SetRastering(true);
    }

    void EndRaster(CommandList* pCmdList, const VariableShadingRateInfo* pVrsInfo)
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        pDevice->GetCmdEndRenderingKHR()(pCmdList->GetImpl()->VKCmdBuffer());

        // Done with raster ops
        pCmdList->SetRastering(false);

        if (pVrsInfo != nullptr)
            pCmdList->EndVRSRendering(pVrsInfo);
    }

    void SetViewport(CommandList* pCmdList, Viewport* pViewport)
    {
        VkViewport viewport = {};
        viewport.x = pViewport->X;
        viewport.y = pViewport->Height + pViewport->Y;
        viewport.width = pViewport->Width;
        viewport.height = -pViewport->Height;
        viewport.minDepth = pViewport->MinDepth;
        viewport.maxDepth = pViewport->MaxDepth;
        vkCmdSetViewport(pCmdList->GetImpl()->VKCmdBuffer(), 0, 1, &viewport);
    }

    void SetScissorRects(CommandList* pCmdList, uint32_t numRects, Rect* pRectList)
    {
        // up to 8 scissors
        CauldronAssert(ASSERT_ERROR, numRects <= 8, L"Cannot set more than 8 scissors sets");
        VkRect2D scissors[8];
        for (uint32_t i = 0; i < numRects; ++i)
        {
            scissors[i].offset.x = pRectList[i].Left;
            scissors[i].offset.y = pRectList[i].Top;
            scissors[i].extent.width = pRectList[i].Right - pRectList[i].Left;
            scissors[i].extent.height = pRectList[i].Bottom - pRectList[i].Top;
        }
        vkCmdSetScissor(pCmdList->GetImpl()->VKCmdBuffer(), 0, numRects, scissors);
    }

    void SetViewportScissorRect(CommandList* pCmdList, uint32_t left, uint32_t top, uint32_t width, uint32_t height, float nearDist, float farDist)
    {
        Viewport vp = {static_cast<float>(left), static_cast<float>(top), static_cast<float>(width), static_cast<float>(height), nearDist, farDist};
        SetViewport(pCmdList, &vp);
        Rect scissorRect = {0, 0, width, height};
        SetScissorRects(pCmdList, 1, &scissorRect);
    }

    void DrawInstanced(CommandList* pCmdList, uint32_t vertexCountPerInstance, uint32_t instanceCount/*=1*/, uint32_t startVertex/*=0*/, uint32_t startInstance/*=0*/)
    {
        vkCmdDraw(pCmdList->GetImpl()->VKCmdBuffer(), vertexCountPerInstance, instanceCount, startVertex, startInstance);
    }

    void DrawIndexedInstanced(CommandList* pCmdList, uint32_t indexCountPerInstance, uint32_t instanceCount/*=1*/, uint32_t startIndex/*=0*/, uint32_t baseVertex/*=0*/, uint32_t startInstance/*=0*/)
    {
        vkCmdDrawIndexed(pCmdList->GetImpl()->VKCmdBuffer(), indexCountPerInstance, instanceCount, startIndex, baseVertex, startInstance);
    }

    void ExecuteIndirect(CommandList* pCmdList, IndirectWorkload* pIndirectWorkload, const Buffer* pArgumentBuffer, uint32_t drawCount, uint32_t offset)
    {
        IndirectWorkloadInternal* pIndirectWorkloadInternal = static_cast<IndirectWorkloadInternal*>(pIndirectWorkload);
        IndirectCommandType& commantType                    = pIndirectWorkloadInternal->m_type;
        uint32_t stride                                     = pIndirectWorkloadInternal->m_stride;

        BufferAddressInfo addressInfo = pArgumentBuffer->GetAddressInfo();

        switch (commantType)
        {
        case IndirectCommandType::Draw:
            vkCmdDrawIndirect(pCmdList->GetImpl()->VKCmdBuffer(), addressInfo.GetImpl()->Buffer, offset, drawCount, stride);
            break;
        case IndirectCommandType::DrawIndexed:
            vkCmdDrawIndexedIndirect(pCmdList->GetImpl()->VKCmdBuffer(), addressInfo.GetImpl()->Buffer, offset, drawCount, stride);
            break;
        case IndirectCommandType::Dispatch:
            vkCmdDispatchIndirect(pCmdList->GetImpl()->VKCmdBuffer(), addressInfo.GetImpl()->Buffer, offset);
            break;
        default:
            CauldronWarning(L"Unsupported command type for indirect workload.");
            return;
        }
    }

    void Dispatch(CommandList* pCmdList, uint32_t numGroupX, uint32_t numGroupY, uint32_t numGroupZ)
    {
        CauldronAssert(ASSERT_CRITICAL, numGroupX && numGroupY && numGroupZ, L"One of the dispatch group sizes is 0. Please ensure at least 1 group per dispatch dimension.");
        vkCmdDispatch(pCmdList->GetImpl()->VKCmdBuffer(), numGroupX, numGroupY, numGroupZ);
    }

    void WriteBufferImmediate(CommandList* pCmdList, const GPUResource* pResource, uint32_t numParams, const uint32_t* offsets, const uint32_t* values)
    {
        uint32_t lastIndex = 0;
        for (uint32_t i = 1; i <= numParams; ++i) // we are going one index too far to execute vkCmdUpdateBuffer for the last batch
        {
            if (i == numParams || (offsets[i] != offsets[i - 1] + static_cast<uint32_t>(sizeof(uint32_t))))
            {
                vkCmdUpdateBuffer(pCmdList->GetImpl()->VKCmdBuffer(), pResource->GetImpl()->GetBuffer(), static_cast<VkDeviceSize>(offsets[lastIndex]), static_cast<VkDeviceSize>(sizeof(uint32_t) * (i - lastIndex)), &values[lastIndex]);
                lastIndex = i;
            }
        }
    }

    void WriteBreadcrumbsMarker(Device* pDevice, CommandList* pCmdList, Buffer* pBuffer, uint64_t gpuAddress, uint32_t value, bool isBegin)
    {
        if (pDevice->FeatureSupported(DeviceFeature::BufferMarkerAMD))
        {
            if (pDevice->FeatureSupported(DeviceFeature::ExtendedSync))
            {
                pDevice->GetImpl()->GetCmdWriteBufferMarker2AMD()(pCmdList->GetImpl()->VKCmdBuffer(),
                    isBegin ? VK_PIPELINE_STAGE_2_NONE_KHR : VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
                    pBuffer->GetResource()->GetImpl()->GetBuffer(), gpuAddress, value);
            }
            else
            {
                pDevice->GetImpl()->GetCmdWriteBufferMarkerAMD()(pCmdList->GetImpl()->VKCmdBuffer(),
                    isBegin ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    pBuffer->GetResource()->GetImpl()->GetBuffer(), gpuAddress, value);
            }
        }
        else
        {
            pDevice->GetImpl()->GetCmdFillBuffer()(pCmdList->GetImpl()->VKCmdBuffer(),
                pBuffer->GetResource()->GetImpl()->GetBuffer(), gpuAddress, sizeof(uint32_t), value);
        }
    }

    static VkFragmentShadingRateCombinerOpKHR GetVKShadingRateCombiner(ShadingRateCombiner combiner)
    {
        VkFragmentShadingRateCombinerOpKHR vkCombiner;
        switch (combiner)
        {
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Passthrough:
            vkCombiner = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
            break;
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Override:
            vkCombiner = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR;
            break;
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Min:
            vkCombiner = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_KHR;
            break;
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Max:
            vkCombiner = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR;
            break;
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Sum:
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Mul:
            vkCombiner = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MUL_KHR;
            break;
        default:
            CauldronCritical(L"Unknown shading rate combiner.");
            break;
        }

        return vkCombiner;
    }

    static VkExtent2D GetShadingRateExtent(ShadingRate shadingRate)
    {
        VkExtent2D extent = {1, 1};
        switch (shadingRate)
        {
        case ShadingRate::ShadingRate_1X1:
            break;
        case ShadingRate::ShadingRate_1X2:
            extent.height = 2;
            break;
        case ShadingRate::ShadingRate_2X1:
            extent.width = 2;
            break;
        case ShadingRate::ShadingRate_2X2:
            extent.width  = 2;
            extent.height = 2;
            break;
        case ShadingRate::ShadingRate_2X4:
            extent.width  = 2;
            extent.height = 4;
            break;
        case ShadingRate::ShadingRate_4X2:
            extent.width  = 4;
            extent.height = 2;
            break;
        case ShadingRate::ShadingRate_4X4:
            extent.width  = 4;
            extent.height = 4;
            break;
        default:
            CauldronCritical(L"Unknown shading rate.");
            break;
        }
        return extent;
    }

    void SetShadingRate(CommandList* pCmdList, const ShadingRate shadingRate, const ShadingRateCombiner* combiners, const GPUResource* pShadingRateImage)
    {
        VkExtent2D                         fragmentSize = GetShadingRateExtent(shadingRate);
        VkFragmentShadingRateCombinerOpKHR vulkanCombiners[2];
        vulkanCombiners[0] = GetVKShadingRateCombiner(combiners[0]);
        vulkanCombiners[1] = GetVKShadingRateCombiner(combiners[1]);

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        pDevice->GetCmdSetFragmentShadingRateKHR()(pCmdList->GetImpl()->VKCmdBuffer(), &fragmentSize, vulkanCombiners);
    }
} // namespace cauldron

#endif // #if defined(_VK)
