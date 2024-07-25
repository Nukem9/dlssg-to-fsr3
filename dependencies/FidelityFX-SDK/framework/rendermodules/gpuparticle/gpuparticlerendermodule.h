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

#include "render/rendermodule.h"
#include "parallelsort.h"

#include <mutex>

namespace cauldron
{
    class ParameterSet;
    class PipelineObject;
    class RootSignature;
    class Texture;
    class ParticleSystem;

}  // namespace cauldron

// 
/**
* @class GPUParticleRenderModule
*
* The GPUParticlesRenderModule is responsible for spawning and simulating all entities with ParticleSpawnerComponents.
* Actual rendering will be handled by the translucency render module (on which GPUParticles is dependent) so they can be sorted
* with other translucent instances.
*
* @ingroup CauldronRender
*/
class GPUParticleRenderModule : public cauldron::RenderModule
{
public:

    /**
    * @brief   Construction.
    */
    GPUParticleRenderModule() : RenderModule(L"GPUParticleRenderModule") {}

    /**
    * @brief   Destruction.
    */
    virtual ~GPUParticleRenderModule();

    /**
    * @brief   Initialization function. Sets up resource pointers, pipeline objects, root signatures, and parameter sets.
    */
    virtual void Init(const json& initData) override;

    /**
    * @brief   Performs GPUParticle simulation if enabled.
    */
    virtual void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
    * @brief   Pre-translucent pass callback to sort particles before rendering in the translucency pass if needed.
    */
    void PreTransCallback(double deltaTime, cauldron::CommandList* pCmdList);

private:

    // No copy, No move
    NO_COPY(GPUParticleRenderModule)
    NO_MOVE(GPUParticleRenderModule)

    void Execute(double deltaTime, cauldron::CommandList* pCmdList, cauldron::ParticleSystem* pParticleSystem);
    void Emit(cauldron::CommandList* pCmdList, cauldron::ParticleSystem* pParticleSystem);
    void Simulate(cauldron::CommandList* pCmdList, cauldron::ParticleSystem* pParticleSystem);

private:

    cauldron::ParameterSet* m_pParameters                   = nullptr;
    cauldron::PipelineObject* m_pSimulatePipelineObj        = nullptr;
    cauldron::PipelineObject* m_pEmitPipelineObj            = nullptr;
    cauldron::PipelineObject* m_pResetParticlesPipelineObj  = nullptr;
    cauldron::PipelineObject* m_pClearAliveCountPipelineObj = nullptr;
    cauldron::RootSignature*  m_pRootSignature              = nullptr;

    const cauldron::Texture* m_pDepthBuffer = nullptr;

    std::mutex m_CriticalSection;

    float m_ElapsedTime  = 0.0f;
    bool  m_bResetSystem = true;

    bool m_bPlayAnimations = true;
    bool m_bSort = true;

    ParallelSort m_ParallelSort;
};
