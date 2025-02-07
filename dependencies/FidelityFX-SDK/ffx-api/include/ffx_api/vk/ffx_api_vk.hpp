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
#include "ffx_api_vk.h"

// Helper types for header initialization. Api definition is in .h file.

namespace ffx
{

template<>
struct struct_type<ffxCreateBackendVKDesc> : std::integral_constant<uint64_t, FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK> {};

struct CreateBackendVKDesc : public InitHelper<ffxCreateBackendVKDesc> {};

template<>
struct struct_type<ffxCreateContextDescFrameGenerationSwapChainVK> : std::integral_constant<uint64_t, FFX_API_CREATE_CONTEXT_DESC_TYPE_FGSWAPCHAIN_VK> {};

struct CreateContextDescFrameGenerationSwapChainVK : public InitHelper<ffxCreateContextDescFrameGenerationSwapChainVK> {};

template<>
struct struct_type<ffxCreateContextDescFrameGenerationSwapChainModeVK> : std::integral_constant<uint64_t, FFX_API_CREATE_CONTEXT_DESC_TYPE_FGSWAPCHAIN_MODE_VK> {};

struct CreateContextDescFrameGenerationSwapChainModeVK : public InitHelper<ffxCreateContextDescFrameGenerationSwapChainModeVK> {};

template<>
struct struct_type<ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceVK> : std::integral_constant<uint64_t, FFX_API_CONFIGURE_DESC_TYPE_FGSWAPCHAIN_REGISTERUIRESOURCE_VK> {};

struct ConfigureDescFrameGenerationSwapChainRegisterUiResourceVK : public InitHelper<ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceVK> {};

template<>
struct struct_type<ffxQueryDescFrameGenerationSwapChainInterpolationCommandListVK> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_INTERPOLATIONCOMMANDLIST_VK> {};

struct QueryDescFrameGenerationSwapChainInterpolationCommandListVK : public InitHelper<ffxQueryDescFrameGenerationSwapChainInterpolationCommandListVK> {};

template<>
struct struct_type<ffxQueryDescFrameGenerationSwapChainInterpolationTextureVK> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_INTERPOLATIONTEXTURE_VK> {};

struct QueryDescFrameGenerationSwapChainInterpolationTextureVK : public InitHelper<ffxQueryDescFrameGenerationSwapChainInterpolationTextureVK> {};

template<>
struct struct_type<ffxQueryDescSwapchainReplacementFunctionsVK> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_FUNCTIONS_VK> {};

struct QueryDescSwapchainReplacementFunctionsVK : public InitHelper<ffxQueryDescSwapchainReplacementFunctionsVK> {};

template <>
struct struct_type<ffxDispatchDescFrameGenerationSwapChainWaitForPresentsVK> : std::integral_constant<uint64_t, FFX_API_DISPATCH_DESC_TYPE_FGSWAPCHAIN_WAIT_FOR_PRESENTS_VK>{};

struct DispatchDescFrameGenerationSwapChainWaitForPresentsVK : public InitHelper<ffxDispatchDescFrameGenerationSwapChainWaitForPresentsVK>{};

template<>
struct struct_type<ffxConfigureDescFrameGenerationSwapChainKeyValueVK> : std::integral_constant<uint64_t, FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_KEYVALUE_VK> {};

struct ConfigureDescFrameGenerationSwapChainKeyValueVK : public InitHelper<ffxConfigureDescFrameGenerationSwapChainKeyValueVK> {};

template<>
struct struct_type<ffxQueryFrameGenerationSwapChainGetGPUMemoryUsageVK> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_GPU_MEMORY_USAGE_VK> {};

struct QueryFrameGenerationSwapChainGetGPUMemoryUsageVK : public InitHelper<ffxQueryFrameGenerationSwapChainGetGPUMemoryUsageVK> {};

}
