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

#include "misc/helpers.h"
#include "misc/math.h"
#include "render/renderdefines.h"

#include <array>
#include <vector>
#include <cstring>

namespace cauldron
{
    class BLAS;
    class Buffer;
    class Material;

    /// Structure representing the vertex buffer information for a vertex data stream (component channel).
    ///
    /// @ingroup CauldronRender
    struct VertexBufferInformation
    {
        AttributeFormat AttributeDataFormat = AttributeFormat::Unknown; ///< Vertex buffer stream attribute format.
        ResourceFormat  ResourceDataFormat  = ResourceFormat::Unknown;  ///< Vertex buffer stream resource format.
        uint32_t        Count               = 0;                        ///< Number of entries in the vertex buffer stream.
        Buffer*         pBuffer             = nullptr;                  ///< <c><i>Buffer</i></c> resource backing the vertex buffer stream.
    };

    /// Structure representing the index buffer information.
    ///
    /// @ingroup CauldronRender
    struct IndexBufferInformation
    {
        ResourceFormat IndexFormat = ResourceFormat::Unknown;           ///< Index resource format (16/32-bit)
        uint32_t       Count       = 0;                                 ///< Number of entries in the index buffer.
        Buffer*        pBuffer     = nullptr;                           ///< <c><i>Buffer</i></c> resource backing the index buffer.
    };

    /**
     * @class Surface
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> surface representation. A surface has a <c><i>Material</i></c> and is one of a
       number of surfaces that make up a <c><i>Mesh</i></c>.
     *
     * @ingroup CauldronRender
     */
    class Surface
    {
    public:

        /**
         * @brief   Construction.
         */
        Surface() = delete;

        /**
         * @brief   Constructor, sets the surfaceID [0,#surfaces in the mesh]
         */
        Surface(uint32_t surfaceID);

        /**
         * @brief   Destruction.
         */
        ~Surface();

        /**
         * @brief   Returns the geometric center of all surface geometry.
         */
        Vec4& Center() { return m_Center; }
        Vec4 Center() const { return m_Center; }

        /**
         * @brief   Returns the geometric radius of all surface geometry.
         */
        Vec4& Radius() { return m_Radius; }
        Vec4 Radius() const { return m_Radius; }

        /**
         * @brief   Returns the index buffer information for the surface geometry.
         */
        const IndexBufferInformation& GetIndexBuffer() const { return m_IndexBuffer; }
        IndexBufferInformation& GetIndexBuffer() { return m_IndexBuffer; }

        /**
         * @brief   Returns the vertex buffer information for a specific vertex attribute of the surface geometry.
         */
        const VertexBufferInformation& GetVertexBuffer(VertexAttributeType Type) const { return m_VertexBuffers[static_cast<uint32_t>(Type)]; }
        VertexBufferInformation& GetVertexBuffer(VertexAttributeType type) { return m_VertexBuffers[static_cast<uint32_t>(type)]; }

        /**
         * @brief   Returns the vertex buffer stream stride for a specific vertex attribute.
         */
        uint32_t GetAttributeStride(VertexAttributeType type) const;

        /**
         * @brief   Sets the surface's material for rendering.
         */
        void SetMaterial(const Material* pMaterial) { m_pMaterial = pMaterial; }

        /**
         * @brief   Gets the surface's material for rendering.
         */
        const Material* GetMaterial() const { return m_pMaterial; }

        /**
         * @brief   Returns true if the material has translucent texture or geometric information.
         */
        bool HasTranslucency() const;

        /**
         * @brief   Returns an ORed bitmask representing all vertex attributes in a surface's geometry.
         */
        uint32_t GetVertexAttributes() const;

        /**
         * @brief   Returns the shader defines necessary to build all surface vertex attribute fetchers in a shader.
         */
        static void GetVertexAttributeDefines(uint32_t attributes, DefineList& defines);

        /**
         * @brief   Returns the ID of the surface in the Mesh.
         */
        const uint32_t GetSurfaceID() const { return m_surfaceID; }

    private:
        NO_COPY(Surface)
        NO_MOVE(Surface)

        // For debug render
        Vec4   m_Center = Vec4(0, 0, 0, 0);
        Vec4   m_Radius = Vec4(0, 0, 0, 0);

        IndexBufferInformation m_IndexBuffer;
        std::array<VertexBufferInformation, static_cast<uint32_t>(VertexAttributeType::Count)> m_VertexBuffers;

        // The surface index inside the Mesh
        uint32_t m_surfaceID = 0;

        const Material* m_pMaterial = nullptr;
    };

    /**
     * @class Mesh
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> mesh representation. Meshes are made up of a combination of <c><i>Surface</i></c>es.
     *
     * @ingroup CauldronRender
     */
    class Mesh
    {
    public:

        /**
         * @brief   Construction.
         */
        Mesh(std::wstring name, size_t surfaceCount = 1);

        /**
         * @brief   Destruction.
         */
        ~Mesh();

        /**
         * @brief   Returns the number of surfaces in this mesh.
         */
        size_t GetNumSurfaces() const { return m_Surfaces.size(); }

        /**
         * @brief   Returns a surface pointer by index.
         */
        const Surface* GetSurface(uint32_t index) const { return m_Surfaces[index]; }
        Surface* GetSurface(uint32_t index) { return m_Surfaces[index]; }

        /**
         * @brief   Returns a point to the Bottom-Level Acceleration Structure (<c><i>BLAS</i></c>) for the mesh.
         */
        BLAS* GetStaticBlas() { return m_pBlas; }
        const BLAS* GetStaticBlas() const { return m_pBlas; }

        /**
         * @brief  Stores the index of this mesh.
         */
        void setMeshIndex(uint32_t index) { m_Index = index; }

        /**
         * @brief  Returns the index of this mesh.
         */
        const uint32_t GetMeshIndex() const { return m_Index; }

        void setAnimatedBlas(bool animatedBlas) { m_bAnimatedBLAS = animatedBlas; }
        const bool HasAnimatedBlas() const { return m_bAnimatedBLAS; }

    private:
        NO_COPY(Mesh)
        NO_MOVE(Mesh)

        Mesh() = delete;

        BLAS*                   m_pBlas;
        uint32_t                m_Index;
        std::wstring            m_Name;
        bool                    m_bAnimatedBLAS = false;
        std::vector<Surface*>   m_Surfaces;
    };
}
