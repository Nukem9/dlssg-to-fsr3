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

#include "render/device.h"
#include "render/buffer.h"
#include "misc/threadsafe_queue.h"
#include "memoryallocator/memoryallocator.h"

#include <queue>
#include <mutex>
#include <vector>

#include <ffx_api/vk/ffx_api_vk.h>
#include <vk/ffx_vk.h>

namespace cauldron
{
    struct FIQueue
    {
        VkQueue        queue      = VK_NULL_HANDLE;
        uint32_t       family     = 0;
        uint32_t       index      = 0;
    };

    struct SwapChainCreationParams
    {
        VkSwapchainCreateInfoKHR swapchainCreateInfo;
    };

    class DeviceInternal final : public Device
    {
    public:
        virtual ~DeviceInternal();

        virtual void GetFeatureInfo(DeviceFeature feature, void* pFeatureInfo) override;
        void FlushQueue(CommandQueue queueType) override;
        virtual uint64_t QueryPerformanceFrequency(CommandQueue queueType) override;

        virtual CommandList* CreateCommandList(const wchar_t* name, CommandQueue queueType) override;

        // For swapchain creation
        virtual void CreateSwapChain(SwapChain*& pSwapChain, const SwapChainCreationParams& params, CommandQueue queueType) override;

        // For swapchain present and signaling (for synchronization)
        virtual uint64_t PresentSwapChain(SwapChain* pSwapChain) override;

        virtual void WaitOnQueue(uint64_t waitValue, CommandQueue queueType) const override;
        virtual uint64_t QueryLastCompletedValue(CommandQueue queueType) override;
        virtual uint64_t SignalQueue(CommandQueue queueType) override;

        virtual uint64_t ExecuteCommandLists(std::vector<CommandList*>& cmdLists, CommandQueue queueType, bool isFirstSubmissionOfFrame = false, bool isLastSubmissionOfFrame = false) override;
        virtual void ExecuteCommandListsImmediate(std::vector<CommandList*>& cmdLists, CommandQueue queueType) override;

        virtual void ExecuteResourceTransitionImmediate(uint32_t barrierCount, const Barrier* pBarriers) override;
        virtual void ExecuteTextureResourceCopyImmediate(uint32_t resourceCopyCount, const TextureCopyDesc* pCopyDescs) override;

        // Vulkan only methods
        // used to transition on any queue. Use it only when necessary
        void ExecuteResourceTransitionImmediate(CommandQueue queueType, uint32_t barrierCount, const Barrier* pBarriers);
        VkSemaphore ExecuteCommandListsWithSignalSemaphore(std::vector<CommandList*>& cmdLists, CommandQueue queueType);
        void ExecuteCommandListsImmediate(std::vector<CommandList*>& cmdLists, CommandQueue queueType, VkSemaphore waitSemaphore, CommandQueue waitQueueType);
        uint64_t ExecuteCommandLists(std::vector<CommandList*>& cmdLists, CommandQueue queueType, VkSemaphore waitSemaphore);

        uint64_t GetLatestSemaphoreValue(CommandQueue queueType);

        void ReleaseCommandPool(CommandList* pCmdList);

        const VkDevice VKDevice() const { return m_Device; }
        VkDevice VKDevice() { return m_Device; }
        const VkPhysicalDevice VKPhysicalDevice() const { return m_PhysicalDevice; }
        VmaAllocator GetVmaAllocator() const { return m_VmaAllocator; }
        VkSampler GetDefaultSampler() const { return m_DefaultSampler; }
        BufferAddressInfo GetDepthToColorCopyBuffer(VkDeviceSize size);

        const VkQueue VKCmdQueue(CommandQueue queueType) const { return m_QueueSyncPrims[static_cast<int32_t>(queueType)].GetQueue(); }
        VkQueue VKCmdQueue(CommandQueue queueType) { return m_QueueSyncPrims[static_cast<int32_t>(queueType)].GetQueue(); }
        const uint32_t VKCmdQueueFamily(CommandQueue queueType) const
        {
            return m_QueueSyncPrims[static_cast<int32_t>(queueType)].GetQueueFamily();
        }

        VkSurfaceKHR GetSurface() { return m_Surface; }

        void SetResourceName(VkObjectType objectType, uint64_t handle, const char* name);
        void SetResourceName(VkObjectType objectType, uint64_t handle, const wchar_t* name);

        const uint32_t GetMinAccelerationStructureScratchOffsetAlignment() { return m_MinAccelerationStructureScratchOffsetAlignment; }
        const uint32_t GetBreadcrumbsMemoryIndex() { return m_BreadcrumbsMemoryIndex; }
        const bool     BreadcrumbsDedicatedAllocRequired() { return m_UseBreadcrumbsDedicatedAlloc; }

        virtual const DeviceInternal* GetImpl() const override { return this; }
        virtual DeviceInternal* GetImpl() override { return this; }

        // extensions
        PFN_vkCmdSetPrimitiveTopologyEXT    GetCmdSetPrimitiveTopology()         const { return m_vkCmdSetPrimitiveTopologyEXT;   }
        PFN_vkCmdBeginDebugUtilsLabelEXT    GetCmdBeginDebugUtilsLabel()         const { return m_vkCmdBeginDebugUtilsLabelEXT;   }
        PFN_vkCmdEndDebugUtilsLabelEXT      GetCmdEndDebugUtilsLabel()           const { return m_vkCmdEndDebugUtilsLabelEXT;     }
        PFN_vkCmdBeginRenderingKHR          GetCmdBeginRenderingKHR()            const { return m_vkCmdBeginRenderingKHR;         }
        PFN_vkCmdEndRenderingKHR            GetCmdEndRenderingKHR()              const { return m_vkCmdEndRenderingKHR;           }
        PFN_vkCmdSetFragmentShadingRateKHR  GetCmdSetFragmentShadingRateKHR() const { return m_vkCmdSetFragmentShadingRateKHR; }
        PFN_vkGetAccelerationStructureBuildSizesKHR GetAccelerationStructureBuildSizesKHR() const { return m_vkGetAccelerationStructureBuildSizesKHR; }
        PFN_vkCreateAccelerationStructureKHR GetCreateAccelerationStructureKHR() const { return m_vkCreateAccelerationStructureKHR; }
        PFN_vkDestroyAccelerationStructureKHR GetDestroyAccelerationStructureKHR() const { return m_vkDestroyAccelerationStructureKHR; }
        PFN_vkGetAccelerationStructureDeviceAddressKHR GetGetAccelerationStructureDeviceAddressKHR() const{ return m_vkGetAccelerationStructureDeviceAddressKHR; }
        PFN_vkCmdBuildAccelerationStructuresKHR GetCmdBuildAccelerationStructuresKHR() const { return m_vkCmdBuildAccelerationStructuresKHR; }

        PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR GetPhysicalDeviceSurfaceCapabilities2KHR() const { return m_vkGetPhysicalDeviceSurfaceCapabilities2KHR; }
        PFN_vkGetPhysicalDeviceSurfaceFormats2KHR      GetPhysicalDeviceSurfaceFormats2()         const { return m_vkGetPhysicalDeviceSurfaceFormats2KHR; }
        PFN_vkSetHdrMetadataEXT                        GetSetHdrMetadata()                        const { return m_vkSetHdrMetadataEXT; }
        PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR GetPhysicalDeviceFragmentShadingRatesKHR() const { return m_vkGetPhysicalDeviceFragmentShadingRatesKHR; }

        PFN_vkCreateBuffer             GetCreateBuffer()             const { return m_vkCreateBuffer; }
        PFN_vkAllocateMemory           GetAllocateMemory()           const { return m_vkAllocateMemory; }
        PFN_vkBindBufferMemory         GetBindBufferMemory()         const { return m_vkBindBufferMemory; }
        PFN_vkMapMemory                GetMapMemory()                const { return m_vkMapMemory; }
        PFN_vkCmdFillBuffer            GetCmdFillBuffer()            const { return m_vkCmdFillBuffer; }
        PFN_vkCmdWriteBufferMarkerAMD  GetCmdWriteBufferMarkerAMD()  const { return m_vkCmdWriteBufferMarkerAMD; }
        PFN_vkCmdWriteBufferMarker2AMD GetCmdWriteBufferMarker2AMD() const { return m_vkCmdWriteBufferMarker2AMD; }
        PFN_vkUnmapMemory              GetUnmapMemory()              const { return m_vkUnmapMemory; }
        PFN_vkDestroyBuffer            GetDestroyBuffer()            const { return m_vkDestroyBuffer; }
        PFN_vkFreeMemory               GetFreeMemory()               const { return m_vkFreeMemory; }

        void SetSwapchainMethodsAndContext(PFN_vkCreateSwapchainFFX           createSwapchainKHR        = nullptr,
                                           PFN_vkDestroySwapchainKHR          destroySwapchainKHR       = nullptr,
                                           PFN_vkGetSwapchainImagesKHR        getSwapchainImagesKHR     = nullptr,
                                           PFN_vkAcquireNextImageKHR          acquireNextImageKHR       = nullptr,
                                           PFN_vkQueuePresentKHR              queuePresentKHR           = nullptr,
                                           PFN_vkSetHdrMetadataEXT            setHdrMetadataEXT         = nullptr,
                                           PFN_vkCreateSwapchainFFXAPI        createSwapchainFFXAPI     = nullptr,
                                           PFN_vkDestroySwapchainFFXAPI       destroySwapchainFFXAPI    = nullptr,
                                           PFN_getLastPresentCountFFX         getLastPresentCountFFX    = nullptr,
                                           PFN_getLastPresentCountFFXAPI      getLastPresentCountFFXAPI = nullptr,
                                           void*                              pSwapchainContext         = nullptr,
                                           const VkFrameInterpolationInfoFFX* pFrameInterpolationInfo   = nullptr);

        VkResult CreateSwapchainKHR(const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) const;
        void     DestroySwapchainKHR(VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) const;
        VkResult GetSwapchainImagesKHR(VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) const;
        VkResult AcquireNextImageKHR(VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) const;
        VkResult QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) const;
        uint64_t GetLastPresentCountFFX(VkSwapchainKHR swapchain);

        // Frame interpolation queues
        const FIQueue* GetFIAsyncComputeQueue() const
        {
            return &m_FIAsyncComputeQueue;
        }
        const FIQueue* GetFIPresentQueue() const
        {
            return &m_FIPresentQueue;
        }
        const FIQueue* GetFIImageAcquireQueue() const
        {
            return &m_FIImageAcquireQueue;
        }

    private:
        friend class Device;
        DeviceInternal();

        virtual void UpdateAntiLag2() override {}

        class QueueSyncPrimitive
        {
        public:
            void Init(Device* pDevice, CommandQueue queueType, uint32_t queueFamilyIndex, uint32_t queueIndex, uint32_t numFramesInFlight, const char* name);
            void Release(VkDevice device);

            VkCommandPool GetCommandPool();
            void ReleaseCommandPool(VkCommandPool commandPool);

            const VkQueue GetQueue() const { return m_Queue; }
            const uint32_t GetQueueFamily() const { return m_FamilyIndex; }

            // thread safe
            uint64_t Submit(const std::vector<CommandList*>& cmdLists, const VkSemaphore signalSemaphore, const VkSemaphore waitSemaphore, bool waitForSwapchainImage, bool useEndOfFrameSemaphore, DeviceRemovedCallback deviceRemovedCallback, void* deviceRemovedCustomData);
            uint64_t Present(const DeviceInternal* pDevice, VkSwapchainKHR swapchain, uint32_t imageIndex, DeviceRemovedCallback deviceRemovedCallback, void* deviceRemovedCustomData);  // only valid on the present queue

            void Wait(VkDevice device, uint64_t waitValue) const;
            uint64_t QueryLastCompletedValue(VkDevice device) const;

            void Flush();

            VkSemaphore GetOwnershipTransferSemaphore();
            void ReleaseOwnershipTransferSemaphore(VkSemaphore semaphore);

            uint64_t GetLatestSemaphoreValue() const { return m_LatestSemaphoreValue; }

        private:
            VkQueue m_Queue = VK_NULL_HANDLE;
            CommandQueue m_QueueType;
            VkSemaphore m_Semaphore;
            uint64_t m_LatestSemaphoreValue = 0;
            uint32_t m_FamilyIndex;

            ThreadSafeQueue<VkCommandPool> m_AvailableCommandPools = {};
            std::vector<VkSemaphore> m_FrameSemaphores = {};

            std::vector<VkSemaphore> m_AvailableOwnershipTransferSemaphores = {};
            std::vector<VkSemaphore> m_UsedOwnershipTransferSemaphores = {};

            std::recursive_mutex m_SubmitMutex;
        };
        QueueSyncPrimitive m_QueueSyncPrims[static_cast<int32_t>(CommandQueue::Count)] = {};

        // Internal members
        VkInstance       m_Instance = VK_NULL_HANDLE;
        VkDevice         m_Device = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkSurfaceKHR     m_Surface = VK_NULL_HANDLE;

        VmaAllocator     m_VmaAllocator = nullptr;

        // minAccelerationStructureScratchOffsetAlignment
        uint32_t m_MinAccelerationStructureScratchOffsetAlignment;
        uint32_t m_BreadcrumbsMemoryIndex = UINT32_MAX;
        bool     m_UseBreadcrumbsDedicatedAlloc = false;

        // buffer to copy depth into color buffers
        Buffer* m_pDepthToColorCopyBuffer = nullptr;

        // Default objects
        VkSampler m_DefaultSampler = VK_NULL_HANDLE;

        // debug helpers
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;

        PFN_vkSetDebugUtilsObjectNameEXT    m_vkSetDebugUtilsObjectNameEXT = nullptr;
        PFN_vkCmdSetPrimitiveTopologyEXT    m_vkCmdSetPrimitiveTopologyEXT = nullptr;
        PFN_vkCmdBeginDebugUtilsLabelEXT    m_vkCmdBeginDebugUtilsLabelEXT = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT      m_vkCmdEndDebugUtilsLabelEXT = nullptr;
        PFN_vkCmdBeginRenderingKHR          m_vkCmdBeginRenderingKHR = nullptr;
        PFN_vkCmdEndRenderingKHR            m_vkCmdEndRenderingKHR = nullptr;
        PFN_vkCmdSetFragmentShadingRateKHR  m_vkCmdSetFragmentShadingRateKHR = nullptr;
        PFN_vkGetAccelerationStructureBuildSizesKHR m_vkGetAccelerationStructureBuildSizesKHR = nullptr;
        PFN_vkCreateAccelerationStructureKHR        m_vkCreateAccelerationStructureKHR = nullptr;
        PFN_vkDestroyAccelerationStructureKHR       m_vkDestroyAccelerationStructureKHR = nullptr;
        PFN_vkGetAccelerationStructureDeviceAddressKHR m_vkGetAccelerationStructureDeviceAddressKHR = nullptr;
        PFN_vkCmdBuildAccelerationStructuresKHR     m_vkCmdBuildAccelerationStructuresKHR = nullptr;

        // hdr helpers
        PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR m_vkGetPhysicalDeviceSurfaceCapabilities2KHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceFormats2KHR      m_vkGetPhysicalDeviceSurfaceFormats2KHR = nullptr;
        PFN_vkSetHdrMetadataEXT                        m_vkSetHdrMetadataEXT = nullptr;
        PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR m_vkGetPhysicalDeviceFragmentShadingRatesKHR = nullptr;

        // Breadcrumbs required procedures
        PFN_vkGetBufferMemoryRequirements2KHR   m_vkGetBufferMemoryRequirements2KHR = nullptr;
        PFN_vkGetBufferMemoryRequirements       m_vkGetBufferMemoryRequirements = nullptr;
        PFN_vkGetPhysicalDeviceMemoryProperties m_vkGetPhysicalDeviceMemoryProperties = nullptr;
        PFN_vkCreateBuffer                      m_vkCreateBuffer = nullptr;
        PFN_vkAllocateMemory                    m_vkAllocateMemory = nullptr;
        PFN_vkBindBufferMemory                  m_vkBindBufferMemory = nullptr;
        PFN_vkMapMemory                         m_vkMapMemory = nullptr;
        PFN_vkCmdFillBuffer                     m_vkCmdFillBuffer = nullptr;
        PFN_vkCmdWriteBufferMarkerAMD           m_vkCmdWriteBufferMarkerAMD = nullptr;
        PFN_vkCmdWriteBufferMarker2AMD          m_vkCmdWriteBufferMarker2AMD = nullptr;
        PFN_vkUnmapMemory                       m_vkUnmapMemory = nullptr;
        PFN_vkDestroyBuffer                     m_vkDestroyBuffer = nullptr;
        PFN_vkFreeMemory                        m_vkFreeMemory = nullptr;

        // swapchain related functions
        PFN_vkCreateSwapchainFFX      m_vkCreateSwapchainFFX      = nullptr;  // When using FFX
        PFN_vkDestroySwapchainKHR     m_vkDestroySwapchainKHR     = nullptr;  // When using FFX
        PFN_vkCreateSwapchainFFXAPI   m_vkCreateSwapchainFFXAPI   = nullptr;  // When using FFX API
        PFN_vkDestroySwapchainFFXAPI  m_vkDestroySwapchainFFXAPI  = nullptr;  // When using FFX API
        PFN_vkGetSwapchainImagesKHR   m_vkGetSwapchainImagesKHR   = nullptr;
        PFN_vkAcquireNextImageKHR     m_vkAcquireNextImageKHR     = nullptr;
        PFN_vkQueuePresentKHR         m_vkQueuePresentKHR         = nullptr;
        PFN_getLastPresentCountFFX    m_getLastPresentCountFFX    = nullptr;  // When using FFX
        PFN_getLastPresentCountFFXAPI m_getLastPresentCountFFXAPI = nullptr;  // When using FFX API
        void*                         m_pSwapchainContext         = nullptr;
        VkFrameInterpolationInfoFFX   m_FrameInterpolationInfo    = {};

        // Frame interpolation queues
        FIQueue m_FIAsyncComputeQueue;
        FIQueue m_FIImageAcquireQueue;
        FIQueue m_FIPresentQueue;
    };

} // namespace cauldron

#endif // #if defined(_VK)
