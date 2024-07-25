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

#include "vk/ffx_vk.h"
#include "render/vk/device_vk.h"
#include "render/vk/commandlist_vk.h"
#include "render/vk/gpuresource_vk.h"
#include "render/vk/pipelineobject_vk.h"

#if defined(__cplusplus)
extern "C" {
#endif  // #if defined(__cplusplus)

// replacementParameters structure to pass to ffxReplaceSwapchainForFrameinterpolation
struct FrameInterpolationReplacementParametersVK
{
    VkSwapchainCreateInfoKHR*    swapchainCreateInfo    = nullptr;
    VkFrameInterpolationInfoFFX* frameInterpolationInfo = nullptr;
};

// Cauldron Backend Interface Function Ptr Typedefs
typedef size_t (*GetScratchMemorySizeFunc)(VkPhysicalDevice, size_t);
typedef FfxErrorCode (*GetInterfaceFunc)(FfxInterface*, FfxDevice, void*, size_t, size_t);
typedef FfxDevice (*GetDeviceVKFunc)(VkDeviceContext*);
typedef FfxCommandList (*GetCommandListFunc)(VkCommandBuffer);
typedef FfxPipeline (*GetPipelineFunc)(VkPipeline);
typedef FfxResource (*GetResourceFunc)(void*, FfxResourceDescription, const wchar_t*, FfxResourceStates);

typedef FfxErrorCode (*ReplaceSwapchainForFrameinterpolationFunc)(FfxCommandQueue, FfxSwapchain&, const VkSwapchainCreateInfoKHR*, const VkFrameInterpolationInfoFFX*);
typedef FfxErrorCode (*RegisterFrameinterpolationUiResourceFunc)(FfxSwapchain, FfxResource, uint32_t);
typedef FfxErrorCode (*GetInterpolationCommandlistFunc)(FfxSwapchain, FfxCommandList&);
typedef FfxSwapchain (*GetSwapchainFunc)(VkSwapchainKHR);
typedef FfxCommandQueue (*GetCommandQueueFunc)(VkQueue);
typedef FfxResourceDescription (*GetImageResourceDescriptionFunc)(const VkImage, const VkImageCreateInfo, FfxResourceUsage);
typedef FfxResourceDescription (*GetBufferResourceDescriptionFunc)(const VkBuffer, const VkBufferCreateInfo, FfxResourceUsage);
typedef FfxResource (*GetFrameinterpolationTextureFunc)(FfxSwapchain);
typedef VkSwapchainKHR (*GetVKSwapchainFunc)(FfxSwapchain);
typedef FfxErrorCode (*GetSwapchainReplacementFunctionsFunc)(FfxDevice, FfxSwapchainReplacementFunctions*);

/// This function is called from the backend_shader_reloader library after runtime loading the backend dll.
void InitVKBackendInterface(
    GetScratchMemorySizeFunc pFfxGetScratchMemorySizeFunc,
    GetInterfaceFunc pFfxGetInterfaceFunc,
    GetDeviceVKFunc pFfxGetDeviceFunc,
    GetCommandListFunc pFfxGetCommandListFunc,
    GetPipelineFunc pFfxGetPipelineFunc,
    GetResourceFunc pFfxGetResourceFunc,
    ReplaceSwapchainForFrameinterpolationFunc pFfxReplaceSwapchainForFrameinterpolationFunc,
    RegisterFrameinterpolationUiResourceFunc pFfxRegisterFrameinterpolationUiResourceFunc,
    GetInterpolationCommandlistFunc pFfxGetInterpolationCommandlistFunc,
    GetSwapchainFunc pFfxGetSwapchainFunc,
    GetCommandQueueFunc pFfxGetCommandQueueFunc,
    GetImageResourceDescriptionFunc pFfxGetImageResourceDescriptionFunc,
    GetBufferResourceDescriptionFunc pFfxGetBufferResourceDescriptionFunc,
    GetFrameinterpolationTextureFunc pFfxGetFrameinterpolationTextureFunc,
    GetVKSwapchainFunc pFfxGetVKSwapchainFunc,
    GetSwapchainReplacementFunctionsFunc pGetSwapchainReplacementFunctionsFunc);

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
