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

#include "ffx_provider.h"

#include "ffx_provider_fsr2.h"
#include "ffx_provider_fsr3upscale.h"
#include "ffx_provider_framegeneration.h"
#include "ffx_provider_external.h"

#include <array>
#include <optional>

#include <d3d12.h>

#ifdef FFX_BACKEND_DX12
#include "dx12/ffx_provider_framegenerationswapchain_dx12.h"
#endif // FFX_BACKEND_DX12

#ifdef FFX_BACKEND_VK
#include "vk/ffx_provider_framegenerationswapchain_vk.h"
#endif // FFX_BACKEND_VK

static constexpr ffxProvider* providers[] = {
    &ffxProvider_FSR3Upscale::Instance,
    &ffxProvider_FSR2::Instance,
    &ffxProvider_FrameGeneration::Instance,
#ifdef FFX_BACKEND_DX12
    &ffxProvider_FrameGenerationSwapChain_DX12::Instance,
#endif // FFX_BACKEND_DX12
#ifdef FFX_BACKEND_VK
    &ffxProvider_FrameGenerationSwapChain_VK::Instance,
#endif // FFX_BACKEND_VK
};
static constexpr size_t providerCount = _countof(providers);

static std::array<std::optional<ffxProviderExternal>, 10> externalProviders = {};

MIDL_INTERFACE("b58d6601-7401-4234-8180-6febfc0e484c")
IAmdExtFfxApi : public IUnknown
{
public:
    // Update FFX API provider
    virtual HRESULT UpdateFfxApiProvider(void* pData, uint32_t dataSizeInBytes) = 0;
};

struct ExternalProviderData
{
    uint32_t structVersion = 0;
    uint64_t descType;
    ffxProviderInterface provider;
};
#define FFX_EXTERNAL_PROVIDER_STRUCT_VERSION 1u

void GetExternalProviders(ID3D12Device* device, uint64_t descType)
{
    static IAmdExtFfxApi* apiExtension = nullptr;

    if (nullptr != device)
    {
        static bool ranOnce = false;
        if (!ranOnce)
        {
            ranOnce = true;
            static HMODULE hModule = GetModuleHandleA("amdxc64.dll");

            if (device && hModule && !apiExtension)
            {
                typedef HRESULT(__cdecl * PFNAmdExtD3DCreateInterface)(IUnknown * pOuter, REFIID riid, void** ppvObject);
                PFNAmdExtD3DCreateInterface AmdExtD3DCreateInterface =
                    static_cast<PFNAmdExtD3DCreateInterface>((VOID*)GetProcAddress(hModule, "AmdExtD3DCreateInterface"));
                if (AmdExtD3DCreateInterface)
                {
                    HRESULT hr = AmdExtD3DCreateInterface(device, IID_PPV_ARGS(&apiExtension));

                    if (hr != S_OK)
                    {
                        if (apiExtension)
                            apiExtension->Release();
                        apiExtension = nullptr;
                    }
                }
            }
        }
    }

    if (apiExtension)
    {
        ExternalProviderData data;
        data.structVersion = FFX_EXTERNAL_PROVIDER_STRUCT_VERSION;
        data.descType = descType;
        HRESULT hr = apiExtension->UpdateFfxApiProvider(&data, sizeof(data));
        if (hr != S_OK)
            return;

        for (auto& slot : externalProviders)
        {
            if (slot.has_value() && slot->GetId() == data.provider.versionId)
            {
                // we already have this provider saved.
                break;
            }
            if (!slot.has_value())
            {
                // first free slot. slots are filled start to end and never released.
                // we do not have this provider yet, add it to the list.
                slot = ffxProviderExternal{data.provider};
                break;
            }
        }
    }
    
}

const ffxProvider* GetffxProvider(ffxStructType_t descType, uint64_t overrideId, void* device)
{
    // check driver-side providers
    GetExternalProviders(reinterpret_cast<ID3D12Device*>(device), descType);

    for (const auto& provider : externalProviders)
    {
        if (provider.has_value())
        {
            if (provider->GetId() == overrideId || (overrideId == 0 && provider->CanProvide(descType)))
                return &*provider;
        }
    }

    for (size_t i = 0; i < providerCount; ++i)
    {
        if (providers[i]->GetId() == overrideId || (overrideId == 0 && providers[i]->CanProvide(descType)))
            return providers[i];
    }

    return nullptr;
}

const ffxProvider* GetAssociatedProvider(ffxContext* context)
{
    const InternalContextHeader* hdr = (const InternalContextHeader*)(*context);
    const ffxProvider*        provider = hdr->provider;
    return provider;
}

uint64_t GetProviderCount(ffxStructType_t descType, void* device)
{
    return GetProviderVersions(descType, device, UINT64_MAX, nullptr, nullptr);
}

uint64_t GetProviderVersions(ffxStructType_t descType, void* device, uint64_t capacity, uint64_t* versionIds, const char** versionNames)
{
    uint64_t count = 0;

    // check driver-side providers
    GetExternalProviders(reinterpret_cast<ID3D12Device*>(device), descType);

    for (const auto& provider : externalProviders)
    {
        if (count >= capacity) break;
        if (provider.has_value() && provider->CanProvide(descType))
        {
            auto index = count;
            count++;
            if (versionIds)
                versionIds[index] = provider->GetId();
            if (versionNames)
                versionNames[index] = provider->GetVersionName();
        }
    }

    for (size_t i = 0; i < providerCount; ++i)
    {
        if (count >= capacity) break;
        if (providers[i]->CanProvide(descType))
        {
            auto index = count;
            count++;
            if (versionIds)
                versionIds[index] = providers[i]->GetId();
            if (versionNames)
                versionNames[index] = providers[i]->GetVersionName();
        }
    }

    return count;
}
