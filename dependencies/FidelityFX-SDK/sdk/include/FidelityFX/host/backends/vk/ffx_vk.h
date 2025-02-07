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

/// @defgroup VKBackend Vulkan Backend
/// FidelityFX SDK native backend implementation for Vulkan.
/// 
/// @ingroup Backends

#pragma once

#include <vulkan/vulkan.h>
#include <FidelityFX/host/ffx_interface.h>

#if defined(__cplusplus)
extern "C" {
#endif // #if defined(__cplusplus)

/// FFX specific callback type when submitting a command buffer to a queue.
typedef VkResult (*PFN_vkQueueSubmitFFX)(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);

typedef struct VkQueueInfoFFX
{
    VkQueue              queue;
    uint32_t             familyIndex;
    PFN_vkQueueSubmitFFX submitFunc;
} VkQueueInfoFFX;


typedef enum VkCompositonModeFFX
{
    VK_COMPOSITION_MODE_NOT_FORCED_FFX,
    VK_COMPOSITION_MODE_GAME_QUEUE_FFX,
    VK_COMPOSITION_MODE_PRESENT_QUEUE_FFX,
} VkCompositonModeFFX;

/// Structure holding additional information to effectively replace the game swapchain by the frame interpolation one.
/// Some notes on the queues:
///   - please pass the queue, its family (for queue family ownership transfer purposes) and an optional function if you want to control concurrent submissions
///   - game queue: the queue where the replacement of vkQueuePresentKHR is called. This queue should have Graphics and Compute capabilities (Transfer is implied as per Vulkan specification).
///                  It can be shared with the engine. No Submit function is necessary.
///                 The code assumes that the UI texture is owned by that queue family when present is called.
///   - async compute queue: optional queue with Compute capability (Transfer is implied as per Vulkan specification). If used by the engine, prefer not to enable the async compute path of FSR3 Frame interpolation.
///   - present queue: queue with Graphics, Compute or Transfer capability, and Present support. This queue cannot be used by the engine. Otherwise, some deadlock can occur.
///   - image acquire queue: this one doesn't need any capability. Strongly prefer a queue not used by the engine. The main graphics queue can work too but it might delay the signaling of the semaphore/fence when acquiring a new image, negatively impacting the performance.
typedef struct VkFrameInterpolationInfoFFX
{
    VkPhysicalDevice             physicalDevice;
    VkDevice                     device;
    VkQueueInfoFFX               gameQueue;
    VkQueueInfoFFX               asyncComputeQueue;
    VkQueueInfoFFX               presentQueue;
    VkQueueInfoFFX               imageAcquireQueue;
    VkCompositonModeFFX          compositionMode;
    const VkAllocationCallbacks* pAllocator;
} VkFrameInterpolationInfoFFX;

/// Query how much memory is required for the Vulkan backend's scratch buffer.
/// 
/// @param [in] physicalDevice              A pointer to the VkPhysicalDevice device.
/// @param [in] maxContexts                 The maximum number of simultaneous effect contexts that will share the backend.
///                                         (Note that some effects contain internal contexts which count towards this maximum)
///
/// @returns
/// The size (in bytes) of the required scratch memory buffer for the VK backend.
/// 
/// @ingroup VKBackend
FFX_API size_t ffxGetScratchMemorySizeVK(VkPhysicalDevice physicalDevice, size_t maxContexts);

/// Convenience structure to hold all VK-related device information
typedef struct VkDeviceContext {
    VkDevice                vkDevice;           /// The Vulkan device
    VkPhysicalDevice        vkPhysicalDevice;   /// The Vulkan physical device
    PFN_vkGetDeviceProcAddr vkDeviceProcAddr;   /// The device's function address table
} VkDeviceContext;

/// Create a <c><i>FfxDevice</i></c> from a <c><i>VkDevice</i></c>.
///
/// @param [in] vkDeviceContext             A pointer to a VKDeviceContext that holds all needed information
///
/// @returns
/// An abstract FidelityFX device.
///
/// @ingroup VKBackend
FFX_API FfxDevice ffxGetDeviceVK(VkDeviceContext* vkDeviceContext);

/// Populate an interface with pointers for the VK backend.
///
/// @param [out] backendInterface           A pointer to a <c><i>FfxInterface</i></c> structure to populate with pointers.
/// @param [in] device                      A pointer to the VkDevice device.
/// @param [in] scratchBuffer               A pointer to a buffer of memory which can be used by the DirectX(R)12 backend.
/// @param [in] scratchBufferSize           The size (in bytes) of the buffer pointed to by <c><i>scratchBuffer</i></c>.
/// @param [in] maxContexts                 The maximum number of simultaneous effect contexts that will share the backend.
///                                         (Note that some effects contain internal contexts which count towards this maximum)
///
/// @retval
/// FFX_OK                                  The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_INVALID_POINTER          The <c><i>interface</i></c> pointer was <c><i>NULL</i></c>.
///
/// @ingroup VKBackend
FFX_API FfxErrorCode ffxGetInterfaceVK(
    FfxInterface* backendInterface,
    FfxDevice device,
    void* scratchBuffer,
    size_t scratchBufferSize, 
    size_t maxContexts);

/// Create a <c><i>FfxCommandList</i></c> from a <c><i>VkCommandBuffer</i></c>.
///
/// @param [in] cmdBuf                      A pointer to the Vulkan command buffer.
///
/// @returns
/// An abstract FidelityFX command list.
///
/// @ingroup VKBackend
FFX_API FfxCommandList ffxGetCommandListVK(VkCommandBuffer cmdBuf);

/// Create a <c><i>FfxPipeline</i></c> from a <c><i>VkPipeline</i></c>.
///
/// @param [in] pipeline                    A pointer to the Vulkan pipeline.
///
/// @returns
/// An abstract FidelityFX pipeline.
///
/// @ingroup VKBackend
FFX_API FfxPipeline ffxGetPipelineVK(VkPipeline pipeline);

/// Fetch a <c><i>FfxResource</i></c> from a <c><i>GPUResource</i></c>.
///
/// @param [in] vkResource                  A pointer to the (agnostic) VK resource.
/// @param [in] ffxResDescription           An <c><i>FfxResourceDescription</i></c> for the resource representation.
/// @param [in] ffxResName                  (optional) A name string to identify the resource in debug mode.
/// @param [in] state                       The state the resource is currently in.
///
/// @returns
/// An abstract FidelityFX resources.
///
/// @ingroup VKBackend
FFX_API FfxResource ffxGetResourceVK(void*  vkResource,
    FfxResourceDescription                  ffxResDescription,
    const wchar_t*                          ffxResName,
    FfxResourceStates                       state = FFX_RESOURCE_STATE_COMPUTE_READ);

/// Fetch a <c><i>FfxSurfaceFormat</i></c> from a VkFormat.
///
/// @param [in] format              The VkFormat to convert to <c><i>FfxSurfaceFormat</i></c>.
///
/// @returns
/// An <c><i>FfxSurfaceFormat</i></c>.
///
/// @ingroup VKBackend
FFX_API FfxSurfaceFormat ffxGetSurfaceFormatVK(VkFormat format);

/// Fetch a <c><i>FfxResourceDescription</i></c> from an existing VkBuffer.
///
/// @param [in] buffer              The VkBuffer resource to create a <c><i>FfxResourceDescription</i></c> for.
/// @param [in] createInfo          The VkBufferCreateInfo of the buffer
/// @param [in] additionalUsages    Optional <c><i>FfxResourceUsage</i></c> flags needed for select resource mapping.
///
/// @returns
/// An <c><i>FfxResourceDescription</i></c>.
///
/// @ingroup VKBackend
FFX_API FfxResourceDescription ffxGetBufferResourceDescriptionVK(const VkBuffer           buffer,
                                                                 const VkBufferCreateInfo createInfo,
                                                                 FfxResourceUsage         additionalUsages = FFX_RESOURCE_USAGE_READ_ONLY);

/// Fetch a <c><i>FfxResourceDescription</i></c> from an existing VkImage.
///
/// @param [in] image               The VkImage resource to create a <c><i>FfxResourceDescription</i></c> for.
/// @param [in] createInfo          The VkImageCreateInfo of the buffer
/// @param [in] additionalUsages    Optional <c><i>FfxResourceUsage</i></c> flags needed for select resource mapping.
///
/// @returns
/// An <c><i>FfxResourceDescription</i></c>.
///
/// @ingroup VKBackend
FFX_API FfxResourceDescription ffxGetImageResourceDescriptionVK(const VkImage           image,
                                                                const VkImageCreateInfo createInfo,
                                                                FfxResourceUsage        additionalUsages = FFX_RESOURCE_USAGE_READ_ONLY);

/// Fetch a <c><i>FfxCommandQueue</i></c> from an existing VkQueue.
///
/// @param [in] commandQueue       The VkQueue to create a <c><i>FfxCommandQueue</i></c> from.
///
/// @returns
/// An <c><i>FfxCommandQueue</i></c>.
///
/// @ingroup VKBackend
FFX_API FfxCommandQueue ffxGetCommandQueueVK(VkQueue commandQueue);

/// Fetch a <c><i>FfxSwapchain</i></c> from an existing VkSwapchainKHR.
///
/// @param [in] pSwapchain          The VkSwapchainKHR to create a <c><i>FfxSwapchain</i></c> from.
///
/// @returns
/// An <c><i>FfxSwapchain</i></c>.
///
/// @ingroup VKBackend
FFX_API FfxSwapchain ffxGetSwapchainVK(VkSwapchainKHR swapchain);

/// Fetch a VkSwapchainKHR from an existing <c><i>FfxSwapchain</i></c>.
///
/// @param [in] ffxSwapchain          The <c><i>FfxSwapchain</i></c> to fetch an VkSwapchainKHR from.
///
/// @returns
/// An VkSwapchainKHR object.
///
/// @ingroup VKBackend
FFX_API VkSwapchainKHR ffxGetVKSwapchain(FfxSwapchain ffxSwapchain);

/// Replaces the current swapchain with the provided <c><i>FfxSwapchain</i></c>.
///
/// @param [in]     gameQueue               The <c><i>FfxCommandQueue</i></c> presentation will occur on.
/// @param [in,out] gameSwapChain           The current <c><i>FfxSwapchain</i></c> to replace, optional. If not NULL, the swapchain will be destroyed. On return, it will hold the <c><i>FfxSwapchain</i></c> to use for frame interpolation presentation.
/// @param [in]     swapchainCreateInfo     The <c><i>VkSwapchainCreateInfoKHR</i></c> of the current swapchain. Its oldSwapchain member should be VK_NULL_HANDLE or the same as gameSwapChain.
/// @param [in]     frameInterpolationInfo  The <c><i>VkFrameInterpolationInfoFFX</i></c> containing additional information for swapchain replacement.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          One of the parameters is invalid. If the returned <c><i>gameSwapChain</i></c> is NULL, the old swapchain has been destroyed.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         Internal generic error. If the returned <c><i>gameSwapChain</i></c> is NULL, the old swapchain has been destroyed.
///
/// @ingroup VKFrameInterpolation
FFX_API FfxErrorCode ffxReplaceSwapchainForFrameinterpolationVK(FfxCommandQueue                    gameQueue,
                                                                FfxSwapchain&                      gameSwapChain,
                                                                const VkSwapchainCreateInfoKHR*    swapchainCreateInfo,
                                                                const VkFrameInterpolationInfoFFX* frameInterpolationInfo);

/// Waits for the <c><i>FfxSwapchain</i></c> to complete presentation.
///
/// @param [in] gameSwapChain           The <c><i>FfxSwapchain</i></c> to wait on.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          Could not query the interface for the frame interpolation swap chain.
///
/// @ingroup VKFrameInterpolation
FFX_API FfxErrorCode ffxWaitForPresents(FfxSwapchain gameSwapChain);

/// Registers a <c><i>FfxResource</i></c> to use for UI with the provided <c><i>FfxSwapchain</i></c>.
///
/// @param [in] gameSwapChain           The <c><i>FfxSwapchain</i></c> to to register the UI resource with.
/// @param [in] uiResource              The <c><i>FfxResource</i></c> representing the UI resource.
/// @param [in] flags                   A set of <c><i>FfxUiCompositionFlags</i></c>.
/// 
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          Could not query the interface for the frame interpolation swap chain.
///
/// @ingroup VKFrameInterpolation
FFX_API FfxErrorCode ffxRegisterFrameinterpolationUiResourceVK(FfxSwapchain gameSwapChain, FfxResource uiResource, uint32_t flags);

/// Fetches a <c><i>FfxCommandList</i></c> from the <c><i>FfxSwapchain</i></c>.
///
/// @param [in] gameSwapChain           The <c><i>FfxSwapchain</i></c> to get a <c><i>FfxCommandList</i></c> from.
/// @param [out] gameCommandlist        The <c><i>FfxCommandList</i></c> from the provided <c><i>FfxSwapchain</i></c>.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          Could not query the interface for the frame interpolation swap chain.
///
/// @ingroup VKFrameInterpolation
FFX_API FfxErrorCode ffxGetFrameinterpolationCommandlistVK(FfxSwapchain gameSwapChain, FfxCommandList& gameCommandlist);

/// Fetches a <c><i>FfxResource</i></c>  representing the backbuffer from the <c><i>FfxSwapchain</i></c>.
///
/// @param [in] gameSwapChain           The <c><i>FfxSwapchain</i></c> to get a <c><i>FfxResource</i></c> backbuffer from.
///
/// @returns
/// An abstract FidelityFX resources for the swapchain backbuffer.
///
/// @ingroup VKFrameInterpolation
FFX_API FfxResource ffxGetFrameinterpolationTextureVK(FfxSwapchain gameSwapChain);

/// Sets a <c><i>FfxFrameGenerationConfig</i></c> to the internal FrameInterpolationSwapChain (in the backend).
///
/// @param [in] config                  The <c><i>FfxFrameGenerationConfig</i></c> to set.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          Could not query the interface for the frame interpolation swap chain.
///
/// @ingroup VKFrameInterpolation
FFX_API FfxErrorCode ffxSetFrameGenerationConfigToSwapchainVK(FfxFrameGenerationConfig const* config);

//enum values should match enum FfxApiConfigureFrameGenerationSwapChainKeyVK
typedef enum FfxFrameInterpolationSwapchainConfigureKey
{
    FFX_FI_SWAPCHAIN_CONFIGURE_KEY_WAITCALLBACK = 0,
    FFX_FI_SWAPCHAIN_CONFIGURE_KEY_FRAMEPACINGTUNING = 2,
} FfxFrameInterpolationSwapchainConfigureKey;

/// Configures <c><i>FfxSwapchain</i></c> via KeyValue API post <c><i>FfxSwapchain</i></c> context creation
///
/// @param [in] gameSwapChain           The <c><i>FfxSwapchain</i></c> to configure via KeyValue API
/// @param [in] key                     The <c><i>FfxFrameInterpolationSwapchainConfigureKey</i></c> is key
/// @param [in] valuePtr                The <c><i><void *></i></c> pointer to value. What this pointer deference to depends on key.
/// 
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          Could not query the interface for the frame interpolation swap chain.
///
/// @ingroup VKFrameInterpolation
FFX_API FfxErrorCode ffxConfigureFrameInterpolationSwapchainVK(FfxSwapchain gameSwapChain, FfxFrameInterpolationSwapchainConfigureKey key, void* valuePtr);

/// Query how much GPU memory created by <c><i>FfxSwapchain</i></c>. This excludes GPU memory created by the VkSwapchain (ie. size of backbuffers).
///
/// @param [in] gameSwapChain           The <c><i>FfxSwapchain</i></c> to configure via KeyValue API
/// @param [in out] vramUsage           The <c><i>FfxEffectMemoryUsage</i></c> is the GPU memory created by FrameInterpolationSwapchain
/// 
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          Could not query the interface for the frame interpolation swap chain.
///
/// @ingroup VKFrameInterpolation
FFX_API FfxErrorCode ffxFrameInterpolationSwapchainGetGpuMemoryUsageVK(FfxSwapchain gameSwapChain, FfxEffectMemoryUsage* vramUsage);

typedef VkResult (*PFN_vkCreateSwapchainFFX)(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain, const VkFrameInterpolationInfoFFX* pFrameInterpolationInfo);

/// Function to get he number of presents. This is useful when using frame interpolation
typedef uint64_t (*PFN_getLastPresentCountFFX)(VkSwapchainKHR);

/// Structure holding the replacement function pointers for frame interpolation to work
/// Not all extensions are supported for now
/// Regarding specific functions:
///   - queuePresentKHR: when using this one, the presenting image should be in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL state
///   - getLastPresentCount: this function isn't part of Vulkan but the engine can use it to get the real number of presented frames since the swapchain creation
struct FfxSwapchainReplacementFunctions
{
    PFN_vkCreateSwapchainFFX    createSwapchainFFX;
    PFN_vkDestroySwapchainKHR   destroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR getSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR   acquireNextImageKHR;
    PFN_vkQueuePresentKHR       queuePresentKHR;
    PFN_vkSetHdrMetadataEXT     setHdrMetadataEXT;
    PFN_getLastPresentCountFFX  getLastPresentCountFFX;
};
FFX_API FfxErrorCode ffxGetSwapchainReplacementFunctionsVK(FfxDevice device, FfxSwapchainReplacementFunctions* functions);

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
