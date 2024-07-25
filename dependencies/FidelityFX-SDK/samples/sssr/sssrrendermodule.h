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

#include "core/contentmanager.h"
#include "render/rendermodule.h"

#include <FidelityFX/host/ffx_sssr.h>

namespace cauldron
{
    class ParameterSet;
    class PipelineObject;
    class RenderTarget;
    struct RootSignatureDesc;
    class RootSignature;
    class Texture;
    class Sampler;
    class RasterView;
}  // namespace cauldron

class SSSRRenderModule : public cauldron::RenderModule
{
public:
    SSSRRenderModule() : RenderModule(L"SSSRRenderModule"){}
    virtual ~SSSRRenderModule();

    void Init(const json& initData) override;
    void InitUI();
    void OnResize(const  cauldron::ResolutionInfo& resInfo) override;
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

private:
    void InitFfxContext();
    void DestroyFfxContext();
    void ResetSSSRContext();
    void CreateSSSRContext();

    float m_SceneSpecularIBLFactor;

    // SSSR Context members
    FfxSssrContextDescription m_InitializationParameters = {};
    FfxSssrContext            m_Context;

    // Debug features
    bool        m_ApplyScreenSpaceReflections   = true;
    bool        m_ShowReflectionTarget          = false;
    // This value should always be 1 for PBR correctness. We expose it in the UI to help visualize reflections better.
    float       m_SpecularReflectionsMultiplier = 1.0f;

    // SSSR settings
    int32_t     m_MaxTraversalIntersections             = 128;
    int32_t     m_MinTraversalOccupancy                 = 4;
    int32_t     m_MostDetailedMip                       = 0;
    float       m_DepthBufferThickness                  = 0.015f;
    float       m_RoughnessThreshold                    = 0.2f;
    float       m_TemporalStabilityFactor               = 0.7f;
    float       m_VarianceThreshold                     = 0.0f;
    bool        m_TemporalVarianceGuidedTracingEnabled  = true;
    int32_t     m_SamplesPerQuadOptionIndex             = 0;
    uint32_t    m_SamplesPerQuad                        = 1;

    // SSSR resources
    const cauldron::Texture* m_pColorTarget = nullptr;
    const cauldron::Texture* m_pDepthTarget = nullptr;
    const cauldron::Texture* m_pOutput = nullptr;
    const cauldron::Texture* m_pBaseColor = nullptr;
    const cauldron::Texture* m_pMotionVectors = nullptr;
    const cauldron::Texture* m_pNormal = nullptr;
    const cauldron::Texture* m_pAoRoughnessMetallic = nullptr;
    const cauldron::Texture* m_pPrefilteredEnvironmentMap = nullptr;
    const cauldron::Texture* m_pBrdfTexture = nullptr;

    // Apply reflection resources
    const cauldron::RasterView* m_pColorRasterView = nullptr;
    cauldron::SamplerDesc m_LinearSamplerDesc;
    cauldron::RootSignature* m_pApplyReflectionsRS = nullptr;
    cauldron::PipelineObject* m_pApplyReflectionsPipeline = nullptr;
    cauldron::ParameterSet* m_pParamSet = nullptr;
};
