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
#include "render/renderdefines.h"
#include "render/resourceresizedlistener.h"
#include "render/resourceview.h"

#include <vector>
#include <utility>

namespace cauldron
{
    class Texture;

    /**
     * @class RasterView
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> representation of a rasterization resource view.
     * Needed to write to render targets and depth targets.
     *
     * @ingroup CauldronRender
     */
    class RasterView final
    {
    public:

        /**
         * @brief   Returns the <c><i>ResourceViewInfo</i></c> for the raster view.
         */
        ResourceViewInfo GetResourceView();
        const ResourceViewInfo GetResourceView() const;

    private:
        // No copy, No move
        NO_COPY(RasterView)
        NO_MOVE(RasterView)

        // Can only be allocated by the RasterViewAllocator or accessed by the CommandList
        friend class RasterViewAllocator;
        friend class CommandList;

        RasterView() = delete;
        RasterView(const Texture* pTex, ViewDimension dimension, int32_t mip = -1, int32_t arraySize = -1, int32_t firstArraySlice = -1);
        ~RasterView();

    private:

        const Texture*          m_pTexture = nullptr;
        ViewDimension   m_Dimension;
        int32_t                 m_Mip = -1;
        int32_t                 m_ArraySize = -1;
        int32_t                 m_FirstArraySlice = -1;

        ResourceView*           m_pResourceView = nullptr;
    };

    /**
     * @class RasterViewAllocator
     *
     * Allocator used for the creation of <c><i>RasterView</i></c> instances.
     *
     * @ingroup CauldronRender
     */
    class RasterViewAllocator final : public ResourceResizedListener
    {
    public:
        
        /**
         * @brief   Returns a new <c><i>RasterView</i></c> instance mapped to the specified parameters. Defaults to whole resource view.
         */
        const RasterView* RequestRasterView(const Texture* pTex, ViewDimension dimension, int32_t mip = -1, int32_t arraySize = -1, int32_t firstArraySlice = -1);

        /**
         * @brief   Callback invoked when a resize event occurs. Rebinds resized resources to their resource views.
         */
        void OnResourceResized() override;

    private:
        // No copy, No move
        NO_COPY(RasterViewAllocator)
        NO_MOVE(RasterViewAllocator)

        friend class Framework;
        RasterViewAllocator();
        ~RasterViewAllocator();

        RasterView* FindRasterView(const Texture* pTex, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstArraySlice);

    private:
        std::vector<std::pair<const Texture*, std::vector<RasterView*>>>  m_AllocatedRasterViews = {};
    };

} // namespace cauldron
