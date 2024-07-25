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

#include "core/components/meshcomponent.h"
#include "core/entity.h"
#include "core/framework.h"
#include "core/scene.h"
#include "misc/assert.h"

namespace cauldron
{
    const wchar_t* MeshComponentMgr::s_ComponentName = L"MeshComponent";
    MeshComponentMgr* MeshComponentMgr::s_pComponentManager = nullptr;

    MeshComponentMgr::MeshComponentMgr() :
        ComponentMgr()
    {
    }

    MeshComponentMgr::~MeshComponentMgr()
    {

    }

    MeshComponent* MeshComponentMgr::SpawnMeshComponent(Entity* pOwner, ComponentData* pData)
    {
        // Create the component
        MeshComponent* pComponent = new MeshComponent(pOwner, pData, this);

        // Add it to the owner
        pOwner->AddComponent(pComponent);

        return pComponent;
    }

    void MeshComponentMgr::Initialize()
    {
        CauldronAssert(ASSERT_CRITICAL, !s_pComponentManager, L"MeshComponentMgr instance is non-null. Component managers can ONLY be created through framework registration using RegisterComponentManager<>()");

        // Initialize the convenience accessor to avoid having to do a map::find each time we want the manager
        s_pComponentManager = this;
    }

    void MeshComponentMgr::Shutdown()
    {
        // Clear out the convenience instance pointer
        CauldronAssert(ASSERT_ERROR, s_pComponentManager, L"MeshComponentMgr instance is null. Component managers can ONLY be destroyed through framework shutdown");
        s_pComponentManager = nullptr;
    }

    MeshComponent::MeshComponent(Entity* pOwner, ComponentData* pData, MeshComponentMgr* pManager) :
        Component(pOwner, pData, pManager),
        m_pData(reinterpret_cast<MeshComponentData*>(pData))
    {
    }

    void MeshComponent::Update(double deltaTime)
    {
        // Push a new ASInstance to the ASManager Instance Queue for later processing
        if (GetConfig()->BuildRayTracingAccelerationStructure)
        {
            // Animated BLASes are handled in the RayTracing Render Module
            if (!GetData().pMesh->HasAnimatedBlas())
                GetScene()->GetASManager()->PushInstance(GetData().pMesh, GetOwner()->GetTransform());
        }
    }

    MeshComponent::~MeshComponent()
    {

    }

} // namespace cauldron
