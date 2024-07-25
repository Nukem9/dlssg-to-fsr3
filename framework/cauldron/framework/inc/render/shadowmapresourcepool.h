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

#include "misc/math.h"
#include "render/gpuresource.h"
#include "render/texture.h"

#include <mutex>
#include <vector>

namespace cauldron
{
    /// An enumeration for shadow cell status
    ///
    /// @ingroup CauldronRender
    enum class CellStatus
    {
        Empty,          ///< The cell is empty.
        Allocated,      ///< The cell has been allocated.
        Subdivided      ///< The cell was subdivided into 4 cells.
    };

    /// An structure represnting a shadow cell entry
    ///
    /// @ingroup CauldronRender
    struct Cell
    {
        uint32_t size = 0;                      ///< The size (squared) of the cell.
        Rect rect;                              ///< The rect (coordinate representation) of the cell.
        CellStatus status = CellStatus::Empty;  ///< The <c><i>CellStatus</i></c> (defaults to CellStatus::Empty).
    };

    /**
     * @class ShadowMapAtlas
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> shadow map atlas representation.
     *
     * @ingroup CauldronRender
     */
    class ShadowMapAtlas
    {
    public:

        /**
         * @brief   Construction. Takes a texture to add to the atlast and it's size (squared).
         */
        ShadowMapAtlas(uint32_t size, Texture* pRenderTarget);

        /**
         * @brief   Destruction.
         */
        ~ShadowMapAtlas();

        /**
         * @brief   Returns the atlas's render target <c><i>Texture</i></c>.
         */
        const Texture* GetRenderTarget() const { return m_pRenderTarget; }

        /**
         * @brief   Returns the <c><i>Cell</i></c> information for the atlas cell corresponding to the requested index.
         */
        Cell GetCell(int32_t index) const;

        /**
         * @brief   Returns an index to a cell that can hold the requested size of texture, or -1 if none was found.
         */
        int32_t FindBestCell(uint32_t size) const;

        /**
         * @brief   Allocates a new sub-cell of specified size into the index-defined cell in the shadow atlas.
         */
        int32_t AllocateCell(uint32_t size, int32_t index);

        /**
         * @brief   Frees the specified cell.
         */
        void FreeCell(int32_t index);

    private:
        static int32_t GetChildrenBaseIndex(int32_t index);
        static int32_t GetParentIndex(int32_t index);
        void FindBestCellRecursive(size_t size, int32_t currentIndex, int32_t& foundCellIndex) const;

        // internal members
        std::vector<Cell> m_Cells;
        Texture* m_pRenderTarget = nullptr;
    };

    /// An enumeration for shadow map resolution occupancy
    ///
    /// @ingroup CauldronRender
    enum class ShadowMapResolution
    {
        Full = 1,       ///< Shadow map entry occupies full shadow map resolution.
        Half = 2,       ///< Shadow map entry occupies half the shadow map resolution.
        Quarter = 4,    ///< Shadow map entry occupies a quarter of the shadow map resolution.
        Eighth = 8,     ///< Shadow map entry occupies an eighth of the shadow map resolution.
    };

    /// Two constexpr global variables representing the full shadow map resolution
    ///
    /// @ingroup CauldronRender
    constexpr uint32_t g_ShadowMapTextureSize      = 2048;
    constexpr float    g_ShadowMapTextureSizeFloat = static_cast<float>(g_ShadowMapTextureSize);

    /**
     * @class ShadowMapResourcePool
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> shadow map resource pool. Handles shadow map texture allocations and memory management.
     *
     * @ingroup CauldronRender
     */
    class ShadowMapResourcePool
    {
    public:

        /**
         * @brief   Construction.
         */
        ShadowMapResourcePool();

        /**
         * @brief   Destruction.
         */
        virtual ~ShadowMapResourcePool();

        /**
         * @brief   Returns the number of currently allocated shadow map render targets.
         */
        uint32_t GetRenderTargetCount();

        /**
         * @brief   Returns a pointer to the specified render target <c><i>Texture</i></c>.
         */
        const Texture* GetRenderTarget(uint32_t index);

        /// An structure representing a view into a shadow map entry
        ///
        struct ShadowMapView
        {
            int index = 0;              ///< Shadow map view's index.
            int32_t cellIndex = -1;     ///< Shadow map view's associated cell ID.
            Rect rect;                  ///< Shadow map view's corresponding rect information.
        };

        /**
         * @brief   Searches the shadow map atlas for an existing entry that will satisfy the request.
         *          If none is found, will add a new entry to the shadow map atlast and divide it up as
         *          needed to return the requested view.
         */
        ShadowMapView GetNewShadowMap(ShadowMapResolution resolution = ShadowMapResolution::Full);

        /**
         * @brief   Releases the specified shadow map backing resource.
         */
        void ReleaseShadowMap(int index, int32_t cellIndex);

        /**
         * @brief   Returns the format used by shadow map textures.
         */
        const ResourceFormat GetShadowMapTextureFormat() const { return ResourceFormat::D32_FLOAT; }

        /**
         * @brief   Converts the provided rect into a <c><i>Viewport</i></c>.
         */
        static Viewport GetViewport(Rect rect);

        /**
         * @brief   Generates a transformation vector used to transform shadow map data in the shader to correct lookup data.
         */
        static Vec4 GetTransformation(Rect rect);

    private:
        std::vector<ShadowMapAtlas*> m_ShadowMapAtlases;
        std::mutex m_CriticalSection;
    };

} // namespace cauldron
