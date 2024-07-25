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

#include "render/dx12/device_dx12.h"
#include "render/dx12/commandlist_dx12.h"
#include "render/dx12/gpuresource_dx12.h"
#include "render/dx12/pipelineobject_dx12.h"

#include <FidelityFX/host/ffx_interface.h>

#if defined(__cplusplus)
extern "C" {
#endif  // #if defined(__cplusplus)
// DX12 Backend Interface Function Ptr Typedefs
typedef size_t (*GetScratchMemorySizeFunc)(size_t);
typedef FfxErrorCode (*GetInterfaceFunc)(FfxInterface*, FfxDevice, void*, size_t, size_t);
typedef FfxDevice (*GetDeviceDX12Func)(ID3D12Device*);
typedef FfxCommandList (*GetCommandListFunc)(ID3D12CommandList*);
typedef FfxPipeline (*GetPipelineFunc)(ID3D12PipelineState*);
typedef FfxResource (*GetResourceFunc)(const ID3D12Resource*, FfxResourceDescription, const wchar_t*, FfxResourceStates);

typedef FfxErrorCode (*ReplaceSwapchainForFrameinterpolationFunc)(FfxCommandQueue, FfxSwapchain&);
typedef FfxErrorCode (*RegisterFrameinterpolationUiResourceFunc)(FfxSwapchain, FfxResource, uint32_t);
typedef FfxErrorCode (*GetInterpolationCommandlistFunc)(FfxSwapchain, FfxCommandList&);
typedef FfxSwapchain (*GetSwapchainFunc)(IDXGISwapChain4*);
typedef FfxCommandQueue (*GetCommandQueueFunc)(ID3D12CommandQueue*);
typedef FfxResourceDescription (*GetResourceDescriptionFunc)(const ID3D12Resource* pResource, FfxResourceUsage);
typedef FfxResource (*GetFrameinterpolationTextureFunc)(FfxSwapchain);
typedef FfxErrorCode (*LoadPixDllFunc)(const wchar_t*);
typedef IDXGISwapChain4* (*GetDX12SwapchainPtrFunc)(FfxSwapchain);

/// This function is called from the backend_shader_reloader library after runtime loading the backend dll.
void InitDX12BackendInterface(
    GetScratchMemorySizeFunc pFfxGetScratchMemorySizeFunc,
    GetInterfaceFunc pFfxGetInterfaceFunc,
    GetDeviceDX12Func pFfxGetDeviceFunc,
    GetCommandListFunc pFfxGetCommandListFunc,
    GetPipelineFunc pFfxGetPipelineFunc,
    GetResourceFunc pFfxGetResourceFunc,
    ReplaceSwapchainForFrameinterpolationFunc pFfxReplaceSwapchainForFrameinterpolationFunc,
    RegisterFrameinterpolationUiResourceFunc pFfxRegisterFrameinterpolationUiResourceFunc,
    GetInterpolationCommandlistFunc pFfxGetInterpolationCommandlistFunc,
    GetSwapchainFunc pFfxGetSwapchainFunc,
    GetCommandQueueFunc pFfxGetCommandQueueFunc,
    GetResourceDescriptionFunc pFfxGetResourceDescriptionFunc,
    GetFrameinterpolationTextureFunc pFfxGetFrameinterpolationTextureFunc,
    LoadPixDllFunc pFfxLoadPixDllFunc,
    GetDX12SwapchainPtrFunc pFfxGetDX12SwapchainPtrFunc);

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
