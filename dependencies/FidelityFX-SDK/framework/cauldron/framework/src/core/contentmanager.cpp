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

#include "core/contentmanager.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/framework.h"
#include "core/taskmanager.h"
#include "core/loaders/gltfloader.h"
#include "core/scene.h"
#include "misc/assert.h"
#include "render/material.h"
#include "render/texture.h"

using namespace std::experimental;

namespace cauldron
{
    ContentManager::ContentManager()
    {
        // Init our content loaders
        m_ContentLoaders.resize(static_cast<uint32_t>(LoaderType::Count));
        m_ContentLoaders[static_cast<uint32_t>(LoaderType::GLTF)]       = new GLTFLoader();
        m_ContentLoaders[static_cast<uint32_t>(LoaderType::Texture)]    = new TextureLoader();
        m_ContentLoaders[static_cast<uint32_t>(LoaderType::Particle)]    = new ParticleLoader();
    }

    ContentManager::~ContentManager()
    {
        // Clear out all loaders
        for (auto loadIter = m_ContentLoaders.begin(); loadIter != m_ContentLoaders.end(); ++loadIter)
            delete (* loadIter);
        m_ContentLoaders.clear();
    }

    void ContentManager::Shutdown()
    {
        // Start unloading all content
        for (auto contentIter = m_LoadedContentBlocks.begin(); contentIter != m_LoadedContentBlocks.end(); ++contentIter)
        {
            UnloadContentBlock(contentIter->second, 0);
        }
        m_LoadedContentBlocks.clear();

        // Delete all the data (will also unload textures)
        DeleteUnloadedContent(0);

        // Remove remaining texture content
        for (auto texIter = m_LoadedTextureContent.begin(); texIter != m_LoadedTextureContent.end(); ++texIter)
            delete texIter->second;
        m_LoadedTextureContent.clear();

    }

    void ContentManager::LoadGLTFToScene(filesystem::path& gltfFile)
    {
        // Perform an asynchronous load from the gltf loader
        ContentLoader* pLoader = m_ContentLoaders[static_cast<uint32_t>(LoaderType::GLTF)];
        CauldronAssert(ASSERT_ERROR, pLoader, L"Could not find GLTF loader");
        if (pLoader)
        {
            // Increment content load count to track
            ++m_ActiveContentLoads;

            pLoader->LoadAsync(&gltfFile);
        }

    }

    void ContentManager::LoadParticlesToScene(const std::vector<ParticleSpawnerDesc>& descList)
    {
        // Perform an asynchronous load from the particle loader
        ContentLoader* pLoader = m_ContentLoaders[static_cast<uint32_t>(LoaderType::Particle)];
        CauldronAssert(ASSERT_ERROR, pLoader, L"Could not find particle loader");
        if (pLoader)
        {
            // Increment content load count to track
            ++m_ActiveContentLoads;

            // Pack the data up for loading
            ParticleLoadParams loadParams;
            for (auto& desc : descList) {
                loadParams.LoadData.push_back(desc);
            }

            pLoader->LoadMultipleAsync(&loadParams);
        }
    }

    void ContentManager::LoadTexture(const TextureLoadInfo& loadInfo, TextureLoadCompletionCallbackFn pCompletionCallback/*= nullptr*/, void* pAdditionalParams /*=nullptr*/)
    {
        ContentLoader* pLoader = m_ContentLoaders[static_cast<uint32_t>(LoaderType::Texture)];
        CauldronAssert(ASSERT_ERROR, pLoader, L"Could not find texture loader");
        if (pLoader)
        {
            // Increment texture load count to track
            ++m_ActiveTextureLoads;

            // Pack the data up for loading
            TextureLoadParams loadParams;
            loadParams.LoadInfo.push_back(loadInfo);
            loadParams.LoadCompleteCallback = pCompletionCallback;
            loadParams.AdditionalParams = pAdditionalParams;

            // Load texture
            pLoader->LoadAsync(&loadParams);
        }
    }

    void ContentManager::LoadTextures(const std::vector<TextureLoadInfo>& loadInfoList, TextureLoadCompletionCallbackFn pCompletionCallback/*= nullptr*/, void* pAdditionalParams /*=nullptr*/)
    {
        ContentLoader* pLoader = m_ContentLoaders[static_cast<uint32_t>(LoaderType::Texture)];
        CauldronAssert(ASSERT_ERROR, pLoader, L"Could not find texture loader");
        if (pLoader)
        {
            // Increment texture load count to track
            m_ActiveTextureLoads += static_cast<uint32_t>(loadInfoList.size());

            // Pack the data up for loading
            TextureLoadParams loadParams;
            loadParams.LoadInfo = loadInfoList;
            loadParams.LoadCompleteCallback = pCompletionCallback;
            loadParams.AdditionalParams = pAdditionalParams;

            // Load texture
            pLoader->LoadMultipleAsync(&loadParams);
        }
    }

    bool ContentManager::StartManagingContent(std::wstring contentName, Texture*& pTextureContent)
    {
        // Decrement texture load count
        --m_ActiveTextureLoads;

        // Lock while we are making changes to content as this happens from multiple threads
        std::lock_guard<std::mutex>    lock(m_ContentChangeMutex);
        std::pair<std::map<std::wstring, Texture*>::iterator, bool> results = m_LoadedTextureContent.emplace(std::make_pair(contentName, pTextureContent));
        return results.second;
    }

    const Texture* ContentManager::GetTexture(std::wstring contentName)
    {
        CauldronAssert(ASSERT_ERROR, std::this_thread::get_id() != GetFramework()->MainThreadID() || !GetFramework()->IsRunning(), L"Performance Warning: Using std::map::find on the main thread while app is running.");
        std::lock_guard<std::mutex>    lock(m_ContentChangeMutex);
        std::map<std::wstring, Texture*>::iterator iter = m_LoadedTextureContent.find(contentName);
        if (iter != m_LoadedTextureContent.end())
            return iter->second;
        else
            return nullptr;
    }

    bool ContentManager::StartManagingContent(std::wstring contentName, ContentBlock*& pContentBlock, bool loadedContent/*=true*/)
    {
        for (auto pListener : m_ContentListeners)
            pListener->OnNewContentLoaded(pContentBlock);

        Content* pContent   = new Content();
        pContent->State     = ContentBlockState::Loading;
        pContent->pBlock    = pContentBlock;
        pContentBlock       = nullptr;    // Moved

        // Decrement content load count (if it was loaded)
        if (loadedContent)
            --m_ActiveContentLoads;

        // Lock while we are making changes to content as this happens from multiple threads
        std::lock_guard<std::mutex>    lock(m_ContentChangeMutex);
        CauldronAssert(ASSERT_ERROR, std::this_thread::get_id() != GetFramework()->MainThreadID() || !GetFramework()->IsRunning(), L"Performance Warning: Using std::map::emplace on the main thread while app is running.");
        std::pair<std::map<std::wstring, Content*>::iterator, bool> results = m_LoadedContentBlocks.emplace(std::make_pair(contentName, pContent));

        return results.second;
    }

    void ContentManager::UnloadContent(std::wstring contentName)
    {
        // lock to delete the block
        std::lock_guard<std::mutex> lock(m_ContentChangeMutex);

        CauldronAssert(ASSERT_ERROR, std::this_thread::get_id() != GetFramework()->MainThreadID() || !GetFramework()->IsRunning(), L"Performance Warning: Using std::map::find on the main thread while app is running.");
        auto iter = m_LoadedContentBlocks.find(contentName);
        if (iter != m_LoadedContentBlocks.end())
            iter->second->State = ContentBlockState::ToDelete;
    }

    void ContentManager::UpdateContent(uint64_t currentFrame)
    {
        std::lock_guard<std::mutex> lock(m_ContentChangeMutex);

        uint64_t frameToUnload = currentFrame - GetConfig()->BackBufferCount;

        #pragma message("move changed blocks into a separate list ot avoid looping over all of them at runtime")
        auto it = m_LoadedContentBlocks.begin();
        while (it != m_LoadedContentBlocks.end())
        {
            if (it->second->State == ContentBlockState::Loading)
            {
                // Complete the loading of the content block (component management, scene additions, etc.)
                CompleteContentBlockLoad(it->second);
                it->second->State = ContentBlockState::Ready;
                ++it;
            }
            else if (it->second->State == ContentBlockState::ToDelete)
            {
                // Start unload procedures
                UnloadContentBlock(it->second, currentFrame);

                // remove the block from the loaded content blocks
                it = m_LoadedContentBlocks.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (!m_ContentToUnload.empty() && (*m_ContentToUnload.begin())->FrameStamp <= frameToUnload)
        {
            // schedule the task to delete everything
            Task unloadingTask(std::bind(&ContentManager::DeleteUnloadedContent, this, frameToUnload), nullptr);
            GetTaskManager()->AddTask(unloadingTask);
        }
    }

    void ContentManager::CompleteContentBlockLoad(Content* pContent)
    {
        // Start managing all entity components
        for (auto* pEntityDataBlock : pContent->pBlock->EntityDataBlocks)
        {
            for (auto* pComponent : pEntityDataBlock->Components)
                pComponent->GetManager()->StartManagingComponent(pComponent);
        }

        // add all the content block's entities to the scene
        GetScene()->AddContentBlockEntities(pContent->pBlock);
    }

    void ContentManager::UnloadContentBlock(Content* pContent, uint64_t currentFrame)
    {
        // Stop managing all entity components
        for (auto* pEntityDataBlock : pContent->pBlock->EntityDataBlocks)
        {
            for (auto* pComponent : pEntityDataBlock->Components)
                pComponent->GetManager()->StopManagingComponent(pComponent);
        }

        // Remove content block's entities from the scene before starting unloading procedure
        GetScene()->RemoveContentBlockEntities(pContent->pBlock);

        // Tag the unload request frame
        pContent->FrameStamp = currentFrame;

        // add the content to the list of content blocks which will be deleted
        m_ContentToUnload.push_back(pContent);
    }

    void ContentManager::AddContentListener(ContentListener* pListener)
    {
        auto it = m_ContentListeners.find(pListener);
        if (it == m_ContentListeners.end())
            m_ContentListeners.insert(pListener);
    }

    void ContentManager::RemoveContentListener(ContentListener* pListener)
    {
        auto it = m_ContentListeners.find(pListener);
        if (it != m_ContentListeners.end())
            m_ContentListeners.erase(it);
    }

    void ContentManager::DeleteUnloadedContent(uint64_t frameToUnload)
    {
        std::lock_guard<std::mutex> lock(m_ContentChangeMutex);

        CauldronAssert(ASSERT_ERROR, std::this_thread::get_id() != GetFramework()->MainThreadID() || !GetFramework()->IsRunning(), L"Performance Warning: Using std::map::find on the main thread while app is running.");

        // notify the unloading
        auto it = m_ContentToUnload.begin();
        while (it != m_ContentToUnload.end())
        {
            if ((*it)->FrameStamp <= frameToUnload)
            {
                // Call unload callbacks
                for (auto pListener : m_ContentListeners)
                    pListener->OnContentUnloaded((*it)->pBlock);

                // Remove all textures in the content block
                for (auto texIter = (*it)->pBlock->TextureAssets.begin(); texIter != (*it)->pBlock->TextureAssets.end(); ++texIter)
                {
                    auto findItr = m_LoadedTextureContent.find((*texIter)->GetDesc().Name);
                    CauldronAssert(ASSERT_ERROR, findItr != m_LoadedTextureContent.end(), L"Could not find texture %ls to unload", (*texIter)->GetDesc().Name.c_str());
                    if (findItr != m_LoadedTextureContent.end())
                    {
                        delete findItr->second;
                        m_LoadedTextureContent.erase(findItr);
                    }
                }

                // Delete the content
                delete (*it);

                // Erase the entry
                it = m_ContentToUnload.erase(it);
                continue;
            }

            ++it;
        }
    }

} // namespace cauldron
