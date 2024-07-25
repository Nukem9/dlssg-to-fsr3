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

#include "render/rendermodules/tonemapping/tonemappingrendermodule.h"
#include "core/framework.h"
#include "render/color_conversion.h"
#include "shaders/shadercommon.h"

#include <FidelityFX/host/ffx_lpm.h>

namespace cauldron
{
    class RootSignature;
    class RasterView;
    class ParameterSet;
    class PipelineObject;
    class Texture;
    class Buffer;
}

/// @defgroup FfxLpmSample FidelityFX Luma Preserving Mapper sample
/// Sample documentation for FidelityFX LPM
///
/// @ingroup SDKEffects

/// @defgroup LpmRM LpmRenderModule
/// LpmRenderModule Reference Documentation
///
/// @ingroup FfxLpmSample
/// @{
///

/// @class LPMRenderModule
/// class extends Tonemapping render module and sets itself as the default tone and gamut mapper.
/// LPMRenderModule::Init() sets up the parameters which will be used as input to the tone and gamut mapping done by LPM
/// This includes input and output resource, and all the parameters described in ffx_lpm header.
/// LPMRenderModule::Execute() calls the dispatch function inside ffx_lpm.cpp which calls the functions to setup LPM consts on CPU side and finally call dispatch on LPM compute shader which will call the LPMFilter call to do the tone and gamut mapping.

class LPMRenderModule : public ToneMappingRenderModule
{
public:
    ///
    /// @brief Constructor setting LPMRendermodule as default tonemapper
    ///
    LPMRenderModule() : ToneMappingRenderModule(L"LPMRenderModule") {}

    ///
    /// @brief Destructor releasing FFX resources and context
    ///
    virtual ~LPMRenderModule();

    ///
    /// @brief Initialize FFX API Context, setup input/output resources and setup UI section for LPM.
    ///
    void Init(const json& initData) override;

    void TextureLoadComplete(const std::vector<const cauldron::Texture*>& textureList, void*);

    ///
    /// @brief call FFX dispatch which handles setting up consts for LPM and LPM compute shader dispatch which calls LPM filter to do tone and gamut mapping
    ///
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * @brief Called by the framework when resolution changes.
     */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

private:
    void InitFfxContext();
    void DestroyFfxContext();

    // common
    cauldron::RootSignature*    m_pRootSignature  = nullptr;
    const cauldron::RasterView* m_pRasterView     = nullptr;
    cauldron::PipelineObject*   m_pPipelineObj    = nullptr;
    cauldron::ParameterSet*     m_pParameters     = nullptr;
    cauldron::SamplerDesc       m_LinearSamplerDesc;
    const cauldron::Texture*    m_pTexture        = nullptr;
    const cauldron::Texture*    m_pRenderTarget   = nullptr;

private:
    bool m_Shoulder;
    float m_SoftGap;
    float m_HdrMax;
    float m_LpmExposure;
    float m_Contrast;
    float m_ShoulderContrast;
    float m_Saturation[3];
    float m_Crosstalk[3];
    cauldron::ColorSpace m_ColorSpace;
    DisplayMode m_DisplayMode;

    // LPM Context members
    FfxLpmContextDescription m_InitializationParameters = {};
    FfxLpmContext            m_LPMContext;

    // LPM resources
    const cauldron::Texture* m_pInputColor  = nullptr;
    const cauldron::Texture* m_pOutputColor = nullptr;

    /// @}
};
