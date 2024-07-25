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

#include "gpuparticlerendermodule.h"

#include "core/framework.h"
#include "core/components/particlespawnercomponent.h"
#include "core/scene.h"
#include "core/uimanager.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/shadowmapresourcepool.h"
#include "shaders/particlesimulationcommon.h"

#include "render/uploadheap.h"

using namespace cauldron;

// Helper function to align values
inline int align(int value, int alignment)
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}

void GPUParticleRenderModule::Init(const json& initData)
{
    m_pDepthBuffer = GetFramework()->GetRenderTexture(L"DepthTarget");

    // root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(2, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(3, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(4, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(5, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(6, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(7, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(8, ShaderBindStage::Compute, 1);

    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);  // t0 - depth buffer
    signatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);  // t1

    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);  // b0 - per frame
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);  // b1 - per emitter

    SamplerDesc samplerDesc = {FilterFunc::MinMagMipPoint, AddressMode::Wrap, AddressMode::Wrap, AddressMode::Clamp};

    signatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, 1, &samplerDesc);

    m_pRootSignature = RootSignature::CreateRootSignature(L"GPUParticleRenderModule_RootSignature_Simulation", signatureDesc);

    DefineList defineList;
    {
        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pRootSignature);

        // Setup the shaders to build on the pipeline object
        std::wstring shaderPath = L"ParticleSimulation.hlsl";
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"CS_Reset", ShaderModel::SM6_0, &defineList));

        m_pResetParticlesPipelineObj = PipelineObject::CreatePipelineObject(L"ResetParticles_PipelineObj", psoDesc);
    }
    
    {
        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pRootSignature);

        // Setup the shaders to build on the pipeline object
        std::wstring shaderPath = L"ParticleSimulation.hlsl";
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"CS_ClearAliveCount", ShaderModel::SM6_0, &defineList));

        m_pClearAliveCountPipelineObj = PipelineObject::CreatePipelineObject(L"ClearAliveCount_PipelineObj", psoDesc);
    }

    {
        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pRootSignature);

        // Setup the shaders to build on the pipeline object
        std::wstring shaderPath = L"ParticleSimulation.hlsl";
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"CS_Simulate", ShaderModel::SM6_0, &defineList));

        m_pSimulatePipelineObj = PipelineObject::CreatePipelineObject(L"Simulation_PipelineObj", psoDesc);
    }

    {
        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pRootSignature);

        // Setup the shaders to build on the pipeline object
        std::wstring shaderPath = L"ParticleEmit.hlsl";
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"CS_Emit", ShaderModel::SM6_0, &defineList));

        m_pEmitPipelineObj = PipelineObject::CreatePipelineObject(L"Emit_PipelineObj", psoDesc);
    }

    m_pParameters = ParameterSet::CreateParameterSet(m_pRootSignature);
    m_pParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SimulationConstantBuffer), 0);
    m_pParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(EmitterConstantBuffer), 1);

    m_ParallelSort.Init(g_maxParticles, true, true);

    //////////////////////////////////////////////////////////////////////////
    // Build UI
    UISection* uiSection = GetUIManager()->RegisterUIElements("Particle");
    if (uiSection)
    {
        uiSection->RegisterUIElement<UICheckBox>("Particle animation", m_bPlayAnimations);
        uiSection->RegisterUIElement<UICheckBox>("Sort", m_bSort);
    }

    //////////////////////////////////////////////////////////////////////////
    // Register additional execution callbacks during the frame

    // Register a pre-transparency callback to sort particles
    ExecuteCallback callbackPreTrans      = [this](double deltaTime, CommandList* pCmdList) { this->PreTransCallback(deltaTime, pCmdList); };
    ExecutionTuple  callbackPreTransTuple = std::make_pair(L"GPUParticleRenderModule::PreTransCallback", std::make_pair(this, callbackPreTrans));
    GetFramework()->RegisterExecutionCallback(L"TranslucencyRenderModule", true, callbackPreTransTuple);

    //////////////////////////////////////////////////////////////////////////
    // Finish up init

    // We are now ready for use
    SetModuleReady(true);
}

GPUParticleRenderModule::~GPUParticleRenderModule()
{
    delete m_pRootSignature;
    delete m_pSimulatePipelineObj;
    delete m_pEmitPipelineObj;
    delete m_pResetParticlesPipelineObj;
    delete m_pClearAliveCountPipelineObj;
    delete m_pParameters;
}

void GPUParticleRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    GPUScopedProfileCapture GPUParticleSimMarker(pCmdList, L"GPUParticleSim");

    m_ElapsedTime += (float)deltaTime;
    if (m_ElapsedTime > 10.0f)
        m_ElapsedTime -= 10.0f;

    const std::vector<Component*>& particleComponents = ParticleSpawnerComponentMgr::Get()->GetComponentList();
    for (auto* pComp : particleComponents)
    {
        // Skip inactive particle spawners
        if (!pComp->GetOwner()->IsActive())
            continue;

        ParticleSpawnerComponent* pSpawnerComp = static_cast<ParticleSpawnerComponent*>(pComp);
        Execute(deltaTime, pCmdList, pSpawnerComp->GetParticleSystem());
    }
}

void GPUParticleRenderModule::Execute(double deltaTime, CommandList* pCmdList, ParticleSystem* pParticleSystem)
{
    m_pParameters->SetBufferUAV(pParticleSystem->m_pParticleBufferA, 0);
    m_pParameters->SetBufferUAV(pParticleSystem->m_pParticleBufferB, 1);
    m_pParameters->SetBufferUAV(pParticleSystem->m_pDeadListBuffer, 2);
    m_pParameters->SetBufferUAV(pParticleSystem->m_pAliveIndexBuffer, 3);
    m_pParameters->SetBufferUAV(pParticleSystem->m_pAliveDistanceBuffer, 4);
    m_pParameters->SetBufferUAV(pParticleSystem->m_pMaxRadiusBuffer, 5);
    m_pParameters->SetBufferUAV(pParticleSystem->m_pPackedViewSpaceParticlePositions, 6);
    m_pParameters->SetBufferUAV(pParticleSystem->m_pIndirectArgsBuffer, 7);
    m_pParameters->SetBufferUAV(pParticleSystem->m_pAliveCountBuffer, 8);
    m_pParameters->SetTextureSRV(m_pDepthBuffer, ViewDimension::Texture2D, 0);
    m_pParameters->SetTextureSRV(pParticleSystem->m_pRandomTexture, ViewDimension::Texture2D, 1);

    if (pParticleSystem->m_WriteBufferStates == ResourceState::CommonResource)
    {
        Barrier barriers[4] = {
            Barrier::Transition(pParticleSystem->m_pParticleBufferB->GetResource(), pParticleSystem->m_WriteBufferStates, ResourceState::UnorderedAccess),
            Barrier::Transition(pParticleSystem->m_pDeadListBuffer->GetResource(), pParticleSystem->m_WriteBufferStates, ResourceState::UnorderedAccess),
            Barrier::Transition(pParticleSystem->m_pAliveDistanceBuffer->GetResource(), pParticleSystem->m_WriteBufferStates, ResourceState::UnorderedAccess),
            Barrier::Transition(pParticleSystem->m_pIndirectArgsBuffer->GetResource(), pParticleSystem->m_WriteBufferStates, ResourceState::UnorderedAccess)};
        ResourceBarrier(pCmdList, 4, barriers);
        pParticleSystem->m_WriteBufferStates = ResourceState::UnorderedAccess;
    }
    const CameraInformation& cameraInfo = GetScene()->GetSceneInfo().CameraInfo;
    const ResolutionInfo&    resInfo    = GetFramework()->GetResolutionInfo();

    SimulationConstantBuffer simulationConstants = {};

    memcpy(simulationConstants.m_StartColor, pParticleSystem->m_StartColor, sizeof(simulationConstants.m_StartColor));
    memcpy(simulationConstants.m_EndColor, pParticleSystem->m_EndColor, sizeof(simulationConstants.m_EndColor));
    memcpy(simulationConstants.m_EmitterLightingCenter, pParticleSystem->m_EmitterLightingCenter, sizeof(simulationConstants.m_EmitterLightingCenter));

    simulationConstants.m_ViewProjection = cameraInfo.ViewProjectionMatrix;
    simulationConstants.m_View           = cameraInfo.ViewMatrix;
    simulationConstants.m_ViewInv        = cameraInfo.InvViewMatrix;
    simulationConstants.m_ProjectionInv  = cameraInfo.InvProjectionMatrix;

    simulationConstants.m_EyePosition  = cameraInfo.InvViewMatrix.getCol3();
    simulationConstants.m_SunDirection = Vec4(0.7f, 0.7f, 0, 0);

    simulationConstants.m_ScreenWidth  = resInfo.RenderWidth;
    simulationConstants.m_ScreenHeight = resInfo.RenderHeight;
    simulationConstants.m_MaxParticles = g_maxParticles;
    simulationConstants.m_FrameTime    = (float)deltaTime;

    //Vec4 sunDirectionVS = simulationConstants.m_View * simulationConstants.m_SunDirection;

    simulationConstants.m_ElapsedTime = m_ElapsedTime;

    //GPUScopedProfileCapture GPUParticleSimMarker(pCmdList, L"Simulation");

    // Update simulation CB info
    BufferAddressInfo simulationBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SimulationConstantBuffer), &simulationConstants);
    m_pParameters->UpdateRootConstantBuffer(&simulationBufferInfo, 0);

    m_pParameters->Bind(pCmdList, m_pSimulatePipelineObj);

    {
        Barrier barriers[5] = {
            Barrier::Transition(pParticleSystem->m_pParticleBufferA->GetResource(), pParticleSystem->m_ReadBufferStates, ResourceState::UnorderedAccess),
            Barrier::Transition(pParticleSystem->m_pPackedViewSpaceParticlePositions->GetResource(), pParticleSystem->m_ReadBufferStates, ResourceState::UnorderedAccess),
            Barrier::Transition(pParticleSystem->m_pMaxRadiusBuffer->GetResource(), pParticleSystem->m_ReadBufferStates, ResourceState::UnorderedAccess),
            Barrier::Transition(pParticleSystem->m_pAliveIndexBuffer->GetResource(), pParticleSystem->m_ReadBufferStates, ResourceState::UnorderedAccess),
            Barrier::Transition(pParticleSystem->m_pAliveCountBuffer->GetResource(), pParticleSystem->m_ReadBufferStates, ResourceState::UnorderedAccess)};
        ResourceBarrier(pCmdList, 5, barriers);
        pParticleSystem->m_ReadBufferStates = ResourceState::UnorderedAccess;
    }

    {
        SetPipelineState(pCmdList, m_pClearAliveCountPipelineObj);

        Dispatch(pCmdList, 1, 1, 1);

        Barrier barriers[2] = {Barrier::UAV(pParticleSystem->m_pAliveCountBuffer->GetResource()),
                               Barrier::UAV(pParticleSystem->m_pIndirectArgsBuffer->GetResource())};
        ResourceBarrier(pCmdList, 2, barriers);
    }
    

    // If we are resetting the particle system, then initialize the dead list
    if (m_bResetSystem)
    {
        SetPipelineState(pCmdList, m_pResetParticlesPipelineObj);

        Dispatch(pCmdList, align(g_maxParticles, 256) / 256, 1, 1);

        {
            Barrier barriers[3] = {Barrier::UAV(pParticleSystem->m_pParticleBufferA->GetResource()),
                                   Barrier::UAV(pParticleSystem->m_pParticleBufferB->GetResource()),
                                   Barrier::UAV(pParticleSystem->m_pDeadListBuffer->GetResource())};
            ResourceBarrier(pCmdList, 3, barriers);
        }

        m_bResetSystem = false;
    }

    if (m_bPlayAnimations)
        Emit(pCmdList, pParticleSystem);

    Simulate(pCmdList, pParticleSystem);

    {
        Barrier barriers[6] = {Barrier::Transition(pParticleSystem->m_pParticleBufferA->GetResource(),
                                                   pParticleSystem->m_ReadBufferStates,
                                                   ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource),
                               Barrier::Transition(pParticleSystem->m_pPackedViewSpaceParticlePositions->GetResource(),
                                                   pParticleSystem->m_ReadBufferStates,
                                                   ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource),
                               Barrier::Transition(pParticleSystem->m_pMaxRadiusBuffer->GetResource(),
                                                   pParticleSystem->m_ReadBufferStates,
                                                   ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource),
                               Barrier::Transition(pParticleSystem->m_pAliveIndexBuffer->GetResource(),
                                                   pParticleSystem->m_ReadBufferStates,
                                                   ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource),
                               Barrier::Transition(pParticleSystem->m_pAliveCountBuffer->GetResource(),
                                                   pParticleSystem->m_ReadBufferStates,
                                                   ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource),
                               Barrier::UAV(pParticleSystem->m_pDeadListBuffer->GetResource())};
        ResourceBarrier(pCmdList, 6, barriers);
    }
    pParticleSystem->m_ReadBufferStates = ResourceState(ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);

    pParticleSystem->m_RenderReady.store(true);
}

void GPUParticleRenderModule::PreTransCallback(double deltaTime, cauldron::CommandList* pCmdList)
{
    const std::vector<Component*>& particleComponents = ParticleSpawnerComponentMgr::Get()->GetComponentList();
    if (particleComponents.empty())
        return;

    GPUScopedProfileCapture GPUParticleSimMarker(pCmdList, L"Pre-Trans (particles sorting)");

    for (auto* pComp : particleComponents)
    {
        // Skip inactive particle spawners
        if (!pComp->GetOwner()->IsActive())
            continue;

        ParticleSpawnerComponent* pSpawnerComp    = static_cast<ParticleSpawnerComponent*>(pComp);
        ParticleSystem*           pParticleSystem = pSpawnerComp->GetParticleSystem();

        if (!pParticleSystem->m_RenderReady)
            continue;

        // Sort if requested. Not doing so results in the particles rendering out of order and not blending correctly
        if (m_bSort && pParticleSystem->m_Sort)
        {
            {
                Barrier barriers[2] = {
                    Barrier::Transition(pParticleSystem->m_pAliveIndexBuffer->GetResource(), pParticleSystem->m_ReadBufferStates, ResourceState::UnorderedAccess),
                    Barrier::Transition(pParticleSystem->m_pAliveCountBuffer->GetResource(), pParticleSystem->m_ReadBufferStates, ResourceState::CopySource)};
                ResourceBarrier(pCmdList, 2, barriers);
            }

            NumKeys numKeys;
            numKeys.pNumKeysBuffer = pParticleSystem->m_pAliveCountBuffer;
            m_ParallelSort.Execute(pCmdList, numKeys, pParticleSystem->m_pAliveDistanceBuffer, pParticleSystem->m_pAliveIndexBuffer);

            {
                Barrier barriers[2] = {
                    Barrier::Transition(pParticleSystem->m_pAliveIndexBuffer->GetResource(), ResourceState::UnorderedAccess, pParticleSystem->m_ReadBufferStates),
                    Barrier::Transition(pParticleSystem->m_pAliveCountBuffer->GetResource(), ResourceState::CopySource, pParticleSystem->m_ReadBufferStates)};
                ResourceBarrier(pCmdList, 2, barriers);
            }
        }
    }
}

// Per-frame emission of particles into the GPU simulation
void GPUParticleRenderModule::Emit(CommandList* pCmdList, ParticleSystem* pParticleSystem)
{
    //GPUScopedProfileCapture GPUParticleSimMarker(pCmdList, L"Emit");

    SetPipelineState(pCmdList, m_pEmitPipelineObj);

    // Run CS for each emitter
    for (int i = 0; i < pParticleSystem->m_Emitters.size(); ++i)
    {
        ParticleSystem::Emitter& emitter = pParticleSystem->m_Emitters[i];
        if (emitter.ParticlesPerSecond > 0)
        {
            // Update emitter CB info
            EmitterConstantBuffer emitterConstants   = {};
            emitterConstants.m_EmitterPosition       = Vec4(pParticleSystem->m_Position + emitter.SpawnOffset, 1.0f);
            emitterConstants.m_EmitterVelocity       = Vec4(emitter.SpawnVelocity, 1.0f);
            emitterConstants.m_MaxParticlesThisFrame = emitter.NumToEmit;
            emitterConstants.m_ParticleLifeSpan      = emitter.Lifespan;
            emitterConstants.m_StartSize             = emitter.SpawnSize;
            emitterConstants.m_EndSize               = emitter.KillSize;
            emitterConstants.m_PositionVariance      = Vec4(emitter.SpawnOffsetVariance, 1.0f);
            emitterConstants.m_VelocityVariance      = emitter.SpawnVelocityVariance;
            emitterConstants.m_Mass                  = emitter.Mass;
            emitterConstants.m_Index                 = i;
            emitterConstants.m_TextureIndex          = emitter.AtlasIndex;
            emitterConstants.m_Streaks               = (emitter.Flags & EmitterDesc::EF_Streaks) ? 1 : 0;

            BufferAddressInfo emitterBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(EmitterConstantBuffer), &emitterConstants);
            m_pParameters->UpdateRootConstantBuffer(&emitterBufferInfo, 1);

            m_pParameters->Bind(pCmdList, m_pEmitPipelineObj);

            // Dispatch enough thread groups to spawn the requested particles
            int numThreadGroups = align(emitter.NumToEmit, 1024) / 1024;
            if (numThreadGroups)
            {
                Dispatch(pCmdList, numThreadGroups, 1, 1);

                {
                    Barrier barriers[1] = { Barrier::UAV(pParticleSystem->m_pDeadListBuffer->GetResource()) };
                    ResourceBarrier(pCmdList, 1, barriers);
                }
            }
        }
    }

    {
        Barrier barriers[1] = {Barrier::UAV(pParticleSystem->m_pParticleBufferA->GetResource())};
        ResourceBarrier(pCmdList, 1, barriers);
        barriers[0] = Barrier::UAV(pParticleSystem->m_pParticleBufferB->GetResource());
        ResourceBarrier(pCmdList, 1, barriers);
    }
}

// Per-frame simulation step
void GPUParticleRenderModule::Simulate(CommandList* pCmdList, ParticleSystem* pParticleSystem)
{
    SetPipelineState(pCmdList, m_pSimulatePipelineObj);
    Dispatch(pCmdList, align(g_maxParticles, 256) / 256, 1, 1);
}
