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

#include "ffx_api.hpp"
#include "ffx_upscale.h"

// Helper types for header initialization. Api definition is in .h file.

namespace ffx
{

template<>
struct struct_type<ffxCreateContextDescUpscale> : std::integral_constant<uint64_t, FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE> {};

struct CreateContextDescUpscale : public InitHelper<ffxCreateContextDescUpscale> {};

template<>
struct struct_type<ffxDispatchDescUpscale> : std::integral_constant<uint64_t, FFX_API_DISPATCH_DESC_TYPE_UPSCALE> {};

struct DispatchDescUpscale : public InitHelper<ffxDispatchDescUpscale> {};

template<>
struct struct_type<ffxQueryDescUpscaleGetUpscaleRatioFromQualityMode> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_UPSCALE_GETUPSCALERATIOFROMQUALITYMODE> {};

struct QueryDescUpscaleGetUpscaleRatioFromQualityMode : public InitHelper<ffxQueryDescUpscaleGetUpscaleRatioFromQualityMode> {};

template<>
struct struct_type<ffxQueryDescUpscaleGetRenderResolutionFromQualityMode> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_UPSCALE_GETRENDERRESOLUTIONFROMQUALITYMODE> {};

struct QueryDescUpscaleGetRenderResolutionFromQualityMode : public InitHelper<ffxQueryDescUpscaleGetRenderResolutionFromQualityMode> {};

template<>
struct struct_type<ffxQueryDescUpscaleGetJitterPhaseCount> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTERPHASECOUNT> {};

struct QueryDescUpscaleGetJitterPhaseCount : public InitHelper<ffxQueryDescUpscaleGetJitterPhaseCount> {};

template<>
struct struct_type<ffxQueryDescUpscaleGetJitterOffset> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTEROFFSET> {};

struct QueryDescUpscaleGetJitterOffset : public InitHelper<ffxQueryDescUpscaleGetJitterOffset> {};

template<>
struct struct_type<ffxDispatchDescUpscaleGenerateReactiveMask> : std::integral_constant<uint64_t, FFX_API_DISPATCH_DESC_TYPE_UPSCALE_GENERATEREACTIVEMASK> {};

struct DispatchDescUpscaleGenerateReactiveMask : public InitHelper<ffxDispatchDescUpscaleGenerateReactiveMask> {};

template<>
struct struct_type<ffxConfigureDescUpscaleKeyValue> : std::integral_constant<uint64_t, FFX_API_CONFIGURE_DESC_TYPE_UPSCALE_KEYVALUE> {};

struct ConfigureDescUpscaleKeyValue : public InitHelper<ffxConfigureDescUpscaleKeyValue> {};

template<>
struct struct_type<ffxQueryDescUpscaleGetGPUMemoryUsage> : std::integral_constant<uint64_t, FFX_API_QUERY_DESC_TYPE_UPSCALE_GPU_MEMORY_USAGE> {};

struct QueryDescUpscaleGetGPUMemoryUsage : public InitHelper<ffxQueryDescUpscaleGetGPUMemoryUsage> {};

}
