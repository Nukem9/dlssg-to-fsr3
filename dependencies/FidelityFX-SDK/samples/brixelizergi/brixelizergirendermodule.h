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

#include <vector>
#include <mutex>

#include "render/rendermodule.h"
#include "core/framework.h"
#include "core/uimanager.h"
#include "core/contentmanager.h"
#include "render/renderdefines.h"

#include <FidelityFX/host/ffx_brixelizer.h>
#include <FidelityFX/host/ffx_brixelizergi.h>

#include "shaders/brixelizergiexampletypes.h"

namespace cauldron
{
    class ParameterSet;
    class RootSignature;
};  // namespace cauldron

/// @defgroup FfxBrixelizerSample FidelityFX Brixelizer sample
/// Sample documentation for FidelityFX Brixelizer
///
/// @ingroup SDKEffects

/// @defgroup BrixelizerRM Brixelizer RenderModule
/// BrixelizerRenderModule Reference Documentation
///
/// @ingroup FfxBrixelizerSample
/// @{

// Brixelizer supports a maximum of 24 raw cascades
// In the sample each cascade level we build is created by building a static cascade,
// a dynamic cascade, and then merging those into a merged cascade. Hence we require
// 3 raw cascades per cascade level.
#define NUM_BRIXELIZER_CASCADES (FFX_BRIXELIZER_MAX_CASCADES / 3)
// Brixelizer makes use of a scratch buffer for calculating cascade updates. Hence in
// this sample we allocate a buffer to be used as scratch space. Here we have chosen
// a somewhat arbitrary large size for use as scratch space, in a real application this
// value should be tuned to what is required by Brixelizer.
#define GPU_SCRATCH_BUFFER_SIZE (1 << 30)

class BrixelizerGIRenderModule : public cauldron::RenderModule, public cauldron::ContentListener
{
public:
    /**
     * @brief   Constructor with default behavior.
     */
    BrixelizerGIRenderModule() : RenderModule(L"BrixelizerGIRenderModule"), ContentListener() {}
    virtual ~BrixelizerGIRenderModule();

    /**
     * @brief   Tear down the FFX API Context and release resources.
     */
    void Init(const json& initData) override;

    /**
     * @brief   Initialize FFX API Context, Brixelizer context, create resources and setup UI section.
     */
    void EnableModule(bool enabled) override;

    /**
     * @brief   Submit dynamic instances, dispatch Brixelizer workloads and visualize sparse distance field using the example shader.
     */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * @brief   Window resize callback with default behavior.
     */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

    /**
     * @brief   Create Brixelizer instances for all loaded mesh geometry.
     */
    void OnNewContentLoaded(cauldron::ContentBlock* pContentBlock) override;

    /**
     * @brief   Delete Brixelizer instances for all unloaded mesh geometry.
     */
    void OnContentUnloaded(cauldron::ContentBlock* pContentBlock) override;

private:
    // Enum representing the Brixelizer cascade types.
    enum class CascadeType
    {
        Static = 0,
        Dynamic,
        Merged
    };

    // Enum representing the Debug Visualization pass output types.
    enum class DebugVisOutputType
    {
        Distance = 0,
        UVW,
        Iterations,
        Gradient,
        BrickID,
        CascadeID
    };

    // Enum representing the output modes of the sample.
    enum class OutputMode
    {
        None,
        ExampleShader,
        DebugVisualization,
        DiffuseGI,
        SpecularGI,
        RadianceCache,
        IrradianceCache
    };

    struct BrixelizerInstanceInfo
    {
        const cauldron::Entity*  entity;
        const cauldron::Surface* surface;
        FfxBrixelizerInstanceID  instanceID;
        bool                     isDynamic;
    };

    struct BrixelizerBufferInfo
    {
        uint32_t                index;
        const cauldron::Buffer* buffer;
    };

    std::mutex m_CriticalSection;
    std::mutex m_TextureLoadCallbackMutex;
    uint32_t   m_FrameIndex = 0;

    // FidelityFX Brixelizer information
    FfxBrixelizerContextDescription     m_InitializationParameters                       = {};
    FfxBrixelizerContext                m_BrixelizerContext                              = {};
    FfxBrixelizerBakedUpdateDescription m_BrixelizerBakedUpdateDesc                      = {};
    const cauldron::Texture*            m_pSdfAtlas                                      = nullptr;
    const cauldron::Buffer*             m_pBrickAABBs                                    = nullptr;
    const cauldron::Buffer*             m_pCascadeAABBTrees[FFX_BRIXELIZER_MAX_CASCADES] = {};
    const cauldron::Buffer*             m_pCascadeBrickMaps[FFX_BRIXELIZER_MAX_CASCADES] = {};
    const cauldron::Buffer*             m_pGpuScratchBuffer                              = nullptr;
    std::vector<BrixelizerInstanceInfo> m_Instances                                      = {};
    std::vector<BrixelizerBufferInfo>   m_Buffers                                        = {};

    // FidelityFX Brixelizer GI information
    FfxBrixelizerGIContextDescription   m_GIInitializationParameters = {};
    FfxBrixelizerGIDispatchDescription m_GIDispatchDesc             = {};
    FfxBrixelizerGIContext             m_BrixelizerGIContext        = {};
    const cauldron::Texture*           m_pDiffuseGI                 = nullptr;
    const cauldron::Texture*           m_pSpecularGI                = nullptr;
    const cauldron::Texture*           m_pDebugVisualization        = nullptr;
    const cauldron::Texture*           m_pLitOutputCopy             = nullptr;

    // Config
    float                       m_MeshUnitSize             = 0.2f;
    float                       m_CascadeSizeRatio         = 2.0f;
    OutputMode                  m_OutputMode               = OutputMode::None;
    CascadeType                 m_CascadeType              = CascadeType::Merged;
    DebugVisOutputType          m_DebugVisOutputType       = DebugVisOutputType::Gradient;
    BrixelizerExampleOutputType m_ExampleOutputType        = BRIXELIZER_EXAMPLE_OUTPUT_TYPE_GRADIENT;
    uint32_t                    m_StartCascadeIdx          = 0;
    uint32_t                    m_EndCascadeIdx            = NUM_BRIXELIZER_CASCADES - 1;
    float                       m_TMin                     = 0.0f;
    float                       m_TMax                     = 10000.0f;
    float                       m_SdfSolveEps              = 0.5f;
    bool                        m_SdfCenterFollowCamera    = true;
    float                       m_SdfCenter[3]             = {};
    bool                        m_ShowStaticInstanceAABBs  = false;
    bool                        m_ShowDynamicInstanceAABBs = false;
    bool                        m_ShowCascadeAABBs         = false;
    int32_t                     m_ShowAABBTreeIndex        = -1;
    bool                        m_ShowBrickOutlines        = false;
    float                       m_Alpha                    = 1.0f;
    bool                        m_ResetStats               = false;
    float                       m_RayPushoff               = 0.25f;
    bool                        m_EnableGI                 = true;
    bool                        m_MultiBounce              = true;
    float                       m_DiffuseGIFactor          = 1.5f;
    float                       m_SpecularGIFactor         = 3.0f;
    bool                        m_InitColorHistory         = true;

    // UI elements
    std::vector<cauldron::UIElement*> m_StaticUIElements  = {};
    std::vector<cauldron::UIElement*> m_CommonUIElements  = {};
    std::vector<cauldron::UIElement*> m_DebugUIElements   = {};
    std::vector<cauldron::UIElement*> m_ExampleUIElements = {};

    cauldron::UIElement* m_FreeBricksTextElement        = nullptr;
    cauldron::UIElement* m_StaticBricksTextElement      = nullptr;
    cauldron::UIElement* m_StaticTrianglesTextElement   = nullptr;
    cauldron::UIElement* m_StaticReferencesTextElement  = nullptr;
    cauldron::UIElement* m_DynamicBricksTextElement     = nullptr;
    cauldron::UIElement* m_DynamicTrianglesTextElement  = nullptr;
    cauldron::UIElement* m_DynamicReferencesTextElement = nullptr;

    uint64_t m_MaxStaticTriangles   = 0;
    uint64_t m_MaxStaticReferences  = 0;
    uint64_t m_MaxStaticBricks      = 0;
    uint64_t m_MaxDynamicTriangles  = 0;
    uint64_t m_MaxDynamicReferences = 0;
    uint64_t m_MaxDynamicBricks     = 0;
    
    // Input Resources
    const cauldron::Texture* m_pColorTarget     = nullptr;
    const cauldron::Texture* m_pDiffuseTexture  = nullptr;
    const cauldron::Texture* m_pDepthBuffer     = nullptr;
    const cauldron::Texture* m_pNormalTarget    = nullptr;
    const cauldron::Texture* m_pVelocityBuffer  = nullptr;
    const cauldron::Texture* m_pRoughnessTarget = nullptr;

    // Created Resources
    const cauldron::Texture* m_pHistoryLitOutput         = nullptr;
    const cauldron::Texture* m_pHistoryDepth             = nullptr;
    const cauldron::Texture* m_pHistoryNormals           = nullptr;
    const cauldron::Texture* m_pEnivornmentMap           = nullptr;

    // Noise Textures
    std::vector<const cauldron::Texture*> m_NoiseTextures;

    // Matrices
    Mat4 m_InvView;
    Mat4 m_InvProj;
    Mat4 m_PrevInvView;
    Mat4 m_PrevInvProj;
    Mat4 m_PrevProjection;

    // Example pass resources
    cauldron::RootSignature*  m_pExampleRootSignature = nullptr;
    cauldron::ParameterSet*   m_pExampleParameterSet  = nullptr;
    cauldron::PipelineObject* m_pExamplePipeline      = nullptr;

    // Copy history pass resources
    cauldron::RootSignature*  m_pPassThroughRootSignature  = nullptr;
    cauldron::ParameterSet*   m_pPassThroughParameterSet   = {};
    cauldron::PipelineObject* m_pPassThroughPipeline       = nullptr;

    // Deferred Lighting pass resources
    cauldron::RootSignature*  m_pDeferredLightingRootSignature = nullptr;
    cauldron::ParameterSet*   m_pDeferredLightingParameterSet  = nullptr;
    cauldron::PipelineObject* m_pDeferredLightingPipeline      = nullptr;

    void CreateBrixelizerContext();
    void DeleteBrixelizerContext();
    void RecreateBrixelizerContext();
    void UpdateBrixelizerContext(cauldron::CommandList* pCmdList);
    void SetupDebugVisualization(FfxBrixelizerUpdateDescription& updateDesc, FfxBrixelizerDebugVisualizationDescription& debugVisDesc);
    void DispatchExampleShader(cauldron::CommandList* pCmdList);
    void FlushInstances(bool flushStaticInstances);
    void DeleteInstances();

    void CreateBrixelizerGIContext();
    void DeleteBrixelizerGIContext();
    void RecreateBrixelizerGIContext();
    void UpdateBrixelizerGIContext(cauldron::CommandList* pCmdList);
   
    void CopyHistoryResource(cauldron::CommandList* pCmdList, const cauldron::Texture* pInput, const cauldron::Texture* pOutput, std::wstring name);
    void CopyHistoryResources(cauldron::CommandList* pCmdList);
    void DeferredLighting(cauldron::CommandList* pCmdList, bool enableGI);
    void VisualizeGIDebug(cauldron::CommandList* pCmdList);
    void TextureLoadComplete(const std::vector<const cauldron::Texture*>& textureList, void*);

    void InitUI(cauldron::UISection* uiSection);
    void UpdateUIElementVisibility();
    void UpdateConfig();

    uint32_t GetBufferIndex(const cauldron::Buffer *buffer);
};
