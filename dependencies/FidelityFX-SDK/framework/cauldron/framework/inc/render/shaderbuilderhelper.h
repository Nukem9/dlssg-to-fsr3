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

#include "shaderbuilder.h"
#include "render/material.h"

namespace cauldron
{
    class Surface;

    /// Helpers to construct the defines for the shader
    /// Adds the texcoord support
    /// 
    /// @ingroup CauldronRender
    void AddTextureToDefineList(DefineList&        defineList,
                                uint32_t&          attributes,
                                const uint32_t     surfaceAttributes,
                                const Material*    pMaterial,
                                const TextureClass textureClass,
                                const wchar_t*     pTextureKey,
                                const wchar_t*     pTexCoordKey);

    /// Helper function to construct hash from defines and attributes in a geometric shader
    /// 
    /// @ingroup CauldronRender
    size_t Hash(DefineList& defineList, uint32_t usedAttributes, const Surface* pSurface);

} // namespace cauldron
