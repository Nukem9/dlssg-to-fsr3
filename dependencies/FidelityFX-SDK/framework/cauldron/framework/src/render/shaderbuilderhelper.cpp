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

#include "render/shaderbuilderhelper.h"
#include "render/mesh.h"
#include "misc/assert.h"

namespace cauldron
{
    // Helpers to construct the defines for the shader
    // Adds the texcoord support
    void AddTextureToDefineList(DefineList&        defineList,
                                uint32_t&          attributes,
                                const uint32_t     surfaceAttributes,
                                const Material*    pMaterial,
                                const TextureClass textureClass,
                                const wchar_t*     pTextureKey,
                                const wchar_t*     pTexCoordKey)
    {
        const cauldron::TextureInfo* pTextureInfo = pMaterial->GetTextureInfo(textureClass);
        if (pTextureInfo != nullptr)
        {
            defineList.insert(std::make_pair(pTextureKey, L""));
            defineList.insert(std::make_pair(pTexCoordKey, std::to_wstring(pTextureInfo->UVSet)));

            // add the attribute if the surface has it
            switch (pTextureInfo->UVSet)
            {
            case 0:
                attributes = attributes | (VertexAttributeFlag_Texcoord0 & surfaceAttributes);
                break;
            case 1:
                attributes = attributes | (VertexAttributeFlag_Texcoord1 & surfaceAttributes);
                break;
            default:
                CauldronCritical(L"Unsupported UV Set (%d). Only Sets 0 and 1 are currently supported.", pTextureInfo->UVSet);
                break;
            }
        }
    }

    size_t Hash(DefineList& defineList, uint32_t usedAttributes, const Surface* pSurface)
    {
        std::wstring concatString = L"";
        size_t       stringSize   = 0;
        for (auto iter = defineList.begin(); iter != defineList.end(); ++iter)
        {
            stringSize += iter->first.size() + iter->second.size() + 2;
        }
        concatString.reserve(stringSize);
        for (auto iter = defineList.begin(); iter != defineList.end(); ++iter)
        {
            concatString += iter->first;
            concatString += L";";
            concatString += iter->second;
            concatString += L";";
        }

        // add attribute formats too
        for (uint32_t attribute = 0; attribute < static_cast<uint32_t>(VertexAttributeType::Count); ++attribute)
        {
            // Check if the attribute is present
            if (usedAttributes & (0x1 << attribute))
            {
                concatString +=
                    std::to_wstring(static_cast<uint32_t>(pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(attribute)).ResourceDataFormat));
                concatString += L";";
            }
        }

        return std::hash<std::wstring>{}(concatString);
    }

} // namespace cauldron
