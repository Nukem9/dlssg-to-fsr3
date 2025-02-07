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

#ifdef FFX_BACKEND_DX12
#include "ffx_provider_framegenerationswapchain_dx12.h"
#include <ffx_api/ffx_framegeneration.hpp>
#include <ffx_api/dx12/ffx_api_dx12.hpp>
#include <FidelityFX/host/backends/dx12/ffx_dx12.h>

#include <stdlib.h>

bool ffxProvider_FrameGenerationSwapChain_DX12::CanProvide(uint64_t type) const
{
    return (type & FFX_API_EFFECT_MASK) == FFX_API_EFFECT_ID_FRAMEGENERATIONSWAPCHAIN_DX12;
}

struct InternalFgScContext
{
    InternalContextHeader header;
    IDXGISwapChain4* fiSwapChain;
};

uint64_t ffxProvider_FrameGenerationSwapChain_DX12::GetId() const
{
    // FG SwapChain DX12, version 1.1.2
    return 0xF65C'DD12'01'001'002ui64;
}

const char* ffxProvider_FrameGenerationSwapChain_DX12::GetVersionName() const
{
    return "1.1.2";
}

ffxReturnCode_t ffxProvider_FrameGenerationSwapChain_DX12::CreateContext(ffxContext* context,
                                                                                              ffxCreateContextDescHeader* header,
                                                                                              Allocator&                  alloc) const
{
    if (auto desc = ffx::DynamicCast<ffxCreateContextDescFrameGenerationSwapChainWrapDX12>(header))
    {
        InternalFgScContext* internal_context = alloc.construct<InternalFgScContext>();
        VERIFY(internal_context, FFX_API_RETURN_ERROR_MEMORY);
        internal_context->header.provider = this;

        FfxSwapchain swapChain = ffxGetSwapchainDX12(*desc->swapchain);
        TRY2(ffxReplaceSwapchainForFrameinterpolationDX12(desc->gameQueue, swapChain));
        internal_context->fiSwapChain = *desc->swapchain = ffxGetDX12SwapchainPtr(swapChain);

        // reference tracked by internal_context
        internal_context->fiSwapChain->AddRef();

        *context = internal_context;
        return FFX_API_RETURN_OK;
    }
    else if (auto desc = ffx::DynamicCast<ffxCreateContextDescFrameGenerationSwapChainNewDX12>(header))
    {
        InternalFgScContext* internal_context = alloc.construct<InternalFgScContext>();
        VERIFY(internal_context, FFX_API_RETURN_ERROR_MEMORY);
        internal_context->header.provider = this;

        FfxSwapchain swapChain;
        TRY2(ffxCreateFrameinterpolationSwapchainDX12(desc->desc, desc->gameQueue, desc->dxgiFactory, swapChain));
        internal_context->fiSwapChain = *desc->swapchain = ffxGetDX12SwapchainPtr(swapChain);

        // reference tracked by internal_context
        internal_context->fiSwapChain->AddRef();

        *context = internal_context;
        return FFX_API_RETURN_OK;
    }
    else if (auto desc = ffx::DynamicCast<ffxCreateContextDescFrameGenerationSwapChainForHwndDX12>(header))
    {
        InternalFgScContext* internal_context = alloc.construct<InternalFgScContext>();
        VERIFY(internal_context, FFX_API_RETURN_ERROR_MEMORY);
        internal_context->header.provider = this;

        FfxSwapchain swapChain;
        TRY2(ffxCreateFrameinterpolationSwapchainForHwndDX12(desc->hwnd, desc->desc, desc->fullscreenDesc, desc->gameQueue, desc->dxgiFactory, swapChain));
        internal_context->fiSwapChain = *desc->swapchain = ffxGetDX12SwapchainPtr(swapChain);

        // reference tracked by internal_context
        internal_context->fiSwapChain->AddRef();

        *context = internal_context;
        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    }
}

ffxReturnCode_t ffxProvider_FrameGenerationSwapChain_DX12::DestroyContext(ffxContext* context, Allocator& alloc) const
{
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFgScContext* internal_context = reinterpret_cast<InternalFgScContext*>(*context);

    internal_context->fiSwapChain->Release();

    alloc.dealloc(internal_context);

    return FFX_API_RETURN_OK;
}

ffxReturnCode_t ffxProvider_FrameGenerationSwapChain_DX12::Configure(ffxContext* context, const ffxConfigureDescHeader* header) const
{
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFgScContext* internal_context = reinterpret_cast<InternalFgScContext*>(*context);
    if (auto desc = ffx::DynamicCast<ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12>(header))
    {
        TRY2(ffxRegisterFrameinterpolationUiResourceDX12(ffxGetSwapchainDX12(internal_context->fiSwapChain), Convert(desc->uiResource), desc->flags));

        return FFX_API_RETURN_OK;
    }
    else if (auto desc = ffx::DynamicCast<ffxConfigureDescFrameGenerationSwapChainKeyValueDX12>(header))
    {
        TRY2(ffxConfigureFrameInterpolationSwapchainDX12(ffxGetSwapchainDX12(internal_context->fiSwapChain), static_cast <FfxFrameInterpolationSwapchainConfigureKey> (desc->key), desc->ptr));

        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_PARAMETER;
    }
}

ffxReturnCode_t ffxProvider_FrameGenerationSwapChain_DX12::Query(ffxContext* context, ffxQueryDescHeader* header) const
{
    VERIFY(header, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(context, FFX_API_RETURN_ERROR_PARAMETER);
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);

    InternalFgScContext* internal_context = reinterpret_cast<InternalFgScContext*>(*context);
    if (auto desc = ffx::DynamicCast<ffxQueryDescFrameGenerationSwapChainInterpolationCommandListDX12>(header))
    {
        FfxCommandList outCommandList{};
        TRY2(ffxGetFrameinterpolationCommandlistDX12(ffxGetSwapchainDX12(internal_context->fiSwapChain), outCommandList));
        *desc->pOutCommandList = outCommandList;

        return FFX_API_RETURN_OK;
    }
    else if (auto desc = ffx::DynamicCast<ffxQueryDescFrameGenerationSwapChainInterpolationTextureDX12>(header))
    {
        *desc->pOutTexture = Convert(ffxGetFrameinterpolationTextureDX12(ffxGetSwapchainDX12(internal_context->fiSwapChain)));
        
        return FFX_API_RETURN_OK;
    }
    else if (auto desc = ffx::DynamicCast<ffxQueryFrameGenerationSwapChainGetGPUMemoryUsageDX12>(header))
    {
        TRY2(ffxFrameInterpolationSwapchainGetGpuMemoryUsageDX12(ffxGetSwapchainDX12(internal_context->fiSwapChain), reinterpret_cast <FfxEffectMemoryUsage*> (desc->gpuMemoryUsageFrameGenerationSwapchain)));
        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR_PARAMETER;
    }
}

ffxReturnCode_t ffxProvider_FrameGenerationSwapChain_DX12::Dispatch(ffxContext* context, const ffxDispatchDescHeader* header) const
{
    VERIFY(*context, FFX_API_RETURN_ERROR_PARAMETER);
    InternalFgScContext* internal_context = reinterpret_cast<InternalFgScContext*>(*context);
    if (auto desc = ffx::DynamicCast<ffxDispatchDescFrameGenerationSwapChainWaitForPresentsDX12>(header))
    {
        ffxWaitForPresents(internal_context->fiSwapChain);
        return FFX_API_RETURN_OK;
    }
    else
    {
        return FFX_API_RETURN_ERROR;
    }
}

ffxProvider_FrameGenerationSwapChain_DX12 ffxProvider_FrameGenerationSwapChain_DX12::Instance;
#endif // FFX_BACKEND_DX12
