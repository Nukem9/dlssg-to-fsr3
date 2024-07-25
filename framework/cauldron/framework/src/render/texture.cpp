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

#include "render/texture.h"
#include "core/framework.h"
#include "misc/assert.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING    // To avoid receiving deprecation error since we are using C++11 only
#include <experimental/filesystem>
using namespace std::experimental;

namespace cauldron
{
    //////////////////////////////////////////////////////////////////////////
    // Texture

    Texture* Texture::CreateTexture(const TextureDesc* pDesc, ResourceState initialState, ResizeFunction fn/*=nullptr*/)
    {
        return new Texture(pDesc, initialState, fn);
    }

    Texture* Texture::CreateSwapchainTexture(const TextureDesc* pDesc, GPUResource* pResource)
    {
        return new Texture(pDesc, pResource);
    }

    Texture* Texture::CreateContentTexture(const TextureDesc* pDesc)
    {
        return new Texture(pDesc, ResourceState::CopyDest, nullptr);
    }

    Texture::~Texture()
    {
        delete m_pResource;
    }

    void Texture::OnRenderingResolutionResize(uint32_t outputWidth, uint32_t outputHeight, uint32_t renderingWidth, uint32_t renderingHeight)
    {
        CauldronAssert(ASSERT_CRITICAL, m_ResizeFn != nullptr, L"There's no method to resize the texture");

        // get the new information
        m_ResizeFn(m_TextureDesc, outputWidth, outputHeight, renderingWidth, renderingHeight);

        // recreate resource
        Recreate();
    }

    //////////////////////////////////////////////////////////////////////////
    // SwapChainRenderTarget

    // Special case, so init a little differently
    SwapChainRenderTarget::SwapChainRenderTarget(const TextureDesc* pDesc, std::vector<GPUResource*>& resources) :
        Texture(pDesc, resources[0])
    {
        // Initialize resources pointers
        for (int i = 1; i < resources.size(); ++i)
            m_AdditionalTextures.push_back(Texture::CreateSwapchainTexture(pDesc, resources[i]));

        // Setup all texture pointers and resource ownership
        Texture::GetResource()->SetOwner(this);
        m_TextureResources.push_back(this);
        for (int i = 0; i < m_AdditionalTextures.size(); ++i)
        {
            m_TextureResources.push_back(m_AdditionalTextures[i]);
            m_AdditionalTextures[i]->GetResource()->SetOwner(m_TextureResources.back());
        }
    }

    SwapChainRenderTarget::~SwapChainRenderTarget()
    {
        // Delete additional resources
        if (m_AdditionalTextures.size())
            ClearResources();
    }

    void SwapChainRenderTarget::SetCurrentBackBufferIndex(uint32_t index)
    {
        CauldronAssert(ASSERT_CRITICAL, index < m_TextureResources.size(), L"Backbuffer index out of bounds.");
        m_CurrentBackBuffer = index;
    }

    void SwapChainRenderTarget::ClearResources()
    {
        delete m_pResource;
        m_pResource = nullptr;

        for (auto texIter = m_AdditionalTextures.begin(); texIter != m_AdditionalTextures.end(); ++texIter)
            delete (*texIter);
        m_AdditionalTextures.clear();
        m_TextureResources.clear();
    }

    void SwapChainRenderTarget::Update(const TextureDesc* pDesc, std::vector<GPUResource*>& resources)
    {
        // Update all backing resources
        m_TextureDesc = *pDesc;

        // Copy the new main resource
        int resourceIndex = 0;
        m_pResource = resources[resourceIndex++];

        // Initialize resources pointers
        for (int i = 1; i < resources.size(); ++i)
            m_AdditionalTextures.push_back(Texture::CreateSwapchainTexture(pDesc, resources[i]));

        // Setup all texture pointers
        m_TextureResources.push_back(this);
        for (int i = 0; i < m_AdditionalTextures.size(); ++i)
            m_TextureResources.push_back(m_AdditionalTextures[i]);
    }

} // namespace cauldron
