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
#include "core/backend_interface.h"
#include "core/backend_implementations/backend_interface_dx12.h"

#include "core/framework.h"
#include "core/win/framework_win.h"

#include "misc/assert.h"

#include "render/buffer.h"
#include "render/texture.h"
#include "render/swapchain.h"
#include "render/dx12/swapchain_dx12.h"
#include "render/dx12/buffer_dx12.h"

#if !SUPPORT_RUNTIME_SHADER_RECOMPILE
// If runtime shader recompile not supported then the backend is a
// static library or load-time linked dll, so use the header to define
// these functions.
#include "dx12/ffx_dx12.h"

#else // SUPPORT_RUNTIME_SHADER_RECOMPILE
// If runtime shader recompile is supported then the backend is a
// dll that is loaded at runtime by the backend_shader_reloader library.
// The definition/address of these functions is set at runtime.
#define ffxGetScratchMemorySizeDX12 nullptr
#define ffxGetInterfaceDX12 nullptr
#define ffxGetDeviceDX12 nullptr
#define ffxGetCommandListDX12 nullptr
#define ffxGetPipelineDX12 nullptr
#define ffxGetResourceDX12 nullptr
#define ffxReplaceSwapchainForFrameinterpolationDX12 nullptr
#define ffxRegisterFrameinterpolationUiResourceDX12 nullptr
#define ffxGetFrameinterpolationCommandlistDX12 nullptr
#define ffxGetSwapchainDX12 nullptr
#define ffxGetCommandQueueDX12 nullptr
#define ffxGetResourceDescriptionDX12 nullptr
#define ffxGetFrameinterpolationTextureDX12 nullptr
#define ffxLoadPixDll nullptr
#define ffxGetDX12SwapchainPtr nullptr

#endif // SUPPORT_RUNTIME_SHADER_RECOMPILE

static GetScratchMemorySizeFunc s_pFfxGetScratchMemorySizeFunc = ffxGetScratchMemorySizeDX12;
static GetInterfaceFunc         s_pFfxGetInterfaceFunc         = ffxGetInterfaceDX12;
static GetDeviceDX12Func        s_pFfxGetDeviceFunc            = ffxGetDeviceDX12;
static GetCommandListFunc       s_pFfxGetCommandListFunc       = ffxGetCommandListDX12;
static GetPipelineFunc          s_pFfxGetPipelineFunc          = ffxGetPipelineDX12;
static GetResourceFunc          s_pFfxGetResourceFunc          = ffxGetResourceDX12;
// These functions were added for FSR 3.
static ReplaceSwapchainForFrameinterpolationFunc s_pFfxReplaceSwapchainForFrameinterpolationFunc = ffxReplaceSwapchainForFrameinterpolationDX12;
static RegisterFrameinterpolationUiResourceFunc  s_pFfxRegisterFrameinterpolationUiResourceFunc  = ffxRegisterFrameinterpolationUiResourceDX12;
static GetInterpolationCommandlistFunc           s_pFfxGetInterpolationCommandlistFunc           = ffxGetFrameinterpolationCommandlistDX12;
static GetSwapchainFunc                          s_pFfxGetSwapchainFunc                          = ffxGetSwapchainDX12;
static GetCommandQueueFunc                       s_pFfxGetCommandQueueFunc                       = ffxGetCommandQueueDX12;
static GetResourceDescriptionFunc                s_pFfxGetResourceDescriptionFunc                = ffxGetResourceDescriptionDX12;
static GetFrameinterpolationTextureFunc          s_pFfxGetFrameinterpolationTextureFunc          = ffxGetFrameinterpolationTextureDX12;
static LoadPixDllFunc                            s_pFfxLoadPixDllFunc                            = ffxLoadPixDll;
static GetDX12SwapchainPtrFunc                   s_pFfxGetDX12SwapchainPtrFunc                   = ffxGetDX12SwapchainPtr;

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
    GetDX12SwapchainPtrFunc pFfxGetDX12SwapchainPtrFunc)
{
    s_pFfxGetScratchMemorySizeFunc = pFfxGetScratchMemorySizeFunc;
    s_pFfxGetInterfaceFunc = pFfxGetInterfaceFunc;
    s_pFfxGetDeviceFunc = pFfxGetDeviceFunc;
    s_pFfxGetCommandListFunc = pFfxGetCommandListFunc;
    s_pFfxGetPipelineFunc = pFfxGetPipelineFunc;
    s_pFfxGetResourceFunc = pFfxGetResourceFunc;

    s_pFfxReplaceSwapchainForFrameinterpolationFunc = pFfxReplaceSwapchainForFrameinterpolationFunc;
    s_pFfxRegisterFrameinterpolationUiResourceFunc = pFfxRegisterFrameinterpolationUiResourceFunc;
    s_pFfxGetInterpolationCommandlistFunc = pFfxGetInterpolationCommandlistFunc;
    s_pFfxGetSwapchainFunc = pFfxGetSwapchainFunc;
    s_pFfxGetCommandQueueFunc = pFfxGetCommandQueueFunc;
    s_pFfxGetResourceDescriptionFunc = pFfxGetResourceDescriptionFunc;
    s_pFfxGetFrameinterpolationTextureFunc = pFfxGetFrameinterpolationTextureFunc;
    s_pFfxLoadPixDllFunc = pFfxLoadPixDllFunc;
    s_pFfxGetDX12SwapchainPtrFunc = pFfxGetDX12SwapchainPtrFunc;
}

namespace SDKWrapper
{

size_t ffxGetScratchMemorySize(size_t maxContexts)
{
    CAULDRON_ASSERT(s_pFfxGetScratchMemorySizeFunc);
    return s_pFfxGetScratchMemorySizeFunc(maxContexts);
}

FfxErrorCode ffxGetInterface(FfxInterface* backendInterface, cauldron::Device* device, void* scratchBuffer, size_t scratchBufferSize, size_t maxContexts)
{
    CAULDRON_ASSERT(s_pFfxLoadPixDllFunc);
    CAULDRON_ASSERT(s_pFfxGetDeviceFunc);
    CAULDRON_ASSERT(s_pFfxGetInterfaceFunc);
    // Load the pix dll in order to enable captures (will only load once)
    wchar_t pixDLL[] = L"..\\sdk\\bin\\ffx_sdk\\WinPixEventRuntime.dll";
    s_pFfxLoadPixDllFunc(pixDLL);
    return s_pFfxGetInterfaceFunc(backendInterface, s_pFfxGetDeviceFunc(device->GetImpl()->DX12Device()), scratchBuffer, scratchBufferSize, maxContexts);
}

FfxCommandList ffxGetCommandList(cauldron::CommandList* cauldronCmdList)
{
    CAULDRON_ASSERT(s_pFfxGetCommandListFunc);
    return s_pFfxGetCommandListFunc(cauldronCmdList->GetImpl()->DX12CmdList());
}

FfxPipeline ffxGetPipeline(cauldron::PipelineObject* cauldronPipeline)
{
    CAULDRON_ASSERT(s_pFfxGetPipelineFunc);
    return s_pFfxGetPipelineFunc(cauldronPipeline->GetImpl()->DX12PipelineState());
}

FfxResource ffxGetResource(const cauldron::GPUResource* cauldronResource,
                           const wchar_t* name,
                           FfxResourceStates state,
                           FfxResourceUsage additionalUsages)
{
    CAULDRON_ASSERT(s_pFfxGetResourceDescriptionFunc);
    CAULDRON_ASSERT(s_pFfxGetResourceFunc);
    ID3D12Resource* pDX12Resource = cauldronResource ? const_cast<ID3D12Resource*>(cauldronResource->GetImpl()->DX12Resource()) : nullptr;
    FfxResource     ffxRes        = s_pFfxGetResourceFunc(pDX12Resource, s_pFfxGetResourceDescriptionFunc(pDX12Resource, additionalUsages), name, state);
    // If this is a buffer, and a stride was specified, preserve it
    if (cauldronResource && cauldronResource->IsBuffer() && cauldronResource->GetBufferResource() && cauldronResource->GetBufferResource()->GetDesc().Stride)
        ffxRes.description.stride = cauldronResource->GetBufferResource()->GetDesc().Stride;
    return ffxRes;
}

FfxErrorCode ffxReplaceSwapchainForFrameinterpolation(FfxCommandQueue gameQueue, FfxSwapchain& gameSwapChain, const void* replacementParameters)
{
    CAULDRON_ASSERT(s_pFfxReplaceSwapchainForFrameinterpolationFunc);
    return s_pFfxReplaceSwapchainForFrameinterpolationFunc(gameQueue, gameSwapChain);
}

FfxErrorCode ffxRegisterFrameinterpolationUiResource(FfxSwapchain gameSwapChain, FfxResource uiResource, uint32_t flags)
{
    CAULDRON_ASSERT(s_pFfxRegisterFrameinterpolationUiResourceFunc);
    return s_pFfxRegisterFrameinterpolationUiResourceFunc(gameSwapChain, uiResource, flags);
}

FfxErrorCode ffxGetInterpolationCommandlist(FfxSwapchain gameSwapChain, FfxCommandList& gameCommandlist)
{
    CAULDRON_ASSERT(s_pFfxGetInterpolationCommandlistFunc);
    return s_pFfxGetInterpolationCommandlistFunc(gameSwapChain, gameCommandlist);
}

FfxSwapchain ffxGetSwapchain(cauldron::SwapChain* pSwapChain)
{
    CAULDRON_ASSERT(s_pFfxGetSwapchainFunc);
    return s_pFfxGetSwapchainFunc(pSwapChain->GetImpl()->DX12SwapChain());
}

FfxCommandQueue ffxGetCommandQueue(cauldron::Device* pDevice)
{
    CAULDRON_ASSERT(s_pFfxGetCommandQueueFunc);
    CAULDRON_ASSERT(false && "Not Implemented!");
    return s_pFfxGetCommandQueueFunc(pDevice->GetImpl()->DX12CmdQueue(cauldron::CommandQueue::Graphics));
}

FfxResourceDescription ffxGetResourceDescription(cauldron::GPUResource* pResource)
{
    CAULDRON_ASSERT(s_pFfxGetResourceDescriptionFunc);
    return s_pFfxGetResourceDescriptionFunc(pResource->GetImpl()->DX12Resource(), FFX_RESOURCE_USAGE_READ_ONLY);
}

FfxResource ffxGetFrameinterpolationTexture(FfxSwapchain ffxSwapChain)
{
    CAULDRON_ASSERT(s_pFfxGetFrameinterpolationTextureFunc);
    return s_pFfxGetFrameinterpolationTextureFunc(ffxSwapChain);
}

void ffxSetupFrameInterpolationSwapChain()
{
    CAULDRON_ASSERT(s_pFfxGetSwapchainFunc);
    CAULDRON_ASSERT(s_pFfxGetCommandQueueFunc);
    CAULDRON_ASSERT(s_pFfxReplaceSwapchainForFrameinterpolationFunc);
    CAULDRON_ASSERT(s_pFfxGetDX12SwapchainPtrFunc);
    // take control over the swapchain: first get the swapchain and set to NULL in engine
    IDXGISwapChain4* dxgiSwapchain = cauldron::GetSwapChain()->GetImpl()->DX12SwapChain();
    dxgiSwapchain->AddRef();

    // Create frameinterpolation swapchain
    FfxSwapchain ffxSwapChain = s_pFfxGetSwapchainFunc(cauldron::GetSwapChain()->GetImpl()->DX12SwapChain());

    // make sure swapchain is not holding a ref to real swapchain
    cauldron::GetSwapChain()->GetImpl()->SetDXGISwapChain(nullptr);
    FfxCommandQueue ffxGameQueue = s_pFfxGetCommandQueueFunc(cauldron::GetDevice()->GetImpl()->DX12CmdQueue(cauldron::CommandQueue::Graphics));

    s_pFfxReplaceSwapchainForFrameinterpolationFunc(ffxGameQueue, ffxSwapChain);
    //SDKWrapper::ffxReplaceSwapchainForFrameinterpolation(ffxGameQueue, ffxSwapChain);

    // Set frameinterpolation swapchain to engine
    IDXGISwapChain4* frameinterpolationSwapchain = s_pFfxGetDX12SwapchainPtrFunc(ffxSwapChain);
    cauldron::GetSwapChain()->GetImpl()->SetDXGISwapChain(frameinterpolationSwapchain);

    // Framework swapchain adds to the refcount, so we need to release the swapchain here
    frameinterpolationSwapchain->Release();

    // In case the app is handling Alt-Enter manually we need to update the window association after creating a different swapchain
    IDXGIFactory7* factory = nullptr;
    if (SUCCEEDED(frameinterpolationSwapchain->GetParent(IID_PPV_ARGS(&factory))))
    {
        factory->MakeWindowAssociation(cauldron::GetFramework()->GetImpl()->GetHWND(), DXGI_MWA_NO_WINDOW_CHANGES);
        factory->Release();
    }

    // Lets do the same for HDR as well as it will need to be re initialized since swapchain was re created
    cauldron::GetSwapChain()->SetHDRMetadataAndColorspace();
    return;
}

void ffxRestoreApplicationSwapChain()
{
    cauldron::SwapChain* pSwapchain  = cauldron::GetSwapChain();
    IDXGISwapChain4*     pSwapChain4 = pSwapchain->GetImpl()->DX12SwapChain();
    pSwapChain4->AddRef();

    IDXGIFactory7*      factory   = nullptr;
    ID3D12CommandQueue* pCmdQueue = cauldron::GetDevice()->GetImpl()->DX12CmdQueue(cauldron::CommandQueue::Graphics);

    // Setup a new swapchain for HWND and set it to cauldron
    IDXGISwapChain1* pSwapChain1 = nullptr;
    if (SUCCEEDED(pSwapChain4->GetParent(IID_PPV_ARGS(&factory))))
    {
        cauldron::GetSwapChain()->GetImpl()->SetDXGISwapChain(nullptr);

        // safe data since release will destroy the swapchain (and we need it distroyed before we can create the new one)
        HWND                            windowHandle = pSwapchain->GetImpl()->DX12SwapChainDesc().OutputWindow;
        DXGI_SWAP_CHAIN_DESC1           desc1        = pSwapchain->GetImpl()->DX12SwapChainDesc1();
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc       = pSwapchain->GetImpl()->DX12SwapChainFullScreenDesc();

        pSwapChain4->Release();

        // check if window is still valid or if app is shutting down bc window was closed
        if (IsWindow(windowHandle))
        {
            if (SUCCEEDED(factory->CreateSwapChainForHwnd(pCmdQueue,
                                                          pSwapchain->GetImpl()->DX12SwapChainDesc().OutputWindow,
                                                          &pSwapchain->GetImpl()->DX12SwapChainDesc1(),
                                                          &pSwapchain->GetImpl()->DX12SwapChainFullScreenDesc(),
                                                          nullptr,
                                                          &pSwapChain1)))
            {
                if (SUCCEEDED(pSwapChain1->QueryInterface(IID_PPV_ARGS(&pSwapChain4))))
                {
                    cauldron::GetSwapChain()->GetImpl()->SetDXGISwapChain(pSwapChain4);
                    pSwapChain4->Release();
                }
                pSwapChain1->Release();
            }

            factory->MakeWindowAssociation(cauldron::GetFramework()->GetImpl()->GetHWND(), DXGI_MWA_NO_WINDOW_CHANGES);
        }
        factory->Release();
    }
    return;
}

FfxConstantAllocation ffxAllocateConstantBuffer(void* data, FfxUInt64 dataSize)
{
    FfxConstantAllocation allocation;
    allocation.resource = FfxResource(); // not needed in directx

    cauldron::BufferAddressInfo bufferInfo = cauldron::GetDynamicBufferPool()->AllocConstantBuffer((uint32_t)dataSize, data);
    cauldron::BufferAddressInfo* pBufferInfo = &bufferInfo;
    allocation.handle = FfxUInt64(pBufferInfo->GetImpl()->GPUBufferView);

    return allocation;
}

} // namespace SDKWrapper
