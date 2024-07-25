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

#include "backends.h"

#ifdef FFX_BACKEND_DX12
#include <ffx_api/dx12/ffx_api_dx12.h>
#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#endif // FFX_BACKEND_DX12
#ifdef FFX_BACKEND_VK
#include <ffx_api/vk/ffx_api_vk.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
#endif // #ifdef FFX_BACKEND_VK

ffxReturnCode_t CreateBackend(const ffxCreateContextDescHeader *desc, bool& backendFound, FfxInterface *iface, size_t contexts, Allocator& alloc)
{
    for (const auto* it = desc->pNext; it; it = it->pNext)
    {
        switch (it->type)
        {
#ifdef FFX_BACKEND_DX12
        case FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12:
        {
            // check for double backend just to make sure.
            if (backendFound)
                return FFX_API_RETURN_ERROR;
            backendFound = true;

            const auto *backendDesc = reinterpret_cast<const ffxCreateBackendDX12Desc*>(it);
            FfxDevice device = ffxGetDeviceDX12(backendDesc->device);
            size_t scratchBufferSize = ffxGetScratchMemorySizeDX12(contexts);
            void* scratchBuffer = alloc.alloc(scratchBufferSize);
            memset(scratchBuffer, 0, scratchBufferSize);
            TRY2(ffxGetInterfaceDX12(iface, device, scratchBuffer, scratchBufferSize, contexts));
            break;
        }
#elif FFX_BACKEND_VK
        case FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK:
        {
            // check for double backend just to make sure.
            if (backendFound)
                return FFX_API_RETURN_ERROR;
            backendFound = true;

            const auto *backendDesc = reinterpret_cast<const ffxCreateBackendVKDesc*>(it);
            VkDeviceContext deviceContext = { backendDesc->vkDevice, backendDesc->vkPhysicalDevice, backendDesc->vkDeviceProcAddr };
            FfxDevice device = ffxGetDeviceVK(&deviceContext);
            size_t scratchBufferSize = ffxGetScratchMemorySizeVK(backendDesc->vkPhysicalDevice, contexts);
            void* scratchBuffer = alloc.alloc(scratchBufferSize);
            memset(scratchBuffer, 0, scratchBufferSize);
            TRY2(ffxGetInterfaceVK(iface, device, scratchBuffer, scratchBufferSize, contexts));
            break;
        }
#endif // FFX_BACKEND_DX12
        }
    }
    return FFX_API_RETURN_OK;
}

void* GetDevice(const ffxApiHeader* desc)
{
    for (const auto* it = desc; it; it = it->pNext)
    {
        switch (it->type)
        {
        case FFX_API_QUERY_DESC_TYPE_GET_VERSIONS:
        {
            return reinterpret_cast<const ffxQueryDescGetVersions*>(it)->device;
        }
#ifdef FFX_BACKEND_DX12
        case FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12:
        {
            return reinterpret_cast<const ffxCreateBackendDX12Desc*>(it)->device;
        }
        case FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12:
        {
            ID3D12Device* device = nullptr;
            reinterpret_cast<const ffxCreateContextDescFrameGenerationSwapChainForHwndDX12*>(it)->gameQueue->GetDevice(IID_PPV_ARGS(&device));
            device->Release();
            return device;
        }
        case FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_NEW_DX12:
        {
            ID3D12Device* device = nullptr;
            reinterpret_cast<const ffxCreateContextDescFrameGenerationSwapChainNewDX12*>(it)->gameQueue->GetDevice(IID_PPV_ARGS(&device));
            device->Release();
            return device;
        }
        case FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WRAP_DX12:
        {
            ID3D12Device* device = nullptr;
            reinterpret_cast<const ffxCreateContextDescFrameGenerationSwapChainWrapDX12*>(it)->gameQueue->GetDevice(IID_PPV_ARGS(&device));
            device->Release();
            return device;
        }
#endif
#ifdef FFX_BACKEND_VK
        case FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK:
        {
            return nullptr; // no idea what to return for now.
        }
#endif
        }
    }
    return nullptr;
}
