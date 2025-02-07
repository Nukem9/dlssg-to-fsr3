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
#include "../ffx_api.h"
#include "../ffx_api_types.h"
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_5.h>

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12 0x0000002u
struct ffxCreateBackendDX12Desc
{
    ffxCreateContextDescHeader header;
    ID3D12Device              *device;  ///< Device on which the backend will run.
};

#define FFX_API_EFFECT_ID_FRAMEGENERATIONSWAPCHAIN_DX12 0x00030000u

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WRAP_DX12 0x30001u
struct ffxCreateContextDescFrameGenerationSwapChainWrapDX12
{
    ffxCreateContextDescHeader header;
    IDXGISwapChain4** swapchain;        ///< Input swap chain to wrap, output frame interpolation swapchain.
    ID3D12CommandQueue* gameQueue;      ///< Input command queue to be used for presentation.
};

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_NEW_DX12 0x30005u
struct ffxCreateContextDescFrameGenerationSwapChainNewDX12
{
    ffxCreateContextDescHeader header;
    IDXGISwapChain4** swapchain;        ///< Output frame interpolation swapchain.
    DXGI_SWAP_CHAIN_DESC* desc;         ///< Swap chain creation parameters.
    IDXGIFactory* dxgiFactory;          ///< IDXGIFactory to use for DX12 swapchain creation.
    ID3D12CommandQueue* gameQueue;      ///< Input command queue to be used for presentation.
};

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12 0x30006u
struct ffxCreateContextDescFrameGenerationSwapChainForHwndDX12
{
    ffxCreateContextDescHeader header;
    IDXGISwapChain4** swapchain;                     ///< Output frame interpolation swapchain.
    HWND hwnd;                                       ///< HWND handle for the calling application;
    DXGI_SWAP_CHAIN_DESC1* desc;                     ///< Swap chain creation parameters.
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreenDesc; ///< Fullscreen swap chain creation parameters.
    IDXGIFactory* dxgiFactory;                       ///< IDXGIFactory to use for DX12 swapchain creation.
    ID3D12CommandQueue* gameQueue;                   ///< Input command queue to be used for presentation.
};

#define FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_REGISTERUIRESOURCE_DX12 0x30002u
struct ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12
{
    ffxConfigureDescHeader header;
    struct FfxApiResource  uiResource;   ///< Resource containing user interface for composition. May be empty.
    uint32_t               flags;        ///< Zero or combination of values from FfxApiUiCompositionFlags.
};

#define FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_INTERPOLATIONCOMMANDLIST_DX12 0x30003u
struct ffxQueryDescFrameGenerationSwapChainInterpolationCommandListDX12
{
    ffxQueryDescHeader header;
    void** pOutCommandList;             ///< Output command list (ID3D12GraphicsCommandList) to be used for frame generation dispatch.
};

#define FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_INTERPOLATIONTEXTURE_DX12 0x30004u
struct ffxQueryDescFrameGenerationSwapChainInterpolationTextureDX12
{
    ffxQueryDescHeader header;
    struct FfxApiResource *pOutTexture; ///< Output resource in which the frame interpolation result should be placed.
};

#define FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WAIT_FOR_PRESENTS_DX12 0x30007u
struct ffxDispatchDescFrameGenerationSwapChainWaitForPresentsDX12
{
    ffxDispatchDescHeader header;
};

#define FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_KEYVALUE_DX12 0x30008u
struct ffxConfigureDescFrameGenerationSwapChainKeyValueDX12
{
    ffxConfigureDescHeader  header;
    uint64_t                key;        ///< Configuration key, member of the FfxApiConfigureFrameGenerationSwapChainKeyDX12 enumeration.
    uint64_t                u64;        ///< Integer value or enum value to set.
    void*                   ptr;        ///< Pointer to set or pointer to value to set.
};

//enum value matches enum FfxFrameInterpolationSwapchainConfigureKey
enum FfxApiConfigureFrameGenerationSwapChainKeyDX12
{
    FFX_API_CONFIGURE_FG_SWAPCHAIN_KEY_WAITCALLBACK = 0,                     ///< Sets FfxWaitCallbackFunc
    FFX_API_CONFIGURE_FG_SWAPCHAIN_KEY_FRAMEPACINGTUNING = 2,                ///< Sets FfxApiSwapchainFramePacingTuning
};

#define FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_GPU_MEMORY_USAGE_DX12 0x00030009u
struct ffxQueryFrameGenerationSwapChainGetGPUMemoryUsageDX12
{
    ffxQueryDescHeader header;
    struct FfxApiEffectMemoryUsage* gpuMemoryUsageFrameGenerationSwapchain;
};

#if defined(__cplusplus)

static inline uint32_t ffxApiGetSurfaceFormatDX12(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_UINT:
        return FFX_API_SURFACE_FORMAT_R32G32B32A32_UINT;
    //case DXGI_FORMAT_R32G32B32A32_SINT:
    //case DXGI_FORMAT_R32G32B32_TYPELESS:
    //case DXGI_FORMAT_R32G32B32_FLOAT:
    //case DXGI_FORMAT_R32G32B32_UINT:
    //case DXGI_FORMAT_R32G32B32_SINT:

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R16G16B16A16_TYPELESS;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT;
    //case DXGI_FORMAT_R16G16B16A16_UNORM:
    //case DXGI_FORMAT_R16G16B16A16_UINT:
    //case DXGI_FORMAT_R16G16B16A16_SNORM:
    //case DXGI_FORMAT_R16G16B16A16_SINT:

    case DXGI_FORMAT_R32G32_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R32G32_TYPELESS;
    case DXGI_FORMAT_R32G32_FLOAT:
        return FFX_API_SURFACE_FORMAT_R32G32_FLOAT;
    //case DXGI_FORMAT_R32G32_FLOAT:
    //case DXGI_FORMAT_R32G32_UINT:
    //case DXGI_FORMAT_R32G32_SINT:

    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R32_FLOAT;

    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R32_UINT;

    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return FFX_API_SURFACE_FORMAT_R8_UINT;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R10G10B10A2_TYPELESS;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM;
    //case DXGI_FORMAT_R10G10B10A2_UINT:
    
    case DXGI_FORMAT_R11G11B10_FLOAT:
        return FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R8G8B8A8_TYPELESS;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB;
    //case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
        return FFX_API_SURFACE_FORMAT_R8G8B8A8_SNORM;

    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        return FFX_API_SURFACE_FORMAT_B8G8R8A8_TYPELESS;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return FFX_API_SURFACE_FORMAT_B8G8R8A8_SRGB;

    case DXGI_FORMAT_R16G16_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R16G16_TYPELESS;
    case DXGI_FORMAT_R16G16_FLOAT:
        return FFX_API_SURFACE_FORMAT_R16G16_FLOAT;
    //case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
        return FFX_API_SURFACE_FORMAT_R16G16_UINT;
    //case DXGI_FORMAT_R16G16_SNORM
    //case DXGI_FORMAT_R16G16_SINT 

    //case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R32_UINT:
        return FFX_API_SURFACE_FORMAT_R32_UINT;
    case DXGI_FORMAT_R32_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R32_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return FFX_API_SURFACE_FORMAT_R32_FLOAT;

    case DXGI_FORMAT_R8G8_UINT:
        return FFX_API_SURFACE_FORMAT_R8G8_UINT;
    case DXGI_FORMAT_R8G8_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R8G8_TYPELESS;
    case DXGI_FORMAT_R8G8_UNORM:
        return FFX_API_SURFACE_FORMAT_R8G8_UNORM;
    //case DXGI_FORMAT_R8G8_SNORM:
    //case DXGI_FORMAT_R8G8_SINT:

    case DXGI_FORMAT_R16_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R16_TYPELESS;
    case DXGI_FORMAT_R16_FLOAT:
        return FFX_API_SURFACE_FORMAT_R16_FLOAT;
    case DXGI_FORMAT_R16_UINT:
        return FFX_API_SURFACE_FORMAT_R16_UINT;
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return FFX_API_SURFACE_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R16_SNORM:
        return FFX_API_SURFACE_FORMAT_R16_SNORM;
    //case DXGI_FORMAT_R16_SINT:

    case DXGI_FORMAT_R8_TYPELESS:
        return FFX_API_SURFACE_FORMAT_R8_TYPELESS;
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_A8_UNORM:
        return FFX_API_SURFACE_FORMAT_R8_UNORM;
    case DXGI_FORMAT_R8_UINT:
        return FFX_API_SURFACE_FORMAT_R8_UINT;
    //case DXGI_FORMAT_R8_SNORM:
    //case DXGI_FORMAT_R8_SINT:
    //case DXGI_FORMAT_R1_UNORM:

    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        return FFX_API_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP;

    case DXGI_FORMAT_UNKNOWN:
    default:
        return FFX_API_SURFACE_FORMAT_UNKNOWN;
    }
}

static inline FfxApiResource ffxApiGetResourceDX12(ID3D12Resource* pRes, uint32_t state = FFX_API_RESOURCE_STATE_COMPUTE_READ, uint32_t additionalUsages = 0)
{
    FfxApiResource res{};
    res.resource = pRes;
    res.state = state;
    if (!pRes) return res;

    D3D12_RESOURCE_DESC desc = pRes->GetDesc();
    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        res.description.flags = FFX_API_RESOURCE_FLAGS_NONE;
        res.description.usage = FFX_API_RESOURCE_USAGE_UAV;
        res.description.size = static_cast<uint32_t>(desc.Width);
        res.description.stride = static_cast<uint32_t>(desc.Height);
        res.description.type = FFX_API_RESOURCE_TYPE_BUFFER;
    }
    else
    {
        res.description.flags = FFX_API_RESOURCE_FLAGS_NONE;
        if (desc.Format == DXGI_FORMAT_D16_UNORM || desc.Format == DXGI_FORMAT_D32_FLOAT)
        {
            res.description.usage = FFX_API_RESOURCE_USAGE_DEPTHTARGET;
        }
        else if (desc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || desc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
        {
            res.description.usage = FFX_API_RESOURCE_USAGE_DEPTHTARGET | FFX_API_RESOURCE_USAGE_STENCILTARGET;
        }
        else
        {
            res.description.usage = FFX_API_RESOURCE_USAGE_READ_ONLY;
        }

        if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
            res.description.usage |= FFX_API_RESOURCE_USAGE_UAV;
        
        res.description.width = static_cast<uint32_t>(desc.Width);
        res.description.height = static_cast<uint32_t>(desc.Height);
        res.description.depth = static_cast<uint32_t>(desc.DepthOrArraySize);
        res.description.mipCount = static_cast<uint32_t>(desc.MipLevels);

        switch (desc.Dimension)
        {
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            res.description.type = FFX_API_RESOURCE_TYPE_TEXTURE1D;
            break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            if (desc.DepthOrArraySize == 6)
                res.description.type = FFX_API_RESOURCE_TYPE_TEXTURE_CUBE;
            else
                res.description.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
            break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            res.description.type = FFX_API_RESOURCE_TYPE_TEXTURE3D;
            break;
        }
    }

    res.description.format = ffxApiGetSurfaceFormatDX12(desc.Format);
    res.description.usage |= additionalUsages;
    return res;
}

#endif
