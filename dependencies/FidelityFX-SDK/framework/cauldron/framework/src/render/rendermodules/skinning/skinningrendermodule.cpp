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

#include "skinningrendermodule.h"
#include "core/framework.h"
#include "render/profiler.h"
#include "render/parameterset.h"
#include "core/components/meshcomponent.h"
#include "core/components/animationcomponent.h"

#include "render/rootsignature.h"
#include "render/pipelineobject.h"

using namespace cauldron;

SkinningRenderModule::~SkinningRenderModule()
{
    for (auto& blob : m_SkinningBlobs)
    {
        delete blob.pParameters;
        delete blob.vertexStrides;
    }

    delete m_pRootSignature;
    delete m_pPipelineObj;

    GetContentManager()->RemoveContentListener(this);
}

void SkinningRenderModule::Init(const json& initData)
{
    // Root Signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1); // SkinningMatrices
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);  // VertexStrides
    signatureDesc.AddBufferSRVSet(0, ShaderBindStage::Compute, 1); // Position
    signatureDesc.AddBufferSRVSet(1, ShaderBindStage::Compute, 1); // Normals
    signatureDesc.AddBufferSRVSet(2, ShaderBindStage::Compute, 1); // Weights0
    signatureDesc.AddBufferSRVSet(3, ShaderBindStage::Compute, 1); // Joints0

    signatureDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1); // positionSkinned
    signatureDesc.AddBufferUAVSet(1, ShaderBindStage::Compute, 1); // prevPositionSkinned
    signatureDesc.AddBufferUAVSet(2, ShaderBindStage::Compute, 1); // normalSkinned

    m_pRootSignature = RootSignature::CreateRootSignature(L"SkinningRenderModule_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pRootSignature);

    // Setup the shaders to build on the pipeline object
    std::wstring shaderPath = L"computeskinning.hlsl";
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"MainCS", ShaderModel::SM6_0));

    m_pPipelineObj = PipelineObject::CreatePipelineObject(L"SkinningRenderModule_PipelineObj", psoDesc, nullptr);

    // Register for content change updates
    GetContentManager()->AddContentListener(this);

    // We are now ready for use
    SetModuleReady(true);
}

void SkinningRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    GPUScopedProfileCapture skinningMarker(pCmdList, L"ComputeSkinning");
    std::lock_guard<std::mutex> paramsLock(m_CriticalSection);

    // Process Barriers
    std::vector<Barrier> transitionBarriers;
    for (const SkinningBlob& blob : m_SkinningBlobs)
    {
        const AnimationComponentData* pAnimationComponentData = blob.pAnimationComponentData;
        const Surface*                pSurface                = blob.pSurface;
        const uint32_t                surfaceID               = pSurface->GetSurfaceID();

        transitionBarriers.push_back(Barrier::Transition(pAnimationComponentData->m_skinnedPositions[surfaceID].pBuffer->GetResource(),
                                                         ResourceState::VertexBufferResource,
                                                         ResourceState::UnorderedAccess));
        transitionBarriers.push_back(Barrier::Transition(
            pAnimationComponentData->m_skinnedNormals[surfaceID].pBuffer->GetResource(), ResourceState::VertexBufferResource, ResourceState::UnorderedAccess));
        transitionBarriers.push_back(Barrier::Transition(pAnimationComponentData->m_skinnedPreviousPosition[surfaceID].pBuffer->GetResource(),
                                                         ResourceState::VertexBufferResource,
                                                            ResourceState::UnorderedAccess));
    }
    ResourceBarrier(pCmdList, (uint32_t)transitionBarriers.size(), transitionBarriers.data());

    for (const SkinningBlob& blob : m_SkinningBlobs)
    {
        const AnimationComponentData* pAnimationComponentData = blob.pAnimationComponentData;
        const Surface*                pSurface                = blob.pSurface;
        const uint32_t                surfaceID               = pSurface->GetSurfaceID();
        ParameterSet* pParameters = blob.pParameters;

        BufferAddressInfo bonesInfo =
            GetDynamicBufferPool()->AllocConstantBuffer(sizeof(MatrixPair) * (uint32_t)blob.SkinningMatrices->size(), blob.SkinningMatrices->data());
        BufferAddressInfo stridesInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(VertexStrides), blob.vertexStrides);

        pParameters->UpdateRootConstantBuffer(&bonesInfo, 0);
        pParameters->UpdateRootConstantBuffer(&stridesInfo, 1);

        // Bind everything
        pParameters->Bind(pCmdList, m_pPipelineObj);

        SetPipelineState(pCmdList, m_pPipelineObj);

        const uint32_t vertexCount = pAnimationComponentData->m_skinnedPositions[surfaceID].Count;
        Dispatch(pCmdList, DivideRoundingUp(vertexCount, 64), 1, 1);
    }

    // Process Barriers
    transitionBarriers.clear();
    for (const SkinningBlob& blob : m_SkinningBlobs)
    {
        const AnimationComponentData* pAnimationComponentData = blob.pAnimationComponentData;
        const Surface*                pSurface                = blob.pSurface;
        const uint32_t                surfaceID               = pSurface->GetSurfaceID();

        transitionBarriers.push_back(Barrier::Transition(pAnimationComponentData->m_skinnedPositions[surfaceID].pBuffer->GetResource(),
                                                         ResourceState::UnorderedAccess,
                                                         ResourceState::VertexBufferResource));
        transitionBarriers.push_back(Barrier::Transition(
            pAnimationComponentData->m_skinnedNormals[surfaceID].pBuffer->GetResource(), ResourceState::UnorderedAccess, ResourceState::VertexBufferResource));
        transitionBarriers.push_back(Barrier::Transition(pAnimationComponentData->m_skinnedPreviousPosition[surfaceID].pBuffer->GetResource(),
                                                         ResourceState::UnorderedAccess,
                                                            ResourceState::VertexBufferResource));
    }
    ResourceBarrier(pCmdList, (uint32_t)transitionBarriers.size(), transitionBarriers.data());
}

void SkinningRenderModule::OnNewContentLoaded(ContentBlock* pContentBlock)
{
    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);

    for (auto* pEntityData : pContentBlock->EntityDataBlocks)
    {
        for (auto* pComponent : pEntityData->Components)
        {
            if (pComponent->GetManager() == MeshComponentMgr::Get())
            {
                const Mesh*  pMesh       = reinterpret_cast<MeshComponent*>(pComponent)->GetData().pMesh;
                const size_t numSurfaces = pMesh->GetNumSurfaces();

                for (uint32_t i = 0; i < numSurfaces; ++i)
                {
                    const Surface*  pSurface  = pMesh->GetSurface(i);

                    if (pComponent->GetOwner()->HasComponent(AnimationComponentMgr::Get()))
                    {
                        const auto& data = pComponent->GetOwner()->GetComponent<const AnimationComponent>(AnimationComponentMgr::Get())->GetData();

                        if (data->m_skinId != -1)
                        {
                            const auto& skinningMatrices = AnimationComponentMgr::Get()->GetSkinningMatrices(data->m_modelId, data->m_skinId);

                            ParameterSet* parameterSet = ParameterSet::CreateParameterSet(m_pRootSignature);
                            parameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(MatrixPair) * MAX_NUM_BONES, 0);
                            parameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(VertexStrides), 1);

                            VertexStrides* vertexStrides = new VertexStrides();
                            InitVertexStrides(pSurface, vertexStrides);

                            parameterSet->SetBufferSRV(pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(VertexAttributeType::Position)).pBuffer, 0);
                            parameterSet->SetBufferSRV(pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(VertexAttributeType::Normal)).pBuffer, 1);
                            parameterSet->SetBufferSRV(pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(VertexAttributeType::Weights0)).pBuffer, 2);
                            parameterSet->SetBufferSRV(pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(VertexAttributeType::Joints0)).pBuffer, 3);

                            const uint32_t surfaceID = pSurface->GetSurfaceID();

                            parameterSet->SetBufferUAV(data->m_skinnedPositions[surfaceID].pBuffer, 0);
                            parameterSet->SetBufferUAV(data->m_skinnedPreviousPosition[surfaceID].pBuffer, 1);
                            parameterSet->SetBufferUAV(data->m_skinnedNormals[surfaceID].pBuffer, 2);

                            m_SkinningBlobs.push_back({data, pSurface, &skinningMatrices, parameterSet, vertexStrides});
                        }
                    }
                }
            }
        }
    }

}

void SkinningRenderModule::OnContentUnloaded(ContentBlock* pContentBlock)
{
}

void cauldron::SkinningRenderModule::InitVertexStrides(const Surface* pSurface, VertexStrides* vertexStrides)
{
    vertexStrides->positionStride = pSurface->GetVertexBuffer(VertexAttributeType::Position).pBuffer->GetDesc().Stride;
    vertexStrides->normalStride   = pSurface->GetVertexBuffer(VertexAttributeType::Normal).pBuffer->GetDesc().Stride;
    vertexStrides->weights0Stride  = pSurface->GetVertexBuffer(VertexAttributeType::Weights0).pBuffer->GetDesc().Stride;
    vertexStrides->joints0Stride  = pSurface->GetVertexBuffer(VertexAttributeType::Joints0).pBuffer->GetDesc().Stride;

    vertexStrides->numVertices = pSurface->GetVertexBuffer(VertexAttributeType::Position).Count;
}
