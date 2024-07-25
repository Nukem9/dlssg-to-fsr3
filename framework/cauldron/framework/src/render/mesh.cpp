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

#include "render/mesh.h"
#include "render/buffer.h"
#include "render/material.h"
#include "render/rtresources.h"
#include "render/shaderbuilder.h"

namespace cauldron
{
    Surface::Surface(uint32_t surfaceID)
    {
        m_surfaceID = surfaceID;
    }

    Surface::~Surface()
    {
        // Delete memory for the index and vertex buffer info
        delete m_IndexBuffer.pBuffer;
        for (int i = 0; i < m_VertexBuffers.size(); ++i)
            delete m_VertexBuffers[i].pBuffer;
    }

    uint32_t Surface::GetAttributeStride(VertexAttributeType type) const
    {
        const auto& vbInfo = m_VertexBuffers[static_cast<uint32_t>(type)];
        return GetResourceFormatStride(vbInfo.ResourceDataFormat);
    }

    bool Surface::HasTranslucency() const
    {
        return m_pMaterial->GetBlendMode() == MaterialBlend::AlphaBlend;
    }

    uint32_t Surface::GetVertexAttributes() const
    {
        uint32_t attributes = 0;

        for (uint32_t i = 0; i < static_cast<uint32_t>(VertexAttributeType::Count); ++i)
        {
            if (m_VertexBuffers[i].pBuffer)
                attributes |= (0x1 << i);
        }

        return attributes;
    }

    void Surface::GetVertexAttributeDefines(uint32_t attributes, DefineList& defines)
    {
        static std::wstring s_DefineString[static_cast<uint32_t>(VertexAttributeType::Count)] =
        {
            L"HAS_POSITION",
            L"HAS_NORMAL",
            L"HAS_TANGENT",
            L"HAS_TEXCOORD_0",
            L"HAS_TEXCOORD_1",
            L"HAS_COLOR_0",
            L"HAS_COLOR_1",
            L"HAS_WEIGHTS_0",
            L"HAS_WEIGHTS_1",
            L"HAS_JOINTS_0",
            L"HAS_JOINTS_1",
            L"HAS_PREV_POSITION",
        };

        for (uint32_t i = 0; i < static_cast<uint32_t>(VertexAttributeType::Count); ++i)
        {
            if (attributes & (0x1 << i))
                defines.insert(std::make_pair(s_DefineString[i], L"1"));
        }
    }

    Mesh::Mesh(std::wstring name, size_t surfaceCount /*=1*/) :
        m_Name(name)
    {
        m_pBlas = BLAS::CreateBLAS();

        m_Surfaces.reserve(surfaceCount);

        for (size_t i = 0; i < surfaceCount; ++i)
            m_Surfaces.push_back(new Surface(uint32_t(i)));
    }

    Mesh::~Mesh()
    {
        for (auto iter = m_Surfaces.begin(); iter != m_Surfaces.end(); ++iter)
            delete (*iter);
        m_Surfaces.clear();

        delete m_pBlas;
    }

}
