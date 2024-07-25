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

#include <FidelityFX/host/ffx_vrs.h>

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
}

/// @defgroup FfxVrsSample FidelityFX VRS sample
/// Sample documentation for FidelityFX VRS
///
/// @ingroup SDKEffects

/// @defgroup VrsRM VrsRenderModule
/// VrsRenderModule Reference Documentation
///
/// @ingroup FfxVrsSample
/// @{

/**
 * @class VRSRenderModule
 *
 * VRSRenderModule takes care of:
 *      - querying hardware VRS support
 *      - generating motion vectors of current frame
 *      - generating VRS image based on motion vectors and history color buffer
 *      - copying color buffer into history color buffer
 *      - displaying overlay of VRS image
 *      - setting VRS options
 */
class VRSRenderModule : public cauldron::RenderModule, public cauldron::ContentListener
{
public:
    /**
     * @brief   Constructor
     */
    VRSRenderModule();
    /**
     * @brief   Destructor
     */
    virtual ~VRSRenderModule();

    /**
     * This function checks hardware VRS suppport, builds user interface, creates GPU resources, sets up
     * callback functions and initializes ffx_vrs backend.
     */
    void Init(const json& initData) override;
    /**
     * Recreate the FFX API context to resize internal resources. Called by the framework when the resolution changes.
     * @param resInfo New resolution info.
     */
    void OnResize(const  cauldron::ResolutionInfo& resInfo) override;
    /**
     * Calls ExecuteVRSImageGen to dipatch computer shader to generate VRS image.
     */
    virtual void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * Creates pipeline object and sets up surface information for each mesh to be rendered in velocity pass.
     */
    virtual void OnNewContentLoaded(cauldron::ContentBlock* pContentBlock) override;
    /**
     * @copydoc ContentListener::OnContentUnloaded()
     */
    virtual void OnContentUnloaded(cauldron::ContentBlock* pContentBlock) override;

private:

    void BuildUI();
    void InitOverlay(const json& initData);
    void InitMotionVectors(const json& initData);
    void InitFfxBackend();
    void InitFfxContext();
    void DestroyFfxContext();

    /**
     * This callback function copies color buffer of current frame into HistoryColorBuffer
     * to be used for next frame to generate VRS image.
     */
    void CopyColorBufferCallback(double deltaTime, cauldron::CommandList* pCmdList);
    /**
     * This callback function draws VRS image over rendered scene.
     */
    void DrawOverlayCallback(double deltaTime, cauldron::CommandList* pCmdList);
    /**
     * This callback function generates motion vector for each pixel. The motion vector
     * will be used to determine on-screen position of a pixel in the last frame.
     */
    void GenerateMotionVectorsCallback(double deltaTime, cauldron::CommandList* pCmdList);

    void ToggleVariableShading();
    void ToggleShadingRateImage();
    void ToggleOverlay();
    void SelectBaseShadingRate();
    void SelectCombiner();

    void UpdateVRSInfo();
    void UpdateVRSContext(bool enabled);

    void ExecuteVRSImageGen(double deltaTime, cauldron::CommandList* pCmdList);

    // Content creation helpers - not thread safe
    uint32_t CreatePipelineObject(const cauldron::Surface* pSurface);

    bool     m_EnableVariableShading       = false;
    uint32_t m_ShadingRateIndex            = 0;
    uint32_t m_ShadingRateCombinerIndex    = 0;
    bool     m_EnableShadingRateImage      = false;
    bool     m_AllowAdditionalShadingRates = false;
    uint32_t m_VRSTierSupported            = 0;
    float    m_VRSThreshold                = 0.015f;
    float    m_VRSMotionFactor             = 0.01f;

    bool m_VariableShadingEnabled  = false;
    bool m_ShadingRateImageEnabled = false;

    std::vector<cauldron::ShadingRateCombiner> m_AvailableCombiners;
    cauldron::FeatureInfo_VRS     m_FeatureInfoVRS;

    const cauldron::Texture* m_pMotionVectors = nullptr;
    const cauldron::Texture* m_pDepthTarget = nullptr;
    const cauldron::Texture* m_pColorTarget   = nullptr;
    const cauldron::Texture* m_pHistoryColorBuffer   = nullptr;
    const cauldron::Texture* m_pVRSTexture      = nullptr;

    // FidelityFX VRS information
    FfxVrsContextDescription m_InitializationParameters = {};
    FfxVrsContext            m_VRSContext;
    bool                     m_ContextCreated = false;

    // Motion Vectors
    bool                        m_GenerateMotionVectors       = false;
    cauldron::RootSignature*    m_pMotionVectorsRootSignature = nullptr;
    cauldron::ParameterSet*     m_pMotionVectorsParameterSet  = nullptr;
    const cauldron::RasterView* m_pMotionVectorsRasterView    = nullptr;
    const cauldron::RasterView* m_pDepthRasterView            = nullptr;

    std::mutex m_CriticalSection;

    struct PipelineSurfaceRenderInfo
    {
        const cauldron::Entity*  pOwner   = nullptr;
        const cauldron::Surface* pSurface = nullptr;
    };

    struct PipelineHashObject
    {
        cauldron::PipelineObject* m_Pipeline     = nullptr;
        uint64_t                  m_PipelineHash = 0;
    };
    std::vector<PipelineHashObject> m_PipelineHashObjects;

    struct MotionVectorsRenderData
    {
        PipelineSurfaceRenderInfo m_RenderSurface = {};
        cauldron::PipelineObject* m_Pipeline      = nullptr;
    };
    std::vector<MotionVectorsRenderData> m_MotionVectorsRenderSurfaces;

    // Overlay
    cauldron::RootSignature*    m_pOverlayRootSignature   = nullptr;
    const cauldron::RasterView* m_pOverlayRasterView      = nullptr;
    cauldron::PipelineObject*   m_pOverlayPipelineObj     = nullptr;
    const cauldron::Texture*    m_pOverlayRenderTarget    = nullptr;
    cauldron::ParameterSet*     m_pOverlayParameters      = nullptr;
    bool                        m_DrawOverlay             = false;

/// @}
};
