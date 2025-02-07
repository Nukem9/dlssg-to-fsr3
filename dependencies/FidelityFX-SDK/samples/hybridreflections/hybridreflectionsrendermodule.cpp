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

#include "shaders/surfacerendercommon.h"

#include "hybridreflectionsrendermodule.h"

#include "core/backend_interface.h"
#include "core/components/meshcomponent.h"
#include "core/framework.h"
#include "core/scene.h"
#include "render/dynamicresourcepool.h"
#include "render/dynamicbufferpool.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/indirectworkload.h"
#include "render/rasterview.h"
#include "render/dynamicresourcepool.h"
#include "render/profiler.h"
#include "shaders/lightingcommon.h"

namespace samplercpp
{
#include "../thirdparty/samplercpp/samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_256spp.cpp"
}

using namespace cauldron;

#define FFX_HYBRID_REFLECTIONS_CONTEXT_COUNT (FFX_CLASSIFIER_CONTEXT_COUNT + FFX_DENOISER_CONTEXT_COUNT + FFX_SPD_CONTEXT_COUNT)

static void setFlag(uint32_t& flags, uint32_t bit, bool enable)
{
    flags &= ~bit;
    flags |= (enable ? bit : 0);
};

static std::vector<const char*> BuildDebugOptions(bool enable)
{
    std::vector<const char*> debugOptions;

    if (enable)
        debugOptions.push_back("Visualize Hit Counter");
    debugOptions.push_back("Show Reflection Target");
    debugOptions.push_back("Visualize Primary Rays");

    return debugOptions;
}

void HybridReflectionsRenderModule::Init(const json& initData)
{
    CauldronAssert(ASSERT_CRITICAL, GetFramework()->GetConfig()->RT_1_1, L"Error: Hybrid Reflections requires RT1.1");
    CauldronAssert(ASSERT_CRITICAL, GetFramework()->GetConfig()->MinShaderModel >= ShaderModel::SM6_5, L"Error: Hybrid Reflections requires SM6_5 or greater");
    CreateResources();

    m_RTInfoTables.m_Textures.reserve(MAX_TEXTURES_COUNT);
    m_RTInfoTables.m_Samplers.reserve(MAX_SAMPLERS_COUNT);
    m_RTInfoTables.m_VertexBuffers.reserve(MAX_BUFFER_COUNT);
    m_RTInfoTables.m_IndexBuffers.reserve(MAX_BUFFER_COUNT);

    ExecuteCallback callbackCopyColorBuffer      = [this](double deltaTime, CommandList* pCmdList) { this->CopyColorBufferCallback(deltaTime, pCmdList); };
    ExecutionTuple  callbackCopyColorBufferTuple = std::make_pair(L"HybridReflectionsRenderModule::CopyColorBufferCallback", std::make_pair(this, callbackCopyColorBuffer));
    GetFramework()->RegisterExecutionCallback(L"ToneMappingRenderModule", true, callbackCopyColorBufferTuple);

    // Register a pre-lighting callback to set the specular IBL factor to 0 for the lighting pass.
    // We're doing this to avoid applying the IBL specular reflections twice, once in the lighting pass and once in the HSR pass.
    ExecuteCallback callbackPreLighting = [this](double deltaTime, CommandList* pCmdList) {
        m_SceneSpecularIBLFactor = GetScene()->GetSpecularIBLFactor();
        if (m_ApplyScreenSpaceReflections)
        {
            GetScene()->SetSpecularIBLFactor(0.0f);
        }
    };

    ExecutionTuple callbackPreLightingTuple = std::make_pair(L"HybridReflectionsRenderModule::PreLightingCallback", std::make_pair(this, callbackPreLighting));
    GetFramework()->RegisterExecutionCallback(L"LightingRenderModule", true, callbackPreLightingTuple);

    // Register a post-lighting callback to reset the IBL factor to what it previously was.
    ExecuteCallback callbackPostLighting = [this](double deltaTime, CommandList* pCmdList) { GetScene()->SetSpecularIBLFactor(m_SceneSpecularIBLFactor); };
    ExecutionTuple  callbackPostLightingTuple =
        std::make_pair(L"HybridReflectionsRenderModule::PostLightingCallback", std::make_pair(this, callbackPostLighting));
    GetFramework()->RegisterExecutionCallback(L"LightingRenderModule", false, callbackPostLightingTuple);

    //////////////////////////////////////////////////////////////////////////
    // Final pass resources to apply reflections

    m_LinearSamplerDesc.Filter        = FilterFunc::MinMagLinearMipPoint;
    m_LinearSamplerDesc.MaxAnisotropy = 1u;

    m_WrapLinearSamplerDesc.AddressU = AddressMode::Wrap;
    m_WrapLinearSamplerDesc.AddressV = AddressMode::Wrap;
    m_WrapLinearSamplerDesc.AddressW = AddressMode::Wrap;

    m_EnvironmentSamplerDesc.AddressW = AddressMode::Wrap;

    m_ComparisonSampler.Comparison    = GetConfig()->InvertedDepth ? ComparisonFunc::GreaterEqual : ComparisonFunc::LessEqual;
    m_ComparisonSampler.Filter        = FilterFunc::ComparisonMinMagLinearMipPoint;
    m_ComparisonSampler.MaxAnisotropy = 1;

    m_SpecularSampler.AddressW      = AddressMode::Wrap;
    m_SpecularSampler.Filter        = FilterFunc::MinMagMipLinear;
    m_SpecularSampler.MaxAnisotropy = 1;

    m_DiffuseSampler.Filter        = FilterFunc::MinMagMipPoint;
    m_DiffuseSampler.AddressW      = AddressMode::Wrap;
    m_DiffuseSampler.Filter        = FilterFunc::MinMagMipPoint;
    m_DiffuseSampler.MaxAnisotropy = 1;

    InitApplyReflections(initData);

    BuildUI();

    // Initialize mask
    m_Mask = HSR_FLAGS_USE_HIT_COUNTER | HSR_FLAGS_APPLY_REFLECTIONS | HSR_FLAGS_USE_RAY_TRACING |
             // HSR_FLAGS_VISUALIZE_HIT_COUNTER |
             HSR_FLAGS_USE_SCREEN_SPACE | 0;

    // DXR Tier support
    if (!GetFramework()->GetDevice()->FeatureSupported(DeviceFeature::RT_1_1))
    {
        m_Mask &= ~HSR_FLAGS_USE_RAY_TRACING;
    }

    CreateFfxContexts();

    GetFramework()->ConfigureRuntimeShaderRecompiler(
        [this](void) {
            DestroyFfxContexts();
        },
        [this](void) {
            CreateFfxContexts();
        });

    InitPrepareBlueNoise(initData);
    InitPrimaryRayTracing(initData);
    InitHybridDeferred(initData);
    InitRTDeferred(initData);
    InitDeferredShadeRays(initData);
    InitPrepareIndirectHybrid(initData);
    InitPrepareIndirectHW(initData);
    InitCopyDepth(initData);

    // Register for content change updates
    GetContentManager()->AddContentListener(this);

    // That's all we need for now
    SetModuleReady(true);
}

HybridReflectionsRenderModule::~HybridReflectionsRenderModule()
{
    // Protection
    if (ModuleEnabled())
    {
        EnableModule(false);

        DestroyFfxContexts();

        delete m_pParamSet;
        delete m_pApplyReflectionsPipeline;
        delete m_pApplyReflectionsRS;
        m_pColorRasterView = nullptr;

        delete m_pPrepareBlueNoiseParameters;
        delete m_pPrepareBlueNoisePipelineObj;
        delete m_pPrepareBlueNoiseRootSignature;

        delete m_pPrimaryRTParameters;
        delete m_pPrimaryRTPipelineObj;
        delete m_pPrimaryRTRootSignature;

        delete m_pHybridDeferredParameters;
        delete m_pHybridDeferredPipelineObj;
        delete m_pHybridDeferredRootSignature;
        delete m_pHybridDeferredIndirectWorkload;

        delete m_pRTDeferredParameters;
        delete m_pRTDeferredPipelineObj;
        delete m_pRTDeferredRootSignature;
        delete m_pRTDeferredIndirectWorkload;

        delete m_pDeferredShadeRaysParameters;
        delete m_pDeferredShadeRaysPipelineObj;
        delete m_pDeferredShadeRaysRootSignature;
        delete m_pDeferredShadeRaysIndirectWorkload;

        delete m_pPrepareIndirectHybridParameters;
        delete m_pPrepareIndirectHybridPipelineObj;
        delete m_pPrepareIndirectHybridRootSignature;

        delete m_pPrepareIndirectHWParameters;
        delete m_pPrepareIndirectHWPipelineObj;
        delete m_pPrepareIndirectHWRootSignature;

        delete m_pCopyDepthParameters;
        delete m_pCopyDepthPipelineObj;
        delete m_pCopyDepthRootSignature;
    }
}

void HybridReflectionsRenderModule::DestroyFfxContexts()
{
    // Flush anything out of the pipes before destroying the context
    GetDevice()->FlushAllCommandQueues();

    ffxClassifierContextDestroy(&m_ClassifierContext);
    ffxDenoiserContextDestroy(&m_DenoiserContext);
    ffxSpdContextDestroy(&m_SpdContext);

    // Destroy the FidelityFX interface memory
    if (m_BackendInterface.scratchBuffer)
    {
        free(m_BackendInterface.scratchBuffer);
        m_BackendInterface.scratchBuffer = nullptr;
    }
}

void HybridReflectionsRenderModule::CreateFfxContexts()
{
    // Initialize the FFX backend
    const size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(FFX_HYBRID_REFLECTIONS_CONTEXT_COUNT);
    void*        scratchBuffer     = calloc(scratchBufferSize, 1u);
    FfxErrorCode errorCode         = SDKWrapper::ffxGetInterface(&m_BackendInterface, GetDevice(), scratchBuffer, scratchBufferSize, FFX_HYBRID_REFLECTIONS_CONTEXT_COUNT);
    CAULDRON_ASSERT(errorCode == FFX_OK);
    CauldronAssert(ASSERT_CRITICAL, m_BackendInterface.fpGetSDKVersion(&m_BackendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
        L"FidelityFX HybridReflections 2.1 sample requires linking with a 1.1.2 version SDK backend");
    CauldronAssert(ASSERT_CRITICAL, ffxClassifierGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 3, 0),
                       L"FidelityFX HybridReflections 2.1 sample requires linking with a 1.3 version FidelityFX Classifier library");
    CauldronAssert(ASSERT_CRITICAL, ffxDenoiserGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 3, 0),
                       L"FidelityFX HybridReflections 2.1 sample requires linking with a 1.3 version FidelityFX Denoiser library");
                       
    m_BackendInterface.fpRegisterConstantBufferAllocator(&m_BackendInterface, SDKWrapper::ffxAllocateConstantBuffer);

    // Init context
    CreateBackendContext();
}

void HybridReflectionsRenderModule::CreateBackendContext()
{
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

    m_DenoiserInitializationParameters.flags                      = FfxDenoiserInitializationFlagBits::FFX_DENOISER_REFLECTIONS;
    m_DenoiserInitializationParameters.windowSize.width           = resInfo.RenderWidth;
    m_DenoiserInitializationParameters.windowSize.height          = resInfo.RenderHeight;
    m_DenoiserInitializationParameters.normalsHistoryBufferFormat = SDKWrapper::GetFfxSurfaceFormat(m_pNormal->GetFormat());
    m_DenoiserInitializationParameters.backendInterface           = m_BackendInterface;
    CAULDRON_ASSERT(ffxDenoiserContextCreate(&m_DenoiserContext, &m_DenoiserInitializationParameters) == FFX_OK);

    m_ClassifierInitializationParameters.flags = FfxClassifierInitializationFlagBits::FFX_CLASSIFIER_REFLECTION;
    m_ClassifierInitializationParameters.flags |= GetConfig()->InvertedDepth ? FFX_CLASSIFIER_ENABLE_DEPTH_INVERTED : 0;
    m_ClassifierInitializationParameters.resolution.width  = resInfo.RenderWidth;
    m_ClassifierInitializationParameters.resolution.height = resInfo.RenderHeight;
    m_ClassifierInitializationParameters.backendInterface  = m_BackendInterface;
    CAULDRON_ASSERT(ffxClassifierContextCreate(&m_ClassifierContext, &m_ClassifierInitializationParameters) == FFX_OK);

    m_SpdInitializationParameters.flags = 0;
    m_SpdInitializationParameters.flags |= FFX_SPD_WAVE_INTEROP_WAVE_OPS;
    m_SpdInitializationParameters.downsampleFilter = GetConfig()->InvertedDepth ? FFX_SPD_DOWNSAMPLE_FILTER_MAX : FFX_SPD_DOWNSAMPLE_FILTER_MIN;
    m_SpdInitializationParameters.backendInterface = m_BackendInterface;
    CAULDRON_ASSERT(ffxSpdContextCreate(&m_SpdContext, &m_SpdInitializationParameters) == FFX_OK);
}

void HybridReflectionsRenderModule::ResetBackendContext()
{
    // Destroy the HSR context
    CAULDRON_ASSERT(ffxDenoiserContextDestroy(&m_DenoiserContext) == FFX_OK);
    CAULDRON_ASSERT(ffxClassifierContextDestroy(&m_ClassifierContext) == FFX_OK);
    CAULDRON_ASSERT(ffxSpdContextDestroy(&m_SpdContext) == FFX_OK);

    // Re-create the HSR context
    CreateBackendContext();
}

void HybridReflectionsRenderModule::CopyColorBufferCallback(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"HSR_CopyColor");

    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(
        m_pColorTarget->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::CopySource));
    barriers.push_back(Barrier::Transition(
        m_pHistoryColorTarget->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::CopyDest));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    // Copy the color render target before we apply translucency
    TextureCopyDesc copyColor = TextureCopyDesc(m_pColorTarget->GetResource(), m_pHistoryColorTarget->GetResource());
    CopyTextureRegion(pCmdList, &copyColor);

    barriers.clear();
    barriers.push_back(Barrier::Transition(
        m_pColorTarget->GetResource(), ResourceState::CopySource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(
        m_pHistoryColorTarget->GetResource(), ResourceState::CopyDest, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void HybridReflectionsRenderModule::ShowDebugTarget()
{
    if (m_ShowDebugTarget)
    {
        m_Mask |= HSR_FLAGS_SHOW_DEBUG_TARGET;

        std::vector<int> flagList;

        if (m_EnableHybridReflection) flagList.push_back(HSR_FLAGS_VISUALIZE_HIT_COUNTER);
        flagList.push_back(HSR_FLAGS_SHOW_REFLECTION_TARGET);
        flagList.push_back(HSR_FLAGS_VISUALIZE_PRIMARY_RAYS);

        m_Mask |= flagList[m_DebugOption];
    }
    else
    {
        m_Mask &= ~HSR_FLAGS_SHOW_DEBUG_TARGET;

        std::vector<int> flagList;

        if (m_EnableHybridReflection) flagList.push_back(HSR_FLAGS_VISUALIZE_HIT_COUNTER);
        flagList.push_back(HSR_FLAGS_SHOW_REFLECTION_TARGET);
        flagList.push_back(HSR_FLAGS_VISUALIZE_PRIMARY_RAYS);

        for (auto flag : flagList)
        {
            m_Mask &= ~flag;
        }
    }
}
void HybridReflectionsRenderModule::SelectDebugOption()
{
    std::vector<int> flagList;

    if (m_EnableHybridReflection) flagList.push_back(HSR_FLAGS_VISUALIZE_HIT_COUNTER);
    flagList.push_back(HSR_FLAGS_SHOW_REFLECTION_TARGET);
    flagList.push_back(HSR_FLAGS_VISUALIZE_PRIMARY_RAYS);

    for (auto flag : flagList)
    {
        m_Mask &= ~flag;
    }

    m_Mask |= flagList[m_DebugOption];
}

void HybridReflectionsRenderModule::ToggleHybridReflection()
{
    m_IsEnableHybridReflectionChanged = true;
}

void HybridReflectionsRenderModule::ToggleHalfResGBuffer()
{
    UpdateReflectionResolution();
    m_HalfResGBufferDisabled = !m_EnableHalfResGBuffer;
}

void HybridReflectionsRenderModule::UpdateReflectionResolution()
{
    int width  = GetFramework()->GetResolutionInfo().RenderWidth;
    int height = GetFramework()->GetResolutionInfo().RenderHeight;

    m_ReflectionResolutionMultiplier = m_EnableHalfResGBuffer ? 0.5f : 1.0f;
    m_ReflectionWidth                = std::max(128u, uint32_t(width * m_ReflectionResolutionMultiplier));
    m_ReflectionHeight               = std::max(128u, uint32_t(height * m_ReflectionResolutionMultiplier));
}

void HybridReflectionsRenderModule::UpdateUI(double deltaTime)
{
    if (m_IsEnableHybridReflectionChanged)
    {
        m_UIDebugOption->SetOption(BuildDebugOptions(m_EnableHybridReflection));
        
        // Keep current selection
        if (m_EnableHybridReflection)
        {
            // Hit counter inserted in front
            m_DebugOption += 1;
        }
        else if (m_DebugOption > 0)
        {
            // Hit counter removed from front
            m_DebugOption -= 1;
        }

        if (m_Mask & HSR_FLAGS_SHOW_DEBUG_TARGET)
            SelectDebugOption();

        m_IsEnableHybridReflectionChanged = false;
    }
}

void HybridReflectionsRenderModule::CreateResources()
{
    // Fetch needed resources
    m_pColorTarget = GetFramework()->GetColorTargetForCallback(GetName());
    // Needed to apply reflections
    m_pColorRasterView = GetRasterViewAllocator()->RequestRasterView(m_pColorTarget, ViewDimension::Texture2D);

    m_pDepthTarget = GetFramework()->GetRenderTexture(L"DepthTarget");
    m_pOutput      = GetFramework()->GetRenderTexture(L"HybridReflectionOutput");

    // Assumed resources, need to check they are there
    m_pMotionVectors             = GetFramework()->GetRenderTexture(L"GBufferMotionVectorRT");
    m_pNormal                    = GetFramework()->GetRenderTexture(L"GBufferNormalRT");
    m_pAlbedo                    = GetFramework()->GetRenderTexture(L"GBufferAlbedoRT");
    m_pAoRoughnessMetallic       = GetFramework()->GetRenderTexture(L"GBufferAoRoughnessMetallicRT");
    CauldronAssert(ASSERT_CRITICAL,
                   m_pMotionVectors && m_pNormal && m_pAoRoughnessMetallic && m_pAlbedo,
                   L"Could not get one of the needed resources for HSR Rendermodule.");

    uint32_t renderWidth  = GetFramework()->GetResolutionInfo().RenderWidth;
    uint32_t renderHeight = GetFramework()->GetResolutionInfo().RenderHeight;

    const uint32_t numPixels              = renderWidth * renderHeight;
    const uint32_t numTiles               = DivideRoundingUp(renderWidth, 8u) * DivideRoundingUp(renderHeight, 8u);
    uint32_t       depthHierarchyMipCount = (uint32_t)ceil(log2(std::max(renderWidth, renderHeight)));

    auto renderSizeFn = [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
        desc.Width  = renderingWidth;
        desc.Height = renderingHeight;
    };

    auto renderSize64Fn = [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
        desc.Width  = DivideRoundingUp(renderingWidth, 8u);
        desc.Height = DivideRoundingUp(renderingHeight, 8u);
    };

    auto renderSizeFnBuffer = [](BufferDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
        desc.Size = renderingWidth * renderingHeight * sizeof(uint32_t);
    };

    auto renderSizeX12FnBuffer = [](BufferDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
        desc.Size = renderingWidth * renderingHeight * 12ull;
    };

    auto renderSize64FnBuffer = [](BufferDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
        desc.Size = DivideRoundingUp(renderingWidth, 8u) * DivideRoundingUp(renderingHeight, 8u) * sizeof(uint32_t);
    };

    TextureDesc desc = TextureDesc::Tex2D(
        L"HSR_DepthHierarchy", ResourceFormat::R32_FLOAT, renderWidth, renderHeight, 1, depthHierarchyMipCount, ResourceFlags::AllowUnorderedAccess);
    m_pDepthHierarchy = GetDynamicResourcePool()->CreateRenderTexture(&desc, renderSizeFn);

    desc = TextureDesc::Tex2D(L"HSR_ExtractedRoughness", ResourceFormat::R8_UNORM, renderWidth, renderHeight, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pExtractedRoughness = GetDynamicResourcePool()->CreateRenderTexture(&desc, renderSizeFn);

    desc                  = TextureDesc::Tex2D(L"HSR_HistoryColor", ResourceFormat::RG11B10_FLOAT, renderWidth, renderHeight, 1, 1, ResourceFlags::None);
    m_pHistoryColorTarget = GetDynamicResourcePool()->CreateRenderTexture(&desc, renderSizeFn);

    desc         = TextureDesc::Tex2D(L"HSR_Radiance0", ResourceFormat::RGBA16_FLOAT, renderWidth, renderHeight, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pRadiance0 = GetDynamicResourcePool()->CreateRenderTexture(&desc, renderSizeFn);

    desc         = TextureDesc::Tex2D(L"HSR_Radiance1", ResourceFormat::RGBA16_FLOAT, renderWidth, renderHeight, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pRadiance1 = GetDynamicResourcePool()->CreateRenderTexture(&desc, renderSizeFn);

    desc         = TextureDesc::Tex2D(L"HSR_Variance0", ResourceFormat::R16_FLOAT, renderWidth, renderHeight, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pVariance0 = GetDynamicResourcePool()->CreateRenderTexture(&desc, renderSizeFn);

    desc         = TextureDesc::Tex2D(L"HSR_Variance1", ResourceFormat::R16_FLOAT, renderWidth, renderHeight, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pVariance1 = GetDynamicResourcePool()->CreateRenderTexture(&desc, renderSizeFn);

    desc           = TextureDesc::Tex2D(L"HSR_HitCounter0",
                              ResourceFormat::R32_UINT,
                              DivideRoundingUp(renderWidth, 8u),
                              DivideRoundingUp(renderHeight, 8u),
                              1,
                              1,
                              ResourceFlags::AllowUnorderedAccess);
    m_pHitCounter0 = GetDynamicResourcePool()->CreateRenderTexture(&desc, renderSize64Fn);

    desc           = TextureDesc::Tex2D(L"HSR_HitCounter1",
                              ResourceFormat::R32_UINT,
                              DivideRoundingUp(renderWidth, 8u),
                              DivideRoundingUp(renderHeight, 8u),
                              1,
                              1,
                              ResourceFlags::AllowUnorderedAccess);
    m_pHitCounter1 = GetDynamicResourcePool()->CreateRenderTexture(&desc, renderSize64Fn);

    desc                = TextureDesc::Tex2D(L"HSR_BlueNoiseTexture", ResourceFormat::RG8_UNORM, 128, 128, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pBlueNoiseTexture = GetDynamicResourcePool()->CreateRenderTexture(&desc);

    desc          = TextureDesc::Tex2D(L"HSR_DebugImage", ResourceFormat::RGBA32_FLOAT, renderWidth, renderHeight, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pDebugImage = GetDynamicResourcePool()->CreateRenderTexture(&desc, renderSizeFn);

    BufferDesc bufferDesc = BufferDesc::Data(L"HSR_RayList", numPixels * sizeof(uint32_t), sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
    m_pRayList            = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, ResourceState::UnorderedAccess, renderSizeFnBuffer);

    bufferDesc   = BufferDesc::Data(L"HSR_HWRayList", numPixels * sizeof(uint32_t), sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
    m_pHWRayList = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, ResourceState::UnorderedAccess, renderSizeFnBuffer);

    bufferDesc          = BufferDesc::Data(L"HSR_DenoiserTileList", numTiles * sizeof(uint32_t), sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
    m_pDenoiserTileList = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, ResourceState::UnorderedAccess, renderSize64FnBuffer);

    bufferDesc    = BufferDesc::Data(L"HSR_RayCounter", 8ull * sizeof(uint32_t), sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
    m_pRayCounter = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, ResourceState::UnorderedAccess);

    bufferDesc                      = BufferDesc::Data(L"HSR_IntersectionPassIndirectArgs",
                                  12ull * sizeof(uint32_t),
                                  sizeof(uint32_t),
                                  0,
                                  static_cast<ResourceFlags>(ResourceFlags::AllowUnorderedAccess | ResourceFlags::AllowIndirect));
    m_pIntersectionPassIndirectArgs = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, ResourceState::IndirectArgument);

    bufferDesc        = BufferDesc::Data(L"HSR_RayGBufferList", numPixels * 12ull, sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
    m_pRayGBufferList = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, ResourceState::UnorderedAccess, renderSizeX12FnBuffer);

    bufferDesc = BufferDesc::Data(L"HSR_Sobol", sizeof(uint32_t) * 256ull * 256ull, sizeof(uint32_t), 0, ResourceFlags::None);
    m_pSobol   = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, ResourceState::CopyDest);
    const_cast<Buffer*>(m_pSobol)->CopyData((void*)samplercpp::sobol_256spp_256d, sizeof(samplercpp::sobol_256spp_256d));

    bufferDesc     = BufferDesc::Data(L"HSR_RankingTile", sizeof(uint32_t) * 128ull * 128ull * 8ull, sizeof(uint32_t), 0, ResourceFlags::None);
    m_pRankingTile = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, ResourceState::CopyDest);
    const_cast<Buffer*>(m_pRankingTile)->CopyData((void*)samplercpp::rankingTile, sizeof(samplercpp::rankingTile));

    bufferDesc        = BufferDesc::Data(L"HSR_ScramblingTile", sizeof(uint32_t) * 128ull * 128ull * 8ull, sizeof(uint32_t), 0, ResourceFlags::None);
    m_pScramblingTile = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, ResourceState::CopyDest);
    const_cast<Buffer*>(m_pScramblingTile)->CopyData((void*)samplercpp::scramblingTile, sizeof(samplercpp::scramblingTile));
}

void HybridReflectionsRenderModule::BuildUI()
{
    UISection* uiSection = GetUIManager()->RegisterUIElements("Hybrid Reflections", UISectionType::Sample);

    std::vector<const char*> debugOptions;

    std::function<void(bool, bool)> showDebugTargetCallback = [this](bool cur, bool old) { ShowDebugTarget(); };
    uiSection->RegisterUIElement<UICheckBox>("Show Debug Target", m_ShowDebugTarget, showDebugTargetCallback);
    std::function<void(int32_t, int32_t)> debugOptionCallback = [this](int32_t cur, int32_t old) { SelectDebugOption(); };
    m_UIDebugOption = uiSection->RegisterUIElement<UICombo>("Visualizer", (int32_t&)m_DebugOption, BuildDebugOptions(m_EnableHybridReflection), m_ShowDebugTarget, debugOptionCallback);

    uiSection->RegisterUIElement<UISlider<float>>("Global Roughness Threshold", m_RoughnessThreshold, 0.f, 1.f);
    uiSection->RegisterUIElement<UISlider<float>>("RT Roughness Threshold", m_RTRoughnessThreshold, 0.f, 1.f);
    uiSection->RegisterUIElement<UICheckBox>("Don't reshade", m_DisableReshading);

    std::function<void(bool,bool)> enableHybridReflectionCallback = [this](bool cur, bool old) { ToggleHybridReflection(); };
    uiSection->RegisterUIElement<UICheckBox>("Enable Hybrid Reflections", m_EnableHybridReflection, enableHybridReflectionCallback);
}

void HybridReflectionsRenderModule::InitApplyReflections(const json& initData)
{
    RootSignatureDesc applyReflectionsSignatureDesc;
    applyReflectionsSignatureDesc.AddConstantBufferView(0, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(1, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(2, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(3, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(4, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(5, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(6, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddStaticSamplers(0, ShaderBindStage::Pixel, 1, &m_LinearSamplerDesc);

    m_pApplyReflectionsRS = RootSignature::CreateRootSignature(L"HSR_ApplyReflections", applyReflectionsSignatureDesc);

    BlendDesc blendDesc                     = {};
    blendDesc.BlendEnabled                  = true;
    blendDesc.SourceBlendColor              = Blend::One;
    blendDesc.ColorOp                       = BlendOp::Add;
    blendDesc.DestBlendColor                = Blend::SrcAlpha;
    blendDesc.SourceBlendAlpha              = Blend::One;
    blendDesc.AlphaOp                       = BlendOp::Add;
    blendDesc.DestBlendAlpha                = Blend::One;
    const std::vector<BlendDesc> blendDescs = {blendDesc};

    DefineList defineList;
    defineList.insert(std::make_pair(L"HSR_DEBUG", std::to_wstring(1)));

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pApplyReflectionsRS);
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"fullscreen.hlsl", L"FullscreenVS", ShaderModel::SM6_0, nullptr));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"apply_reflections.hlsl", L"ps_main", ShaderModel::SM6_0, &defineList));
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
    psoDesc.AddBlendStates(blendDescs, false, false);
    psoDesc.AddRasterFormats(m_pColorTarget->GetFormat());  // Use the first raster set, as we just want the format and they are all the same

    m_pApplyReflectionsPipeline = PipelineObject::CreatePipelineObject(L"HSR_ApplyReflections", psoDesc);

    m_pParamSet = ParameterSet::CreateParameterSet(m_pApplyReflectionsRS);
    m_pParamSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(FrameInfo), 0);
    m_pParamSet->SetTextureSRV(m_pOutput, ViewDimension::Texture2D, 0);
    m_pParamSet->SetTextureSRV(m_pNormal, ViewDimension::Texture2D, 1);
    m_pParamSet->SetTextureSRV(m_pAoRoughnessMetallic, ViewDimension::Texture2D, 2);
    m_pParamSet->SetTextureSRV(m_pDepthTarget, ViewDimension::Texture2D, 4);
    m_pParamSet->SetTextureSRV(m_pDebugImage, ViewDimension::Texture2D, 5);
    m_pParamSet->SetTextureSRV(m_pAlbedo, ViewDimension::Texture2D, 6);
}

void HybridReflectionsRenderModule::InitPrepareBlueNoise(const json& initData)
{
    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(2, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);

    m_pPrepareBlueNoiseRootSignature = RootSignature::CreateRootSignature(L"PrepareBlueNoise_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pPrepareBlueNoiseRootSignature);

    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"prepare_blue_noise.hlsl", L"main", ShaderModel::SM6_5, nullptr));

    m_pPrepareBlueNoisePipelineObj = PipelineObject::CreatePipelineObject(L"PrepareBlueNoise_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pPrepareBlueNoiseParameters = ParameterSet::CreateParameterSet(m_pPrepareBlueNoiseRootSignature);
    m_pPrepareBlueNoiseParameters->SetBufferSRV(m_pSobol, 0);
    m_pPrepareBlueNoiseParameters->SetBufferSRV(m_pScramblingTile, 1);
    m_pPrepareBlueNoiseParameters->SetBufferSRV(m_pRankingTile, 2);
    m_pPrepareBlueNoiseParameters->SetTextureUAV(m_pBlueNoiseTexture, ViewDimension::Texture2D, 0);
    m_pPrepareBlueNoiseParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(FrameInfo), 0);
}

void HybridReflectionsRenderModule::InitPrimaryRayTracing(const json& initData)
{
    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(2, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(3, ShaderBindStage::Compute, 1);

    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddRTAccelerationStructureSet(2, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(3, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(4, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(5, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(6, ShaderBindStage::Compute, 1);

    signatureDesc.AddTextureSRVSet(SHADOW_MAP_BEGIN_SLOT, ShaderBindStage::Compute, MAX_SHADOW_MAP_TEXTURES_COUNT);  // shadow maps

    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 1, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 2, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 3, ShaderBindStage::Compute, 1);

    signatureDesc.AddTextureSRVSet(TEXTURE_BEGIN_SLOT, ShaderBindStage::Compute, MAX_TEXTURES_COUNT);

    signatureDesc.AddBufferSRVSet(INDEX_BUFFER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_BUFFER_COUNT);
    signatureDesc.AddBufferSRVSet(VERTEX_BUFFER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_BUFFER_COUNT);

    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);

    signatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, 1, &m_SpecularSampler);
    signatureDesc.AddStaticSamplers(1, ShaderBindStage::Compute, 1, &m_DiffuseSampler);
    signatureDesc.AddStaticSamplers(2, ShaderBindStage::Compute, 1, &m_SpecularSampler);
    signatureDesc.AddStaticSamplers(3, ShaderBindStage::Compute, 1, &m_ComparisonSampler);

    signatureDesc.AddSamplerSet(SAMPLER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_SAMPLERS_COUNT);

    m_pPrimaryRTRootSignature = RootSignature::CreateRootSignature(L"PrimaryRayTracing_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pPrimaryRTRootSignature);

    // Setup the shaders to build on the pipeline object
    //psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"intersect.hlsl", L"main", ShaderModel::SM6_5, nullptr));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"primary_ray_tracing.hlsl", L"main", ShaderModel::SM6_5, nullptr));

    m_pPrimaryRTPipelineObj = PipelineObject::CreatePipelineObject(L"PrimaryRayTracing_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pPrimaryRTParameters = ParameterSet::CreateParameterSet(m_pPrimaryRTRootSignature);
    m_pPrimaryRTParameters->SetTextureSRV(m_pDepthTarget, ViewDimension::Texture2D, 0);
    m_pPrimaryRTParameters->SetTextureSRV(m_pNormal, ViewDimension::Texture2D, 1);
    m_pPrimaryRTParameters->SetAccelerationStructure(GetScene()->GetASManager()->GetTLAS(), 2);
    m_pPrimaryRTParameters->SetTextureSRV(m_pBlueNoiseTexture, ViewDimension::Texture2D, 6);
    m_pPrimaryRTParameters->SetTextureUAV(m_pDebugImage, ViewDimension::Texture2D, 0);

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
    {
        m_pPrimaryRTParameters->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, SHADOW_MAP_BEGIN_SLOT + i);
    }

    m_pPrimaryRTParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(FrameInfo), 0);
    m_pPrimaryRTParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 1);
    m_pPrimaryRTParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneLightingInformation), 2);
    m_pPrimaryRTParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(LightingCBData), 3);
}

void HybridReflectionsRenderModule::InitHybridDeferred(const json& initData)
{
    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(2, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(3, ShaderBindStage::Compute, 1);

    signatureDesc.AddRTAccelerationStructureSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(2, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(3, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(4, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(5, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(6, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(7, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(8, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(9, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(10, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(11, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(12, ShaderBindStage::Compute, 1);

    signatureDesc.AddTextureSRVSet(SHADOW_MAP_BEGIN_SLOT, ShaderBindStage::Compute, MAX_SHADOW_MAP_TEXTURES_COUNT);  // shadow maps

    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 1, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 2, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 3, ShaderBindStage::Compute, 1);

    signatureDesc.AddTextureSRVSet(TEXTURE_BEGIN_SLOT, ShaderBindStage::Compute, MAX_TEXTURES_COUNT);

    signatureDesc.AddBufferSRVSet(INDEX_BUFFER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_BUFFER_COUNT);
    signatureDesc.AddBufferSRVSet(VERTEX_BUFFER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_BUFFER_COUNT);

    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 2);
    signatureDesc.AddTextureUAVSet(2, ShaderBindStage::Compute, 2);
    signatureDesc.AddTextureUAVSet(4, ShaderBindStage::Compute, 2);
    signatureDesc.AddTextureUAVSet(6, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(7, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(8, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(9, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(10, ShaderBindStage::Compute, 1);

    signatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, 1, &m_LinearSamplerDesc);
    signatureDesc.AddStaticSamplers(1, ShaderBindStage::Compute, 1, &m_WrapLinearSamplerDesc);
    signatureDesc.AddStaticSamplers(2, ShaderBindStage::Compute, 1, &m_SpecularSampler);
    signatureDesc.AddStaticSamplers(3, ShaderBindStage::Compute, 1, &m_DiffuseSampler);
    signatureDesc.AddStaticSamplers(4, ShaderBindStage::Compute, 1, &m_SpecularSampler);
    signatureDesc.AddStaticSamplers(5, ShaderBindStage::Compute, 1, &m_ComparisonSampler);

    signatureDesc.AddSamplerSet(SAMPLER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_SAMPLERS_COUNT);

    m_pHybridDeferredRootSignature = RootSignature::CreateRootSignature(L"HybridDeferred_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pHybridDeferredRootSignature);

    DefineList defineList;
    defineList.insert(std::make_pair(L"USE_SSR", std::to_wstring(1)));
    defineList.insert(std::make_pair(L"HSR_DEBUG", std::to_wstring(1)));

    if (GetConfig()->InvertedDepth)
        defineList.insert(std::make_pair(L"FFX_SSSR_OPTION_INVERTED_DEPTH", std::to_wstring(1)));
    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"intersect.hlsl", L"main", ShaderModel::SM6_5, &defineList));

    m_pHybridDeferredPipelineObj = PipelineObject::CreatePipelineObject(L"HybridDeferred_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pHybridDeferredParameters = ParameterSet::CreateParameterSet(m_pHybridDeferredRootSignature);
    m_pHybridDeferredParameters->SetAccelerationStructure(GetScene()->GetASManager()->GetTLAS(), 0);
    m_pHybridDeferredParameters->SetTextureSRV(m_pMotionVectors, ViewDimension::Texture2D, 4);
    m_pHybridDeferredParameters->SetTextureSRV(m_pNormal, ViewDimension::Texture2D, 5);
    m_pHybridDeferredParameters->SetTextureSRV(m_pDepthTarget, ViewDimension::Texture2D, 6);
    m_pHybridDeferredParameters->SetTextureSRV(m_pAoRoughnessMetallic, ViewDimension::Texture2D, 7);
    m_pHybridDeferredParameters->SetTextureSRV(m_pDepthHierarchy, ViewDimension::Texture2D, 8);
    m_pHybridDeferredParameters->SetTextureSRV(m_pExtractedRoughness, ViewDimension::Texture2D, 9);
    m_pHybridDeferredParameters->SetTextureSRV(m_pHistoryColorTarget, ViewDimension::Texture2D, 10);
    m_pHybridDeferredParameters->SetTextureSRV(m_pBlueNoiseTexture, ViewDimension::Texture2D, 11);
    m_pHybridDeferredParameters->SetTextureSRV(m_pAlbedo, ViewDimension::Texture2D, 12);

    m_pHybridDeferredParameters->SetTextureUAV(m_pHitCounter0, ViewDimension::Texture2D, 0);
    m_pHybridDeferredParameters->SetTextureUAV(m_pHitCounter1, ViewDimension::Texture2D, 1);
    m_pHybridDeferredParameters->SetTextureUAV(m_pRadiance0, ViewDimension::Texture2D, 2);
    m_pHybridDeferredParameters->SetTextureUAV(m_pRadiance1, ViewDimension::Texture2D, 3);
    m_pHybridDeferredParameters->SetTextureUAV(m_pVariance0, ViewDimension::Texture2D, 4);
    m_pHybridDeferredParameters->SetTextureUAV(m_pVariance1, ViewDimension::Texture2D, 5);
    m_pHybridDeferredParameters->SetTextureUAV(m_pDebugImage, ViewDimension::Texture2D, 6);
    m_pHybridDeferredParameters->SetBufferUAV(m_pHWRayList, 7);
    m_pHybridDeferredParameters->SetBufferUAV(m_pRayList, 8);
    m_pHybridDeferredParameters->SetBufferUAV(m_pRayCounter, 9);
    m_pHybridDeferredParameters->SetBufferUAV(m_pRayGBufferList, 10);

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
    {
        m_pPrimaryRTParameters->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, SHADOW_MAP_BEGIN_SLOT + i);
    }

    m_pHybridDeferredParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(FrameInfo), 0);
    m_pHybridDeferredParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 1);
    m_pHybridDeferredParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneLightingInformation), 2);
    m_pHybridDeferredParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(LightingCBData), 3);

    m_pHybridDeferredIndirectWorkload = IndirectWorkload::CreateIndirectWorkload(IndirectCommandType::Dispatch);
}

void HybridReflectionsRenderModule::InitRTDeferred(const json& initData)
{
    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(2, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(3, ShaderBindStage::Compute, 1);

    signatureDesc.AddRTAccelerationStructureSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(2, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(3, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(4, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(5, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(6, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(7, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(8, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(9, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(10, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(11, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(12, ShaderBindStage::Compute, 1);

    signatureDesc.AddTextureSRVSet(SHADOW_MAP_BEGIN_SLOT, ShaderBindStage::Compute, MAX_SHADOW_MAP_TEXTURES_COUNT);  // shadow maps

    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 1, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 2, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 3, ShaderBindStage::Compute, 1);

    signatureDesc.AddTextureSRVSet(TEXTURE_BEGIN_SLOT, ShaderBindStage::Compute, MAX_TEXTURES_COUNT);

    signatureDesc.AddBufferSRVSet(INDEX_BUFFER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_BUFFER_COUNT);
    signatureDesc.AddBufferSRVSet(VERTEX_BUFFER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_BUFFER_COUNT);

    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 2);
    signatureDesc.AddTextureUAVSet(2, ShaderBindStage::Compute, 2);
    signatureDesc.AddTextureUAVSet(4, ShaderBindStage::Compute, 2);
    signatureDesc.AddTextureUAVSet(6, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(7, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(8, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(9, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(10, ShaderBindStage::Compute, 1);

    signatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, 1, &m_LinearSamplerDesc);
    signatureDesc.AddStaticSamplers(1, ShaderBindStage::Compute, 1, &m_WrapLinearSamplerDesc);
    signatureDesc.AddStaticSamplers(2, ShaderBindStage::Compute, 1, &m_SpecularSampler);
    signatureDesc.AddStaticSamplers(3, ShaderBindStage::Compute, 1, &m_DiffuseSampler);
    signatureDesc.AddStaticSamplers(4, ShaderBindStage::Compute, 1, &m_SpecularSampler);
    signatureDesc.AddStaticSamplers(5, ShaderBindStage::Compute, 1, &m_ComparisonSampler);

    signatureDesc.AddSamplerSet(SAMPLER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_SAMPLERS_COUNT);

    m_pRTDeferredRootSignature = RootSignature::CreateRootSignature(L"RTDeferred_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pRTDeferredRootSignature);

    DefineList defineList;
    defineList.insert(std::make_pair(L"USE_INLINE_RAYTRACING", std::to_wstring(1)));
    defineList.insert(std::make_pair(L"HSR_DEBUG", std::to_wstring(1)));
    defineList.insert(std::make_pair(L"USE_DEFERRED_RAYTRACING", std::to_wstring(1)));
    if (GetConfig()->InvertedDepth)
        defineList.insert(std::make_pair(L"FFX_SSSR_OPTION_INVERTED_DEPTH", std::to_wstring(1)));
    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"intersect.hlsl", L"main", ShaderModel::SM6_5, &defineList));

    m_pRTDeferredPipelineObj = PipelineObject::CreatePipelineObject(L"RTDeferred_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pRTDeferredParameters = ParameterSet::CreateParameterSet(m_pRTDeferredRootSignature);
    m_pRTDeferredParameters->SetAccelerationStructure(GetScene()->GetASManager()->GetTLAS(), 0);
    m_pRTDeferredParameters->SetTextureSRV(m_pMotionVectors, ViewDimension::Texture2D, 4);
    m_pRTDeferredParameters->SetTextureSRV(m_pNormal, ViewDimension::Texture2D, 5);
    m_pRTDeferredParameters->SetTextureSRV(m_pDepthTarget, ViewDimension::Texture2D, 6);
    m_pRTDeferredParameters->SetTextureSRV(m_pAoRoughnessMetallic, ViewDimension::Texture2D, 7);
    m_pRTDeferredParameters->SetTextureSRV(m_pDepthHierarchy, ViewDimension::Texture2D, 8);
    m_pRTDeferredParameters->SetTextureSRV(m_pExtractedRoughness, ViewDimension::Texture2D, 9);
    m_pRTDeferredParameters->SetTextureSRV(m_pHistoryColorTarget, ViewDimension::Texture2D, 10);
    m_pRTDeferredParameters->SetTextureSRV(m_pBlueNoiseTexture, ViewDimension::Texture2D, 11);
    m_pRTDeferredParameters->SetTextureSRV(m_pAlbedo, ViewDimension::Texture2D, 12);

    m_pRTDeferredParameters->SetTextureUAV(m_pHitCounter0, ViewDimension::Texture2D, 0);
    m_pRTDeferredParameters->SetTextureUAV(m_pHitCounter1, ViewDimension::Texture2D, 1);
    m_pRTDeferredParameters->SetTextureUAV(m_pRadiance0, ViewDimension::Texture2D, 2);
    m_pRTDeferredParameters->SetTextureUAV(m_pRadiance1, ViewDimension::Texture2D, 3);
    m_pRTDeferredParameters->SetTextureUAV(m_pVariance0, ViewDimension::Texture2D, 4);
    m_pRTDeferredParameters->SetTextureUAV(m_pVariance1, ViewDimension::Texture2D, 5);
    m_pRTDeferredParameters->SetTextureUAV(m_pDebugImage, ViewDimension::Texture2D, 6);
    m_pRTDeferredParameters->SetBufferUAV(m_pHWRayList, 7);
    m_pRTDeferredParameters->SetBufferUAV(m_pRayList, 8);
    m_pRTDeferredParameters->SetBufferUAV(m_pRayCounter, 9);
    m_pRTDeferredParameters->SetBufferUAV(m_pRayGBufferList, 10);

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
    {
        m_pPrimaryRTParameters->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, SHADOW_MAP_BEGIN_SLOT + i);
    }

    m_pRTDeferredParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(FrameInfo), 0);
    m_pRTDeferredParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 1);
    m_pRTDeferredParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneLightingInformation), 2);
    m_pRTDeferredParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(LightingCBData), 3);

    m_pRTDeferredIndirectWorkload = IndirectWorkload::CreateIndirectWorkload(IndirectCommandType::Dispatch);
}
void HybridReflectionsRenderModule::InitDeferredShadeRays(const json& initData)
{
    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(2, ShaderBindStage::Compute, 1);
    signatureDesc.AddConstantBufferView(3, ShaderBindStage::Compute, 1);

    signatureDesc.AddRTAccelerationStructureSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(2, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(3, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(4, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(5, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(6, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(7, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(8, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(9, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(10, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(11, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(12, ShaderBindStage::Compute, 1);

    signatureDesc.AddTextureSRVSet(SHADOW_MAP_BEGIN_SLOT, ShaderBindStage::Compute, MAX_SHADOW_MAP_TEXTURES_COUNT);  // shadow maps

    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 1, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 2, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(RAYTRACING_INFO_BEGIN_SLOT + 3, ShaderBindStage::Compute, 1);

    signatureDesc.AddTextureSRVSet(TEXTURE_BEGIN_SLOT, ShaderBindStage::Compute, MAX_TEXTURES_COUNT);

    signatureDesc.AddBufferSRVSet(INDEX_BUFFER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_BUFFER_COUNT);
    signatureDesc.AddBufferSRVSet(VERTEX_BUFFER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_BUFFER_COUNT);

    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 2);
    signatureDesc.AddTextureUAVSet(2, ShaderBindStage::Compute, 2);
    signatureDesc.AddTextureUAVSet(4, ShaderBindStage::Compute, 2);
    signatureDesc.AddTextureUAVSet(6, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(7, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(8, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(9, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(10, ShaderBindStage::Compute, 1);

    signatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, 1, &m_LinearSamplerDesc);
    signatureDesc.AddStaticSamplers(1, ShaderBindStage::Compute, 1, &m_WrapLinearSamplerDesc);
    signatureDesc.AddStaticSamplers(2, ShaderBindStage::Compute, 1, &m_SpecularSampler);
    signatureDesc.AddStaticSamplers(3, ShaderBindStage::Compute, 1, &m_DiffuseSampler);
    signatureDesc.AddStaticSamplers(4, ShaderBindStage::Compute, 1, &m_SpecularSampler);
    signatureDesc.AddStaticSamplers(5, ShaderBindStage::Compute, 1, &m_ComparisonSampler);

    signatureDesc.AddSamplerSet(SAMPLER_BEGIN_SLOT, ShaderBindStage::Compute, MAX_SAMPLERS_COUNT);

    m_pDeferredShadeRaysRootSignature = RootSignature::CreateRootSignature(L"DeferredShadeRays_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pDeferredShadeRaysRootSignature);

    DefineList defineList;
    if (GetConfig()->InvertedDepth)
        defineList.insert(std::make_pair(L"FFX_SSSR_OPTION_INVERTED_DEPTH", std::to_wstring(1)));
    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"intersect.hlsl", L"DeferredShade", ShaderModel::SM6_5, nullptr));

    m_pDeferredShadeRaysPipelineObj = PipelineObject::CreatePipelineObject(L"DeferredShadeRays_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pDeferredShadeRaysParameters = ParameterSet::CreateParameterSet(m_pDeferredShadeRaysRootSignature);
    m_pDeferredShadeRaysParameters->SetAccelerationStructure(GetScene()->GetASManager()->GetTLAS(), 0);
    m_pDeferredShadeRaysParameters->SetTextureSRV(m_pMotionVectors, ViewDimension::Texture2D, 4);
    m_pDeferredShadeRaysParameters->SetTextureSRV(m_pNormal, ViewDimension::Texture2D, 5);
    m_pDeferredShadeRaysParameters->SetTextureSRV(m_pDepthTarget, ViewDimension::Texture2D, 6);
    m_pDeferredShadeRaysParameters->SetTextureSRV(m_pAoRoughnessMetallic, ViewDimension::Texture2D, 7);
    m_pDeferredShadeRaysParameters->SetTextureSRV(m_pDepthHierarchy, ViewDimension::Texture2D, 8);
    m_pDeferredShadeRaysParameters->SetTextureSRV(m_pExtractedRoughness, ViewDimension::Texture2D, 9);
    m_pDeferredShadeRaysParameters->SetTextureSRV(m_pHistoryColorTarget, ViewDimension::Texture2D, 10);
    m_pDeferredShadeRaysParameters->SetTextureSRV(m_pBlueNoiseTexture, ViewDimension::Texture2D, 11);
    m_pDeferredShadeRaysParameters->SetTextureSRV(m_pAlbedo, ViewDimension::Texture2D, 12);

    m_pDeferredShadeRaysParameters->SetTextureUAV(m_pHitCounter0, ViewDimension::Texture2D, 0);
    m_pDeferredShadeRaysParameters->SetTextureUAV(m_pHitCounter1, ViewDimension::Texture2D, 1);
    m_pDeferredShadeRaysParameters->SetTextureUAV(m_pRadiance0, ViewDimension::Texture2D, 2);
    m_pDeferredShadeRaysParameters->SetTextureUAV(m_pRadiance1, ViewDimension::Texture2D, 3);
    m_pDeferredShadeRaysParameters->SetTextureUAV(m_pVariance0, ViewDimension::Texture2D, 4);
    m_pDeferredShadeRaysParameters->SetTextureUAV(m_pVariance1, ViewDimension::Texture2D, 5);
    m_pDeferredShadeRaysParameters->SetTextureUAV(m_pDebugImage, ViewDimension::Texture2D, 6);
    m_pDeferredShadeRaysParameters->SetBufferUAV(m_pHWRayList, 7);
    m_pDeferredShadeRaysParameters->SetBufferUAV(m_pRayList, 8);
    m_pDeferredShadeRaysParameters->SetBufferUAV(m_pRayCounter, 9);
    m_pDeferredShadeRaysParameters->SetBufferUAV(m_pRayGBufferList, 10);

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
    {
        m_pPrimaryRTParameters->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, SHADOW_MAP_BEGIN_SLOT + i);
    }

    m_pDeferredShadeRaysParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(FrameInfo), 0);
    m_pDeferredShadeRaysParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 1);
    m_pDeferredShadeRaysParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneLightingInformation), 2);
    m_pDeferredShadeRaysParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(LightingCBData), 3);

    m_pDeferredShadeRaysIndirectWorkload = IndirectWorkload::CreateIndirectWorkload(IndirectCommandType::Dispatch);
}

void HybridReflectionsRenderModule::InitPrepareIndirectHybrid(const json& initData)
{
    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(1, ShaderBindStage::Compute, 1);

    m_pPrepareIndirectHybridRootSignature = RootSignature::CreateRootSignature(L"PrepareIndirectHybrid_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pPrepareIndirectHybridRootSignature);

    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"prepare_indirect_args_hybrid.hlsl", L"main", ShaderModel::SM6_5, nullptr));

    m_pPrepareIndirectHybridPipelineObj = PipelineObject::CreatePipelineObject(L"PrepareIndirectHybrid_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pPrepareIndirectHybridParameters = ParameterSet::CreateParameterSet(m_pPrepareIndirectHybridRootSignature);
    m_pPrepareIndirectHybridParameters->SetBufferUAV(m_pRayCounter, 0);
    m_pPrepareIndirectHybridParameters->SetBufferUAV(m_pIntersectionPassIndirectArgs, 1);

    m_pPrepareIndirectHybridParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(FrameInfo), 0);
}
void HybridReflectionsRenderModule::InitPrepareIndirectHW(const json& initData)
{
    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferUAVSet(1, ShaderBindStage::Compute, 1);

    m_pPrepareIndirectHWRootSignature = RootSignature::CreateRootSignature(L"PrepareIndirectHW_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pPrepareIndirectHWRootSignature);

    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"prepare_indirect_args_hw.hlsl", L"main", ShaderModel::SM6_5, nullptr));

    m_pPrepareIndirectHWPipelineObj = PipelineObject::CreatePipelineObject(L"PrepareIndirectHW_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pPrepareIndirectHWParameters = ParameterSet::CreateParameterSet(m_pPrepareIndirectHWRootSignature);
    m_pPrepareIndirectHWParameters->SetBufferUAV(m_pRayCounter, 0);
    m_pPrepareIndirectHWParameters->SetBufferUAV(m_pIntersectionPassIndirectArgs, 1);
}

void HybridReflectionsRenderModule::InitCopyDepth(const json& initData)
{
    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureUAVSet(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureUAVSet(2, ShaderBindStage::Compute, 2);
    signatureDesc.AddTextureUAVSet(4, ShaderBindStage::Compute, 2);

    m_pCopyDepthRootSignature = RootSignature::CreateRootSignature(L"CopyDepth_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pCopyDepthRootSignature);

    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"copy_depth_and_reset_buffers.hlsl", L"main", ShaderModel::SM6_5, nullptr));

    m_pCopyDepthPipelineObj = PipelineObject::CreatePipelineObject(L"CopyDepth_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pCopyDepthParameters = ParameterSet::CreateParameterSet(m_pCopyDepthRootSignature);
    m_pCopyDepthParameters->SetTextureSRV(m_pDepthTarget, ViewDimension::Texture2D, 0);
    m_pCopyDepthParameters->SetTextureUAV(m_pDepthHierarchy, ViewDimension::Texture2D, 0);
    m_pCopyDepthParameters->SetTextureUAV(m_pDebugImage, ViewDimension::Texture2D, 1);
    m_pCopyDepthParameters->SetTextureUAV(m_pVariance0, ViewDimension::Texture2D, 2);
    m_pCopyDepthParameters->SetTextureUAV(m_pVariance1, ViewDimension::Texture2D, 3);
    m_pCopyDepthParameters->SetTextureUAV(m_pRadiance0, ViewDimension::Texture2D, 4);
    m_pCopyDepthParameters->SetTextureUAV(m_pRadiance1, ViewDimension::Texture2D, 5);

    m_pCopyDepthParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(FrameInfo), 0);
}

void HybridReflectionsRenderModule::OnResize(const ResolutionInfo& resInfo)
{
    if (!ModuleEnabled())
        return;

    int width          = GetFramework()->GetResolutionInfo().RenderWidth;
    int height         = GetFramework()->GetResolutionInfo().RenderHeight;
    m_ReflectionWidth  = std::max(128u, uint32_t(width * m_ReflectionResolutionMultiplier));
    m_ReflectionHeight = std::max(128u, uint32_t(height * m_ReflectionResolutionMultiplier));
    m_IsResized = true;

    // Need to recreate the HSR context on resource resize
    ResetBackendContext();
}

constexpr uint32_t g_NumThreadX = 8;
constexpr uint32_t g_NumThreadY = 8;

void HybridReflectionsRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    if (!m_pBRDFTexture || !m_pPrefilteredEnvironmentMap || !m_pIrradianceEnvironmentMap)
    {
        m_pBRDFTexture               = GetScene()->GetBRDFLutTexture();
        m_pPrefilteredEnvironmentMap = GetScene()->GetIBLTexture(IBLTexture::Prefiltered);
        m_pIrradianceEnvironmentMap = GetScene()->GetIBLTexture(IBLTexture::Irradiance);

        // These might not yet be loaded
        if (m_pBRDFTexture && m_pPrefilteredEnvironmentMap && m_pIrradianceEnvironmentMap)
        {
            m_pParamSet->SetTextureSRV(m_pBRDFTexture, ViewDimension::Texture2D, 3);

            m_pHybridDeferredParameters->SetTextureSRV(m_pBRDFTexture, ViewDimension::Texture2D, 1);
            m_pHybridDeferredParameters->SetTextureSRV(m_pPrefilteredEnvironmentMap, ViewDimension::TextureCube, 2);
            m_pHybridDeferredParameters->SetTextureSRV(m_pIrradianceEnvironmentMap, ViewDimension::TextureCube, 3);

            m_pRTDeferredParameters->SetTextureSRV(m_pBRDFTexture, ViewDimension::Texture2D, 1);
            m_pRTDeferredParameters->SetTextureSRV(m_pPrefilteredEnvironmentMap, ViewDimension::TextureCube, 2);
            m_pRTDeferredParameters->SetTextureSRV(m_pIrradianceEnvironmentMap, ViewDimension::TextureCube, 3);

            m_pDeferredShadeRaysParameters->SetTextureSRV(m_pBRDFTexture, ViewDimension::Texture2D, 1);
            m_pDeferredShadeRaysParameters->SetTextureSRV(m_pPrefilteredEnvironmentMap, ViewDimension::TextureCube, 2);
            m_pDeferredShadeRaysParameters->SetTextureSRV(m_pIrradianceEnvironmentMap, ViewDimension::TextureCube, 3);

            m_pPrimaryRTParameters->SetTextureSRV(m_pBRDFTexture, ViewDimension::Texture2D, 3);
            m_pPrimaryRTParameters->SetTextureSRV(m_pPrefilteredEnvironmentMap, ViewDimension::TextureCube, 4);
            m_pPrimaryRTParameters->SetTextureSRV(m_pIrradianceEnvironmentMap, ViewDimension::TextureCube, 5);
        }
        return;
    }

    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);
    // Content not ready.
    if (m_RTInfoTables.m_cpuSurfaceBuffer.size() == 0)
        return;

    int width          = GetFramework()->GetResolutionInfo().RenderWidth;
    int height         = GetFramework()->GetResolutionInfo().RenderHeight;
    m_ReflectionWidth  = std::max(128u, uint32_t(width * m_ReflectionResolutionMultiplier));
    m_ReflectionHeight = std::max(128u, uint32_t(height * m_ReflectionResolutionMultiplier));
    UpdatePerFrameConstants();

    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX HSR");

    {
        const bool isOddFrame = !!(m_FrameIndex & 1);
        m_pRadianceA          = isOddFrame ? m_pRadiance0 : m_pRadiance1;
        m_pRadianceB          = isOddFrame ? m_pRadiance1 : m_pRadiance0;
        m_pVarianceA          = isOddFrame ? m_pVariance0 : m_pVariance1;
        m_pVarianceB          = isOddFrame ? m_pVariance1 : m_pVariance0;
        m_pHitCounterA        = isOddFrame ? m_pHitCounter0 : m_pHitCounter1;
        m_pHitCounterB        = isOddFrame ? m_pHitCounter1 : m_pHitCounter0;
    }

    ExecutePrepareBlueNoise(deltaTime, pCmdList);

    std::vector<Barrier> barriers;
    for (uint32_t i = 0; i < m_RTInfoTables.m_IndexBuffers.size(); ++i)
    {
        barriers.push_back(Barrier::Transition(m_RTInfoTables.m_IndexBuffers[i]->GetResource(),
            ResourceState::IndexBufferResource,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    }

    for (uint32_t i = 0; i < m_RTInfoTables.m_VertexBuffers.size(); ++i)
    {
        barriers.push_back(Barrier::Transition(m_RTInfoTables.m_VertexBuffers[i]->GetResource(),
            ResourceState::VertexBufferResource,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    }
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    if (m_ShowDebugTarget && (m_Mask & HSR_FLAGS_VISUALIZE_PRIMARY_RAYS))
    {
        ExecutePrimaryRayTracing(deltaTime, pCmdList);
    }
    else
    {
        ExecuteDepthDownsample(deltaTime, pCmdList);
        ExecuteClassifier(deltaTime, pCmdList);

        barriers.clear();
        barriers.push_back(Barrier::Transition(
            m_pDebugImage->GetResource(), ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource, ResourceState::UnorderedAccess));
        barriers.push_back(Barrier::Transition(
            m_pRadianceA->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess));
        barriers.push_back(Barrier::Transition(
            m_pVarianceA->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess));
        barriers.push_back(Barrier::Transition(
            m_pHitCounterA->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

        ExecutePrepareIndirectHybrid(deltaTime, pCmdList);

        if (m_EnableHybridReflection)
        {
            ExecuteHybridDeferred(deltaTime, pCmdList);
        }

        if (m_Mask & HSR_FLAGS_USE_RAY_TRACING)
        {
            ExecutePrepareIndirectHW(deltaTime, pCmdList);
            ExecuteRTDeferred(deltaTime, pCmdList);
            ExecuteDeferredShadeRays(deltaTime, pCmdList);
        }

        barriers.clear();
        barriers.push_back(Barrier::Transition(
            m_pDebugImage->GetResource(), ResourceState::UnorderedAccess, ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource));
        barriers.push_back(Barrier::Transition(
            m_pRadianceA->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
        barriers.push_back(Barrier::Transition(
            m_pVarianceA->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
        barriers.push_back(Barrier::Transition(
            m_pHitCounterA->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

        ExecuteDenoiser(deltaTime, pCmdList);
    }

    barriers.clear();
    for (uint32_t i = 0; i < m_RTInfoTables.m_IndexBuffers.size(); ++i)
    {
        barriers.push_back(Barrier::Transition(m_RTInfoTables.m_IndexBuffers[i]->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::IndexBufferResource));
    }

    for (uint32_t i = 0; i < m_RTInfoTables.m_VertexBuffers.size(); ++i)
    {
        barriers.push_back(Barrier::Transition(m_RTInfoTables.m_VertexBuffers[i]->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::VertexBufferResource));
    }
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    ExecuteApplyReflections(deltaTime, pCmdList);

    // We are now done with upscaling
    GetFramework()->SetUpscalingState(UpscalerState::PostUpscale);

    m_FrameIndex++;
    m_IsResized = false;
}

void HybridReflectionsRenderModule::UpdatePerFrameConstants()
{
    setFlag(m_Mask, HSR_FLAGS_SHADING_USE_SCREEN, m_DisableReshading);
    setFlag(m_Mask, HSR_FLAGS_USE_SCREEN_SPACE, m_EnableHybridReflection);

    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    CameraComponent*      pCamera = GetScene()->GetCurrentCamera();

    memcpy(&m_FrameInfoConstants.inv_view_proj, &pCamera->GetInverseViewProjection(), sizeof(Mat4));
    memcpy(&m_FrameInfoConstants.proj, &pCamera->GetProjection(), sizeof(Mat4));
    memcpy(&m_FrameInfoConstants.inv_proj, &pCamera->GetInverseProjection(), sizeof(Mat4));
    memcpy(&m_FrameInfoConstants.view, &pCamera->GetView(), sizeof(Mat4));
    memcpy(&m_FrameInfoConstants.inv_view, &pCamera->GetInverseView(), sizeof(Mat4));
    memcpy(&m_FrameInfoConstants.prev_view_proj, &pCamera->GetPreviousViewProjection(), sizeof(Mat4));
    memcpy(&m_FrameInfoConstants.prev_view, &pCamera->GetPreviousView(), sizeof(Mat4));

    m_FrameInfoConstants.frame_index                              = m_FrameIndex;
    m_FrameInfoConstants.max_traversal_intersections              = m_MaxTraversalIntersections;
    m_FrameInfoConstants.min_traversal_occupancy                  = m_MinTraversalOccupancy;
    m_FrameInfoConstants.most_detailed_mip                        = m_MostDetailedMip;
    m_FrameInfoConstants.temporal_stability_factor                = m_TemporalStabilityFactor;
    m_FrameInfoConstants.ssr_confidence_threshold                 = m_SSRConfidenceThreshold;
    m_FrameInfoConstants.depth_buffer_thickness                   = m_DepthBufferThickness;
    m_FrameInfoConstants.roughness_threshold                      = m_RoughnessThreshold;
    m_FrameInfoConstants.samples_per_quad                         = m_SamplesPerQuad;
    m_FrameInfoConstants.temporal_variance_guided_tracing_enabled = m_TemporalVarianceGuidedTracingEnabled;
    m_FrameInfoConstants.hsr_mask                                 = m_Mask;
    m_FrameInfoConstants.random_samples_per_pixel                 = m_RandomSamplesPerPixel;
    m_FrameInfoConstants.base_width                               = resInfo.RenderWidth;
    m_FrameInfoConstants.base_height                              = resInfo.RenderHeight;
    m_FrameInfoConstants.reflection_width                         = m_ReflectionWidth;
    m_FrameInfoConstants.reflection_height                        = m_ReflectionHeight;
    m_FrameInfoConstants.hybrid_miss_weight                       = m_HybridMissWeight;
    m_FrameInfoConstants.max_raytraced_distance                   = m_MaxRaytracedDistance;
    m_FrameInfoConstants.hybrid_spawn_rate                        = m_HybridSpawnRate;
    m_FrameInfoConstants.reflections_backfacing_threshold         = m_ReflectionsBackfacingThreshold;
    m_FrameInfoConstants.vrt_variance_threshold                   = m_VRTVarianceThreshold;
    m_FrameInfoConstants.ssr_thickness_length_factor              = m_SSRThicknessLengthFactor;
    m_FrameInfoConstants.fsr_roughness_threshold                  = m_FSRRoughnessThreshold;
    m_FrameInfoConstants.ray_length_exp_factor                    = m_RayLengthExpFactor;
    m_FrameInfoConstants.rt_roughness_threshold                   = m_RTRoughnessThreshold;
    m_FrameInfoConstants.reflection_factor                        = m_ReflectionFactor;
    m_FrameInfoConstants.ibl_factor                               = GetScene()->GetIBLFactor();
    m_FrameInfoConstants.emissive_factor                          = m_EmissiveFactor;
    m_FrameInfoConstants.reset                                    = m_IsResized;
}

void HybridReflectionsRenderModule::ExecutePrepareIndirectHybrid(double deltaTime, cauldron::CommandList* pCmdList)
{
    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(m_pIntersectionPassIndirectArgs->GetResource(), ResourceState::IndirectArgument, ResourceState::UnorderedAccess));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    BufferAddressInfo sceneInfoBufferInfo;
    sceneInfoBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(FrameInfo), reinterpret_cast<const void*>(&m_FrameInfoConstants));
    m_pPrepareIndirectHybridParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo, 0);

    m_pPrepareIndirectHybridParameters->Bind(pCmdList, m_pPrepareIndirectHybridPipelineObj);

    SetPipelineState(pCmdList, m_pPrepareIndirectHybridPipelineObj);

    Dispatch(pCmdList, 1, 1, 1);

    barriers.clear();
    barriers.push_back(Barrier::UAV(m_pRayCounter->GetResource()));
    barriers.push_back(Barrier::Transition(m_pIntersectionPassIndirectArgs->GetResource(), ResourceState::UnorderedAccess, ResourceState::IndirectArgument));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void HybridReflectionsRenderModule::ExecutePrepareIndirectHW(double deltaTime, cauldron::CommandList* pCmdList)
{
    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(m_pIntersectionPassIndirectArgs->GetResource(), ResourceState::IndirectArgument, ResourceState::UnorderedAccess));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    m_pPrepareIndirectHWParameters->Bind(pCmdList, m_pPrepareIndirectHWPipelineObj);

    SetPipelineState(pCmdList, m_pPrepareIndirectHWPipelineObj);

    Dispatch(pCmdList, 1, 1, 1);

    barriers.clear();
    barriers.push_back(Barrier::UAV(m_pRayCounter->GetResource()));
    barriers.push_back(Barrier::Transition(m_pIntersectionPassIndirectArgs->GetResource(), ResourceState::UnorderedAccess, ResourceState::IndirectArgument));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void HybridReflectionsRenderModule::ExecuteApplyReflections(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX HSR ApplyReflections");
    const ResolutionInfo&   resInfo = GetFramework()->GetResolutionInfo();

    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(
        m_pColorTarget->GetResource(), ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource, ResourceState::RenderTargetResource));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    BeginRaster(pCmdList, 1, &m_pColorRasterView);

    // Allocate a dynamic constant buffer and set
    BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(FrameInfo), &m_FrameInfoConstants);
    m_pParamSet->UpdateRootConstantBuffer(&bufferInfo, 0);

    // Bind all parameters
    m_pParamSet->Bind(pCmdList, m_pApplyReflectionsPipeline);

    // Set pipeline and draw
    Viewport vp = {0.f, 0.f, resInfo.fDisplayWidth(), resInfo.fDisplayHeight(), 0.f, 1.f};
    SetViewport(pCmdList, &vp);
    Rect scissorRect = {0, 0, resInfo.RenderWidth, resInfo.RenderHeight};
    SetScissorRects(pCmdList, 1, &scissorRect);
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

    SetPipelineState(pCmdList, m_pApplyReflectionsPipeline);
    DrawInstanced(pCmdList, 3, 1, 0, 0);

    // End raster into cube map mip face
    EndRaster(pCmdList);

    barriers.clear();
    barriers.push_back(Barrier::Transition(
        m_pColorTarget->GetResource(), ResourceState::RenderTargetResource, ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void HybridReflectionsRenderModule::ExecutePrepareBlueNoise(double deltaTime, cauldron::CommandList* pCmdList)
{
    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(
        m_pBlueNoiseTexture->GetResource(), ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource, ResourceState::UnorderedAccess));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    BufferAddressInfo sceneInfoBufferInfo;
    sceneInfoBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(FrameInfo), reinterpret_cast<const void*>(&m_FrameInfoConstants));
    m_pPrepareBlueNoiseParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo, 0);
    m_pPrepareBlueNoiseParameters->Bind(pCmdList, m_pPrepareBlueNoisePipelineObj);

    SetPipelineState(pCmdList, m_pPrepareBlueNoisePipelineObj);

    Dispatch(pCmdList, 128 / 8u, 128 / 8u, 1);

    barriers.clear();
    barriers.push_back(Barrier::Transition(
        m_pBlueNoiseTexture->GetResource(), ResourceState::UnorderedAccess, ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void HybridReflectionsRenderModule::ExecuteDepthDownsample(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"HSR_DepthDownsample");
    const ResolutionInfo&   resInfo = GetFramework()->GetResolutionInfo();

    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(
        m_pDepthHierarchy->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess));
    if (m_IsResized) {
        barriers.push_back(Barrier::Transition(
            m_pVarianceB->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess));
        barriers.push_back(Barrier::Transition(
            m_pRadianceB->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess));
    }

    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    BufferAddressInfo sceneInfoBufferInfo;
    sceneInfoBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(FrameInfo), reinterpret_cast<const void*>(&m_FrameInfoConstants));
    m_pCopyDepthParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo, 0);
    m_pCopyDepthParameters->Bind(pCmdList, m_pCopyDepthPipelineObj);

    SetPipelineState(pCmdList, m_pCopyDepthPipelineObj);

    Dispatch(pCmdList, DivideRoundingUp(resInfo.RenderWidth, 64u), DivideRoundingUp(resInfo.RenderHeight, 64u), 1);

    barriers.clear();
    barriers.push_back(Barrier::Transition(
        m_pDepthHierarchy->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    if (m_IsResized)
    {
        barriers.push_back(Barrier::Transition(
            m_pVarianceB->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
        barriers.push_back(Barrier::Transition(
            m_pRadianceB->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    }
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    FfxSpdDispatchDescription dispatchParameters = {};
    dispatchParameters.commandList               = SDKWrapper::ffxGetCommandList(pCmdList);
    dispatchParameters.resource = SDKWrapper::ffxGetResource(m_pDepthHierarchy->GetResource(), L"HSR_DepthHierarchy", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ, FFX_RESOURCE_USAGE_ARRAYVIEW);

    // Disabled until remaining things are fixes
    FfxErrorCode errorCode = ffxSpdContextDispatch(&m_SpdContext, &dispatchParameters);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);
}

void HybridReflectionsRenderModule::ExecutePrimaryRayTracing(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX HSR PrimaryRayTracing");

    //Primary ray tracing
    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(
        m_pDebugImage->GetResource(), ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource, ResourceState::UnorderedAccess));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    // Allocate a dynamic constant buffers and set
    LightingCBData lightingConstantData;
    lightingConstantData.IBLFactor         = GetScene()->GetIBLFactor();
    lightingConstantData.SpecularIBLFactor = GetScene()->GetSpecularIBLFactor();

    BufferAddressInfo sceneInfoBufferInfo[4];
    sceneInfoBufferInfo[0] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(FrameInfo), reinterpret_cast<const void*>(&m_FrameInfoConstants));
    sceneInfoBufferInfo[1] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneInfo()));
    sceneInfoBufferInfo[2] =
        GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneLightingInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneLightInfo()));
    sceneInfoBufferInfo[3] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(LightingCBData), &lightingConstantData);
    m_pPrimaryRTParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[0], 0);
    m_pPrimaryRTParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[1], 1);
    m_pPrimaryRTParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[2], 2);
    m_pPrimaryRTParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[3], 3);

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    CauldronAssert(
        ASSERT_CRITICAL,
        pShadowMapResourcePool->GetRenderTargetCount() <= MAX_SHADOW_MAP_TEXTURES_COUNT,
        L"HybridReflectionsRenderModule can only support up to %d shadow maps. There are currently %d shadow maps",
        MAX_SHADOW_MAP_TEXTURES_COUNT,
        pShadowMapResourcePool->GetRenderTargetCount());
    for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
    {
        m_pPrimaryRTParameters->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, SHADOW_MAP_BEGIN_SLOT + i);
    }
    // Bind all parameters
    m_pPrimaryRTParameters->Bind(pCmdList, m_pPrimaryRTPipelineObj);

    SetPipelineState(pCmdList, m_pPrimaryRTPipelineObj);

    const uint32_t numGroupX = (m_pOutput->GetDesc().Width + g_NumThreadX - 1) / g_NumThreadX;
    const uint32_t numGroupY = (m_pOutput->GetDesc().Height + g_NumThreadY - 1) / g_NumThreadY;
    Dispatch(pCmdList, numGroupX, numGroupY, 1);

    barriers.clear();
    barriers.push_back(Barrier::Transition(
        m_pDebugImage->GetResource(), ResourceState::UnorderedAccess, ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void HybridReflectionsRenderModule::ExecuteHybridDeferred(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX HSR HybridDeferred");

    // Allocate a dynamic constant buffers and set
    LightingCBData lightingConstantData;
    lightingConstantData.IBLFactor         = GetScene()->GetIBLFactor();
    lightingConstantData.SpecularIBLFactor = GetScene()->GetSpecularIBLFactor();

    BufferAddressInfo sceneInfoBufferInfo[4];
    sceneInfoBufferInfo[0] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(FrameInfo), reinterpret_cast<const void*>(&m_FrameInfoConstants));
    sceneInfoBufferInfo[1] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneInfo()));
    sceneInfoBufferInfo[2] =
        GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneLightingInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneLightInfo()));
    sceneInfoBufferInfo[3] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(LightingCBData), &lightingConstantData);
    m_pHybridDeferredParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[0], 0);
    m_pHybridDeferredParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[1], 1);
    m_pHybridDeferredParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[2], 2);
    m_pHybridDeferredParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[3], 3);

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    CauldronAssert(
        ASSERT_CRITICAL,
        pShadowMapResourcePool->GetRenderTargetCount() <= MAX_SHADOW_MAP_TEXTURES_COUNT,
        L"HybridReflectionsRenderModule can only support up to %d shadow maps. There are currently %d shadow maps",
        MAX_SHADOW_MAP_TEXTURES_COUNT,
        pShadowMapResourcePool->GetRenderTargetCount());
    for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
    {
        m_pHybridDeferredParameters->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, SHADOW_MAP_BEGIN_SLOT + i);
    }

    // Bind all parameters
    m_pHybridDeferredParameters->Bind(pCmdList, m_pHybridDeferredPipelineObj);

    SetPipelineState(pCmdList, m_pHybridDeferredPipelineObj);

    ExecuteIndirect(pCmdList, m_pHybridDeferredIndirectWorkload, m_pIntersectionPassIndirectArgs, 1, INDIRECT_ARGS_SW_OFFSET);

    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::UAV(m_pRayCounter->GetResource()));
    barriers.push_back(Barrier::UAV(m_pRadianceA->GetResource()));
    barriers.push_back(Barrier::UAV(m_pVarianceA->GetResource()));
    barriers.push_back(Barrier::UAV(m_pHWRayList->GetResource()));
    barriers.push_back(Barrier::UAV(m_pDebugImage->GetResource()));
    barriers.push_back(Barrier::UAV(m_pHitCounterA->GetResource()));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void HybridReflectionsRenderModule::ExecuteRTDeferred(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX HSR RTDeferred");

    // Allocate a dynamic constant buffers and set
    LightingCBData lightingConstantData;
    lightingConstantData.IBLFactor         = GetScene()->GetIBLFactor();
    lightingConstantData.SpecularIBLFactor = GetScene()->GetSpecularIBLFactor();

    BufferAddressInfo sceneInfoBufferInfo[4];
    sceneInfoBufferInfo[0] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(FrameInfo), reinterpret_cast<const void*>(&m_FrameInfoConstants));
    sceneInfoBufferInfo[1] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneInfo()));
    sceneInfoBufferInfo[2] =
        GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneLightingInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneLightInfo()));
    sceneInfoBufferInfo[3] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(LightingCBData), &lightingConstantData);
    m_pRTDeferredParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[0], 0);
    m_pRTDeferredParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[1], 1);
    m_pRTDeferredParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[2], 2);
    m_pRTDeferredParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[3], 3);

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    CauldronAssert(
        ASSERT_CRITICAL,
        pShadowMapResourcePool->GetRenderTargetCount() <= MAX_SHADOW_MAP_TEXTURES_COUNT,
        L"HybridReflectionsRenderModule can only support up to %d shadow maps. There are currently %d shadow maps",
        MAX_SHADOW_MAP_TEXTURES_COUNT,
        pShadowMapResourcePool->GetRenderTargetCount());
    for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
    {
        m_pRTDeferredParameters->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, SHADOW_MAP_BEGIN_SLOT + i);
    }

    // Bind all parameters
    m_pRTDeferredParameters->Bind(pCmdList, m_pRTDeferredPipelineObj);

    SetPipelineState(pCmdList, m_pRTDeferredPipelineObj);

    ExecuteIndirect(pCmdList, m_pRTDeferredIndirectWorkload, m_pIntersectionPassIndirectArgs, 1, INDIRECT_ARGS_HW_OFFSET);

    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::UAV(m_pRayGBufferList->GetResource()));
    barriers.push_back(Barrier::UAV(m_pDebugImage->GetResource()));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void HybridReflectionsRenderModule::ExecuteDeferredShadeRays(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX HSR DeferredShadeRays");

    // Allocate a dynamic constant buffers and set
    LightingCBData lightingConstantData;
    lightingConstantData.IBLFactor         = GetScene()->GetIBLFactor();
    lightingConstantData.SpecularIBLFactor = GetScene()->GetSpecularIBLFactor();

    BufferAddressInfo sceneInfoBufferInfo[4];
    sceneInfoBufferInfo[0] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(FrameInfo), reinterpret_cast<const void*>(&m_FrameInfoConstants));
    sceneInfoBufferInfo[1] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneInfo()));
    sceneInfoBufferInfo[2] =
        GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneLightingInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneLightInfo()));
    sceneInfoBufferInfo[3] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(LightingCBData), &lightingConstantData);
    m_pDeferredShadeRaysParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[0], 0);
    m_pDeferredShadeRaysParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[1], 1);
    m_pDeferredShadeRaysParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[2], 2);
    m_pDeferredShadeRaysParameters->UpdateRootConstantBuffer(&sceneInfoBufferInfo[3], 3);

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    CauldronAssert(
        ASSERT_CRITICAL,
        pShadowMapResourcePool->GetRenderTargetCount() <= MAX_SHADOW_MAP_TEXTURES_COUNT,
        L"HybridReflectionsRenderModule can only support up to %d shadow maps. There are currently %d shadow maps",
        MAX_SHADOW_MAP_TEXTURES_COUNT,
        pShadowMapResourcePool->GetRenderTargetCount());
    for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
    {
        m_pDeferredShadeRaysParameters->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, SHADOW_MAP_BEGIN_SLOT + i);
    }
    // Bind all parameters
    m_pDeferredShadeRaysParameters->Bind(pCmdList, m_pDeferredShadeRaysPipelineObj);

    SetPipelineState(pCmdList, m_pDeferredShadeRaysPipelineObj);

    ExecuteIndirect(pCmdList, m_pDeferredShadeRaysIndirectWorkload, m_pIntersectionPassIndirectArgs, 1, INDIRECT_ARGS_HW_OFFSET);

    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::UAV(m_pRadianceA->GetResource()));
    barriers.push_back(Barrier::UAV(m_pVarianceA->GetResource()));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void HybridReflectionsRenderModule::ExecuteClassifier(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX HSR Classifier");
    const ResolutionInfo&   resInfo = GetFramework()->GetResolutionInfo();
    CameraComponent*        pCamera = GetScene()->GetCurrentCamera();

    // All cauldron resources come into a render module in a generic read state (ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource)
    FfxClassifierReflectionDispatchDescription dispatchParameters = {};
    dispatchParameters.commandList                                = SDKWrapper::ffxGetCommandList(pCmdList);
    dispatchParameters.depth         = SDKWrapper::ffxGetResource(m_pDepthTarget->GetResource(), L"HSR_InputDepth", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.motionVectors = SDKWrapper::ffxGetResource(m_pMotionVectors->GetResource(), L"HSR_InputMotionVectors", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.normal        = SDKWrapper::ffxGetResource(m_pNormal->GetResource(), L"HSR_InputNormal", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.materialParameters =
        SDKWrapper::ffxGetResource(m_pAoRoughnessMetallic->GetResource(), L"HSR_InputSpecularRoughness", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.environmentMap =
        SDKWrapper::ffxGetResource(m_pPrefilteredEnvironmentMap->GetResource(), L"HSR_InputEnvironmentMapTexture", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.radiance          = SDKWrapper::ffxGetResource(m_pRadianceA->GetResource(), L"HSR_Radiance", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.varianceHistory   = SDKWrapper::ffxGetResource(m_pVarianceB->GetResource(), L"HSR_VarianceHistory", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.hitCounter        = SDKWrapper::ffxGetResource(m_pHitCounterA->GetResource(), L"HSR_HitCounter", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.hitCounterHistory = SDKWrapper::ffxGetResource(m_pHitCounterB->GetResource(), L"HSR_HitCounterHistory", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.rayList           = SDKWrapper::ffxGetResource(m_pRayList->GetResource(), L"HSR_RayList", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    dispatchParameters.rayListHW         = SDKWrapper::ffxGetResource(m_pHWRayList->GetResource(), L"HSR_RayListHW", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    dispatchParameters.extractedRoughness =
        SDKWrapper::ffxGetResource(m_pExtractedRoughness->GetResource(), L"HSR_ExtractedRoughness", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.rayCounter            = SDKWrapper::ffxGetResource(m_pRayCounter->GetResource(), L"HSR_RayCounter", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    dispatchParameters.denoiserTileList      = SDKWrapper::ffxGetResource(m_pDenoiserTileList->GetResource(), L"HSR_DenoiserTileList", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    dispatchParameters.renderSize.width      = resInfo.RenderWidth;
    dispatchParameters.renderSize.height     = resInfo.RenderHeight;
    dispatchParameters.motionVectorScale[0]  = -1.f;
    dispatchParameters.motionVectorScale[1]  = -1.f;
    dispatchParameters.normalsUnpackMul      = 2.0f;   // Cauldron's GBuffer stores normals in the [0, 1] range, SSSR exepects them in the [-1, 1] range.
    dispatchParameters.normalsUnpackAdd      = -1.0f;  // Cauldron's GBuffer stores normals in the [0, 1] range, SSSR exepects them in the [-1, 1] range.
    dispatchParameters.roughnessChannel      = 1;
    dispatchParameters.isRoughnessPerceptual = false;
    dispatchParameters.iblFactor             = GetScene()->GetIBLFactor();
    dispatchParameters.samplesPerQuad        = m_SamplesPerQuad;
    dispatchParameters.temporalVarianceGuidedTracingEnabled = m_TemporalVarianceGuidedTracingEnabled;
    dispatchParameters.globalRoughnessThreshold             = m_RoughnessThreshold;
    dispatchParameters.rtRoughnessThreshold                 = m_RTRoughnessThreshold;
    dispatchParameters.mask                                 = m_Mask;
    dispatchParameters.reflectionWidth                      = m_ReflectionWidth;
    dispatchParameters.reflectionHeight                     = m_ReflectionHeight;
    dispatchParameters.hybridMissWeight                     = m_HybridMissWeight;
    dispatchParameters.hybridSpawnRate                      = m_HybridSpawnRate;
    dispatchParameters.vrtVarianceThreshold                 = m_VRTVarianceThreshold;
    dispatchParameters.reflectionsBackfacingThreshold       = m_ReflectionsBackfacingThreshold;
    dispatchParameters.randomSamplesPerPixel                = m_RandomSamplesPerPixel;
    dispatchParameters.frameIndex                           = m_FrameIndex;

    memcpy(&dispatchParameters.invViewProjection, &pCamera->GetInverseViewProjection(), sizeof(Mat4));
    memcpy(&dispatchParameters.projection, &pCamera->GetProjection(), sizeof(Mat4));
    memcpy(&dispatchParameters.invProjection, &pCamera->GetInverseProjection(), sizeof(Mat4));
    memcpy(&dispatchParameters.view, &pCamera->GetView(), sizeof(Mat4));
    memcpy(&dispatchParameters.invView, &pCamera->GetInverseView(), sizeof(Mat4));
    memcpy(&dispatchParameters.prevViewProjection, &pCamera->GetPreviousViewProjection(), sizeof(Mat4));

    FfxErrorCode errorCode = ffxClassifierContextReflectionDispatch(&m_ClassifierContext, &dispatchParameters);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);
}

void HybridReflectionsRenderModule::ExecuteDenoiser(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX HSR Denoiser");
    const ResolutionInfo&   resInfo = GetFramework()->GetResolutionInfo();
    CameraComponent*        pCamera = GetScene()->GetCurrentCamera();

    // Denoise
    FfxDenoiserReflectionsDispatchDescription denoiserDispatchParameters = {};
    denoiserDispatchParameters.commandList                               = SDKWrapper::ffxGetCommandList(pCmdList);
    denoiserDispatchParameters.depthHierarchy = SDKWrapper::ffxGetResource(m_pDepthHierarchy->GetResource(), L"HSR_DepthHierarchy", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchParameters.motionVectors  = SDKWrapper::ffxGetResource(m_pMotionVectors->GetResource(), L"HSR_MotionVectors", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchParameters.normal         = SDKWrapper::ffxGetResource(m_pNormal->GetResource(), L"HSR_InputNormal", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchParameters.radianceA      = SDKWrapper::ffxGetResource(m_pRadianceA->GetResource(), L"HSR_RadianceA", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchParameters.radianceB      = SDKWrapper::ffxGetResource(m_pRadianceB->GetResource(), L"HSR_RadianceB", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchParameters.varianceA      = SDKWrapper::ffxGetResource(m_pVarianceA->GetResource(), L"HSR_VarianceA", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchParameters.varianceB      = SDKWrapper::ffxGetResource(m_pVarianceB->GetResource(), L"HSR_VarianceB", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchParameters.extractedRoughness =
        SDKWrapper::ffxGetResource(m_pExtractedRoughness->GetResource(), L"HSR_ExtractedRoughness", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchParameters.denoiserTileList =
        SDKWrapper::ffxGetResource(m_pDenoiserTileList->GetResource(), L"HSR_DenoiserTileList", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    denoiserDispatchParameters.indirectArgumentsBuffer =
        SDKWrapper::ffxGetResource(m_pIntersectionPassIndirectArgs->GetResource(), L"HSR_IndirectArgumentsBuffer ", FFX_RESOURCE_STATE_INDIRECT_ARGUMENT);
    denoiserDispatchParameters.output              = SDKWrapper::ffxGetResource(m_pOutput->GetResource(), L"HSR_Output", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchParameters.normalsUnpackMul    = 2.0f;   // Cauldron's GBuffer stores normals in the [0, 1] range, SSSR exepects them in the [-1, 1] range.
    denoiserDispatchParameters.normalsUnpackAdd    = -1.0f;  // Cauldron's GBuffer stores normals in the [0, 1] range, SSSR exepects them in the [-1, 1] range.
    denoiserDispatchParameters.motionVectorScale.x = 1.0f;
    denoiserDispatchParameters.motionVectorScale.y = 1.0f;
    denoiserDispatchParameters.renderSize.width   = resInfo.RenderWidth;
    denoiserDispatchParameters.renderSize.height  = resInfo.RenderHeight;
    denoiserDispatchParameters.roughnessThreshold = m_RoughnessThreshold;
    denoiserDispatchParameters.frameIndex         = m_FrameIndex;
    denoiserDispatchParameters.temporalStabilityFactor = m_TemporalStabilityFactor;
    denoiserDispatchParameters.reset = m_IsResized;

    memcpy(&denoiserDispatchParameters.invProjection, &pCamera->GetInverseProjection(), sizeof(Mat4));
    memcpy(&denoiserDispatchParameters.invView, &pCamera->GetInverseView(), sizeof(Mat4));
    memcpy(&denoiserDispatchParameters.prevViewProjection, &pCamera->GetPreviousViewProjection(), sizeof(Mat4));

    FfxErrorCode errorCode = ffxDenoiserContextDispatchReflections(&m_DenoiserContext, &denoiserDispatchParameters);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);
}

void HybridReflectionsRenderModule::OnNewContentLoaded(ContentBlock* pContentBlock)
{
    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);
    // Material
    uint32_t textureCount = 0;
    for (auto* pMat : pContentBlock->Materials)
    {
        Material_Info materialInfo;

        materialInfo.albedo_factor_x = pMat->GetAlbedoColor().getX();
        materialInfo.albedo_factor_y = pMat->GetAlbedoColor().getY();
        materialInfo.albedo_factor_z = pMat->GetAlbedoColor().getZ();
        materialInfo.albedo_factor_w = pMat->GetAlbedoColor().getW();

        materialInfo.emission_factor_x = pMat->GetEmissiveColor().getX();
        materialInfo.emission_factor_y = pMat->GetEmissiveColor().getY();
        materialInfo.emission_factor_z = pMat->GetEmissiveColor().getZ();

        materialInfo.arm_factor_x = 1.0f;
        materialInfo.arm_factor_y = pMat->GetPBRInfo().getY();
        materialInfo.arm_factor_z = pMat->GetPBRInfo().getX();

        materialInfo.is_opaque    = pMat->GetBlendMode() == MaterialBlend::Opaque;
        materialInfo.alpha_cutoff = pMat->GetAlphaCutOff();

        int32_t samplerIndex;
        if (pMat->HasPBRInfo())
        {
            materialInfo.albedo_tex_id         = AddTexture(pMat, TextureClass::Albedo, samplerIndex);
            materialInfo.albedo_tex_sampler_id = samplerIndex;

            if (pMat->HasPBRMetalRough())
            {
                materialInfo.arm_tex_id         = AddTexture(pMat, TextureClass::MetalRough, samplerIndex);
                materialInfo.arm_tex_sampler_id = samplerIndex;
            }
            else if (pMat->HasPBRSpecGloss())
            {
                materialInfo.arm_tex_id         = AddTexture(pMat, TextureClass::SpecGloss, samplerIndex);
                materialInfo.arm_tex_sampler_id = samplerIndex;
            }
        }

        materialInfo.normal_tex_id           = AddTexture(pMat, TextureClass::Normal, samplerIndex);
        materialInfo.normal_tex_sampler_id   = samplerIndex;
        materialInfo.emission_tex_id         = AddTexture(pMat, TextureClass::Emissive, samplerIndex);
        materialInfo.emission_tex_sampler_id = samplerIndex;

        m_RTInfoTables.m_cpuMaterialBuffer.push_back(materialInfo);
    }

    MeshComponentMgr* pMeshComponentManager = MeshComponentMgr::Get();

    uint32_t nodeID = 0, surfaceID = 0;
    for (auto* pEntityData : pContentBlock->EntityDataBlocks)
    {
        for (auto* pComponent : pEntityData->Components)
        {
            if (pComponent->GetManager() == pMeshComponentManager)
            {
                Instance_Info instance_info{};
                instance_info.surface_id_table_offset = (uint32_t)m_RTInfoTables.m_cpuSurfaceIDsBuffer.size();
                const Mesh*  pMesh                    = reinterpret_cast<MeshComponent*>(pComponent)->GetData().pMesh;
                const size_t numSurfaces              = pMesh->GetNumSurfaces();
                size_t       numOpaqueSurfaces        = 0;

                for (uint32_t i = 0; i < numSurfaces; ++i)
                {
                    const Surface*  pSurface  = pMesh->GetSurface(i);
                    const Material* pMaterial = pSurface->GetMaterial();

                    m_RTInfoTables.m_cpuSurfaceIDsBuffer.push_back(surfaceID++);

                    Surface_Info surface_info{};
                    memset(&surface_info, -1, sizeof(surface_info));
                    surface_info.num_indices  = pSurface->GetIndexBuffer().Count;
                    surface_info.num_vertices = pSurface->GetVertexBuffer(VertexAttributeType::Position).Count;

                    int foundIndex = -1;
                    for (size_t i = 0; i < m_RTInfoTables.m_IndexBuffers.size(); i++)
                    {
                        if (m_RTInfoTables.m_IndexBuffers[i] == pSurface->GetIndexBuffer().pBuffer)
                        {
                            foundIndex = (int)i;
                            break;
                        }
                    }

                    surface_info.index_offset = foundIndex >= 0 ? foundIndex : (int)m_RTInfoTables.m_IndexBuffers.size();
                    if (foundIndex < 0) m_RTInfoTables.m_IndexBuffers.push_back(pSurface->GetIndexBuffer().pBuffer);

                    switch (pSurface->GetIndexBuffer().IndexFormat)
                    {
                    case ResourceFormat::R16_UINT:
                        surface_info.index_type = SURFACE_INFO_INDEX_TYPE_U16;
                        break;
                    case ResourceFormat::R32_UINT:
                        surface_info.index_type = SURFACE_INFO_INDEX_TYPE_U32;
                        break;
                    default:
                        CauldronError(L"Unsupported resource format for ray tracing indices");
                    }

                    uint32_t usedAttributes = VertexAttributeFlag_Position | VertexAttributeFlag_Normal | VertexAttributeFlag_Tangent |
                                              VertexAttributeFlag_Texcoord0 | VertexAttributeFlag_Texcoord1;

                    const uint32_t surfaceAttributes = pSurface->GetVertexAttributes();
                    usedAttributes                   = usedAttributes & surfaceAttributes;

                    for (uint32_t attribute = 0; attribute < static_cast<uint32_t>(VertexAttributeType::Count); ++attribute)
                    {
                        // Check if the attribute is present
                        if (usedAttributes & (0x1 << attribute))
                        {
                            int foundIndex = -1;
                            for (size_t i = 0; i < m_RTInfoTables.m_VertexBuffers.size(); i++)
                            {
                                if (m_RTInfoTables.m_VertexBuffers[i] == pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(attribute)).pBuffer)
                                {
                                    foundIndex = (int)i;
                                    break;
                                }
                            }
                            if (foundIndex < 0) m_RTInfoTables.m_VertexBuffers.push_back(pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(attribute)).pBuffer);
                            switch (static_cast<VertexAttributeType>(attribute))
                            {
                            case cauldron::VertexAttributeType::Position:
                                surface_info.position_attribute_offset = foundIndex >= 0 ? foundIndex : (int)m_RTInfoTables.m_VertexBuffers.size() - 1;
                                break;
                            case cauldron::VertexAttributeType::Normal:
                                surface_info.normal_attribute_offset = foundIndex >= 0 ? foundIndex : (int)m_RTInfoTables.m_VertexBuffers.size() - 1;
                                break;
                            case cauldron::VertexAttributeType::Tangent:
                                surface_info.tangent_attribute_offset = foundIndex >= 0 ? foundIndex : (int)m_RTInfoTables.m_VertexBuffers.size() - 1;
                                break;
                            case cauldron::VertexAttributeType::Texcoord0:
                                surface_info.texcoord0_attribute_offset = foundIndex >= 0 ? foundIndex : (int)m_RTInfoTables.m_VertexBuffers.size() - 1;
                                break;
                            case cauldron::VertexAttributeType::Texcoord1:
                                surface_info.texcoord1_attribute_offset = foundIndex >= 0 ? foundIndex : (int)m_RTInfoTables.m_VertexBuffers.size() - 1;
                                break;
                            default:
                                break;
                            }
                        }
                    }

                    for (size_t i = 0; i < pContentBlock->Materials.size(); i++)
                    {
                        if (pContentBlock->Materials[i] == pMaterial)
                        {
                            surface_info.material_id = (uint32_t)i;
                            break;
                        }
                    }
                    m_RTInfoTables.m_cpuSurfaceBuffer.push_back(surface_info);

                    if (!pSurface->HasTranslucency())
                        numOpaqueSurfaces++;
                }

                instance_info.num_surfaces        = (uint32_t)(numOpaqueSurfaces);
                instance_info.num_opaque_surfaces = (uint32_t)(numSurfaces);
                instance_info.node_id             = nodeID++;
                m_RTInfoTables.m_cpuInstanceBuffer.push_back(instance_info);
            }
        }
    }

    if (m_RTInfoTables.m_cpuSurfaceBuffer.size() > 0)
    {
        // Upload
        BufferDesc bufferMaterial = BufferDesc::Data(
            L"HSR_MaterialBuffer", uint32_t(m_RTInfoTables.m_cpuMaterialBuffer.size() * sizeof(Material_Info)), sizeof(Material_Info), 0, ResourceFlags::None);
        m_RTInfoTables.m_pMaterialBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferMaterial, ResourceState::CopyDest);
        const_cast<Buffer*>(m_RTInfoTables.m_pMaterialBuffer)
            ->CopyData(m_RTInfoTables.m_cpuMaterialBuffer.data(), m_RTInfoTables.m_cpuMaterialBuffer.size() * sizeof(Material_Info));

        BufferDesc bufferInstance = BufferDesc::Data(
            L"HSR_InstanceBuffer", uint32_t(m_RTInfoTables.m_cpuInstanceBuffer.size() * sizeof(Instance_Info)), sizeof(Instance_Info), 0, ResourceFlags::None);
        m_RTInfoTables.m_pInstanceBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferInstance, ResourceState::CopyDest);
        const_cast<Buffer*>(m_RTInfoTables.m_pInstanceBuffer)
            ->CopyData(m_RTInfoTables.m_cpuInstanceBuffer.data(), m_RTInfoTables.m_cpuInstanceBuffer.size() * sizeof(Instance_Info));

        BufferDesc bufferSurfaceID = BufferDesc::Data(
            L"HSR_SurfaceIDBuffer", uint32_t(m_RTInfoTables.m_cpuSurfaceIDsBuffer.size() * sizeof(uint32_t)), sizeof(uint32_t), 0, ResourceFlags::None);
        m_RTInfoTables.m_pSurfaceIDsBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferSurfaceID, ResourceState::CopyDest);
        const_cast<Buffer*>(m_RTInfoTables.m_pSurfaceIDsBuffer)
            ->CopyData(m_RTInfoTables.m_cpuSurfaceIDsBuffer.data(), m_RTInfoTables.m_cpuSurfaceIDsBuffer.size() * sizeof(uint32_t));

        BufferDesc bufferSurface = BufferDesc::Data(
            L"HSR_SurfaceBuffer", uint32_t(m_RTInfoTables.m_cpuSurfaceBuffer.size() * sizeof(Surface_Info)), sizeof(Surface_Info), 0, ResourceFlags::None);
        m_RTInfoTables.m_pSurfaceBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferSurface, ResourceState::CopyDest);
        const_cast<Buffer*>(m_RTInfoTables.m_pSurfaceBuffer)
            ->CopyData(m_RTInfoTables.m_cpuSurfaceBuffer.data(), m_RTInfoTables.m_cpuSurfaceBuffer.size() * sizeof(Surface_Info));

        m_pPrimaryRTParameters->SetBufferSRV(m_RTInfoTables.m_pMaterialBuffer, RAYTRACING_INFO_BEGIN_SLOT);
        m_pPrimaryRTParameters->SetBufferSRV(m_RTInfoTables.m_pInstanceBuffer, RAYTRACING_INFO_BEGIN_SLOT + 1);
        m_pPrimaryRTParameters->SetBufferSRV(m_RTInfoTables.m_pSurfaceIDsBuffer, RAYTRACING_INFO_BEGIN_SLOT + 2);
        m_pPrimaryRTParameters->SetBufferSRV(m_RTInfoTables.m_pSurfaceBuffer, RAYTRACING_INFO_BEGIN_SLOT + 3);

        m_pHybridDeferredParameters->SetBufferSRV(m_RTInfoTables.m_pMaterialBuffer, RAYTRACING_INFO_BEGIN_SLOT);
        m_pHybridDeferredParameters->SetBufferSRV(m_RTInfoTables.m_pInstanceBuffer, RAYTRACING_INFO_BEGIN_SLOT + 1);
        m_pHybridDeferredParameters->SetBufferSRV(m_RTInfoTables.m_pSurfaceIDsBuffer, RAYTRACING_INFO_BEGIN_SLOT + 2);
        m_pHybridDeferredParameters->SetBufferSRV(m_RTInfoTables.m_pSurfaceBuffer, RAYTRACING_INFO_BEGIN_SLOT + 3);

        m_pRTDeferredParameters->SetBufferSRV(m_RTInfoTables.m_pMaterialBuffer, RAYTRACING_INFO_BEGIN_SLOT);
        m_pRTDeferredParameters->SetBufferSRV(m_RTInfoTables.m_pInstanceBuffer, RAYTRACING_INFO_BEGIN_SLOT + 1);
        m_pRTDeferredParameters->SetBufferSRV(m_RTInfoTables.m_pSurfaceIDsBuffer, RAYTRACING_INFO_BEGIN_SLOT + 2);
        m_pRTDeferredParameters->SetBufferSRV(m_RTInfoTables.m_pSurfaceBuffer, RAYTRACING_INFO_BEGIN_SLOT + 3);

        m_pDeferredShadeRaysParameters->SetBufferSRV(m_RTInfoTables.m_pMaterialBuffer, RAYTRACING_INFO_BEGIN_SLOT);
        m_pDeferredShadeRaysParameters->SetBufferSRV(m_RTInfoTables.m_pInstanceBuffer, RAYTRACING_INFO_BEGIN_SLOT + 1);
        m_pDeferredShadeRaysParameters->SetBufferSRV(m_RTInfoTables.m_pSurfaceIDsBuffer, RAYTRACING_INFO_BEGIN_SLOT + 2);
        m_pDeferredShadeRaysParameters->SetBufferSRV(m_RTInfoTables.m_pSurfaceBuffer, RAYTRACING_INFO_BEGIN_SLOT + 3);
    }

    {
        // Update the parameter set with loaded texture entries
        CauldronAssert(ASSERT_CRITICAL, m_RTInfoTables.m_Textures.size() <= MAX_TEXTURES_COUNT, L"Too many textures.");
        for (uint32_t i = 0; i < m_RTInfoTables.m_Textures.size(); ++i)
        {
            m_pPrimaryRTParameters->SetTextureSRV(m_RTInfoTables.m_Textures[i].pTexture, ViewDimension::Texture2D, i + TEXTURE_BEGIN_SLOT);
            m_pHybridDeferredParameters->SetTextureSRV(m_RTInfoTables.m_Textures[i].pTexture, ViewDimension::Texture2D, i + TEXTURE_BEGIN_SLOT);
            m_pRTDeferredParameters->SetTextureSRV(m_RTInfoTables.m_Textures[i].pTexture, ViewDimension::Texture2D, i + TEXTURE_BEGIN_SLOT);
            m_pDeferredShadeRaysParameters->SetTextureSRV(m_RTInfoTables.m_Textures[i].pTexture, ViewDimension::Texture2D, i + TEXTURE_BEGIN_SLOT);
        }

        // Update sampler bindings as well
        CauldronAssert(ASSERT_CRITICAL, m_RTInfoTables.m_Samplers.size() <= MAX_SAMPLERS_COUNT, L"Too many samplers.");
        for (uint32_t i = 0; i < m_RTInfoTables.m_Samplers.size(); ++i)
        {
            m_pPrimaryRTParameters->SetSampler(m_RTInfoTables.m_Samplers[i], i + SAMPLER_BEGIN_SLOT);
            m_pHybridDeferredParameters->SetSampler(m_RTInfoTables.m_Samplers[i], i + SAMPLER_BEGIN_SLOT);
            m_pRTDeferredParameters->SetSampler(m_RTInfoTables.m_Samplers[i], i + SAMPLER_BEGIN_SLOT);
            m_pDeferredShadeRaysParameters->SetSampler(m_RTInfoTables.m_Samplers[i], i + SAMPLER_BEGIN_SLOT);
        }

        CauldronAssert(ASSERT_CRITICAL, m_RTInfoTables.m_IndexBuffers.size() <= MAX_BUFFER_COUNT, L"Too many index buffers.");
        for (uint32_t i = 0; i < m_RTInfoTables.m_IndexBuffers.size(); ++i)
        {
            m_pPrimaryRTParameters->SetBufferSRV(m_RTInfoTables.m_IndexBuffers[i], i + INDEX_BUFFER_BEGIN_SLOT);
            m_pHybridDeferredParameters->SetBufferSRV(m_RTInfoTables.m_IndexBuffers[i], i + INDEX_BUFFER_BEGIN_SLOT);
            m_pRTDeferredParameters->SetBufferSRV(m_RTInfoTables.m_IndexBuffers[i], i + INDEX_BUFFER_BEGIN_SLOT);
            m_pDeferredShadeRaysParameters->SetBufferSRV(m_RTInfoTables.m_IndexBuffers[i], i + INDEX_BUFFER_BEGIN_SLOT);
        }

        CauldronAssert(ASSERT_CRITICAL, m_RTInfoTables.m_VertexBuffers.size() <= MAX_BUFFER_COUNT, L"Too many vertex buffers.");
        for (uint32_t i = 0; i < m_RTInfoTables.m_VertexBuffers.size(); ++i)
        {
            m_pPrimaryRTParameters->SetBufferSRV(m_RTInfoTables.m_VertexBuffers[i], i + VERTEX_BUFFER_BEGIN_SLOT);
            m_pHybridDeferredParameters->SetBufferSRV(m_RTInfoTables.m_VertexBuffers[i], i + VERTEX_BUFFER_BEGIN_SLOT);
            m_pRTDeferredParameters->SetBufferSRV(m_RTInfoTables.m_VertexBuffers[i], i + VERTEX_BUFFER_BEGIN_SLOT);
            m_pDeferredShadeRaysParameters->SetBufferSRV(m_RTInfoTables.m_VertexBuffers[i], i + VERTEX_BUFFER_BEGIN_SLOT);
        }
    }
}

void HybridReflectionsRenderModule::OnContentUnloaded(ContentBlock* pContentBlock)
{
    for (auto materialInfo : m_RTInfoTables.m_cpuMaterialBuffer)
    {
        if (materialInfo.albedo_tex_id > 0)
            RemoveTexture(materialInfo.albedo_tex_id);
        if (materialInfo.arm_tex_id > 0)
            RemoveTexture(materialInfo.arm_tex_id);
        if (materialInfo.emission_tex_id > 0)
            RemoveTexture(materialInfo.emission_tex_id);
        if (materialInfo.normal_tex_id > 0)
            RemoveTexture(materialInfo.normal_tex_id);
    }
}

// Add texture index info and return the index to the texture in the texture array
int32_t HybridReflectionsRenderModule::AddTexture(const Material* pMaterial, const TextureClass textureClass, int32_t& textureSamplerIndex)
{
    const cauldron::TextureInfo* pTextureInfo = pMaterial->GetTextureInfo(textureClass);
    if (pTextureInfo != nullptr)
    {
        // Check if the texture's sampler is already one we have, and if not add it
        for (textureSamplerIndex = 0; textureSamplerIndex < m_RTInfoTables.m_Samplers.size(); ++textureSamplerIndex)
        {
            if (m_RTInfoTables.m_Samplers[textureSamplerIndex]->GetDesc() == pTextureInfo->TexSamplerDesc)
                break;  // found
        }

        // If we didn't find the sampler, add it
        if (textureSamplerIndex == m_RTInfoTables.m_Samplers.size())
        {
            Sampler* pSampler = Sampler::CreateSampler(L"HSRSampler", pTextureInfo->TexSamplerDesc);
            CauldronAssert(ASSERT_WARNING, pSampler, L"Could not create sampler for loaded content %s", pTextureInfo->pTexture->GetDesc().Name.c_str());
            m_RTInfoTables.m_Samplers.push_back(pSampler);
        }

        // Find a slot for the texture
        int32_t firstFreeIndex = -1;
        for (int32_t i = 0; i < m_RTInfoTables.m_Textures.size(); ++i)
        {
            RTInfoTables::BoundTexture& boundTexture = m_RTInfoTables.m_Textures[i];

            // If this texture is already mapped, bump it's reference count
            if (pTextureInfo->pTexture == boundTexture.pTexture)
            {
                boundTexture.count += 1;
                return i;
            }

            // Try to re-use an existing entry that was released
            else if (firstFreeIndex < 0 && boundTexture.count == 0)
            {
                firstFreeIndex = i;
            }
        }

        // Texture wasn't found
        RTInfoTables::BoundTexture b = {pTextureInfo->pTexture, 1};
        if (firstFreeIndex < 0)
        {
            m_RTInfoTables.m_Textures.push_back(b);
            return static_cast<int32_t>(m_RTInfoTables.m_Textures.size()) - 1;
        }
        else
        {
            m_RTInfoTables.m_Textures[firstFreeIndex] = b;
            return firstFreeIndex;
        }
    }
    return -1;
}

void HybridReflectionsRenderModule::RemoveTexture(int32_t index)
{
    if (index >= 0)
    {
        m_RTInfoTables.m_Textures[index].count -= 1;
        if (m_RTInfoTables.m_Textures[index].count == 0)
        {
            m_RTInfoTables.m_Textures[index].pTexture = nullptr;
        }
    }
}
