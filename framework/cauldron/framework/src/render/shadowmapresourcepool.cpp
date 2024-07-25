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

#include "render/shadowmapresourcepool.h"
#include "misc/assert.h"
#include "core/framework.h"

namespace cauldron
{
    ShadowMapAtlas::ShadowMapAtlas(uint32_t size, Texture* pRenderTarget)
        : m_pRenderTarget{ pRenderTarget }
    {
        Cell rootCell = { size, { 0, 0, size, size }, CellStatus::Empty };
        m_Cells.push_back(rootCell);
    }

    ShadowMapAtlas::~ShadowMapAtlas()
    {
        CauldronAssert(ASSERT_CRITICAL, m_Cells[0].status == CellStatus::Empty, L"All the cells haven't been freed.");
        CauldronAssert(ASSERT_ERROR, m_pRenderTarget != nullptr, L"A shadow map atlas texture is null.");
        delete m_pRenderTarget;
    }

    Cell ShadowMapAtlas::GetCell(int32_t index) const
    {
        CauldronAssert(ASSERT_CRITICAL, m_Cells.size() > index, L"This cell index %d doesn't exist yet.", index);
        return m_Cells[index];
    }

    int32_t ShadowMapAtlas::FindBestCell(uint32_t size) const
    {
        int32_t foundCellIndex = -1;
        FindBestCellRecursive(size, 0, foundCellIndex);
        return foundCellIndex;
    }

    int32_t ShadowMapAtlas::AllocateCell(uint32_t size, int32_t index)
    {
        CauldronAssert(ASSERT_CRITICAL, m_Cells.size() > index, L"This cell index %d doesn't exist yet.", index);
        CauldronAssert(ASSERT_CRITICAL, m_Cells[index].status == CellStatus::Empty, L"The cell %d we are trying to allocate/subdivide isn't empty.", index);
        while (m_Cells[index].size > size)
        {
            // subdivide
            int32_t childrenBaseIndex = GetChildrenBaseIndex(index);
            if (m_Cells.size() < childrenBaseIndex + 4)
                m_Cells.resize(childrenBaseIndex + 4);

            Cell currentCell = m_Cells[index];
            uint32_t childCellSize = currentCell.size / 2;
            // init the children
            for (int32_t i = 0; i < 4; ++i)
            {
                Cell childCell;

                childCell.size = childCellSize;
                childCell.status = CellStatus::Empty;
                childCell.rect = currentCell.rect;

                switch (i)
                {
                case 0: // top left
                    childCell.rect.Right = childCell.rect.Left + childCellSize;
                    childCell.rect.Bottom = childCell.rect.Top + childCellSize;
                    break;
                case 1: // top right
                    childCell.rect.Left = childCell.rect.Right - childCellSize;
                    childCell.rect.Bottom = childCell.rect.Top + childCellSize;
                    break;
                case 2: // bottom right
                    childCell.rect.Right = childCell.rect.Left + childCellSize;
                    childCell.rect.Top = childCell.rect.Bottom - childCellSize;
                    break;
                case 3: // bottom right
                    childCell.rect.Left = childCell.rect.Right - childCellSize;
                    childCell.rect.Top = childCell.rect.Bottom - childCellSize;
                    break;
                }

                m_Cells[childrenBaseIndex + i] = childCell;
            }

            // mark the cell non empty
            m_Cells[index].status = CellStatus::Subdivided;

            // select the first child cell
            index = childrenBaseIndex;
        }

        CauldronAssert(ASSERT_CRITICAL, m_Cells.size() > index, L"This cell index doesn't exist yet.");
        CauldronAssert(ASSERT_CRITICAL, m_Cells[index].size == size, L"This cell size doesn't match the expected one.");
        CauldronAssert(ASSERT_CRITICAL, m_Cells[index].status == CellStatus::Empty, L"The cell %d we are trying to allocate isn't empty.", index);

        m_Cells[index].status = CellStatus::Allocated;
        return index;
    }

    void ShadowMapAtlas::FreeCell(int32_t index)
    {
        CauldronAssert(ASSERT_CRITICAL, m_Cells.size() > index, L"This cell index %d doesn't exist.", index);
        CauldronAssert(ASSERT_CRITICAL, m_Cells[index].status == CellStatus::Allocated, L"The cell %d we are trying to free isn't allocated.", index);

        // free the cell
        m_Cells[index].status = CellStatus::Empty;

        // merge with the sibling cells
        while (index != 0)
        {
            // move to parent
            index = GetParentIndex(index);

            CauldronAssert(ASSERT_CRITICAL, m_Cells[index].status == CellStatus::Subdivided, L"The cell %d isn't subdivided. We are trying to merge its children so it should be subdivided.", index);
            bool allChildrenEmpty = true;
            int32_t childrenBaseIndex = GetChildrenBaseIndex(index);
            for (int32_t i = 0; i < 4; ++i)
            {
                allChildrenEmpty &= (m_Cells[childrenBaseIndex + i].status == CellStatus::Empty);
            }

            if (allChildrenEmpty)
            {
                // merge the cells
                m_Cells[index].status = CellStatus::Empty;
            }
            else
            {
                // cannot merge because only child is still allocated or subdivided
                break;
            }
        }
    }

    int32_t ShadowMapAtlas::GetChildrenBaseIndex(int32_t index)
    {
        return (index << 2) + 1;
    }

    int32_t ShadowMapAtlas::GetParentIndex(int32_t index)
    {
        return (index - 1) >> 2;
    }

    void ShadowMapAtlas::FindBestCellRecursive(size_t size, int32_t currentIndex, int32_t& foundCellIndex) const
    {
        if (currentIndex < m_Cells.size())
        {
            Cell cell = m_Cells[currentIndex];

            if (cell.status == CellStatus::Empty) // we can only select this cell if it is completely empty
            {
                // only replace if this is a better cell
                //   - size is bigger than the requested one
                //   - size is smaller than the current best cell
                if (cell.size >= size && (foundCellIndex < 0 || m_Cells[foundCellIndex].size > cell.size))
                {
                    foundCellIndex = currentIndex;
                }
            }
            else if (cell.status == CellStatus::Subdivided) // explore children
            {
                int32_t childrenBaseIndex = GetChildrenBaseIndex(currentIndex);
                for (int32_t i = 0; i < 4; ++i)
                {
                    FindBestCellRecursive(size, childrenBaseIndex + i, foundCellIndex);
                }
            }
        }
    }

    ShadowMapResourcePool::ShadowMapResourcePool()
    {
    }

    ShadowMapResourcePool::~ShadowMapResourcePool()
    {
        // Release all resources
        std::lock_guard<std::mutex> lock(m_CriticalSection);
        for (auto pShadowMapAtlas : m_ShadowMapAtlases)
            delete pShadowMapAtlas;
        m_ShadowMapAtlases.clear();
    }

    uint32_t ShadowMapResourcePool::GetRenderTargetCount()
    {
        std::lock_guard<std::mutex> lock(m_CriticalSection);
        return static_cast<uint32_t>(m_ShadowMapAtlases.size());
    }

    const Texture* ShadowMapResourcePool::GetRenderTarget(uint32_t index)
    {
        if (index >= 0)
        {
            std::lock_guard<std::mutex> lock(m_CriticalSection);
            if (index < (uint32_t)m_ShadowMapAtlases.size())
            {
                return m_ShadowMapAtlases[index]->GetRenderTarget();
            }
        }
        return nullptr;
    }

    ShadowMapResourcePool::ShadowMapView ShadowMapResourcePool::GetNewShadowMap(ShadowMapResolution resolution)
    {
        ShadowMapView view;
        const uint32_t cellSize = g_ShadowMapTextureSize / static_cast<uint32_t>(resolution);

        std::lock_guard<std::mutex> lock(m_CriticalSection);
        view.index = -1;
        view.cellIndex = -1;
        for (int i = 0; i < m_ShadowMapAtlases.size(); ++i)
        {
            int32_t cellIndex = m_ShadowMapAtlases[i]->FindBestCell(cellSize);
            if (cellIndex >= 0) // a cell was found
            {
                uint32_t foundCellSize = m_ShadowMapAtlases[i]->GetCell(cellIndex).size;
                // if this isn't better than a previously found cell, skip to the next atlas
                if (view.index >= 0 && foundCellSize >= m_ShadowMapAtlases[view.index]->GetCell(view.cellIndex).size)
                    continue;

                // this cell is better, save it
                view.index = i;
                view.cellIndex = cellIndex;

                // if this is the best we can find, early exit
                if (foundCellSize == cellSize)
                    break;
            }
        }

        if (view.index >= 0 && view.cellIndex >= 0)
        {
            view.cellIndex = m_ShadowMapAtlases[view.index]->AllocateCell(cellSize, view.cellIndex);
            CauldronAssert(ASSERT_CRITICAL, view.cellIndex >= 0, L"Failed to allocate cell.");
            view.rect = m_ShadowMapAtlases[view.index]->GetCell(view.cellIndex).rect;
            return view;
        }

        // no cell could be inserted into the existing shadow maps

        TextureDesc desc;
        desc.Format = GetShadowMapTextureFormat();
        desc.Flags = ResourceFlags::AllowDepthStencil;
        desc.Width = g_ShadowMapTextureSize;
        desc.Height = g_ShadowMapTextureSize;
        desc.Dimension = TextureDimension::Texture2D;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;

        wchar_t buffer[16];
        memset(buffer, 0, 16 * sizeof(wchar_t));
        swprintf(buffer, 16, L"ShadowMapAtlas%2zd", m_ShadowMapAtlases.size());
        desc.Name = buffer;

        Texture* pRenderTarget = Texture::CreateTexture(&desc, static_cast<ResourceState>(ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource), nullptr);
        ShadowMapAtlas* pShadowMapAtlas = new ShadowMapAtlas(g_ShadowMapTextureSize, pRenderTarget);

        view.index = static_cast<int>(m_ShadowMapAtlases.size());
        view.cellIndex = pShadowMapAtlas->AllocateCell(cellSize, 0);
        view.rect = pShadowMapAtlas->GetCell(view.cellIndex).rect;

        m_ShadowMapAtlases.push_back(pShadowMapAtlas);

        return view;
    }

    void ShadowMapResourcePool::ReleaseShadowMap(int index, int32_t cellIndex)
    {
        if (index >= 0)
        {
            std::lock_guard<std::mutex> lock(m_CriticalSection);
            if (index < m_ShadowMapAtlases.size())
            {
                ShadowMapAtlas* pShadowMapAtlas = m_ShadowMapAtlases[index];
                pShadowMapAtlas->FreeCell(cellIndex);
            }
        }
    }

    Viewport ShadowMapResourcePool::GetViewport(Rect rect)
    {
        return {
            static_cast<float>(rect.Left),
            static_cast<float>(rect.Top),
            static_cast<float>(rect.Right - rect.Left),
            static_cast<float>(rect.Bottom - rect.Top),
            0.0f,
            1.0f
        };
    }

    Vec4 ShadowMapResourcePool::GetTransformation(Rect rect)
    {
        // transformations from clip space to UV space
        // xy: scale
        // zw: offset
        //
        //       1               0
        //       |               |
        // -1 ---+---> 1 => 0 ---+---> 1
        //       |               |
        //      -1               1
        //

        return Vec4(
             0.5f * static_cast<float>(rect.Right - rect.Left) / g_ShadowMapTextureSizeFloat,
            -0.5f * static_cast<float>(rect.Bottom - rect.Top) / g_ShadowMapTextureSizeFloat, // Y is negative
             0.5f * static_cast<float>(rect.Right + rect.Left) / g_ShadowMapTextureSizeFloat,
             0.5f * static_cast<float>(rect.Bottom + rect.Top) / g_ShadowMapTextureSizeFloat
        );
    }
} // namespace cauldron
