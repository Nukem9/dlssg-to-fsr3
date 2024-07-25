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

#include "render/rasterview.h"
#include "render/resourceviewallocator.h"
#include "render/texture.h"
#include "render/swapchain.h"
#include "core/framework.h"

namespace cauldron
{
    RasterView::RasterView(const Texture* pTex, ViewDimension dimension, int32_t mip /*= -1*/, int32_t arraySize /*= -1*/, int32_t firstArraySlice /*= -1*/) :
        m_pTexture(pTex), m_Dimension(dimension), m_Mip(mip), m_ArraySize(arraySize), m_FirstArraySlice(firstArraySlice)
    {
        // No resource allocation for swap chains
        if (pTex->IsSwapChain())
            return;

        // Allocate a resource view for this raster view
        ResourceViewType viewType = static_cast<bool>(pTex->GetDesc().Flags & ResourceFlags::AllowDepthStencil) ? ResourceViewType::DSV : ResourceViewType::RTV;
        if (viewType == ResourceViewType::RTV)
            GetResourceViewAllocator()->AllocateCPURenderViews(&m_pResourceView);
        else
            GetResourceViewAllocator()->AllocateCPUDepthViews(&m_pResourceView);

        // Bind it
        m_pResourceView->BindTextureResource(pTex->GetResource(), pTex->GetDesc(), viewType, dimension, m_Mip, m_ArraySize, m_FirstArraySlice);
    }

    RasterView::~RasterView()
    {
        delete m_pResourceView;
    }

    ResourceViewInfo RasterView::GetResourceView()
    {
        return m_pTexture->IsSwapChain() ? GetSwapChain()->GetBackBufferRTV() : m_pResourceView->GetViewInfo();
    }

    const ResourceViewInfo RasterView::GetResourceView() const
    {
        return m_pTexture->IsSwapChain() ? GetSwapChain()->GetBackBufferRTV() : m_pResourceView->GetViewInfo();
    }

    RasterViewAllocator::RasterViewAllocator()
    {
        // Mark ourselves as a resizable resource so we get a callback when resources are resized
        MarkAsResizableResourceDependent();
    }

    RasterViewAllocator::~RasterViewAllocator()
    {
        // Release all raster views
        for (auto& resourceItr = m_AllocatedRasterViews.begin(); resourceItr != m_AllocatedRasterViews.end(); ++resourceItr)
        {
            for (auto& rasterViewItr = resourceItr->second.begin(); rasterViewItr != resourceItr->second.end(); ++rasterViewItr)
                delete (*rasterViewItr);
            resourceItr->second.clear();
        }
        m_AllocatedRasterViews.clear();
    }

    const RasterView* RasterViewAllocator::RequestRasterView(const Texture* pTex, ViewDimension dimension, int32_t mip /*= -1*/, int32_t arraySize /*= -1*/, int32_t firstArraySlice /*= -1*/)
    {
        // Start by looking to see if we've already got a view allocated for this resource
        RasterView* pView = FindRasterView(pTex, dimension, mip, arraySize, firstArraySlice);

        // If we didn't find one, create one
        if (!pView)
        {
            pView = new RasterView(pTex, dimension, mip, arraySize, firstArraySlice);
            CauldronAssert(ASSERT_CRITICAL, pView, L"Could not allocate a new RasterView, critical exception");

            // Find the list of raster views for this resource
            bool foundResource = false;
            for (auto& resourceItr = m_AllocatedRasterViews.begin(); resourceItr != m_AllocatedRasterViews.end(); ++resourceItr)
            {
                if (resourceItr->first == pTex)
                {
                    resourceItr->second.push_back(pView);
                    foundResource = true;
                    break;
                }

            }

            // If it wasn't found, it's new, push it in
            if (!foundResource)
            {
                std::vector<RasterView*> rasterViews = { pView };
                m_AllocatedRasterViews.push_back(std::make_pair(pTex, std::move(rasterViews)));
            }
        }

        return pView;
    }

    void RasterViewAllocator::OnResourceResized()
    {
        // Rebind all the views for the new resources created due to resize
        for (auto& resourceItr = m_AllocatedRasterViews.begin(); resourceItr != m_AllocatedRasterViews.end(); ++resourceItr)
        {
            // Swap chain handles itself
            if (resourceItr->first->IsSwapChain() || !resourceItr->first->GetResource()->IsResizable())
                continue;

            for (auto& rasterViewItr = resourceItr->second.begin(); rasterViewItr != resourceItr->second.end(); ++rasterViewItr)
            {
                RasterView* pRasterView = *rasterViewItr;
                ResourceViewType viewType = static_cast<bool>(pRasterView->m_pTexture->GetDesc().Flags & ResourceFlags::AllowDepthStencil) ? ResourceViewType::DSV : ResourceViewType::RTV;
                pRasterView->m_pResourceView->BindTextureResource(pRasterView->m_pTexture->GetResource(), pRasterView->m_pTexture->GetDesc(), viewType, pRasterView->m_Dimension,
                                                                  pRasterView->m_Mip, pRasterView->m_ArraySize, pRasterView->m_FirstArraySlice);
            }
        }
    }

    RasterView* RasterViewAllocator::FindRasterView(const Texture* pTex, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstArraySlice)
    {
        // First find the resource
        for (auto& resourceItr = m_AllocatedRasterViews.begin(); resourceItr != m_AllocatedRasterViews.end(); ++resourceItr)
        {
            if (resourceItr->first == pTex)
            {
                // Special case for swapchain views
                if (resourceItr->first->IsSwapChain())
                    return resourceItr->second.at(0);

                for (auto& rasterViewItr = resourceItr->second.begin(); rasterViewItr != resourceItr->second.end(); ++rasterViewItr)
                {
                    if ((*rasterViewItr)->m_Dimension == dimension && (*rasterViewItr)->m_Mip == mip &&
                        (*rasterViewItr)->m_ArraySize == arraySize && (*rasterViewItr)->m_FirstArraySlice == firstArraySlice)
                        return *rasterViewItr;
                }

                // Couldn't find a matching view
                return nullptr;
            }
        }

        // Couldn't find a matching view
        return nullptr;
    }

} // namespace cauldron
