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
#include "core/uimanager.h"

#include <FidelityFX/host/ffx_cacao.h>

#include <functional>

/// @defgroup FfxCacaoSample FidelityFX CACAO sample
/// Sample documentation for FidelityFX CACAO
///
/// @ingroup SDKEffects

/// @defgroup CacaoRM CacaoRenderModule
/// CacaoRenderModule Reference Documentation
///
/// @ingroup FfxCacaoSample
/// @{
typedef struct CacaoPreset{
	bool useDownsampledSsao;
	FfxCacaoSettings settings;
} CacaoPreset;


static std::vector<const char*> s_FfxCacaoPresetNames = {
	"Native - Adaptive Quality",
	"Native - High Quality",
	"Native - Medium Quality",
	"Native - Low Quality",
	"Native - Lowest Quality",
	"Downsampled - Adaptive Quality",
	"Downsampled - High Quality",
	"Downsampled - Medium Quality",
	"Downsampled - Low Quality",
	"Downsampled - Lowest Quality",
	"Custom"
};

static const CacaoPreset s_FfxCacaoPresets[] = {
	// Native - Adaptive Quality
	{
		/* useDownsampledSsao */ false,
		{
			/* radius                            */ 1.2f,
			/* shadowMultiplier                  */ 1.0f,
			/* shadowPower                       */ 1.50f,
			/* shadowClamp                       */ 0.98f,
			/* horizonAngleThreshold             */ 0.06f,
			/* fadeOutFrom                       */ 20.0f,
			/* fadeOutTo                         */ 40.0f,
			/* qualityLevel                      */ FFX_CACAO_QUALITY_HIGHEST,
			/* adaptiveQualityLimit              */ 0.75f,
			/* blurPassCount                     */ 2,
			/* sharpness                         */ 0.98f,
			/* temporalSupersamplingAngleOffset  */ 0.0f,
			/* temporalSupersamplingRadiusOffset */ 0.0f,
			/* detailShadowStrength              */ 0.5f,
			/* generateNormals                   */ false,
			/* bilateralSigmaSquared             */ 5.0f,
			/* bilateralSimilarityDistanceSigma  */ 0.1f,
		}
	},
	// Native - High Quality
	{
		/* useDownsampledSsao */ false,
		{
			/* radius                            */ 1.2f,
			/* shadowMultiplier                  */ 1.0f,
			/* shadowPower                       */ 1.50f,
			/* shadowClamp                       */ 0.98f,
			/* horizonAngleThreshold             */ 0.06f,
			/* fadeOutFrom                       */ 20.0f,
			/* fadeOutTo                         */ 40.0f,
			/* qualityLevel                      */ FFX_CACAO_QUALITY_HIGH,
			/* adaptiveQualityLimit              */ 0.75f,
			/* blurPassCount                     */ 2,
			/* sharpness                         */ 0.98f,
			/* temporalSupersamplingAngleOffset  */ 0.0f,
			/* temporalSupersamplingRadiusOffset */ 0.0f,
			/* detailShadowStrength              */ 0.5f,
			/* generateNormals                   */ false,
			/* bilateralSigmaSquared             */ 5.0f,
			/* bilateralSimilarityDistanceSigma  */ 0.1f,
		}
	},
	// Native - Medium Quality
	{
		/* useDownsampledSsao */ false,
		{
			/* radius                            */ 1.2f,
			/* shadowMultiplier                  */ 1.0f,
			/* shadowPower                       */ 1.50f,
			/* shadowClamp                       */ 0.98f,
			/* horizonAngleThreshold             */ 0.06f,
			/* fadeOutFrom                       */ 20.0f,
			/* fadeOutTo                         */ 40.0f,
			/* qualityLevel                      */ FFX_CACAO_QUALITY_MEDIUM,
			/* adaptiveQualityLimit              */ 0.75f,
			/* blurPassCount                     */ 2,
			/* sharpness                         */ 0.98f,
			/* temporalSupersamplingAngleOffset  */ 0.0f,
			/* temporalSupersamplingRadiusOffset */ 0.0f,
			/* detailShadowStrength              */ 0.5f,
			/* generateNormals                   */ false,
			/* bilateralSigmaSquared             */ 5.0f,
			/* bilateralSimilarityDistanceSigma  */ 0.1f,
		}
	},
	// Native - Low Quality
	{
		/* useDownsampledSsao */ false,
		{
			/* radius                            */ 1.2f,
			/* shadowMultiplier                  */ 1.0f,
			/* shadowPower                       */ 1.50f,
			/* shadowClamp                       */ 0.98f,
			/* horizonAngleThreshold             */ 0.06f,
			/* fadeOutFrom                       */ 20.0f,
			/* fadeOutTo                         */ 40.0f,
			/* qualityLevel                      */ FFX_CACAO_QUALITY_LOW,
			/* adaptiveQualityLimit              */ 0.75f,
			/* blurPassCount                     */ 6,
			/* sharpness                         */ 0.98f,
			/* temporalSupersamplingAngleOffset  */ 0.0f,
			/* temporalSupersamplingRadiusOffset */ 0.0f,
			/* detailShadowStrength              */ 0.5f,
			/* generateNormals                   */ false,
			/* bilateralSigmaSquared             */ 5.0f,
			/* bilateralSimilarityDistanceSigma  */ 0.1f,
		}
	},
	// Native - Lowest Quality
	{
		/* useDownsampledSsao */ false,
		{
			/* radius                            */ 1.2f,
			/* shadowMultiplier                  */ 1.0f,
			/* shadowPower                       */ 1.50f,
			/* shadowClamp                       */ 0.98f,
			/* horizonAngleThreshold             */ 0.06f,
			/* fadeOutFrom                       */ 20.0f,
			/* fadeOutTo                         */ 40.0f,
			/* qualityLevel                      */ FFX_CACAO_QUALITY_LOWEST,
			/* adaptiveQualityLimit              */ 0.75f,
			/* blurPassCount                     */ 6,
			/* sharpness                         */ 0.98f,
			/* temporalSupersamplingAngleOffset  */ 0.0f,
			/* temporalSupersamplingRadiusOffset */ 0.0f,
			/* detailShadowStrength              */ 0.5f,
			/* generateNormals                   */ false,
			/* bilateralSigmaSquared             */ 5.0f,
			/* bilateralSimilarityDistanceSigma  */ 0.1f,
		}
	},
	// Downsampled - Highest Quality
	{
		/* useDownsampledSsao */ true,
		{
			/* radius                            */ 1.2f,
			/* shadowMultiplier                  */ 1.0f,
			/* shadowPower                       */ 1.50f,
			/* shadowClamp                       */ 0.98f,
			/* horizonAngleThreshold             */ 0.06f,
			/* fadeOutFrom                       */ 20.0f,
			/* fadeOutTo                         */ 40.0f,
			/* qualityLevel                      */ FFX_CACAO_QUALITY_HIGHEST,
			/* adaptiveQualityLimit              */ 0.75f,
			/* blurPassCount                     */ 2,
			/* sharpness                         */ 0.98f,
			/* temporalSupersamplingAngleOffset  */ 0.0f,
			/* temporalSupersamplingRadiusOffset */ 0.0f,
			/* detailShadowStrength              */ 0.5f,
			/* generateNormals                   */ false,
			/* bilateralSigmaSquared             */ 5.0f,
			/* bilateralSimilarityDistanceSigma  */ 0.1f,
		}
	},
	// Downsampled - High Quality
	{
		/* useDownsampledSsao */ true,
		{
			/* radius                            */ 1.2f,
			/* shadowMultiplier                  */ 1.0f,
			/* shadowPower                       */ 1.50f,
			/* shadowClamp                       */ 0.98f,
			/* horizonAngleThreshold             */ 0.06f,
			/* fadeOutFrom                       */ 20.0f,
			/* fadeOutTo                         */ 40.0f,
			/* qualityLevel                      */ FFX_CACAO_QUALITY_HIGH,
			/* adaptiveQualityLimit              */ 0.75f,
			/* blurPassCount                     */ 2,
			/* sharpness                         */ 0.98f,
			/* temporalSupersamplingAngleOffset  */ 0.0f,
			/* temporalSupersamplingRadiusOffset */ 0.0f,
			/* detailShadowStrength              */ 0.5f,
			/* generateNormals                   */ false,
			/* bilateralSigmaSquared             */ 5.0f,
			/* bilateralSimilarityDistanceSigma  */ 0.1f,
		}
	},
	// Downsampled - Medium Quality
	{
		/* useDownsampledSsao */ true,
		{
			/* radius                            */ 1.2f,
			/* shadowMultiplier                  */ 1.0f,
			/* shadowPower                       */ 1.50f,
			/* shadowClamp                       */ 0.98f,
			/* horizonAngleThreshold             */ 0.06f,
			/* fadeOutFrom                       */ 20.0f,
			/* fadeOutTo                         */ 40.0f,
			/* qualityLevel                      */ FFX_CACAO_QUALITY_MEDIUM,
			/* adaptiveQualityLimit              */ 0.75f,
			/* blurPassCount                     */ 3,
			/* sharpness                         */ 0.98f,
			/* temporalSupersamplingAngleOffset  */ 0.0f,
			/* temporalSupersamplingRadiusOffset */ 0.0f,
			/* detailShadowStrength              */ 0.5f,
			/* generateNormals                   */ false,
			/* bilateralSigmaSquared             */ 5.0f,
			/* bilateralSimilarityDistanceSigma  */ 0.2f,
		}
	},
	// Downsampled - Low Quality
	{
		/* useDownsampledSsao */ true,
		{
			/* radius                            */ 1.2f,
			/* shadowMultiplier                  */ 1.0f,
			/* shadowPower                       */ 1.50f,
			/* shadowClamp                       */ 0.98f,
			/* horizonAngleThreshold             */ 0.06f,
			/* fadeOutFrom                       */ 20.0f,
			/* fadeOutTo                         */ 40.0f,
			/* qualityLevel                      */ FFX_CACAO_QUALITY_LOW,
			/* adaptiveQualityLimit              */ 0.75f,
			/* blurPassCount                     */ 6,
			/* sharpness                         */ 0.98f,
			/* temporalSupersamplingAngleOffset  */ 0.0f,
			/* temporalSupersamplingRadiusOffset */ 0.0f,
			/* detailShadowStrength              */ 0.5f,
			/* generateNormals                   */ false,
			/* bilateralSigmaSquared             */ 8.0f,
			/* bilateralSimilarityDistanceSigma  */ 0.8f,
		}
	},
	// Downsampled - Lowest Quality
	{
		/* useDownsampledSsao */ true,
		{
			/* radius                            */ 1.2f,
			/* shadowMultiplier                  */ 1.0f,
			/* shadowPower                       */ 1.50f,
			/* shadowClamp                       */ 0.98f,
			/* horizonAngleThreshold             */ 0.06f,
			/* fadeOutFrom                       */ 20.0f,
			/* fadeOutTo                         */ 40.0f,
			/* qualityLevel                      */ FFX_CACAO_QUALITY_LOWEST,
			/* adaptiveQualityLimit              */ 0.75f,
			/* blurPassCount                     */ 6,
			/* sharpness                         */ 0.98f,
			/* temporalSupersamplingAngleOffset  */ 0.0f,
			/* temporalSupersamplingRadiusOffset */ 0.0f,
			/* detailShadowStrength              */ 0.5f,
			/* generateNormals                   */ false,
			/* bilateralSigmaSquared             */ 8.0f,
			/* bilateralSimilarityDistanceSigma  */ 0.8f,
		}
	}
};


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
 * @class CACAORenderModule
 * 	
 * This RenderModule -- using the depth, color, and optionally normal targets -- performs SSAO and outputs it to the color target. It also creates a UI section enabling users to modify the settings used for CACAO.
 * 
 */
class CACAORenderModule : public cauldron::RenderModule
{
public:
	/**
	 * @brief Construct a new CACAORenderModule object
	 * 
	 */
    CACAORenderModule() : RenderModule(L"CACAORenderModule") {}

	/**
	 * @brief Destroy the CACAORenderModule object
	 * 
	 */
    virtual ~CACAORenderModule();

	/**
	 * @brief Initialize the FFX API Context, setup CACAO settings to default, and setup the UI section for CACAO.
	 */
    void Init(const json& initData) override;

	    /**
     * @brief   If render module is enabled, initialize the FSR 1 API Context. If disabled, destroy the FSR 1 API Context.
     */
    void EnableModule(bool enabled) override;
	
	/**
	 * @brief Prepare input/output textures, parameters, and other resources necessary for the frame then call the FFX Dispatch.
	 */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

	/**
	 * @brief Called by the framework when resolution changes.
	 */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

    /**
    * @brief If set to true, CACAO will output to the callback target to be presented to the screen. If set to false, it will output to the SSAO channel.
    */
    void SetOutputToCallbackTarget(const bool outputToCallbackTarget);

    /**
    * @brief Init UI from master.
    */
    void InitUI(cauldron::UISection* uiSection);

private:

    // Input/Output Textures
    const cauldron::Texture*        m_pColorTarget        = nullptr;
    const cauldron::Texture*        m_pCallbackColorTarget = nullptr;
    const cauldron::Texture*        m_pDepthTarget        = nullptr;
    const cauldron::Texture*        m_pNormalTarget       = nullptr;

    // Prepare output resources
    const cauldron::RasterView* m_pColorRasterView = nullptr;
    cauldron::SamplerDesc       m_LinearSamplerDesc;
    cauldron::RootSignature*    m_pPrepareOutputRS       = nullptr;
    cauldron::PipelineObject*   m_pPrepareOutputPipeline = nullptr;
    cauldron::ParameterSet*     m_pParamSet                 = nullptr;

    // FidelityFX CACAO information
    int32_t                  m_PresetId = _countof(s_FfxCacaoPresets);
    bool                     m_UseDownsampledSSAO = false;
    bool                     m_GenerateNormals = false;
    FfxCacaoSettings         m_CacaoSettings;
    FfxCacaoContext          m_CacaoContext;
    FfxCacaoContext          m_CacaoDownsampledContext;
    bool                     m_ContextCreated = false;
    bool                     m_OutputToCallbackTarget = true;

	//Sample UI
    std::vector<cauldron::UIElement*> m_UIElements;  // weak ptr.

    FfxInterface m_FfxInterface = {0};

    void InitSdkContexts();

    void CreateCacaoContexts(const cauldron::ResolutionInfo& resInfo);

    void DestroyCacaoContexts();
};
