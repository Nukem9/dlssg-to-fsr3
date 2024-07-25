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
#include "misc/math.h"

#include <FidelityFX/host/ffx_classifier.h>
#include <FidelityFX/host/ffx_denoiser.h>

#include <vector>

namespace cauldron
{
    class RootSignature;
    class RasterView;
    class ParameterSet;
    class PipelineObject;
    class Texture;
    class Buffer;
    class IndirectWorkload;
}  // namespace cauldron

class HybridShadowsRenderModule : public cauldron::RenderModule
{
public:
    HybridShadowsRenderModule() : RenderModule(L"HybridShadowsRenderModule") {}
    virtual ~HybridShadowsRenderModule();

    /// Initialize FFX API context and set up UI.
    /// @param initData Not used.
    void Init(const json& initData) override;

    /// Dispatch the Classifier using FFX API
    /// @param deltaTime Not used.
    /// @param pCmdList Command list on which to dispatch.
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * @brief Runs a debug tile visualization pass over the geometry
     */
    void TileDebugCallback(double deltaTime, cauldron::CommandList* pCmdList);

    /// Recreate the FFX API context to resize internal resources. Called by the framework when the resolution changes.
    /// @param resInfo New resolution info.
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

private:
    void InitEffect();
    void DestroyEffect();

    /// Destroy or create the FFX API context using the currently set parameters.
    void UpdateEffectContext(bool enabled);

    // Renders the RayTracing Mask to screen
    void ResolveRayTracingToShadowTexture(cauldron::CommandList* pCmdList);
    // Creates the pipeline objects related to ray tracing
    void CreateRayTracingPipelines();
    // Creates the resources (buffers, textures) needed for this sample
    void CreateResources();
    // Creates the pipelines used to visualize shadow tiles
    void CreateDebugTilesPipeline();

    // Execute the ray tracing shadow test
    void RunRayTracingShadowDispatch(const FfxClassifierShadowDispatchDescription& shadowClassifierDispatchParams,
      const float            sunSize,
      cauldron::CommandList* pCmdList);
    // Execute the FidelityFx ShadowDenoiser
    void RunFfxShadowDenoiser(cauldron::CommandList* pCmdList);

    // Effect resources
    const cauldron::Texture* m_pDepthTarget       = nullptr;
    const cauldron::Texture* m_pCopyDepth         = nullptr;
    const cauldron::Texture* m_pNormalTarget      = nullptr;
    const cauldron::Buffer*  m_pWorkQueue         = nullptr;
    const cauldron::Buffer*  m_pWorkQueueCount    = nullptr;
    const cauldron::Texture* m_pRayHitTexture     = nullptr;
    const cauldron::Texture* m_pShadowMaskOutput  = nullptr;
    // DebugTiles
    const cauldron::Texture* m_pColorOutput       = nullptr;
    // ShadowDenoiser
    const cauldron::Texture* m_pMotionVectors     = nullptr;

    enum class ClassificationMode : uint32_t
    {
        ClassifyByNormals = 0,
        ClassifyByCascades
    };
    ClassificationMode m_ClassificationMode = ClassificationMode::ClassifyByCascades;

    // UI controls
    uint32_t m_TileCutoff          = 0;
    float    m_blockerOffset       = 0.002f;
    int      m_DebugMode           = 0;
    bool     m_bRejectLitPixels    = true;
    bool     m_bUseCascadesForRayT = true;
    bool     m_bRunHybridShadows   = true;
    float    m_sunSolidAngle       = 0.25f;

    // FidelityFX Classifier information
    FfxInterface                    m_SDKInterface = {0};
    FfxClassifierContextDescription m_ClassifierCtxDesc = {0};
    FfxClassifierContext            m_ClassifierContext = {0};

    // FidelityFX Denoiser information
    bool                          m_bUseDenoiser    = true;
    FfxDenoiserContextDescription m_DenoiserCtxDesc = {0};
    FfxDenoiserContext            m_DenoiserContext = {0};


    // RayTracing
    struct RTConstantBuffer
    {
        alignas(16) Vec4 textureSize;
        alignas(16) Vec3 lightDir;
        float pad[1];
        Vec4  pixelThickness_bUseCascadesForRayT_noisePhase_sunSize;

        alignas(16) Mat4 viewToWorld;
    };
    RTConstantBuffer            m_RTConstantBuffer;
    cauldron::IndirectWorkload* m_pIndirectWorkLoad      = nullptr;
    const cauldron::Texture*     m_pBlueNoise            = nullptr;
    cauldron::RootSignature*  m_pRayTracingRootSignature = nullptr;
    cauldron::PipelineObject* m_pRayTracingPipelineObj   = nullptr;
    cauldron::ParameterSet*   m_pRayTracingParameters    = nullptr;

    // Resolve Ray Tracing
    cauldron::RootSignature*  m_pResolveRayTracingRootSignature = nullptr;
    cauldron::PipelineObject* m_pResolveRayTracingPipelineObj   = nullptr;
    cauldron::ParameterSet*   m_pResolveRayTracingParameters    = nullptr;

    // Debug Tiles
    struct DebugTilesConstantBuffer
    {
        alignas(16) int32_t debugMode;
    };
    cauldron::RootSignature*  m_pDebugTilesRootSignature = nullptr;
    cauldron::PipelineObject* m_pDebugTilesPipelineObj   = nullptr;
    cauldron::ParameterSet*   m_pDebugTilesParameters    = nullptr;

    // Copy depth pass
    void CreateCopyDepthPipeline();
    void RunCopyDepth(cauldron::CommandList* pCmdList);
    cauldron::RootSignature*  m_pCopyDepthRootSignature = nullptr;
    cauldron::PipelineObject* m_pCopyDepthPipelineObj   = nullptr;
    cauldron::ParameterSet*   m_pCopyDepthParameters    = nullptr;

    // Debug RayTracing
    void CreateDebugRayTracingPipeline();
    void RunDebugRayTracingPipeline(cauldron::CommandList* pCmdList);
    cauldron::RootSignature*  m_pDebugRayTracingRootSignature = nullptr;
    cauldron::PipelineObject* m_pDebugRayTracingPipelineObj   = nullptr;
    cauldron::ParameterSet*   m_pDebugRayTracingParameters    = nullptr;
};
