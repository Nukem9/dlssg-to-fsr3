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

#include "core/loaders/particleloader.h"
#include "core/contentmanager.h"
#include "core/framework.h"
#include "core/loaders/textureloader.h"
#include "core/components/particlespawnercomponent.h"

namespace cauldron
{
    void ParticleLoader::LoadAsync(void* pLoadParams)
    {
        CauldronCritical(L"No support for single async load for this loader");
    }

    void ParticleLoader::LoadMultipleAsync(void* pLoadParams)
    {
        ParticleLoadParams* pParams = reinterpret_cast<ParticleLoadParams*>(pLoadParams);

        // Copy the load parameters to use when loading particles
        ParticleLoadParams* pParticleLoadData = new ParticleLoadParams();
        *pParticleLoadData                    = *pParams;

        // Enqueue a single task to load all spawners
        GetTaskManager()->AddTask(Task(&ParticleLoader::LoadParticleContent, pParticleLoadData));
    }

    void ParticleLoader::LoadParticleContent(void* pParam)
    {
        ParticleLoadParams* pLoadParams = reinterpret_cast<ParticleLoadParams*>(pParam);

        // Create a content block to use for loaded data and init with the entities we'll create
        ContentBlock* pParticleContentBlock = new ContentBlock();

        std::vector<TextureLoadInfo> texLoadInfo;

        // Create an entity data block for each particle spawner we have
        for (auto& spawnerDesc : pLoadParams->LoadData) {
            EntityDataBlock* pEntityDataBlock = new EntityDataBlock();
            CauldronAssert(ASSERT_CRITICAL, pEntityDataBlock, L"Could not allocate new entity data block for particle spawner %ls", spawnerDesc.Name.c_str());
            pParticleContentBlock->EntityDataBlocks.push_back(pEntityDataBlock);

            // Create the spawner entity
            Entity* pNewEntity = new Entity(spawnerDesc.Name.c_str());
            CauldronAssert(ASSERT_CRITICAL, pNewEntity, L"Could not allocate new entity %ls", spawnerDesc.Name.c_str());

            // Set it's root transform
            Mat4 transform = Mat4::identity();
            transform.setTranslation(spawnerDesc.Position);
            pNewEntity->SetTransform(transform);
            pEntityDataBlock->pEntity = pNewEntity;

            // And setup the component
            ParticleSpawnerComponentData* pComponentData = new ParticleSpawnerComponentData(spawnerDesc);
            pEntityDataBlock->ComponentsData.push_back(pComponentData);
            ParticleSpawnerComponent* pComponent = ParticleSpawnerComponentMgr::Get()->SpawnParticleSpawnerComponent(pNewEntity, pComponentData);
            pEntityDataBlock->Components.push_back(pComponent);

            // Add the atlas that we need to load for this spawner
            texLoadInfo.push_back(TextureLoadInfo(spawnerDesc.AtlasPath));
        }

        // No longer need the load data
        delete pLoadParams;

        // Setup the loading of all textures backing particle spawners
        GetContentManager()->LoadTextures(texLoadInfo, &ParticleLoader::TextureLoadCompleted, pParticleContentBlock);
    }

    void ParticleLoader::TextureLoadCompleted(const std::vector<const Texture*>& textureList, void* pParam)
    {
        ContentBlock* pParticleContentBlock = reinterpret_cast<ContentBlock*>(pParam);
        pParticleContentBlock->TextureAssets = textureList;

        // Hook up the texture atlas's
        for (auto pEntityBlock : pParticleContentBlock->EntityDataBlocks)
        {
            for (auto pComponent : pEntityBlock->Components)
            {
                if (pComponent->GetManager() == ParticleSpawnerComponentMgr::Get())
                {
                    ParticleSpawnerComponent* pSpawnerComp = reinterpret_cast<ParticleSpawnerComponent*>(pComponent);
                    for (const Texture* pTexture : textureList)
                    {
                        if (pTexture->GetDesc().Name == pSpawnerComp->GetData().particleSpawnerDesc.AtlasPath) {
                            pSpawnerComp->GetParticleSystem()->m_pAtlas = pTexture;
                            continue;
                        }
                    }
                }
            }
        }

        GetContentManager()->StartManagingContent(L"ParticleSpawnerEntity", pParticleContentBlock);
    }

}  // namespace cauldron
