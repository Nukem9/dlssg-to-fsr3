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

#include "render/dynamicresourcepool.h"
#include "misc/assert.h"
#include "core/framework.h"
#include "render/texture.h"
#include "render/buffer.h"

namespace cauldron
{
    DynamicResourcePool::DynamicResourcePool()
    {
    }

    DynamicResourcePool::~DynamicResourcePool()
    {
        // Release all resources
        for (auto texEntry : m_Textures)
            delete texEntry.second;
        m_Textures.clear();

        for (auto bufEntry : m_Buffers)
            delete bufEntry.second;
        m_Buffers.clear();
    }

    void DynamicResourcePool::OnResolutionChanged(const ResolutionInfo& resInfo)
    {
        std::lock_guard<std::mutex> lock(m_CriticalSection);

        for (auto t : m_ResizableTextures)
            t->OnRenderingResolutionResize(resInfo.DisplayWidth, resInfo.DisplayHeight, resInfo.RenderWidth, resInfo.RenderHeight);
        for (auto b : m_ResizableBuffers)
            b->OnRenderingResolutionResize(resInfo.DisplayWidth, resInfo.DisplayHeight, resInfo.RenderWidth, resInfo.RenderHeight);
    }

    void DynamicResourcePool::DestroyResource(const GPUResource* pResource)
    {
        if (pResource->IsBuffer())
        {
            for (auto bufPairItr = m_Buffers.begin(); bufPairItr != m_Buffers.end(); ++bufPairItr)
            {
                if (bufPairItr->second == pResource->GetBufferResource())
                {
                    delete bufPairItr->second;
                    bufPairItr = m_Buffers.erase(bufPairItr);
                    return;
                }
            }
        }
        else
        {
            for (auto texPairItr = m_Textures.begin(); texPairItr != m_Textures.end(); ++texPairItr)
            {
                if (texPairItr->second == pResource->GetTextureResource())
                {
                    delete texPairItr->second;
                    texPairItr = m_Textures.erase(texPairItr);
                    return;
                }
            }
        }

        CauldronWarning(L"Could not find resource %ls for destruction.", pResource->GetName());
    }

    const Texture* DynamicResourcePool::GetTexture(const wchar_t* name)
    {
        std::lock_guard<std::mutex> lock(m_CriticalSection);
        CauldronAssert(ASSERT_WARNING, std::this_thread::get_id() != GetFramework()->MainThreadID() || !GetFramework()->IsRunning(), L"Performance Warning: Search for texture by name on the main thread while app is running.");
        for (auto texEntry : m_Textures)
        {
            if (texEntry.first == name)
                return texEntry.second;
        }

        return nullptr;
    }

    const Buffer* DynamicResourcePool::GetBuffer(const wchar_t* name)
    {
        std::lock_guard<std::mutex> lock(m_CriticalSection);
        CauldronAssert(ASSERT_WARNING, std::this_thread::get_id() != GetFramework()->MainThreadID() || !GetFramework()->IsRunning(), L"Performance Warning: Search for buffer by name on the main thread while app is running.");
        for (auto bufEntry : m_Buffers)
        {
            if (bufEntry.first == name)
                return bufEntry.second;
        }

        return nullptr;
    }

    const Texture* DynamicResourcePool::CreateTexture(const TextureDesc* pDesc, ResourceState initialState, TextureResizeFunction fn)
    {
        Texture* pTex = Texture::CreateTexture(pDesc, initialState, fn);
        
        std::lock_guard<std::mutex> lock(m_CriticalSection);
        #ifdef _DEBUG
            // In debug make sure we are not creating resources with the same name (as it could lead to errors later if someone looks it up by name)
            for (auto texEntry : m_Textures)
            {
                if (texEntry.first == pDesc->Name)
                    CauldronError(L"DynamicResourcePool: Creating multiple textures with the name %ls, this can cause conflicts if searching for textures by name later.", pDesc->Name.c_str());
            }
        #endif // _DEBUG
        m_Textures.push_back(std::make_pair(pDesc->Name, pTex));

        if (fn != nullptr)
            m_ResizableTextures.push_back(pTex);

        return pTex;
    }

    const Texture* DynamicResourcePool::CreateRenderTexture(const TextureDesc* pDesc, TextureResizeFunction fn)
    {
        // Explicitly add depth or render target flag when creating render textures
        TextureDesc desc = *pDesc;
        if (IsDepth(pDesc->Format))
            desc.Flags = static_cast<ResourceFlags>(desc.Flags | ResourceFlags::AllowDepthStencil);
        else
            desc.Flags = static_cast<ResourceFlags>(desc.Flags | ResourceFlags::AllowRenderTarget);

        Texture* pRT = Texture::CreateTexture(pDesc, static_cast<ResourceState>(ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource), fn);

        std::lock_guard<std::mutex> lock(m_CriticalSection);
        #ifdef _DEBUG
                // In debug make sure we are not creating resources with the same name (as it could lead to errors later if someone looks it up by name)
                for (auto texEntry : m_Textures)
                {
                    if (texEntry.first == pDesc->Name)
                        CauldronError(L"DynamicResourcePool: Creating multiple textures with the name %ls, this can cause conflicts if searching for textures by name later.", pDesc->Name.c_str());
                }
        #endif  // _DEBUG

        m_Textures.push_back(std::make_pair(pDesc->Name, pRT));

        if (fn != nullptr)
            m_ResizableTextures.push_back(pRT);

        return pRT;
    }

    const Buffer* DynamicResourcePool::CreateBuffer(const BufferDesc* pDesc, ResourceState initialState, BufferResizeFunction fn/*=nullptr*/)
    {
        Buffer* pBuf = Buffer::CreateBufferResource(pDesc, initialState, fn);
        
        std::lock_guard<std::mutex> lock(m_CriticalSection);
        #ifdef _DEBUG
                // In debug make sure we are not creating resources with the same name (as it could lead to errors later if someone looks it up by name)
                for (auto bufEntry : m_Buffers)
                {
                    if (bufEntry.first == pDesc->Name)
                    CauldronError(L"DynamicResourcePool: Creating multiple buffers with the name %ls, this can cause conflicts if searching for buffers by name later.", pDesc->Name.c_str());
                }
        #endif  // _DEBUG

        m_Buffers.push_back(std::make_pair(pDesc->Name, pBuf));

        if (fn != nullptr)
            m_ResizableBuffers.push_back(pBuf);

        return pBuf;
    }

} // namespace cauldron
