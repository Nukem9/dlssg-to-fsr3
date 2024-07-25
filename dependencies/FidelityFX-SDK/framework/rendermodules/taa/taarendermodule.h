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
#include "core/uimanager.h"
#include "misc/math.h"

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

/**
* @class TAARenderModule
*
* The TAA render module is responsible for performing the Temporal Anti-Aliasing GPU work when included.
*
* @ingroup CauldronRender
*/
class TAARenderModule : public cauldron::RenderModule
{
public:

    /**
    * @brief   Construction.
    */
    TAARenderModule() : RenderModule(L"TAARenderModule") {}

    /**
    * @brief   Destruction.
    */
    virtual ~TAARenderModule();

    /**
    * @brief   Initialization function. Sets up resource pointers, pipeline objects, root signatures, and parameter sets.
    */
    void Init(const json& initData) override;

    /**
    * @brief   Enables/Disables TAA from executing.
    */
    void EnableModule(bool enabled) override;

    /**
    * @brief   Performs TAA GPU workloads if enabled.
    */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
    * @brief   Callback invoked as part of OnResize events. Allows up to re-init resolution dependent information.
    */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

private:
    bool m_bEnableTaa = true;


    // First and Main TAA
    void                        InitTaa();
    cauldron::RootSignature*    m_pTAARootSignature = nullptr;
    cauldron::ParameterSet*     m_pTAAParameters    = nullptr;
    bool                        m_bFirst            = true;
    cauldron::PipelineObject*   m_pFirstPipelineObj = nullptr;
    cauldron::PipelineObject*   m_pTAAPipelineObj   = nullptr;

    // Sharpner
    void                        InitPost();
    cauldron::RootSignature*    m_pPostRootSignature = nullptr;
    cauldron::ParameterSet*     m_pPostParameters    = nullptr;
    cauldron::PipelineObject*   m_pPostPipelineObj        = nullptr;

    // TAA resources
    const cauldron::Texture*    m_pColorBuffer      = nullptr;
    const cauldron::Texture*    m_pDepthBuffer      = nullptr;
    const cauldron::Texture*    m_pHistoryBuffer    = nullptr;
    const cauldron::Texture*    m_pVelocityBuffer   = nullptr;
    const cauldron::Texture*    m_pTAAOutputBuffer  = nullptr;

    // UI
    cauldron::UISection* m_UISection = nullptr; // weak ptr.

    Vec2 CalculateJitterOffsets(uint32_t width, uint32_t height, uint32_t& seed);
};
