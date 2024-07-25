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
#include "render/sampler.h"

#include <FidelityFX/host/ffx_lens.h>

/// @defgroup FfxLensSample FidelityFX Lens sample
/// Sample documentation for FidelityFX Lens
///
/// @ingroup SDKEffects

/// @defgroup LensRM LensRenderModule
/// LensRenderModule Reference Documentation
///
/// @ingroup FfxLensSample
/// @{

namespace cauldron
{
    class RootSignature;
    class RasterView;
    class ParameterSet;
    class PipelineObject;
    class Texture;
    class Buffer;
} // namespace cauldron

/**
 * @class LensRenderModule
 * LensRenderModule handles a number of tasks related to Lens.
 *
 * LensRenderModule takes care of:
 *      - creating UI section that enable users to switch between options of Lens
 *      - performing the lens effect and output to the color target
 */
class LensRenderModule : public cauldron::RenderModule
{
public:
    /**
     * @brief   Constructor with default behavior.
     */
    LensRenderModule()
        : RenderModule(L"LensRenderModule")
    {
    }

    /**
     * @brief   Tear down the FFX API Context and release resources.
     */
    virtual ~LensRenderModule();

    /**
     * @brief   Initialize FFX API Context, setup the intermediate color texture, and setup UI section for Lens.
     */
    void Init(const json& initData) override;

    /**
     * @brief   Setup input/output texture and parameters FFX API needs this frame and then call the FFX Dispatch.
     */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * @brief Called by the framework when resolution changes.
     */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

private:

    void InitFfxContext();

    void DestroyFfxContext();

    /**
     * @brief   Destroy or re-create the FFX API Context.
     */
    void     UpdateLensContext(bool enabled);

    int32_t             m_LensMath = FFX_LENS_FLOAT_PRECISION_32BIT;

    float m_grainScale = 0.0f;
    float m_grainAmount = 0.0f;
    float m_chromAb = 0.0f;
    float m_vignette = 0.0f;
    const double m_seedUpdateRate = 0.02; // every m_seedUpdateRate seconds

    // FidelityFX Lens resources
    const cauldron::Texture*        m_pColorSrc = nullptr;
    const cauldron::Texture*        m_pColorIntermediate = nullptr;
    
    // FidelityFX Lens information
    FfxLensContextDescription m_InitializationParameters = {};
    FfxLensContext            m_LensContext;
    bool                      m_ContextCreated = false;
};
