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
#include "core/framework.h"

#include "FidelityFX/host/ffx_cas.h"

#include <functional>

/// @defgroup FfxCasSample FidelityFX CAS sample
/// Sample documentation for FidelityFX CAS
///
/// @ingroup SDKEffects

/// @defgroup CasRM CasRenderModule
/// CasRenderModule reference documentation
///
/// @ingroup FfxCasSample
/// @{

/**
 * @class CASRenderModule
 * CASRenderModule handles a number of tasks related to CAS.
 *
 * CASRenderModule takes care of:
 *      - creating UI section that enable users to switch between options of CAS
 *      - performing sharpening or upscaling and output to the color target
 */
class CASRenderModule : public cauldron::RenderModule
{
public:
    /**
     * @brief   Constructor with default behavior.
     */
    CASRenderModule()
        : RenderModule(L"CASRenderModule")
    {
    }

    /**
     * @brief   Tear down the FFX API Context and release resources.
     */
    virtual ~CASRenderModule();

    /**
     * @brief   Initialize FFX API Context, setup the internal color texture used as temporary input, and setup UI section for CAS.
     */
    virtual void Init(const json& initData) override;

    /**
     * @brief   Setup input/output texture and parameters FFX API needs this frame and then call the FFX Dispatch.
     */
    virtual void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * @brief   Called by the framework when resolution changes. FFX API Context for CAS need to be reset in respond to this.
     */
    virtual void OnResize(const cauldron::ResolutionInfo& resInfo) override;

private:
    enum CAS_State
    {
        CAS_State_NoCas,
        CAS_State_Upsample,
        CAS_State_SharpenOnly,
    };

    enum class CASScalePreset
    {
        UltraQuality = 0,  // 1.3f
        Quality,           // 1.5f
        Balanced,          // 1.7f
        Performance,       // 2.f
        UltraPerformance,  // 3.f
        Custom             // 1.f - 3.f range
    };

    /**
     * @brief   Required by the framework so that CAS RenderModule can take care of the upscaling ratio.
     */
    cauldron::ResolutionInfo UpdateResolution(uint32_t displayWidth, uint32_t displayHeight);

    /**
     * @brief   Callback function called when upscaling is enabled or upscaling preset is changed.
     */
    void                     UpdatePreset(const int32_t* pOldPreset);

    /**
     * @brief   Callback function for upscaling ratio slider
     */
    void                     UpdateUpscaleRatio(const float* pOldRatio);

    void                     SetupFfxInterface();
    void                     InitCasContext();
    void                     DestroyCasContext();

    CAS_State m_CasState   = CAS_State_SharpenOnly;
    bool      m_CasEnabled = true;
    float     m_Sharpness  = 0.8f;

    CASScalePreset m_ScalePreset         = CASScalePreset::UltraQuality;
    float          m_UpscaleRatio        = 1.3f;
    bool           m_UpscaleRatioEnabled = false;
    bool           m_CasUpscalingEnabled = false;

    // CAS Context members
    FfxCasContextDescription m_InitializationParameters = {0};
    FfxCasContext            m_CasContext;

    // CAS resources
    const cauldron::Texture* m_pColorTarget     = nullptr;
    const cauldron::Texture* m_pTempColorTarget = nullptr;

    // For resolution updates
    std::function<cauldron::ResolutionInfo(uint32_t, uint32_t)> m_pUpdateFunc = nullptr;
};

/// @}
