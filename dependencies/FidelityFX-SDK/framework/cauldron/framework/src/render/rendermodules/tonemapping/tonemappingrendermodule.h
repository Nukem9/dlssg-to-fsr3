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
#include "shaders/tonemapping/tonemappercommon.h"
#include "render/sampler.h"


namespace cauldron
{
    class ParameterSet;
    class PipelineObject;
    class RasterView;
    class ResourceView;
    class RootSignature;
    class RenderTarget;
    class Texture;
}  // namespace cauldron

class ToneMappingRenderModule : public cauldron::RenderModule
{
public:
    ToneMappingRenderModule();
    ToneMappingRenderModule(const wchar_t* pName);
    virtual ~ToneMappingRenderModule();

    virtual void Init(const json& initData) override;
    virtual void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    void SetDoubleBufferedTextureIndex(uint32_t textureIndex);

    /**
     * @brief ClearRenderTarget() on render targets that may not be written to before being read. Called by the framework when the resolution changes.
     */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;
private:
    // No copy, No move
    NO_COPY(ToneMappingRenderModule)
    NO_MOVE(ToneMappingRenderModule)

private:
    // Constant data
    uint32_t m_DispatchThreadGroupCountXY[2];

    AutoExposureSpdConstants m_AutoExposureSpdConstants;
    TonemapperCBData         m_TonemapperConstantData;

    // common
    cauldron::RootSignature*    m_pAutoExposureSpdRootSignature = nullptr;
    cauldron::PipelineObject*   m_pAutoExposureSpdPipelineObj   = nullptr;
    cauldron::ParameterSet*     m_pAutoExposureSpdParameters    = nullptr;

    cauldron::RootSignature*    m_pBuildDistortionFieldRootSignature = nullptr;
    cauldron::PipelineObject*   m_pBuildDistortionFieldPipelineObj   = nullptr;
    cauldron::ParameterSet*     m_pBuildDistortionFieldParameters    = nullptr;

    cauldron::RootSignature*    m_pTonemapperRootSignature  = nullptr;
    const cauldron::RasterView* m_pRasterView               = nullptr;
    cauldron::PipelineObject*   m_pTonemapperPipelineObj    = nullptr;
    cauldron::ParameterSet*     m_pTonemapperParameters     = nullptr;

    const cauldron::Texture* m_pAutomaticExposureSpdAtomicCounter = nullptr;
    const cauldron::Texture* m_pAutomaticExposureMipsShadingChange = nullptr;
    const cauldron::Texture* m_pAutomaticExposureMips5             = nullptr;
    const cauldron::Texture* m_pAutomaticExposureValue             = nullptr;
    cauldron::SamplerDesc    m_LinearSamplerDesc;

    const cauldron::Texture* m_pRenderTargetIn   = nullptr;
    const cauldron::Texture* m_pRenderTargetOut  = nullptr;
    const cauldron::Texture* m_pDistortionField[2] = {};
    const cauldron::RasterView* m_pDistortionFieldRasterView[2] = {};

    uint32_t m_curDoubleBufferedTextureIndex = 0;

    bool shouldClearRenderTargets = true;
};
