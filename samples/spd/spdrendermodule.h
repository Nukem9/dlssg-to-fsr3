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

#include <FidelityFX/host/ffx_spd.h>

#include <vector>

namespace cauldron
{
    class RootSignature;
    class RasterView;
    class ParameterSet;
    class PipelineObject;
    class Texture;
    class Buffer;
} // namespace cauldron

/// @defgroup FfxSpdSample FidelityFX SPD sample
/// Sample documentation for FidelityFX Single Pass Downsampler
///
/// @ingroup SDKEffects

/// @defgroup SpdRM SPD RenderModule
/// SPDRenderModule Reference Documentation
///
/// @ingroup FfxSpdSample
/// @{

enum class DownsampleTechnique : uint32_t
{
    PSDownsample = 0,
    CSDownsample,
    SPDDownsample,

    Count
};

/**
 * @class SPDRenderModule
 * SPDRenderModule handles a number of tasks related to SPD.
 *
 * SPDRenderModule takes care of:
 *      - creating UI section that enable users to switch between options of SPD
 *      - performs downsampling of all faces of a cubemap texture using FidelityFX SPD effect component
 */
class SPDRenderModule : public cauldron::RenderModule
{
public:
    /**
     * @brief   Constructor with default behavior.
     */
    SPDRenderModule() : RenderModule(L"SPDRenderModule") {}

    /**
     * @brief   Tear down the FFX API Context and release resources.
     */
    virtual ~SPDRenderModule();

    /**
     * @brief   Initialize FFX API Context, load the downsampling resource, and setup UI section for SPD.
     */
    void Init(const json& initData) override;

    /**
     * @brief   Setup downsample texture and parameters FFX API needs this frame and then call the FFX Dispatch.
     */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * @brief Called by the framework when resolution changes.
     */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

private:

    /**
     * @brief   Callback for texture loading so we can complete parameter binding and mark the module "ready"
     */
    void TextureLoadComplete(const std::vector<const cauldron::Texture*>& textureList, void*);

    void InitFfxContext();
    void DestroyFfxContext();

    /**
     * @brief   Performs traditional raster-based hierarchical downsampling via consecutive pixel shader invocations
     */
    void ExecutePSDownsample(double deltaTime, cauldron::CommandList* pCmdList);
    
    /**
     * @brief   Performs traditional compute-based hierarchical downsampling via consecutive compute shader invocations
     */
    void ExecuteCSDownsample(double deltaTime, cauldron::CommandList* pCmdList);

    /**
     * @brief   Performs FidelityFX SPD-based downsampling via a single dispatch call
     */
    void ExecuteSPDDownsample(double deltaTime, cauldron::CommandList* pCmdList);

    /**
     * @brief   Renders the mip-map quads to the scene for verification
     */
    void ExecuteVerificationQuads(double deltaTime, cauldron::CommandList* pCmdList);

    /**
     * @brief   Destroys and/or recreates FidelityFX SPD context when feature changes are made at the UI level
     */
    void UpdateSPDContext(bool enabled);

    /**
     * @brief   Constructs all GPU resources (signatures, pipelines, parameter bindings) needed for comparison downsampling.
     */
    void InitTraditionalDSPipeline(bool computeDownsample);

    /**
     * @brief   Constructs all GPU resources (signatures, pipelines, parameter bindings) needed to output SPD verification mips.
     */
    void InitVerificationPipeline();

    struct PipelineSet
    {
        cauldron::RootSignature*                pRootSignature  = nullptr;
        cauldron::PipelineObject*               pPipelineObj    = nullptr;
        std::vector<cauldron::ParameterSet*>    ParameterSets   = {};
    };

    PipelineSet m_PipelineSets[static_cast<uint32_t>(DownsampleTechnique::Count)];
    PipelineSet m_VerificationSet = {};

    int32_t     m_DownsamplerUsed = 2;
    int32_t     m_SPDLoadLinear = 0;
    int32_t     m_SPDWaveInterop = 0;
    int32_t     m_SPDMath = 0;
    uint32_t    m_ViewSlice = 0;

    // For pixel shader-based down sample
    std::vector<const cauldron::RasterView*>   m_RasterViews = {};

    // Shared SPD stuff
    const cauldron::Texture*        m_pCubeTexture  = nullptr;
    cauldron::SamplerDesc           m_LinearSamplerDesc;
    const cauldron::Texture*        m_pColorTarget = nullptr; // Render target to render downsample results to on-screen
    const cauldron::RasterView*     m_pColorRasterView = nullptr;

    // FidelityFX SPD information
    FfxSpdContextDescription m_InitializationParameters = {};
    FfxSpdContext            m_Context;
    bool                     m_ContextCreated = false;
};

/// @}
