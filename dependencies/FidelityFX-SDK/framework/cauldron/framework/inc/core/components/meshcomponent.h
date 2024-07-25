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
#include "render/mesh.h"
#include "core/contentloader.h"

namespace cauldron
{
    class MeshComponent;

    /**
     * @class MeshComponentMgr
     *
     * Component manager class for <c><i>MeshComponent</i></c>s.
     *
     * @ingroup CauldronComponent
     */
    class MeshComponentMgr : public ComponentMgr
    {
    public:
        static const wchar_t* s_ComponentName;      ///< Component name

    public:

        /**
         * @brief   Constructor with default behavior.
         */
        MeshComponentMgr();

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~MeshComponentMgr();

        /**
         * @brief   Component creator.
         */
        virtual Component* SpawnComponent(Entity* pOwner, ComponentData* pData) override { return reinterpret_cast<Component*>(SpawnMeshComponent(pOwner, pData)); }

        /**
         * @brief   Allocates a new <c><i>MeshComponent</i></c> for the given entity.
         */
        MeshComponent* SpawnMeshComponent(Entity* pOwner, ComponentData* pData);

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
        static MeshComponentMgr* Get() { return s_pComponentManager; }

    private:
        static MeshComponentMgr* s_pComponentManager;
    };

    /**
     * @struct MeshComponentData
     *
     * Initialization data structure for the <c><i>MeshComponent</i></c>.
     *
     * @ingroup CauldronComponent
     */
    struct MeshComponentData : public ComponentData
    {
         const Mesh* pMesh = nullptr;   ///< The <c><i>Mesh</i></c> reference for the component.
    };

    /**
     * @class MeshComponent
     *
     * Mesh component class. Implements mesh accessor functionality for a given entity.
     *
     * @ingroup CauldronComponent
     */
    class MeshComponent : public Component
    {
    public:

        /**
         * @brief   Constructor.
         */
        MeshComponent(Entity* pOwner, ComponentData* pData, MeshComponentMgr* pManager);

        /**
         * @brief   Destructor.
         */
        virtual ~MeshComponent();

        /**
         * @brief   Component update. If ray tracing is enabled, will push a new TLAS instance.
         */
        virtual void Update(double deltaTime) override;

        /**
         * @brief   Component data accessor.
         */
        MeshComponentData& GetData() { return *m_pData; }
        const MeshComponentData& GetData() const { return *m_pData; }

    private:
        MeshComponent() = delete;

    protected:

        MeshComponentData* m_pData;
    };

} // namespace cauldron
