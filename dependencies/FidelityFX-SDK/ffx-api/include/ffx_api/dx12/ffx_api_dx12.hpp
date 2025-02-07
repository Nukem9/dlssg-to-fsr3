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

#include "../ffx_api.hpp"
#include "ffx_api_dx12.h"

// Helper types for header initialization. Api definition is in .h file.

namespace ffx
{

template<>
struct struct_type<ffxCreateBackendDX12Desc> : std::integral_constant<uint64_t, FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12> {};

struct CreateBackendDX12Desc : public InitHelper<ffxCreateBackendDX12Desc> {};

template<>
struct struct_type<ffxCreateContextDescFrameGenerationSwapChainWrapDX12> : std::integral_constant<uint64_t, FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WRAP_DX12> {};

struct CreateContextDescFrameGenerationSwapChainWrapDX12 : public InitHelper<ffxCreateContextDescFrameGenerationSwapChainWrapDX12> {};

template<>
struct struct_type<ffxCreateContextDescFrameGenerationSwapChainNewDX12> : std::integral_constant<uint64_t, FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_NEW_DX12> {};

struct CreateContextDescFrameGenerationSwapChainNewDX12 : public InitHelper<ffxCreateContextDescFrameGenerationSwapChainNewDX12> {};

template<>
struct struct_type<ffxCreateContextDescFrameGenerationSwapChainForHwndDX12> : std::integral_constant<uint64_t, FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12> {};

struct CreateContextDescFrameGenerationSwapChainForHwndDX12 : public InitHelper<ffxCreateContextDescFrameGenerationSwapChainForHwndDX12> {};

template<>
struct struct_type<ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12> : std::integral_constant<uint64_t, FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_REGISTERUIRESOURCE_DX12> {};

struct ConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12 : public InitHelper<ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12> {};

template<>
struct struct_type<ffxQueryDescFrameGenerationSwapChainInterpolationCommandListDX12> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_INTERPOLATIONCOMMANDLIST_DX12> {};

struct QueryDescFrameGenerationSwapChainInterpolationCommandListDX12 : public InitHelper<ffxQueryDescFrameGenerationSwapChainInterpolationCommandListDX12> {};

template<>
struct struct_type<ffxQueryDescFrameGenerationSwapChainInterpolationTextureDX12> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_INTERPOLATIONTEXTURE_DX12> {};

struct QueryDescFrameGenerationSwapChainInterpolationTextureDX12 : public InitHelper<ffxQueryDescFrameGenerationSwapChainInterpolationTextureDX12> {};

template<>
struct struct_type<ffxDispatchDescFrameGenerationSwapChainWaitForPresentsDX12> : std::integral_constant<uint64_t, FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WAIT_FOR_PRESENTS_DX12> {};

struct DispatchDescFrameGenerationSwapChainWaitForPresentsDX12 : public InitHelper<ffxDispatchDescFrameGenerationSwapChainWaitForPresentsDX12> {};

template<>
struct struct_type<ffxConfigureDescFrameGenerationSwapChainKeyValueDX12> : std::integral_constant<uint64_t, FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_KEYVALUE_DX12> {};

struct ConfigureDescFrameGenerationSwapChainKeyValueDX12 : public InitHelper<ffxConfigureDescFrameGenerationSwapChainKeyValueDX12> {};

template<>
struct struct_type<ffxQueryFrameGenerationSwapChainGetGPUMemoryUsageDX12> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_GPU_MEMORY_USAGE_DX12> {};

struct QueryFrameGenerationSwapChainGetGPUMemoryUsageDX12 : public InitHelper<ffxQueryFrameGenerationSwapChainGetGPUMemoryUsageDX12> {};

}
