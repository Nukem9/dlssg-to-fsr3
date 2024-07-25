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

#include "core/component.h"
#include "core/entity.h"
#include "misc/helpers.h"
#include "render/material.h"
#include "render/mesh.h"
#include "render/animation.h"
#include <mutex>
#include <vector>

namespace cauldron
{
    class ComponentMgr;
    class Texture;
    class Animation;
    class Material;
    class Mesh;
    struct Anim;

    /**
     * @struct EntityDataBlock
     *
     * Contains all loaded data to back created entities and their components.
     *
     * @ingroup CauldronCore
     */
    struct EntityDataBlock
    {
        Entity*                         pEntity = nullptr;          ///< Pointer to the entity this data block represents
        std::vector<ComponentData*>     ComponentsData = {};        ///< Initialization data for all of the entity's components
        std::vector<Component*>         Components = {};            ///< Pointers for all of the entity's components

        EntityDataBlock() {}
        ~EntityDataBlock()
        {
            // Delete entity (backed by component)
            delete pEntity;

            // Delete components (back by data)
            for (auto* pComponent : Components)
                delete pComponent;
            Components.clear();

            // Delete components data
            for (auto* pCompData : ComponentsData)
                delete pCompData;
            ComponentsData.clear();
        }

        private:
            // Don't copy/move these given we delete information in the destructor
            EntityDataBlock(const EntityDataBlock& rhs) = delete;
            EntityDataBlock& operator=(const EntityDataBlock& rhs) = delete;
            EntityDataBlock(const EntityDataBlock&& rhs)           = delete;
            EntityDataBlock& operator=(const EntityDataBlock&& rhs) = delete;
    };

    /**
     * @struct ContentBlock
     *
     * ContentBlock struct used when loading scene content.
     * Contains all meshes, textures, etc. needed for rendering a loaded asset.
     *
     * @ingroup CauldronCore
     */
    struct ContentBlock
    {

        std::vector<EntityDataBlock*>   EntityDataBlocks = {};  ///< Entity storage

        // Active camera for content block (if any)
        const Entity*                   ActiveCamera = nullptr; ///< Defines an entity was loaded that was marked as an active camera


        std::vector<const Texture*> TextureAssets = {};         ///< Pointer list to loaded <c><i>Texture</i></c> resources.
        std::vector<Material*>      Materials     = {};         ///< Pointer list to loaded <c><i>Material</i></c> resources.
        std::vector<Mesh*>          Meshes        = {};         ///< Pointer list to loaded <c><i>Mesh</i></c> resources.
        std::vector<Animation*>     Animations    = {};         ///< Pointer list to loaded <c><i>Animation</i></c> resources.
        std::vector<AnimationSkin*> Skins    = {};              ///< Pointer list to loaded <c><i>AnimationSkin</i></c> resources.

        /**
         * @brief   Construction.
         */
        ContentBlock() {}

        /**
         * @brief   Destruction.
         */
        ~ContentBlock()
        {
            for (auto* pMat : Materials)
                delete pMat;
            Materials.clear();

            for (auto* pMesh : Meshes)
                delete pMesh;
            Meshes.clear();

            for (auto* pAnim : Animations)
                delete pAnim;
            Animations.clear();

            for (auto pEntDataBlock : EntityDataBlocks)
                delete pEntDataBlock;
            EntityDataBlocks.clear();
        }

        private:
            // Don't copy/move these given we delete information in the destructor
            ContentBlock(const ContentBlock& rhs) = delete;
            ContentBlock& operator=(const ContentBlock& rhs)  = delete;
            ContentBlock(const ContentBlock&& rhs)           = delete;
            ContentBlock& operator=(const ContentBlock&& rhs) = delete;
    };

    /// @defgroup CauldronLoaders ContentLoaders
    /// FidelityFX Cauldron Framework ContentLoaders Reference Documentation
    ///
    /// @ingroup CauldronCore

    /**
     * @class ContentLoader
     *
     * Base class from which all content loaders inherit.
     *
     * @ingroup CauldronLoaders
     */
    class ContentLoader
    {
    public:

        /**
         * @brief   Construction.
         */
        ContentLoader() = default;

        /**
         * @brief   Destruction.
         */
        virtual ~ContentLoader() = default;

        /**
         * @brief   Loads content asynchronously. Must be overridden by each ContentLoader type.
         */
        virtual void LoadAsync(void* pLoadParams) = 0;

        /**
         * @brief   Loads multiple content files asynchronously.
         *          Must be overridden by each ContentLoader type.
         */
        virtual void LoadMultipleAsync(void* pLoadParams) = 0;

    private:
        NO_COPY(ContentLoader)
        NO_MOVE(ContentLoader)
    };

} // namespace cauldron
