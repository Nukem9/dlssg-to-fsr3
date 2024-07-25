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

#include "raytracingrendermodule.h"

#include "core/framework.h"
#include "core/scene.h"
#include "core/components/meshcomponent.h"
#include "core/components/animationcomponent.h"
#include "render/profiler.h"
#include "render/parameterset.h"
#include "render/device.h"

using namespace cauldron;

RayTracingRenderModule::~RayTracingRenderModule()
{
    GetContentManager()->RemoveContentListener(this);
}

void RayTracingRenderModule::Init(const json& initData)
{
    if (!GetDevice()->FeatureSupported(DeviceFeature::RT_1_1))
    {
        SetModuleReady(false);
        return;
    }

    // Register for content change updates
    GetContentManager()->AddContentListener(this);

    // We are now ready for use
    SetModuleReady(true);
}

void RayTracingRenderModule::RebuildAnimatedBLAS(CommandList* pCmdList)
{
    GPUScopedProfileCapture rayTracingMarker(pCmdList, L"BLAS Build");

    std::vector<Barrier> blasBuildBarriers;
    for (RTAnimatedMeshes& animatedMeshes : m_RTAnimatedMeshes)
    {
        const Mesh* pMesh = animatedMeshes.pMeshComponent->GetData().pMesh;
        animatedMeshes.pAnimationComponentData->m_animatedBlas->Build(pCmdList);

        // Update AS instance for animated meshes
        GetScene()->GetASManager()->PushInstance(
            pMesh, animatedMeshes.pMeshComponent->GetOwner()->GetTransform(), animatedMeshes.pAnimationComponentData->m_animatedBlas);
        blasBuildBarriers.push_back(Barrier::UAV(animatedMeshes.pAnimationComponentData->m_animatedBlas->GetBuffer()->GetResource()));
    }
    ResourceBarrier(pCmdList, (uint32_t)blasBuildBarriers.size(), blasBuildBarriers.data());
}

void RayTracingRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    GPUScopedProfileCapture rayTracingMarker(pCmdList, L"RayTracing Updates");
    std::lock_guard<std::mutex> paramsLock(m_CriticalSection);

    // Rebuild BLAS for animated meshes
    RebuildAnimatedBLAS(pCmdList);

    // Build TLAS
    GetScene()->GetASManager()->Update(pCmdList);
}

void RayTracingRenderModule::OnNewContentLoaded(ContentBlock* pContentBlock)
{
    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);

    for (auto* pEntityData : pContentBlock->EntityDataBlocks)
    {
        for (auto* pComponent : pEntityData->Components)
        {
            if (pComponent->GetManager() == MeshComponentMgr::Get())
            {
                const Mesh*  pMesh       = reinterpret_cast<MeshComponent*>(pComponent)->GetData().pMesh;

                if (pMesh->HasAnimatedBlas())
                {
                    const auto& animationData = pComponent->GetOwner()->GetComponent<const AnimationComponent>(AnimationComponentMgr::Get())->GetData();

                    m_RTAnimatedMeshes.push_back({animationData, reinterpret_cast<MeshComponent*>(pComponent)});
                }
            }
        }
    }

}

void RayTracingRenderModule::OnContentUnloaded(ContentBlock* pContentBlock)
{
}
