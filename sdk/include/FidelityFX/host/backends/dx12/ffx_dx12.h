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

/// @defgroup DX12Backend DX12 Backend
/// FidelityFX SDK native backend implementation for DirectX 12.
/// 
/// @ingroup Backends

/// @defgroup DX12FrameInterpolation DX12 FrameInterpolation
/// FidelityFX SDK native frame interpolation implementation for DirectX 12 backend.
/// 
/// @ingroup DX12Backend

#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <FidelityFX/host/ffx_interface.h>

#if defined(__cplusplus)
extern "C" {
#endif // #if defined(__cplusplus)

/// Query how much memory is required for the DirectX 12 backend's scratch buffer.
/// 
/// @param [in] maxContexts                 The maximum number of simultaneous effect contexts that will share the backend.
///                                         (Note that some effects contain internal contexts which count towards this maximum)
///
/// @returns
/// The size (in bytes) of the required scratch memory buffer for the DX12 backend.
/// @ingroup DX12Backend
FFX_API size_t ffxGetScratchMemorySizeDX12(size_t maxContexts);

/// Create a <c><i>FfxDevice</i></c> from a <c><i>ID3D12Device</i></c>.
///
/// @param [in] device                      A pointer to the DirectX12 device.
///
/// @returns
/// An abstract FidelityFX device.
///
/// @ingroup DX12Backend
FFX_API FfxDevice ffxGetDeviceDX12(ID3D12Device* device);

/// Populate an interface with pointers for the DX12 backend.
///
/// @param [out] backendInterface           A pointer to a <c><i>FfxInterface</i></c> structure to populate with pointers.
/// @param [in] device                      A pointer to the DirectX12 device.
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
/// @ingroup DX12Backend
FFX_API FfxErrorCode ffxGetInterfaceDX12(
    FfxInterface* backendInterface,
    FfxDevice device,
    void* scratchBuffer,
    size_t scratchBufferSize, 
    size_t maxContexts);

/// Create a <c><i>FfxCommandList</i></c> from a <c><i>ID3D12CommandList</i></c>.
///
/// @param [in] cmdList                     A pointer to the DirectX12 command list.
///
/// @returns
/// An abstract FidelityFX command list.
///
/// @ingroup DX12Backend
FFX_API FfxCommandList ffxGetCommandListDX12(ID3D12CommandList* cmdList);

/// Create a <c><i>FfxPipeline</i></c> from a <c><i>ID3D12PipelineState</i></c>.
///
/// @param [in] pipelineState               A pointer to the DirectX12 pipeline state.
///
/// @returns
/// An abstract FidelityFX pipeline.
///
/// @ingroup DX12Backend
FFX_API FfxPipeline ffxGetPipelineDX12(ID3D12PipelineState* pipelineState);

/// Fetch a <c><i>FfxResource</i></c> from a <c><i>GPUResource</i></c>.
///
/// @param [in] dx12Resource                A pointer to the DX12 resource.
/// @param [in] ffxResDescription           An <c><i>FfxResourceDescription</i></c> for the resource representation.
/// @param [in] ffxResName                  (optional) A name string to identify the resource in debug mode.
/// @param [in] state                       The state the resource is currently in.
///
/// @returns
/// An abstract FidelityFX resources.
///
/// @ingroup DX12Backend
FFX_API FfxResource ffxGetResourceDX12(const ID3D12Resource*  dx12Resource,
                                       FfxResourceDescription ffxResDescription,
                                       const wchar_t*         ffxResName,
                                       FfxResourceStates      state = FFX_RESOURCE_STATE_COMPUTE_READ);

/// Loads PIX runtime dll to allow SDK calls to show up in Microsoft PIX.
///
/// @param [in] pixDllPath                  The path to the DLL to load.
///
/// @retval
/// FFX_OK                                  The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_PATH                  Could not load the DLL using the provided path.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR             Could not get proc addresses for PIXBeginEvent and/or PIXEndEvent
///
/// @ingroup DX12Backend
FFX_API FfxErrorCode ffxLoadPixDll(const wchar_t*  pixDllPath);

/// Fetch a <c><i>FfxSurfaceFormat</i></c> from a DXGI_FORMAT.
///
/// @param [in] format              The DXGI_FORMAT to convert to <c><i>FfxSurfaceFormat</i></c>.
///
/// @returns
/// An <c><i>FfxSurfaceFormat</i></c>.
///
/// @ingroup DX12Backend
FFX_API FfxSurfaceFormat ffxGetSurfaceFormatDX12(DXGI_FORMAT format);

/// Fetch a DXGI_FORMAT from a <c><i>FfxSurfaceFormat</i></c>.
///
/// @param [in] surfaceFormat       The <c><i>FfxSurfaceFormat</i></c> to convert to DXGI_FORMAT.
///
/// @returns
/// An DXGI_FORMAT.
///
/// @ingroup DX12Backend
FFX_API DXGI_FORMAT ffxGetDX12FormatFromSurfaceFormat(FfxSurfaceFormat surfaceFormat);

/// Fetch a <c><i>FfxResourceDescription</i></c> from an existing ID3D12Resource.
///
/// @param [in] pResource           The ID3D12Resource resource to create a <c><i>FfxResourceDescription</i></c> for.
/// @param [in] additionalUsages    Optional <c><i>FfxResourceUsage</i></c> flags needed for select resource mapping.
///
/// @returns
/// An <c><i>FfxResourceDescription</i></c>.
///
/// @ingroup DX12Backend
FFX_API FfxResourceDescription ffxGetResourceDescriptionDX12(const ID3D12Resource* pResource, FfxResourceUsage additionalUsages = FFX_RESOURCE_USAGE_READ_ONLY);

/// Fetch a <c><i>FfxCommandQueue</i></c> from an existing ID3D12CommandQueue.
///
/// @param [in] pCommandQueue       The ID3D12CommandQueue to create a <c><i>FfxCommandQueue</i></c> from.
///
/// @returns
/// An <c><i>FfxCommandQueue</i></c>.
///
/// @ingroup DX12Backend
FFX_API FfxCommandQueue ffxGetCommandQueueDX12(ID3D12CommandQueue* pCommandQueue);

/// Fetch a <c><i>FfxSwapchain</i></c> from an existing IDXGISwapChain4.
///
/// @param [in] pSwapchain          The IDXGISwapChain4 to create a <c><i>FfxSwapchain</i></c> from.
///
/// @returns
/// An <c><i>FfxSwapchain</i></c>.
///
/// @ingroup DX12Backend
FFX_API FfxSwapchain ffxGetSwapchainDX12(IDXGISwapChain4* pSwapchain);

/// Fetch a IDXGISwapChain4 from an existing <c><i>FfxSwapchain</i></c>.
///
/// @param [in] ffxSwapchain          The <c><i>FfxSwapchain</i></c> to fetch an IDXGISwapChain4 from.
///
/// @returns
/// An IDXGISwapChain4 object.
///
/// @ingroup DX12Backend
FFX_API IDXGISwapChain4* ffxGetDX12SwapchainPtr(FfxSwapchain ffxSwapchain);

/// Replaces the current swapchain with the provided <c><i>FfxSwapchain</i></c>.
///
/// @param [in] gameQueue               The <c><i>FfxCommandQueue</i></c> presentation will occur on.
/// @param [in] gameSwapChain           The <c><i>FfxSwapchain</i></c> to use for frame interpolation presentation.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          One of the parameters is invalid.
///
/// @ingroup DX12FrameInterpolation
FFX_API FfxErrorCode ffxReplaceSwapchainForFrameinterpolationDX12(FfxCommandQueue gameQueue, FfxSwapchain& gameSwapChain);

/// Creates a <c><i>FfxSwapchain</i></c> from passed in parameters.
///
/// @param [in] desc                    The DXGI_SWAP_CHAIN_DESC describing the swapchain creation parameters from the calling application.
/// @param [in] queue                   The ID3D12CommandQueue to use for frame interpolation presentation.
/// @param [in] dxgiFactory             The IDXGIFactory to use for DX12 swapchain creation.
/// @param [out] outGameSwapChain       The created <c><i>FfxSwapchain</i></c>.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          One of the parameters is invalid.
/// FFX_ERROR_OUT_OF_MEMORY             Insufficient memory available to allocate <c><i>FfxSwapchain</i></c> or underlying component.
///
/// @ingroup DX12FrameInterpolation
FFX_API FfxErrorCode ffxCreateFrameinterpolationSwapchainDX12(const DXGI_SWAP_CHAIN_DESC* desc,
                                                      ID3D12CommandQueue* queue,
                                                      IDXGIFactory* dxgiFactory,
                                                      FfxSwapchain& outGameSwapChain);

/// Creates a <c><i>FfxSwapchain</i></c> from passed in parameters.
///
/// @param [in] hWnd                    The HWND handle for the calling application.
/// @param [in] desc1                   The DXGI_SWAP_CHAIN_DESC1 describing the swapchain creation parameters from the calling application.
/// @param [in] fullscreenDesc          The DXGI_SWAP_CHAIN_FULLSCREEN_DESC describing the full screen swapchain creation parameters from the calling application.
/// @param [in] queue                   The ID3D12CommandQueue to use for frame interpolation presentation.
/// @param [in] dxgiFactory             The IDXGIFactory to use for DX12 swapchain creation.
/// @param [out] outGameSwapChain       The created <c><i>FfxSwapchain</i></c>.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          One of the parameters is invalid.
/// FFX_ERROR_OUT_OF_MEMORY             Insufficient memory available to allocate <c><i>FfxSwapchain</i></c> or underlying component.
///
/// @ingroup DX12FrameInterpolation
FFX_API FfxErrorCode ffxCreateFrameinterpolationSwapchainForHwndDX12(HWND hWnd,
                                                             const DXGI_SWAP_CHAIN_DESC1* desc1,
                                                             const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreenDesc,
                                                             ID3D12CommandQueue* queue,
                                                             IDXGIFactory* dxgiFactory,
                                                             FfxSwapchain& outGameSwapChain);

/// Waits for the <c><i>FfxSwapchain</i></c> to complete presentation.
///
/// @param [in] gameSwapChain           The <c><i>FfxSwapchain</i></c> to wait on.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          Could not query the interface for the frame interpolation swap chain.
///
/// @ingroup DX12FrameInterpolation
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
/// @ingroup DX12FrameInterpolation
FFX_API FfxErrorCode ffxRegisterFrameinterpolationUiResourceDX12(FfxSwapchain gameSwapChain, FfxResource uiResource, uint32_t flags);

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
/// @ingroup DX12FrameInterpolation
FFX_API FfxErrorCode ffxGetFrameinterpolationCommandlistDX12(FfxSwapchain gameSwapChain, FfxCommandList& gameCommandlist);

/// Fetches a <c><i>FfxResource</i></c>  representing the backbuffer from the <c><i>FfxSwapchain</i></c>.
///
/// @param [in] gameSwapChain           The <c><i>FfxSwapchain</i></c> to get a <c><i>FfxResource</i></c> backbuffer from.
///
/// @returns
/// An abstract FidelityFX resources for the swapchain backbuffer.
///
/// @ingroup DX12FrameInterpolation
FFX_API FfxResource ffxGetFrameinterpolationTextureDX12(FfxSwapchain gameSwapChain);

/// Sets a <c><i>FfxFrameGenerationConfig</i></c> to the internal FrameInterpolationSwapChain (in the backend).
///
/// @param [in] config                  The <c><i>FfxFrameGenerationConfig</i></c> to set.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          Could not query the interface for the frame interpolation swap chain.
///
/// @ingroup DX12FrameInterpolation
FFX_API FfxErrorCode ffxSetFrameGenerationConfigToSwapchainDX12(FfxFrameGenerationConfig const* config);

struct FfxFrameInterpolationContext;
typedef FfxErrorCode (*FfxCreateFiSwapchain)(FfxFrameInterpolationContext* fiContext, FfxDevice device, FfxCommandQueue gameQueue, FfxSwapchain& swapchain);
typedef FfxErrorCode (*FfxReleaseFiSwapchain)(FfxFrameInterpolationContext* fiContext, FfxSwapchain* outRealSwapchain);

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
