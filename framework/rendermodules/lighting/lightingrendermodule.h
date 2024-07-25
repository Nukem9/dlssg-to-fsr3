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
#include "shaders/lightingcommon.h"

#include <vector>

namespace cauldron
{
    class RootSignature;
    class RasterView;
    class ParameterSet;
    class PipelineObject;
    class RenderTarget;
    class Texture;
    class ResourceView;
} // namespace cauldron

/**
* @class LightingRenderModule
*
* The lighting render module is responsible rendering deferred lighting from the gbuffer information.
*
* @ingroup CauldronRender
*/
class LightingRenderModule : public cauldron::RenderModule
{
public:

    /**
    * @brief   Construction.
    */
    LightingRenderModule() : RenderModule(L"LightingRenderModule") {}

    /**
    * @brief   Destruction.
    */
    virtual ~LightingRenderModule();

    /**
    * @brief   Initialization function. Sets up resource pointers, pipeline objects, root signatures, and parameter sets.
    */
    void Init(const json& initData) override;

    /**
    * @brief   Performs deferred lighting pass if enabled.
    */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

private:

    cauldron::RootSignature*    m_pRootSignature                = nullptr;
    cauldron::PipelineObject*   m_pPipelineObj                  = nullptr;
    const cauldron::Texture*    m_pRenderTarget                 = nullptr;
    const cauldron::Texture*    m_pDiffuseTexture               = nullptr;
    const cauldron::Texture*    m_pNormalTexture                = nullptr;
    const cauldron::Texture*    m_pAoRoughnessMetallicTexture   = nullptr;
    const cauldron::Texture*    m_pDepthTexture                 = nullptr;
    cauldron::ParameterSet*     m_pParameters                   = nullptr;

    uint32_t            m_ShadowMapCount                        = 0; // temporary variable to track the number of shadow maps

    // Constant data for Lighting pass
    LightingCBData m_LightingConstantData;

    float m_IBLFactor;
};
