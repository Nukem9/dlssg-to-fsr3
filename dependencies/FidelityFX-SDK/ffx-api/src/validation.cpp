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

#include "validation.h"

#ifdef FFX_BACKEND_DX12
#include <ffx_api/dx12/ffx_api_dx12.h>
#endif // FFX_BACKEND_DX12
#ifdef FFX_BACKEND_VK
#include <ffx_api/vk/ffx_api_vk.h>
#endif // #ifdef FFX_BACKEND_VK

#include <ffx_api/ffx_framegeneration.h>
#include <ffx_api/ffx_upscale.h>

#include <unordered_map>
#include <algorithm>
#include <sstream>

#define MAP_ENUM_NAME(_value) {_value, #_value}

#ifdef FFXAPI_VALIDATION

static std::unordered_map<uint64_t, const char*> EnumNameMap{
#ifdef FFX_BACKEND_DX12
    MAP_ENUM_NAME(FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION),
    MAP_ENUM_NAME(FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_REGISTERUIRESOURCE_DX12),
    MAP_ENUM_NAME(FFX_API_CONFIGURE_DESC_TYPE_GLOBALDEBUG1),
    MAP_ENUM_NAME(FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12),
    MAP_ENUM_NAME(FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION),
    MAP_ENUM_NAME(FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_DX12),
    MAP_ENUM_NAME(FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_GET_VERSIONS),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_INTERPOLATIONCOMMANDLIST_DX12),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_INTERPOLATIONTEXTURE_DX12),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTEROFFSET),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTERPHASECOUNT),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_UPSCALE_GETRENDERRESOLUTIONFROMQUALITYMODE),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_UPSCALE_GETUPSCALERATIOFROMQUALITYMODE),
    MAP_ENUM_NAME(FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION),
    MAP_ENUM_NAME(FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE),
    MAP_ENUM_NAME(FFX_API_DISPATCH_DESC_TYPE_UPSCALE_GENERATEREACTIVEMASK),
    MAP_ENUM_NAME(FFX_API_DISPATCH_DESC_TYPE_UPSCALE),
    MAP_ENUM_NAME(FFX_API_CONFIGURE_DESC_TYPE_UPSCALE_KEYVALUE),
#elif FFX_BACKEND_VK
    MAP_ENUM_NAME(FFX_API_CONFIGURE_DESC_TYPE_FG),
    MAP_ENUM_NAME(FFX_API_CONFIGURE_DESC_TYPE_FGSWAPCHAIN_REGISTERUIRESOURCE_VK),
    MAP_ENUM_NAME(FFX_API_CONFIGURE_DESC_TYPE_GLOBALDEBUG1),
    MAP_ENUM_NAME(FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK),
    MAP_ENUM_NAME(FFX_API_CREATE_CONTEXT_DESC_TYPE_FG),
    MAP_ENUM_NAME(FFX_API_CREATE_CONTEXT_DESC_TYPE_FGSWAPCHAIN_VK),
    MAP_ENUM_NAME(FFX_API_CREATE_CONTEXT_DESC_TYPE_FSR_UPSCALE),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_GET_VERSIONS),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_FUNCTIONS_VK),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_INTERPOLATIONCOMMANDLIST_VK),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_INTERPOLATIONTEXTURE_DX12),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_INTERPOLATIONTEXTURE_VK),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_FSR_GETJITTEROFFSET),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_FSR_GETJITTERPHASECOUNT),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_FSR_GETRENDERRESOLUTIONFROMQUALITYMODE),
    MAP_ENUM_NAME(FFX_API_QUERY_DESC_TYPE_FSR_GETUPSCALERATIOFROMQUALITYMODE),
    MAP_ENUM_NAME(FFX_API_DISPATCH_DESC_TYPE_FG),
    MAP_ENUM_NAME(FFX_API_DISPATCH_DESC_TYPE_FG_PREPARE),
    MAP_ENUM_NAME(FFX_API_DISPATCH_DESC_TYPE_FSR_GENERATEREACTIVEMASK),
    MAP_ENUM_NAME(FFX_API_DISPATCH_DESC_TYPE_FSR_UPSCALE),
#endif // FFX_BACKEND_VK
};

static const char* GetEnumName(uint64_t value)
{
    auto it = EnumNameMap.find(value);
    if (it == EnumNameMap.end()) return "INVALID_ENUM";
    return it->second;
}

#endif

Validator& Validator::NoExtensions()
{
#ifdef FFXAPI_VALIDATION
    for (auto *it = header->pNext; it; it = it->pNext)
    {
        // invalid extension!
        std::wostringstream message{};
        message << "After header " << GetEnumName(header->type) << ": ignoring unexpected extension " << GetEnumName(it->type);
        callback(FFX_API_MESSAGE_TYPE_WARNING, message.str().c_str());
    }
#endif
    return *this;
}

Validator& Validator::AcceptExtensions(std::initializer_list<uint64_t> extensionsOnce, std::initializer_list<uint64_t> extensionsMany)
{
#ifdef FFXAPI_VALIDATION
    std::vector<bool> seenOnce(extsOnce.size(), false);
    for (auto *it = header->pNext; it; it = it->pNext)
    {
        bool canHave = extsMany.end() == std::find(extsMany.begin(), extsMany.end(), it->type);
        if (!canHave)
        {
            auto it_once = std::find(extsOnce.begin(), extsOnce.end(), it->type);
            if (it_once != extsOnce.end())
            {
                canHave = true; // prevent error further down even if present more than once.
                auto idx = std::distance(extsOnce.begin(), it_once);
                if (seenOnce[idx])
                {
                    // extension present more than once!
                    std::wostringstream message{};
                    message << "After header " << GetEnumName(header->type) << ": extension " << GetEnumName(it->type) << " present more than once";
                    callback(FFX_API_MESSAGE_TYPE_WARNING, message.str().c_str());
                }
                seenOnce[idx] = true;
            }
        }
        if (!canHave)
        {
            // invalid extension!
            std::wostringstream message{};
            message << "After header " << GetEnumName(header->type) << ": ignoring unexpected extension " << GetEnumName(it->type);
            callback(FFX_API_MESSAGE_TYPE_WARNING, message.str().c_str());
        }
    }
#endif

    return *this;
}
