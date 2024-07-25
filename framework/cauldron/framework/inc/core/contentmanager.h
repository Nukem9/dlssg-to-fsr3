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

#include "core/contentloader.h"
#include "core/loaders/textureloader.h"
#include "core/loaders/particleloader.h"
#include "misc/helpers.h"
#include "render/texture.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING    // To avoid receiving deprecation error since we are using C++11 only
#include <experimental/filesystem>

#include <map>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace cauldron
{
    /**
     * @enum LoaderType
     *
     * Identifies the type of content a <c><i>ContentLoader</i></c> is responsible for.
     *
     * @ingroup CauldronCore
     */
    enum class LoaderType : uint32_t
    {
        GLTF    = 0,    ///< glTF content loader
        Texture,        ///< Texture content loader
        Particle,       ///< Particle content loader

        Count           ///< Content loader count
    };

    /**
     * @class ContentListener
     *
     * Listener base class allowing content load notifications.
     * Any class that wishes to receive content load/unload notifications must
     * derive from this class.
     *
     * @ingroup CauldronCore
     */
    class ContentListener
    {
    public:
        /**
         * @brief   Override in ContentListener derived classes to be notified when
         *          content has been loaded.
         */
        virtual void OnNewContentLoaded(ContentBlock* pContentBlock) = 0;

        /**
         * @brief   Override in ContentListener derived classes to be notified when
         *          content has been unloaded.
         */
        virtual void OnContentUnloaded(ContentBlock* pContentBlock) = 0;
    };

    /**
     * @class ContentManager
     *
     * The ContentManager instance is responsible for managing all loaded content.
     * It is also used to query and fetch loaded content.
     *
     * @ingroup CauldronCore
     */
    class ContentManager
    {
    public:

        /**
         * @brief   Constructor with default behavior.
         */
        ContentManager();

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~ContentManager();

        /**
         * @brief   Shuts down the content manager when framework is terminating.
         */
        void Shutdown();

        /**
         * @brief   Loads a glTF file into the scene.
         */
        void LoadGLTFToScene(std::experimental::filesystem::path& gltfFile);

        /**
         * @brief   Loads a number of particle spawners into the scene.
         */
        void LoadParticlesToScene(const std::vector<ParticleSpawnerDesc>& descList);

        /**
         * @brief   Loads a texture.
         */
        void LoadTexture(const TextureLoadInfo& loadInfo, TextureLoadCompletionCallbackFn pCompletionCallback = nullptr, void* pAdditionalParams = nullptr);

        /**
         * @brief   Loads multiple texture resources.
         */
        void LoadTextures(const std::vector<TextureLoadInfo>& loadInfoList, TextureLoadCompletionCallbackFn pCompletionCallback = nullptr, void* pAdditionalParams = nullptr);

        /**
         * @brief   Tells the content manager it can start managing the texture content once it's been 
         *          fully loaded and initialized.
         */
        bool StartManagingContent(std::wstring contentName, Texture*& pTextureContent);

        /**
         * @brief   Fetches the requested texture. Returns nullptr if texture isn't found.
         */
        const Texture* GetTexture(std::wstring contentName);

        /**
         * @brief   Tells the content manager it can start managing a <c><i>ContentBlock</i></c> once it's been
         *          fully loaded and initialized.
         */
        bool StartManagingContent(std::wstring contentName, ContentBlock*& pContentBlock, bool loadedContent = true);

        /**
         * @brief   Unloads previously loaded content (texture or <c><i>ContentBlock</i></c>).
         */
        void UnloadContent(std::wstring contentName);

        /**
         * @brief   Manages the loading state of content as it flows through loading and unloading.
         */
        void UpdateContent(uint64_t currentFrame);

        /**
         * @brief   Registers a <c><i>ContentListener</i></c>-derived class for content load/unload callbacks.
         */
        void AddContentListener(ContentListener* pListener);
        
        /**
         * @brief   Removes a <c><i>ContentListener</i></c>-derived class from content load/unload callbacks.
         */
        void RemoveContentListener(ContentListener* pListener);

        /**
         * @brief   Queries whether the ContentManager is currently in the process of loading anything.
         */
        bool IsCurrentlyLoading() const { return m_ActiveContentLoads || m_ActiveTextureLoads; }

    private:
        NO_COPY(ContentManager)
        NO_MOVE(ContentManager)

        // We will manage data in a few different ways:
        // 1) As individual asset lists
        // 2) As content blocks which will contain the assets tied to those for unloading later (i.e. gltf)

        enum class ContentBlockState
        {
            Loading,        ///< Content block is in the process of loading
            Ready,          ///< Content block is loaded and ready for use
            ToDelete        ///< Content block been unloaded and is ready to delete
        };

        struct Content
        {
            ContentBlockState   State = ContentBlockState::Loading;
            uint64_t            FrameStamp = -1;
            ContentBlock*       pBlock = nullptr;

            Content() = default;

            Content(Content&& moveInst) noexcept :
                State(moveInst.State),
                FrameStamp(moveInst.FrameStamp),
                pBlock(moveInst.pBlock)
            {
                moveInst.pBlock = nullptr;  // Has been moved
            }

            ~Content()
            {
                delete pBlock;
            }
        };

        void CompleteContentBlockLoad(Content* pContent);
        void UnloadContentBlock(Content* pContent, uint64_t currentFrame);

        void DeleteUnloadedContent(uint64_t frameToUnload);

        std::vector<ContentLoader*>         m_ContentLoaders;

        std::map<std::wstring, Texture*>    m_LoadedTextureContent;
        std::mutex                          m_ContentChangeMutex;
        std::map<std::wstring, Content*>    m_LoadedContentBlocks;
        std::vector<Content*>               m_ContentToUnload;

        std::atomic_uint32_t                m_ActiveContentLoads = 0;
        std::atomic_uint32_t                m_ActiveTextureLoads = 0;

        std::unordered_set<ContentListener*> m_ContentListeners;
    };

} // namespace cauldron
