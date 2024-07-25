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
#include "core/uimanager.h"
#include "render/rendermodule.h"
#include "shaders/declarations.h"
#include "shaders/common_types.h"

#include <FidelityFX/host/ffx_denoiser.h>
#include <FidelityFX/host/ffx_classifier.h>
#include <FidelityFX/host/ffx_spd.h>

#include <vector>

namespace cauldron
{
    class ParameterSet;
    class PipelineObject;
    class RenderTarget;
    class ResourceView;
    struct RootSignatureDesc;
    class RootSignature;
    class Texture;
    class Sampler;
    class RasterView;
    class IndirectWorkload;
}  // namespace cauldron

/// @defgroup FfxHybridReflectionsSample FidelityFX Hybrid Reflections sample
/// Sample documentation for FidelityFX Hybrid Reflections
///
/// @ingroup SDKEffects

/// @defgroup HybridReflectionsRM HybridReflectionsRenderModule
/// HybridReflectionsRenderModule Reference Documentation
///
/// @ingroup FfxHybridReflectionsSample
/// @{

/**
 * @class HybridReflectionsRenderModule
 *
 * HybridReflectionsRenderModule creates reflections effect by using ffx_classifier, ffx_spd ffx_denoiser
 * techniques. 
 *
 * There are 3 main passes:
 *      - classification
 *      - intersection
 *      - denoising
 */

class HybridReflectionsRenderModule : public cauldron::RenderModule, public cauldron::ContentListener
{
public:
    /**
     * @brief   Constructor
     */
    HybridReflectionsRenderModule()
        : RenderModule(L"HybridReflectionsRenderModule")
    {}
    /**
     * @brief   Destructor
     */
    virtual ~HybridReflectionsRenderModule();

    /**
     * This function checks hardware suppport, builds user interface, creates GPU resources, sets up
     * callback functions, create pipeline objects and initializes ffx_classifier, ffx_spd, ffx_denoiser backend.
     */
    void Init(const json& initData) override;
    /**
     * Recreate the FFX API context to resize internal resources. Called by the framework when the resolution changes.
     * @param resInfo New resolution info.
     */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;
    
    /**
     * Update the Debug Option UI element.
     */
    void UpdateUI(double deltaTime) override;
    
    /**
     * Dispatch all the shaders.
     */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * Prepare shading information for raytracing passes.
     */
    virtual void OnNewContentLoaded(cauldron::ContentBlock* pContentBlock) override;
    /**
     * @copydoc ContentListener::OnContentUnloaded()
     */
    virtual void OnContentUnloaded(cauldron::ContentBlock* pContentBlock) override;

private:
    void CreateFfxContexts();
    void DestroyFfxContexts();

    void ResetBackendContext();
    void CreateBackendContext();

    /**
     * This callback function copies color buffer of current frame into HistoryColorBuffer
     * to be used for next frame.
     */
    void CopyColorBufferCallback(double deltaTime, cauldron::CommandList* pCmdList);

    void ShowDebugTarget();
    void SelectDebugOption();
    void ToggleHybridReflection();
    void ToggleHalfResGBuffer();
    void UpdateReflectionResolution();

    int32_t AddTexture(const cauldron::Material* pMaterial, const cauldron::TextureClass textureClass, int32_t& textureSamplerIndex);
    void    RemoveTexture(int32_t index);

    void CreateResources();

    void BuildUI();

    void InitApplyReflections(const json& initData);
    void InitPrepareBlueNoise(const json& initData);
    void InitPrimaryRayTracing(const json& initData);
    void InitHybridDeferred(const json& initData);
    void InitRTDeferred(const json& initData);
    void InitDeferredShadeRays(const json& initData);
    void InitPrepareIndirectHybrid(const json& initData);
    void InitPrepareIndirectHW(const json& initData);
    void InitCopyDepth(const json& initData);

    void ExecutePrepareBlueNoise(double deltaTime, cauldron::CommandList* pCmdList);
    void ExecuteDepthDownsample(double deltaTime, cauldron::CommandList* pCmdList);
    void ExecutePrimaryRayTracing(double deltaTime, cauldron::CommandList* pCmdList);
    void ExecuteHybridDeferred(double deltaTime, cauldron::CommandList* pCmdList);
    void ExecuteRTDeferred(double deltaTime, cauldron::CommandList* pCmdList);
    void ExecuteDeferredShadeRays(double deltaTime, cauldron::CommandList* pCmdList);
    void ExecuteClassifier(double deltaTime, cauldron::CommandList* pCmdList);
    void ExecuteDenoiser(double deltaTime, cauldron::CommandList* pCmdList);
    void ExecutePrepareIndirectHybrid(double deltaTime, cauldron::CommandList* pCmdList);
    void ExecutePrepareIndirectHW(double deltaTime, cauldron::CommandList* pCmdList);
    void ExecuteApplyReflections(double deltaTime, cauldron::CommandList* pCmdList);

    void UpdatePerFrameConstants();

    float m_SceneSpecularIBLFactor;

    // HSR Context members
    FfxInterface                    m_BackendInterface;
    FfxDenoiserContextDescription   m_DenoiserInitializationParameters = {};
    FfxDenoiserContext              m_DenoiserContext;
    FfxClassifierContextDescription m_ClassifierInitializationParameters = {};
    FfxClassifierContext            m_ClassifierContext;
    FfxSpdContextDescription        m_SpdInitializationParameters = {};
    FfxSpdContext                   m_SpdContext;

    // Debug features
    bool m_ApplyScreenSpaceReflections = true;
    bool m_ShowReflectionTarget        = false;

    uint32_t m_FrameIndex = 0;
    bool     m_IsResized = false;

    // HSR settings
    float    m_SSRConfidenceThreshold               = 0.9f;
    float    m_TemporalStabilityFactor              = 0.7f;
    float    m_DepthBufferThickness                 = 0.2f;
    int32_t  m_MostDetailedMip                      = 0;
    uint32_t m_SamplesPerQuad                       = 4;
    bool     m_TemporalVarianceGuidedTracingEnabled = false;
    float    m_ReflectionResolutionMultiplier       = 1.f;
    uint32_t m_ReflectionUpscaleMode                = 3;
    float    m_FSRRoughnessThreshold                = 0.03f;
    float    m_RoughnessThreshold                   = 0.22f;
    float    m_RTRoughnessThreshold                 = 0.22f;
    bool     m_DisableReshading                     = false;
    bool     m_EnableHybridReflection               = true;
    bool     m_IsEnableHybridReflectionChanged      = false;
    bool     m_EnableHalfResGBuffer                 = false;
    bool     m_ShowDebugTarget                      = false;
    uint32_t m_ReflectionWidth                      = 128;
    uint32_t m_ReflectionHeight                     = 128;
    float    m_HybridMissWeight                     = 0.5f;
    float    m_HybridSpawnRate                      = 0.02f;
    float    m_VRTVarianceThreshold                 = 0.02f;
    float    m_SSRThicknessLengthFactor             = 0.01f;
    float    m_ReflectionsBackfacingThreshold       = 1.f;
    uint32_t m_RandomSamplesPerPixel                = 32;
    float    m_MaxRaytracedDistance                 = 100.f;
    float    m_RayLengthExpFactor                   = 5.f;
    uint32_t m_MinTraversalOccupancy                = 0;
    uint32_t m_MaxTraversalIntersections            = 128;
    float    m_EmissiveFactor                       = 30.f;
    float    m_ReflectionFactor                     = 1.3f;
    uint32_t m_DebugOption                          = 0;
    uint32_t m_Mask                                 = 0;

    bool m_HalfResGBufferDisabled = true;

    // HSR resources
    const cauldron::Texture* m_pColorTarget               = nullptr;
    const cauldron::Texture* m_pHistoryColorTarget        = nullptr;
    const cauldron::Texture* m_pDepthTarget               = nullptr;
    const cauldron::Texture* m_pOutput                    = nullptr;
    const cauldron::Texture* m_pMotionVectors             = nullptr;
    const cauldron::Texture* m_pNormal                    = nullptr;
    const cauldron::Texture* m_pAlbedo                    = nullptr;
    const cauldron::Texture* m_pAoRoughnessMetallic       = nullptr;
    const cauldron::Texture* m_pPrefilteredEnvironmentMap = nullptr;
    const cauldron::Texture* m_pIrradianceEnvironmentMap  = nullptr;
    const cauldron::Texture* m_pBRDFTexture               = nullptr;
    const cauldron::Texture* m_pDepthHierarchy            = nullptr;
    const cauldron::Texture* m_pExtractedRoughness        = nullptr;
    const cauldron::Texture* m_pRadiance0                 = nullptr;
    const cauldron::Texture* m_pRadiance1                 = nullptr;
    const cauldron::Texture* m_pVariance0                 = nullptr;
    const cauldron::Texture* m_pVariance1                 = nullptr;
    const cauldron::Texture* m_pHitCounter0               = nullptr;
    const cauldron::Texture* m_pHitCounter1               = nullptr;
    const cauldron::Texture* m_pBlueNoiseTexture          = nullptr;
    const cauldron::Texture* m_pDebugImage                = nullptr;

    const cauldron::Texture* m_pRadianceA   = nullptr;
    const cauldron::Texture* m_pRadianceB   = nullptr;
    const cauldron::Texture* m_pVarianceA   = nullptr;
    const cauldron::Texture* m_pVarianceB   = nullptr;
    const cauldron::Texture* m_pHitCounterA = nullptr;
    const cauldron::Texture* m_pHitCounterB = nullptr;

    const cauldron::Buffer* m_pRayList                      = nullptr;
    const cauldron::Buffer* m_pHWRayList                    = nullptr;
    const cauldron::Buffer* m_pDenoiserTileList             = nullptr;
    const cauldron::Buffer* m_pRayCounter                   = nullptr;
    const cauldron::Buffer* m_pIntersectionPassIndirectArgs = nullptr;
    const cauldron::Buffer* m_pRayGBufferList               = nullptr;
    const cauldron::Buffer* m_pSobol                        = nullptr;
    const cauldron::Buffer* m_pScramblingTile               = nullptr;
    const cauldron::Buffer* m_pRankingTile                  = nullptr;

    cauldron::SamplerDesc m_LinearSamplerDesc;
    cauldron::SamplerDesc m_WrapLinearSamplerDesc;
    cauldron::SamplerDesc m_EnvironmentSamplerDesc;
    cauldron::SamplerDesc m_ComparisonSampler;
    cauldron::SamplerDesc m_SpecularSampler;
    cauldron::SamplerDesc m_DiffuseSampler;

    const cauldron::RasterView* m_pColorRasterView          = nullptr;
    cauldron::RootSignature*    m_pApplyReflectionsRS       = nullptr;
    cauldron::PipelineObject*   m_pApplyReflectionsPipeline = nullptr;
    cauldron::ParameterSet*     m_pParamSet                 = nullptr;

    struct RTInfoTables
    {
        struct BoundTexture
        {
            const cauldron::Texture* pTexture = nullptr;
            uint32_t                 count    = 1;
        };

        std::vector<const cauldron::Buffer*> m_VertexBuffers;
        std::vector<const cauldron::Buffer*> m_IndexBuffers;
        std::vector<BoundTexture>            m_Textures;
        std::vector<cauldron::Sampler*>      m_Samplers;

        std::vector<Material_Info>       m_cpuMaterialBuffer;
        std::vector<Instance_Info>       m_cpuInstanceBuffer;
        std::vector<Vectormath::Matrix4> m_cpuInstanceTransformBuffer;
        std::vector<Surface_Info>        m_cpuSurfaceBuffer;
        std::vector<uint32_t>            m_cpuSurfaceIDsBuffer;

        const cauldron::Buffer* m_pMaterialBuffer   = NULL;  // material_id -> Material buffer
        const cauldron::Buffer* m_pSurfaceBuffer    = NULL;  // surface_id -> Surface_Info buffer
        const cauldron::Buffer* m_pSurfaceIDsBuffer = NULL;  // flat array of uint32_t
        const cauldron::Buffer* m_pInstanceBuffer   = NULL;  // instance_id -> Instance_Info buffer
    } m_RTInfoTables;

    std::mutex m_CriticalSection;

    cauldron::RootSignature*  m_pPrepareBlueNoiseRootSignature = nullptr;
    cauldron::PipelineObject* m_pPrepareBlueNoisePipelineObj   = nullptr;
    cauldron::ParameterSet*   m_pPrepareBlueNoiseParameters    = nullptr;

    cauldron::RootSignature*  m_pPrimaryRTRootSignature = nullptr;
    cauldron::PipelineObject* m_pPrimaryRTPipelineObj   = nullptr;
    cauldron::ParameterSet*   m_pPrimaryRTParameters    = nullptr;

    cauldron::RootSignature*    m_pHybridDeferredRootSignature    = nullptr;
    cauldron::PipelineObject*   m_pHybridDeferredPipelineObj      = nullptr;
    cauldron::ParameterSet*     m_pHybridDeferredParameters       = nullptr;
    cauldron::IndirectWorkload* m_pHybridDeferredIndirectWorkload = nullptr;

    cauldron::RootSignature*    m_pRTDeferredRootSignature    = nullptr;
    cauldron::PipelineObject*   m_pRTDeferredPipelineObj      = nullptr;
    cauldron::ParameterSet*     m_pRTDeferredParameters       = nullptr;
    cauldron::IndirectWorkload* m_pRTDeferredIndirectWorkload = nullptr;

    cauldron::RootSignature*    m_pDeferredShadeRaysRootSignature    = nullptr;
    cauldron::PipelineObject*   m_pDeferredShadeRaysPipelineObj      = nullptr;
    cauldron::ParameterSet*     m_pDeferredShadeRaysParameters       = nullptr;
    cauldron::IndirectWorkload* m_pDeferredShadeRaysIndirectWorkload = nullptr;

    cauldron::RootSignature*  m_pPrepareIndirectHybridRootSignature = nullptr;
    cauldron::PipelineObject* m_pPrepareIndirectHybridPipelineObj   = nullptr;
    cauldron::ParameterSet*   m_pPrepareIndirectHybridParameters    = nullptr;

    cauldron::RootSignature*  m_pPrepareIndirectHWRootSignature = nullptr;
    cauldron::PipelineObject* m_pPrepareIndirectHWPipelineObj   = nullptr;
    cauldron::ParameterSet*   m_pPrepareIndirectHWParameters    = nullptr;

    cauldron::RootSignature*  m_pCopyDepthRootSignature = nullptr;
    cauldron::PipelineObject* m_pCopyDepthPipelineObj   = nullptr;
    cauldron::ParameterSet*   m_pCopyDepthParameters    = nullptr;

    FrameInfo m_FrameInfoConstants;

    cauldron::UICombo* m_UIDebugOption = nullptr; // weak ptr

/// @}
};
