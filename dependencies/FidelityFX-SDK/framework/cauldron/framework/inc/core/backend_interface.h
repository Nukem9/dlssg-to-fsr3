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

#include "render/buffer.h"
#include "render/commandlist.h"
#include "render/device.h"
#include "render/gpuresource.h"
#include "render/pipelineobject.h"
#include "render/texture.h"

#include <FidelityFX/host/ffx_interface.h>
#include <FidelityFX/host/ffx_error.h>
#include <FidelityFX/host/ffx_types.h>

#ifdef USE_FFX_API
#include <ffx_api/ffx_api_types.h>
#if defined(FFX_API_DX12)
#include "render/dx12/swapchain_dx12.h"
#include "render/dx12/gpuresource_dx12.h"
#include <ffx_api/dx12/ffx_api_dx12.h>
#endif  // defined(FFX_API_DX12)
#if defined(FFX_API_VK)
#include "render/vk/swapchain_vk.h"
#include "render/vk/gpuresource_vk.h"
#include <ffx_api/vk/ffx_api_vk.h>
#endif  // defined(FFX_API_VK)
#endif  // USE_FFX_API

namespace SDKWrapper
{
/**
* @brief  Query how much memory is required for the backend's scratch buffer.
*/
size_t         ffxGetScratchMemorySize(size_t maxContexts);
/**
* @brief  Initialize the <c><i>FfxInterface</i></c> with function pointers for the backend.
*/
FfxErrorCode   ffxGetInterface(FfxInterface* backendInterface, cauldron::Device* device, void* scratchBuffer, size_t scratchBufferSize, size_t maxContexts);
/**
* @brief  Create a <c><i>FfxCommandList</i></c> from a <c><i>CommandList</i></c>.
*/
FfxCommandList ffxGetCommandList(cauldron::CommandList* cauldronCmdList);
/**
* @brief  Create a <c><i>FfxPipeline</i></c> from a <c><i>PipelineObject</i></c>.
*/
FfxPipeline    ffxGetPipeline(cauldron::PipelineObject* cauldronPipeline);
/**
* @brief  Fetch a <c><i>FfxResource</i></c> from a <c><i>GPUResource</i></c>.
*/
FfxResource    ffxGetResource(const cauldron::GPUResource* cauldronResource,
                              const wchar_t*               name             = nullptr,
                              FfxResourceStates            state            = FFX_RESOURCE_STATE_COMPUTE_READ,
                              FfxResourceUsage             additionalUsages = (FfxResourceUsage)0);


#ifdef USE_FFX_API
    static inline FfxApiResource ffxGetResourceApi(const cauldron::GPUResource* cauldronResource,
        uint32_t            state = FFX_API_RESOURCE_STATE_COMPUTE_READ,
        uint32_t            additionalUsages = 0)
    {
#ifdef FFX_API_DX12
        ID3D12Resource* pDX12Resource = cauldronResource ? const_cast<ID3D12Resource*>(cauldronResource->GetImpl()->DX12Resource()) : nullptr;
        FfxApiResource apiRes = ffxApiGetResourceDX12(pDX12Resource, state, additionalUsages);
        // If this is a buffer, and a stride was specified, preserve it
        if (cauldronResource && cauldronResource->IsBuffer() && cauldronResource->GetBufferResource() && cauldronResource->GetBufferResource()->GetDesc().Stride)
            apiRes.description.stride = cauldronResource->GetBufferResource()->GetDesc().Stride;
        return apiRes;
#elif FFX_API_VK
        if (cauldronResource == nullptr)
        {
            return ffxApiGetResourceVK(nullptr, FfxApiResourceDescription(), state);
        }
        else if (cauldronResource->GetImpl()->IsBuffer())
        {
            VkBuffer buffer = cauldronResource->GetImpl()->GetBuffer();
            return ffxApiGetResourceVK(
                (void*)buffer, ffxApiGetBufferResourceDescriptionVK(buffer, cauldronResource->GetImpl()->GetBufferCreateInfo(), additionalUsages), state);
        }
        else  //if (cauldronResource->GetImpl()->IsTexture())
        {
            VkImage image = cauldronResource->GetImpl()->GetImage();
            return ffxApiGetResourceVK(
                (void*)image, ffxApiGetImageResourceDescriptionVK(image, cauldronResource->GetImpl()->GetImageCreateInfo(), additionalUsages), state);
        }

#endif  // FFX_API_DX12

        cauldron::CauldronCritical(L"Unsupported API or Platform for FFX Validation Remap");
        return FfxApiResource();   // Error
    }
#endif

/**
* @brief Replaces the current swapchain with the provided <c><i>FfxSwapchain</i></c> for FSR 3 frame interpolation support.
*/
FfxErrorCode           ffxReplaceSwapchainForFrameinterpolation(FfxCommandQueue gameQueue, FfxSwapchain& gameSwapChain, const void* replacementParameters);
/**
* @brief Registers a <c><i>FfxResource</i></c> to use for UI with the provided <c><i>FfxSwapchain</i></c> for FSR 3 frame interpolation support.
*/
FfxErrorCode           ffxRegisterFrameinterpolationUiResource(FfxSwapchain gameSwapChain, FfxResource uiResource, uint32_t flags);
/**
* @brief Fetches a <c><i>FfxCommandList</i></c> from the <c><i>FfxSwapchain</i> for FSR 3 frame interpolation support</c> for FSR 3 frame interpolation support.
*/
FfxErrorCode           ffxGetInterpolationCommandlist(FfxSwapchain gameSwapChain, FfxCommandList& gameCommandlist);
/**
* @brief Fetch a <c><i>FfxSwapchain</i></c> from a Cauldron SwapChain.
*/
FfxSwapchain           ffxGetSwapchain(cauldron::SwapChain* pSwapChain);
/**
* @brief Fetch a <c><i>FfxCommandQueue</i></c> from a Cauldron Device.
*/
FfxCommandQueue        ffxGetCommandQueue(cauldron::Device* pDevice);
/**
* @brief Fetch a <c><i>FfxResourceDescription</i></c> from a Cauldron GPUResource.
*/
FfxResourceDescription ffxGetResourceDescription(cauldron::GPUResource* pResource);
/**
* @brief Fetches a <c><i>FfxResource</i></c> representing the backbuffer from the <c><i>FfxSwapchain</i></c> for FSR 3 frame interpolation support.
*/
FfxResource            ffxGetFrameinterpolationTexture(FfxSwapchain ffxSwapChain);
/**
* @brief Configures the swap chain for FSR 3 interpolation.
*/
void                   ffxSetupFrameInterpolationSwapChain();
/**
* @brief Restores previous configuration swap chain to state before FSR 3 interpolation was configured (see ffxSetupFrameInterpolationSwapChain).
*/
void                   ffxRestoreApplicationSwapChain();

/**
* @brief Performs constant buffer allocation using our own allocator
*/
FfxConstantAllocation ffxAllocateConstantBuffer(void* data, FfxUInt64 dataSize);

//////////////////////////////////////////////////////////////////////////
// FFX to Framework conversion functions

static cauldron::ResourceFormat GetFrameworkSurfaceFormat(FfxSurfaceFormat format)
{
    switch (format)
    {
    case FFX_SURFACE_FORMAT_UNKNOWN:
        return cauldron::ResourceFormat::Unknown;
    case FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS:
        return cauldron::ResourceFormat::RGBA32_TYPELESS;
    case FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT:
        return cauldron::ResourceFormat::RGBA32_FLOAT;
    case FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT:
        return cauldron::ResourceFormat::RGBA16_FLOAT;
    case FFX_SURFACE_FORMAT_R32G32_FLOAT:
        return cauldron::ResourceFormat::RG32_FLOAT;
    case FFX_SURFACE_FORMAT_R32_UINT:
        return cauldron::ResourceFormat::R32_UINT;
    case FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS:
        return cauldron::ResourceFormat::RGBA8_TYPELESS;
    case FFX_SURFACE_FORMAT_R8G8B8A8_UNORM:
        return cauldron::ResourceFormat::RGBA8_UNORM;
    case FFX_SURFACE_FORMAT_R8G8B8A8_SNORM:
        return cauldron::ResourceFormat::RGBA8_SNORM;
    case FFX_SURFACE_FORMAT_R8G8B8A8_SRGB:
        return cauldron::ResourceFormat::RGBA8_SRGB;
    case FFX_SURFACE_FORMAT_B8G8R8A8_TYPELESS:
        return cauldron::ResourceFormat::BGRA8_TYPELESS;
    case FFX_SURFACE_FORMAT_B8G8R8A8_UNORM:
        return cauldron::ResourceFormat::BGRA8_UNORM;
    case FFX_SURFACE_FORMAT_B8G8R8A8_SRGB:
        return cauldron::ResourceFormat::BGRA8_SRGB;
    case FFX_SURFACE_FORMAT_R11G11B10_FLOAT:
        return cauldron::ResourceFormat::RG11B10_FLOAT;
    case FFX_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP:
        return cauldron::ResourceFormat::RGB9E5_SHAREDEXP;
    case FFX_SURFACE_FORMAT_R16G16_FLOAT:
        return cauldron::ResourceFormat::RG16_FLOAT;
    case FFX_SURFACE_FORMAT_R16G16_UINT:
        return cauldron::ResourceFormat::RG16_UINT;
    case FFX_SURFACE_FORMAT_R16G16_SINT:
        return cauldron::ResourceFormat::RG16_SINT;
    case FFX_SURFACE_FORMAT_R16_FLOAT:
        return cauldron::ResourceFormat::R16_FLOAT;
    case FFX_SURFACE_FORMAT_R16_UINT:
        return cauldron::ResourceFormat::R16_UINT;
    case FFX_SURFACE_FORMAT_R16_UNORM:
        return cauldron::ResourceFormat::R16_UNORM;
    case FFX_SURFACE_FORMAT_R16_SNORM:
        return cauldron::ResourceFormat::R16_SNORM;
    case FFX_SURFACE_FORMAT_R8_UNORM:
        return cauldron::ResourceFormat::R8_UNORM;
    case FFX_SURFACE_FORMAT_R8_UINT:
        return cauldron::ResourceFormat::R8_UINT;
    case FFX_SURFACE_FORMAT_R8G8_UNORM:
        return cauldron::ResourceFormat::RG8_UNORM;
    case FFX_SURFACE_FORMAT_R32_FLOAT:
        return cauldron::ResourceFormat::R32_FLOAT;
    case FFX_SURFACE_FORMAT_R10G10B10A2_UNORM:
        return cauldron::ResourceFormat::RGB10A2_UNORM;
    case FFX_SURFACE_FORMAT_R10G10B10A2_TYPELESS:
        return cauldron::ResourceFormat::RGB10A2_TYPELESS;
    default:
        cauldron::CauldronCritical(L"FFXInterface: Framework: Unsupported format requested. Please implement.");
        return cauldron::ResourceFormat::Unknown;
    }
}

static cauldron::ResourceFlags GetFrameworkResourceFlags(FfxResourceUsage flags)
{
    cauldron::ResourceFlags cauldronResourceFlags = cauldron::ResourceFlags::None;

    if (flags & FFX_RESOURCE_USAGE_RENDERTARGET)
        cauldronResourceFlags |= cauldron::ResourceFlags::AllowRenderTarget;

    if (flags & FFX_RESOURCE_USAGE_DEPTHTARGET)
        cauldronResourceFlags |= cauldron::ResourceFlags::AllowDepthStencil;

    if (flags & FFX_RESOURCE_USAGE_UAV)
        cauldronResourceFlags |= cauldron::ResourceFlags::AllowUnorderedAccess;

    if (flags & FFX_RESOURCE_USAGE_INDIRECT)
        cauldronResourceFlags |= cauldron::ResourceFlags::AllowIndirect;

    return cauldronResourceFlags;
}

inline cauldron::ResourceState GetFrameworkState(FfxResourceStates state)
{
    switch (state)
    {
    case FFX_RESOURCE_STATE_UNORDERED_ACCESS:
        return cauldron::ResourceState::UnorderedAccess;
    case FFX_RESOURCE_STATE_COMPUTE_READ:
        return cauldron::ResourceState::NonPixelShaderResource;
    case FFX_RESOURCE_STATE_PIXEL_READ:
        return cauldron::ResourceState::PixelShaderResource;
    case FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ:
        return cauldron::ResourceState(cauldron::ResourceState::NonPixelShaderResource | cauldron::ResourceState::PixelShaderResource);
    case FFX_RESOURCE_STATE_COPY_SRC:
        return cauldron::ResourceState::CopySource;
    case FFX_RESOURCE_STATE_COPY_DEST:
        return cauldron::ResourceState::CopyDest;
    case FFX_RESOURCE_STATE_GENERIC_READ:
        return (cauldron::ResourceState)((uint32_t)cauldron::ResourceState::NonPixelShaderResource | (uint32_t)cauldron::ResourceState::PixelShaderResource);
    case FFX_RESOURCE_STATE_INDIRECT_ARGUMENT:
        return cauldron::ResourceState::IndirectArgument;
    case FFX_RESOURCE_STATE_PRESENT:
        return cauldron::ResourceState::Present;
    case FFX_RESOURCE_STATE_RENDER_TARGET:
        return cauldron::ResourceState::RenderTargetResource;
    default:
        cauldron::CauldronCritical(L"FFXInterface: Cauldron: Unsupported resource state requested. Please implement.");
        return cauldron::ResourceState::CommonResource;
    }
}

inline cauldron::TextureDesc GetFrameworkTextureDescription(FfxResourceDescription desc)
{
    cauldron::ResourceFormat format = GetFrameworkSurfaceFormat(desc.format);
    cauldron::ResourceFlags  flags  = GetFrameworkResourceFlags(desc.usage);
    switch (desc.type)
    {
    case FFX_RESOURCE_TYPE_TEXTURE1D:
        return cauldron::TextureDesc::Tex1D(L"", format, desc.width, 1, desc.mipCount, flags);
    case FFX_RESOURCE_TYPE_TEXTURE2D:
        return cauldron::TextureDesc::Tex2D(L"", format, desc.width, desc.height, 1, desc.mipCount, flags);
    case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
        return cauldron::TextureDesc::TexCube(L"", format, desc.width, desc.height, 1, desc.mipCount, flags);
    case FFX_RESOURCE_TYPE_TEXTURE3D:
        return cauldron::TextureDesc::Tex3D(L"", format, desc.width, desc.height, desc.depth, desc.mipCount, flags);
    default:
        cauldron::CauldronCritical(L"Description should be a texture.");
        return cauldron::TextureDesc();
    }
}

inline cauldron::BufferDesc GetFrameworkBufferDescription(FfxResourceDescription desc)
{
    if (desc.type == FFX_RESOURCE_TYPE_BUFFER)
    {
        cauldron::ResourceFlags flags = GetFrameworkResourceFlags(desc.usage);
        return cauldron::BufferDesc::Data(L"", desc.size, desc.stride, desc.alignment, flags);
    }
    else
    {
        cauldron::CauldronCritical(L"Description should be a texture.");
        return cauldron::BufferDesc();
    }
}

#ifdef USE_FFX_API
static cauldron::ResourceFormat GetFrameworkSurfaceFormatApi(uint32_t format)
{
    switch (format)
    {
    case FFX_API_SURFACE_FORMAT_UNKNOWN:
        return cauldron::ResourceFormat::Unknown;
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS:
        return cauldron::ResourceFormat::RGBA32_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT:
        return cauldron::ResourceFormat::RGBA32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32G32B32_FLOAT:
        return cauldron::ResourceFormat::RGB32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_TYPELESS:
        return cauldron::ResourceFormat::RGBA16_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT:
        return cauldron::ResourceFormat::RGBA16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32G32_TYPELESS:
        return cauldron::ResourceFormat::RG32_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R32G32_FLOAT:
        return cauldron::ResourceFormat::RG32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32_UINT:
        return cauldron::ResourceFormat::R32_UINT;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_TYPELESS:
        return cauldron::ResourceFormat::RGBA8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM:
        return cauldron::ResourceFormat::RGBA8_UNORM;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_SNORM:
        return cauldron::ResourceFormat::RGBA8_SNORM;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB:
        return cauldron::ResourceFormat::RGBA8_SRGB;
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_TYPELESS:
        return cauldron::ResourceFormat::BGRA8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM:
        return cauldron::ResourceFormat::BGRA8_UNORM;
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_SRGB:
        return cauldron::ResourceFormat::BGRA8_SRGB;
    case FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT:
        return cauldron::ResourceFormat::RG11B10_FLOAT;
    case FFX_API_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP:
        return cauldron::ResourceFormat::RGB9E5_SHAREDEXP;
    case FFX_API_SURFACE_FORMAT_R16G16_TYPELESS:
        return cauldron::ResourceFormat::RG16_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R16G16_FLOAT:
        return cauldron::ResourceFormat::RG16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16G16_UINT:
        return cauldron::ResourceFormat::RG16_UINT;
    case FFX_API_SURFACE_FORMAT_R16G16_SINT:
        return cauldron::ResourceFormat::RG16_SINT;
    case FFX_API_SURFACE_FORMAT_R16_TYPELESS:
        return cauldron::ResourceFormat::R16_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R16_FLOAT:
        return cauldron::ResourceFormat::R16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16_UINT:
        return cauldron::ResourceFormat::R16_UINT;
    case FFX_API_SURFACE_FORMAT_R16_UNORM:
        return cauldron::ResourceFormat::R16_UNORM;
    case FFX_API_SURFACE_FORMAT_R16_SNORM:
        return cauldron::ResourceFormat::R16_SNORM;
    case FFX_API_SURFACE_FORMAT_R8_TYPELESS:
        return cauldron::ResourceFormat::R8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R8_UNORM:
        return cauldron::ResourceFormat::R8_UNORM;
    case FFX_API_SURFACE_FORMAT_R8_UINT:
        return cauldron::ResourceFormat::R8_UINT;
    case FFX_API_SURFACE_FORMAT_R8G8_TYPELESS:
        return cauldron::ResourceFormat::RG8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R8G8_UNORM:
        return cauldron::ResourceFormat::RG8_UNORM;
    case FFX_API_SURFACE_FORMAT_R32_TYPELESS:
        return cauldron::ResourceFormat::R32_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R32_FLOAT:
        return cauldron::ResourceFormat::R32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R10G10B10A2_TYPELESS:
        return cauldron::ResourceFormat::RGB10A2_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM:
        return cauldron::ResourceFormat::RGB10A2_UNORM;
    default:
        cauldron::CauldronCritical(L"FFXInterface: Framework: Unsupported format requested. Please implement.");
        return cauldron::ResourceFormat::Unknown;
    }
}

static cauldron::ResourceFlags GetFrameworkResourceFlagsApi(uint32_t flags)
{
    cauldron::ResourceFlags cauldronResourceFlags = cauldron::ResourceFlags::None;

    if (flags & FFX_API_RESOURCE_USAGE_RENDERTARGET)
        cauldronResourceFlags |= cauldron::ResourceFlags::AllowRenderTarget;

    if (flags & FFX_API_RESOURCE_USAGE_DEPTHTARGET)
        cauldronResourceFlags |= cauldron::ResourceFlags::AllowDepthStencil;

    if (flags & FFX_API_RESOURCE_USAGE_UAV)
        cauldronResourceFlags |= cauldron::ResourceFlags::AllowUnorderedAccess;

    if (flags & FFX_API_RESOURCE_USAGE_INDIRECT)
        cauldronResourceFlags |= cauldron::ResourceFlags::AllowIndirect;

    return cauldronResourceFlags;
}

inline cauldron::TextureDesc GetFrameworkTextureDescription(FfxApiResourceDescription desc)
{
    cauldron::ResourceFormat format = GetFrameworkSurfaceFormatApi(desc.format);
    cauldron::ResourceFlags  flags  = GetFrameworkResourceFlagsApi(desc.usage);
    switch (desc.type)
    {
    case FFX_RESOURCE_TYPE_TEXTURE1D:
        return cauldron::TextureDesc::Tex1D(L"", format, desc.width, 1, desc.mipCount, flags);
    case FFX_RESOURCE_TYPE_TEXTURE2D:
        return cauldron::TextureDesc::Tex2D(L"", format, desc.width, desc.height, 1, desc.mipCount, flags);
    case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
        return cauldron::TextureDesc::TexCube(L"", format, desc.width, desc.height, 1, desc.mipCount, flags);
    case FFX_RESOURCE_TYPE_TEXTURE3D:
        return cauldron::TextureDesc::Tex3D(L"", format, desc.width, desc.height, desc.depth, desc.mipCount, flags);
    default:
        cauldron::CauldronCritical(L"Description should be a texture.");
        return cauldron::TextureDesc();
    }
}

inline cauldron::BufferDesc GetFrameworkBufferDescription(FfxApiResourceDescription desc)
{
    if (desc.type == FFX_RESOURCE_TYPE_BUFFER)
    {
        cauldron::ResourceFlags flags = GetFrameworkResourceFlagsApi(desc.usage);
        return cauldron::BufferDesc::Data(L"", desc.size, desc.stride, desc.alignment, flags);
    }
    else
    {
        cauldron::CauldronCritical(L"Description should be a texture.");
        return cauldron::BufferDesc();
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
// Framework to FFX conversion functions

inline FfxSurfaceFormat GetFfxSurfaceFormat(cauldron::ResourceFormat format)
{
    switch (format)
    {
    case (cauldron::ResourceFormat::RGBA32_TYPELESS):
        return FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS;
    case (cauldron::ResourceFormat::RGBA32_UINT):
        return FFX_SURFACE_FORMAT_R32G32B32A32_UINT;
    case (cauldron::ResourceFormat::RGBA32_FLOAT):
        return FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT;
    case (cauldron::ResourceFormat::RGBA16_TYPELESS):
        return FFX_SURFACE_FORMAT_R16G16B16A16_TYPELESS;
    case (cauldron::ResourceFormat::RGBA16_FLOAT):
        return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
    case (cauldron::ResourceFormat::RGB32_FLOAT):
        return FFX_SURFACE_FORMAT_R32G32B32_FLOAT;
    case (cauldron::ResourceFormat::RG32_TYPELESS):
        return FFX_SURFACE_FORMAT_R32G32_TYPELESS;
    case (cauldron::ResourceFormat::RG32_FLOAT):
        return FFX_SURFACE_FORMAT_R32G32_FLOAT;
    case (cauldron::ResourceFormat::R8_UINT):
        return FFX_SURFACE_FORMAT_R8_UINT;
    case (cauldron::ResourceFormat::R32_UINT):
        return FFX_SURFACE_FORMAT_R32_UINT;
    case (cauldron::ResourceFormat::RGBA8_TYPELESS):
        return FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS;
    case (cauldron::ResourceFormat::RGBA8_UNORM):
        return FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
    case (cauldron::ResourceFormat::RGBA8_SNORM):
        return FFX_SURFACE_FORMAT_R8G8B8A8_SNORM;
    case (cauldron::ResourceFormat::RGBA8_SRGB):
        return FFX_SURFACE_FORMAT_R8G8B8A8_SRGB;
    case (cauldron::ResourceFormat::BGRA8_TYPELESS):
        return FFX_SURFACE_FORMAT_B8G8R8A8_TYPELESS;
    case (cauldron::ResourceFormat::BGRA8_UNORM):
        return FFX_SURFACE_FORMAT_B8G8R8A8_UNORM;
    case (cauldron::ResourceFormat::BGRA8_SRGB):
        return FFX_SURFACE_FORMAT_B8G8R8A8_SRGB;
    case (cauldron::ResourceFormat::RG11B10_FLOAT):
        return FFX_SURFACE_FORMAT_R11G11B10_FLOAT;
    case (cauldron::ResourceFormat::RGB9E5_SHAREDEXP):
        return FFX_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP;
    case (cauldron::ResourceFormat::RGB10A2_UNORM):
        return FFX_SURFACE_FORMAT_R10G10B10A2_UNORM;
    case (cauldron::ResourceFormat::RGB10A2_TYPELESS):
        return FFX_SURFACE_FORMAT_R10G10B10A2_TYPELESS;
    case (cauldron::ResourceFormat::RG16_TYPELESS):
        return FFX_SURFACE_FORMAT_R16G16_TYPELESS;
    case (cauldron::ResourceFormat::RG16_FLOAT):
        return FFX_SURFACE_FORMAT_R16G16_FLOAT;
    case (cauldron::ResourceFormat::RG16_UINT):
        return FFX_SURFACE_FORMAT_R16G16_UINT;
    case (cauldron::ResourceFormat::RG16_SINT):
        return FFX_SURFACE_FORMAT_R16G16_SINT;
    case (cauldron::ResourceFormat::R16_TYPELESS):
        return FFX_SURFACE_FORMAT_R16_TYPELESS;
    case (cauldron::ResourceFormat::R16_FLOAT):
        return FFX_SURFACE_FORMAT_R16_FLOAT;
    case (cauldron::ResourceFormat::R16_UINT):
        return FFX_SURFACE_FORMAT_R16_UINT;
    case (cauldron::ResourceFormat::R16_UNORM):
        return FFX_SURFACE_FORMAT_R16_UNORM;
    case (cauldron::ResourceFormat::R16_SNORM):
        return FFX_SURFACE_FORMAT_R16_SNORM;
    case (cauldron::ResourceFormat::R8_TYPELESS):
        return FFX_SURFACE_FORMAT_R8_TYPELESS;
    case (cauldron::ResourceFormat::R8_UNORM):
        return FFX_SURFACE_FORMAT_R8_UNORM;
    case cauldron::ResourceFormat::RG8_TYPELESS:
        return FFX_SURFACE_FORMAT_R8G8_TYPELESS;
    case cauldron::ResourceFormat::RG8_UNORM:
        return FFX_SURFACE_FORMAT_R8G8_UNORM;
    case cauldron::ResourceFormat::RG8_UINT:
        return FFX_SURFACE_FORMAT_R8G8_UINT;
    case cauldron::ResourceFormat::R32_TYPELESS:
        return FFX_SURFACE_FORMAT_R32_TYPELESS;
    case cauldron::ResourceFormat::R32_FLOAT:
    case cauldron::ResourceFormat::D32_FLOAT:
        return FFX_SURFACE_FORMAT_R32_FLOAT;
    case (cauldron::ResourceFormat::Unknown):
        return FFX_SURFACE_FORMAT_UNKNOWN;
    default:
        cauldron::CauldronCritical(L"ValidationRemap: Unsupported format requested. Please implement.");
        return FFX_SURFACE_FORMAT_UNKNOWN;
    }
}

inline FfxResourceDescription GetFfxResourceDescription(const cauldron::GPUResource* pResource, FfxResourceUsage additionalUsages)
{
    FfxResourceDescription resourceDescription = {};

    // This is valid
    if (!pResource)
        return resourceDescription;

    if (pResource->IsBuffer())
    {
        const cauldron::BufferDesc& bufDesc = pResource->GetBufferResource()->GetDesc();

        resourceDescription.flags  = FFX_RESOURCE_FLAGS_NONE;
        resourceDescription.usage  = FFX_RESOURCE_USAGE_UAV;
        resourceDescription.width  = bufDesc.Size;
        resourceDescription.height = bufDesc.Stride;
        resourceDescription.format = GetFfxSurfaceFormat(cauldron::ResourceFormat::Unknown);

        // Does not apply to buffers
        resourceDescription.depth    = 0;
        resourceDescription.mipCount = 0;

        // Set the type
        resourceDescription.type = FFX_RESOURCE_TYPE_BUFFER;
    }
    else
    {
        const cauldron::TextureDesc& texDesc = pResource->GetTextureResource()->GetDesc();

        // Set flags properly for resource registration
        resourceDescription.flags = FFX_RESOURCE_FLAGS_NONE;
        resourceDescription.usage = IsDepth(texDesc.Format) ? FFX_RESOURCE_USAGE_DEPTHTARGET : FFX_RESOURCE_USAGE_READ_ONLY;
        if (static_cast<bool>(texDesc.Flags & cauldron::ResourceFlags::AllowUnorderedAccess))
            resourceDescription.usage = (FfxResourceUsage)(resourceDescription.usage | FFX_RESOURCE_USAGE_UAV);

        resourceDescription.width    = texDesc.Width;
        resourceDescription.height   = texDesc.Height;
        resourceDescription.depth    = texDesc.DepthOrArraySize;
        resourceDescription.mipCount = texDesc.MipLevels;
        resourceDescription.format   = GetFfxSurfaceFormat(texDesc.Format);

        resourceDescription.usage = (FfxResourceUsage)(resourceDescription.usage | additionalUsages);

        switch (texDesc.Dimension)
        {
        case cauldron::TextureDimension::Texture1D:
            resourceDescription.type = FFX_RESOURCE_TYPE_TEXTURE1D;
            break;
        case cauldron::TextureDimension::Texture2D:
            resourceDescription.type = FFX_RESOURCE_TYPE_TEXTURE2D;
            break;
        case cauldron::TextureDimension::CubeMap:
            // 2D array access to cube map resources
            if (FFX_CONTAINS_FLAG(resourceDescription.usage, FFX_RESOURCE_USAGE_ARRAYVIEW))
                resourceDescription.type = FFX_RESOURCE_TYPE_TEXTURE2D;
            else
                resourceDescription.type = FFX_RESOURCE_TYPE_TEXTURE_CUBE;
            break;
        case cauldron::TextureDimension::Texture3D:
            resourceDescription.type = FFX_RESOURCE_TYPE_TEXTURE3D;
            break;
        default:
            cauldron::CauldronCritical(L"FFXInterface: Cauldron: Unsupported texture dimension requested. Please implement.");
            break;
        }
    }

    return resourceDescription;
}

} // namespace SDKWrapper
