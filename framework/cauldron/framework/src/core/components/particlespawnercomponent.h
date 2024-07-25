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
#include "render/particle.h"

namespace cauldron
{
    class ParticleSpawnerComponent;

    /**
     * @class ParticleSpawnerComponentMgr
     *
     * Component manager class for <c><i>ParticleSpawnerComponent</i></c>s.
     *
     * @ingroup CauldronComponent
     */
    class ParticleSpawnerComponentMgr : public ComponentMgr
    {
    public:
        static const wchar_t* s_ComponentName;

    public:

        /**
         * @brief   Constructor with default behavior.
         */
        ParticleSpawnerComponentMgr();

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~ParticleSpawnerComponentMgr();

        /**
         * @brief   Component creator.
         */
        virtual Component* SpawnComponent(Entity* pOwner, ComponentData* pData) override
        {
            return reinterpret_cast<Component*>(SpawnParticleSpawnerComponent(pOwner, pData));
        }

        /**
         * @brief   Allocates a new <c><i>ParticleSpawnerComponent</i></c> for the given entity.
         */
        ParticleSpawnerComponent* SpawnParticleSpawnerComponent(Entity* pOwner, ComponentData* pData);

        /**
         * @brief   Gets the component type string ID.
         */
        virtual const wchar_t* ComponentType() const override { return s_ComponentName; }

        /**
         * @brief   Initializes the component manager.
         */
        virtual void Initialize() override;

        /**
         * @brief   Shuts down the component manager.
         */
        virtual void Shutdown() override;

        /**
         * @brief   Component manager instance accessor.
         */
        static ParticleSpawnerComponentMgr* Get() { return s_pComponentManager; }

    private:
        static ParticleSpawnerComponentMgr* s_pComponentManager;
    };

    /**
     * @struct ParticleSpawnerComponentData
     *
     * Initialization data structure for the <c><i>ParticleSpawnerComponent</i></c>.
     *
     * @ingroup CauldronComponent
     */
    struct ParticleSpawnerComponentData : public ComponentData
    {
        const ParticleSpawnerDesc particleSpawnerDesc;  ///< The particle spawner description

        ParticleSpawnerComponentData(const ParticleSpawnerDesc& spawner)
            : particleSpawnerDesc(spawner)
        {
        }
    };

    /**
     * @class ParticleSpawnerComponent
     *
     * Camera component class. Implements particle spawning functionality on a given entity.
     *
     * @ingroup CauldronComponent
     */
    class ParticleSpawnerComponent : public Component
    {
    public:

        /**
         * @brief   Constructor.
         */
        ParticleSpawnerComponent(Entity* pOwner, ComponentData* pData, ParticleSpawnerComponentMgr* pManager);

        /**
         * @brief   Destructor.
         */
        virtual ~ParticleSpawnerComponent();

        /**
         * @brief   Component update. Updates the particle system attached to the component.
         *          This will setup the number of particles to emit for the current frame.
         *          Particles are emitted from the <c><i>GPUParticleRenderModule</i></c>.
         */
        virtual void Update(double deltaTime) override { m_pParticleSystem->Update(deltaTime); }

        /**
         * @brief   Component data accessor.
         */
        ParticleSpawnerComponentData& GetData() { return *m_pData; }
        const ParticleSpawnerComponentData& GetData() const { return *m_pData; }

        /**
         * @brief   Gets the component's particle system.
         */
        ParticleSystem* GetParticleSystem() { return m_pParticleSystem; }

    private:
        ParticleSpawnerComponent() = delete;

    protected:

        ParticleSpawnerComponentData* m_pData;
        ParticleSystem*               m_pParticleSystem = nullptr;
    };

}  // namespace cauldron
