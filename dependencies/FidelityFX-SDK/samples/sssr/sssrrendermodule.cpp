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

#include "sssrrendermodule.h"

#include "core/backend_interface.h"
#include "core/framework.h"
#include "core/scene.h"
#include "core/uimanager.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rasterview.h"
#include "shaders/sssr_apply_reflections_common.h"

using namespace cauldron;

void SSSRRenderModule::Init(const json& initData)
{    
    //////////////////////////////////////////////////////////////////////////
    // Cauldron resources

    // Fetch needed resources
    m_pColorTarget = GetFramework()->GetColorTargetForCallback(GetName());
    // Needed to apply reflections
    m_pColorRasterView = GetRasterViewAllocator()->RequestRasterView(m_pColorTarget, ViewDimension::Texture2D);

    m_pDepthTarget = GetFramework()->GetRenderTexture(L"DepthTarget");
    m_pOutput = GetFramework()->GetRenderTexture(L"SSSR_Output");

    // Assumed resources, need to check they are there
    m_pBaseColor = GetFramework()->GetRenderTexture(L"GBufferAlbedoRT");
    m_pMotionVectors = GetFramework()->GetRenderTexture(L"GBufferMotionVectorRT");
    m_pNormal = GetFramework()->GetRenderTexture(L"GBufferNormalRT");
    m_pAoRoughnessMetallic = GetFramework()->GetRenderTexture(L"GBufferAoRoughnessMetallicRT");
    CauldronAssert(ASSERT_CRITICAL, m_pBaseColor && m_pMotionVectors && m_pNormal && m_pAoRoughnessMetallic && m_pBaseColor, L"Could not get one of the needed resources for SSSR Rendermodule.");

    // Register a pre-lighting callback to set the specular IBL factor to 0 for the lighting pass.
    // We're doing this to avoid applying the IBL specular reflections twice, once in the lighting pass and once in the SSSR pass.
    ExecuteCallback callbackPreLighting = [this](double deltaTime, CommandList* pCmdList) {
        m_SceneSpecularIBLFactor = GetScene()->GetSpecularIBLFactor();
        if (m_ApplyScreenSpaceReflections)
        {
            GetScene()->SetSpecularIBLFactor(0.0f);
        }
    };

    ExecutionTuple callbackPreLightingTuple = std::make_pair(L"SSSRRenderModule::PreLightingCallback", std::make_pair(this, callbackPreLighting));
    GetFramework()->RegisterExecutionCallback(L"LightingRenderModule", true, callbackPreLightingTuple);

    // Register a post-lighting callback to reset the IBL factor to what it previously was.
    ExecuteCallback callbackPostLighting = [this](double deltaTime, CommandList* pCmdList) {
        GetScene()->SetSpecularIBLFactor(m_SceneSpecularIBLFactor);
    };
    ExecutionTuple callbackPostLightingTuple = std::make_pair(L"SSSRRenderModule::PostLightingCallback", std::make_pair(this, callbackPostLighting));
    GetFramework()->RegisterExecutionCallback(L"LightingRenderModule", false, callbackPostLightingTuple);

    //////////////////////////////////////////////////////////////////////////
    // Final pass resources to apply reflections

    m_LinearSamplerDesc.Filter = FilterFunc::MinMagLinearMipPoint;
    m_LinearSamplerDesc.MaxAnisotropy = 1u;

    RootSignatureDesc applyReflectionsSignatureDesc;
    applyReflectionsSignatureDesc.AddConstantBufferView(0, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(1, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(2, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(3, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddTextureSRVSet(4, ShaderBindStage::Pixel, 1);
    applyReflectionsSignatureDesc.AddStaticSamplers(0, ShaderBindStage::Pixel, 1, &m_LinearSamplerDesc);

    m_pApplyReflectionsRS = RootSignature::CreateRootSignature(L"SSSR_ApplyReflections", applyReflectionsSignatureDesc);

    BlendDesc blendDesc = {};
    blendDesc.BlendEnabled = true;
    blendDesc.SourceBlendColor = Blend::One;
    blendDesc.ColorOp = BlendOp::Add;
    blendDesc.DestBlendColor = Blend::SrcAlpha;
    blendDesc.SourceBlendAlpha = Blend::One;
    blendDesc.AlphaOp = BlendOp::Add;
    blendDesc.DestBlendAlpha = Blend::One;
    const std::vector<BlendDesc> blendDescs = { blendDesc };

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pApplyReflectionsRS);
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"fullscreen.hlsl", L"FullscreenVS", ShaderModel::SM6_0, nullptr));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"sssr_apply_reflections.hlsl", L"ps_main", ShaderModel::SM6_0, nullptr));
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
    psoDesc.AddBlendStates(blendDescs, false, false);
    psoDesc.AddRasterFormats(m_pColorTarget->GetFormat());   // Use the first raster set, as we just want the format and they are all the same
    
    m_pApplyReflectionsPipeline = PipelineObject::CreatePipelineObject(L"SSSR_ApplyReflections", psoDesc);

    m_pParamSet = ParameterSet::CreateParameterSet(m_pApplyReflectionsRS);
    m_pParamSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(ApplyReflectionsConstants), 0);
    m_pParamSet->SetTextureSRV(m_pOutput, ViewDimension::Texture2D, 0);
    m_pParamSet->SetTextureSRV(m_pNormal, ViewDimension::Texture2D, 1);
    m_pParamSet->SetTextureSRV(m_pBaseColor, ViewDimension::Texture2D, 2);
    m_pParamSet->SetTextureSRV(m_pAoRoughnessMetallic, ViewDimension::Texture2D, 3);

    // UI
    InitUI();

    //////////////////////////////////////////////////////////////////////////
    // Backend init
    InitFfxContext();

    GetFramework()->ConfigureRuntimeShaderRecompiler(
        [this](void) { DestroyFfxContext(); },
        [this](void) { InitFfxContext(); });

    // That's all we need for now
    SetModuleReady(true);
}

void SSSRRenderModule::InitFfxContext()
{
    // Initialize the FFX backend
    const size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(FFX_SSSR_CONTEXT_COUNT);
    void* scratchBuffer = calloc(scratchBufferSize, 1);
    FfxErrorCode errorCode = SDKWrapper::ffxGetInterface(&m_InitializationParameters.backendInterface, GetDevice(), scratchBuffer, scratchBufferSize, FFX_SSSR_CONTEXT_COUNT);
    CAULDRON_ASSERT(errorCode == FFX_OK);
    CauldronAssert(ASSERT_CRITICAL, m_InitializationParameters.backendInterface.fpGetSDKVersion(&m_InitializationParameters.backendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
        L"FidelityFX SSSR 2.1 sample requires linking with a 1.1.2 version SDK backend");
    CauldronAssert(ASSERT_CRITICAL, ffxSssrGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 5, 0),
                       L"FidelityFX SSSR 2.1 sample requires linking with a 1.5 version FidelityFX SSSR library");
                       
    m_InitializationParameters.backendInterface.fpRegisterConstantBufferAllocator(&m_InitializationParameters.backendInterface, SDKWrapper::ffxAllocateConstantBuffer);
    
    // Init SSSR context
    CreateSSSRContext();
}

SSSRRenderModule::~SSSRRenderModule()
{
    // Protection
    if (ModuleEnabled())
    {
        EnableModule(false);

        // Destroy the SSSR context
        CAULDRON_ASSERT(ffxSssrContextDestroy(&m_Context) == FFX_OK);

        // Destroy the FidelityFX interface memory
        free(m_InitializationParameters.backendInterface.scratchBuffer);
        
        delete m_pParamSet;
        delete m_pApplyReflectionsPipeline;
        delete m_pApplyReflectionsRS;
    }
}

void SSSRRenderModule::DestroyFfxContext()
{
    // Flush anything out of the pipes before destroying the context
    GetDevice()->FlushAllCommandQueues();

    // Destroy the SSSR context
    CAULDRON_ASSERT(ffxSssrContextDestroy(&m_Context) == FFX_OK);

    // Destroy the FidelityFX interface memory
    if (m_InitializationParameters.backendInterface.scratchBuffer != nullptr)
    {
        free(m_InitializationParameters.backendInterface.scratchBuffer);
        m_InitializationParameters.backendInterface.scratchBuffer = nullptr;
    }
}

void SSSRRenderModule::InitUI()
{
    UISection* uiSection = GetUIManager()->RegisterUIElements("SSSR", UISectionType::Sample);

    uiSection->RegisterUIElement<UICheckBox>("Apply Screen Space Reflections", m_ApplyScreenSpaceReflections);
    uiSection->RegisterUIElement<UICheckBox>("Show Reflection Target", m_ShowReflectionTarget);
    uiSection->RegisterUIElement<UISlider<float>>("Reflections Intensity (1 for PBR correctness)", m_SpecularReflectionsMultiplier, 0.0f, 10.0f);

    uiSection->RegisterUIElement<UISlider<int32_t>>("Max Traversal Iterations", m_MaxTraversalIntersections, 0, 256);
    uiSection->RegisterUIElement<UISlider<int32_t>>("Min Traversal Occupancy", m_MinTraversalOccupancy, 0, 32);
    uiSection->RegisterUIElement<UISlider<float>>("Depth Buffer Thickness", m_DepthBufferThickness, 0.0f, 0.03f);
    uiSection->RegisterUIElement<UISlider<float>>("Roughness Threshold", m_RoughnessThreshold, 0.0f, 1.0f);
    uiSection->RegisterUIElement<UISlider<float>>("Temporal Stability", m_TemporalStabilityFactor, 0.f, 1.f);
    uiSection->RegisterUIElement<UISlider<float>>("Temporal Variance Threshold", m_VarianceThreshold, 0.0f, 0.01f);
    uiSection->RegisterUIElement<UICheckBox>("Enable Variance Guided Tracing", m_TemporalVarianceGuidedTracingEnabled);

    static std::vector<const char*> samplesPerQuadOptions = { "1", "2", "4" };
    uiSection->RegisterUIElement<UICombo>(
        "Samples Per Quad",
        m_SamplesPerQuadOptionIndex,
        samplesPerQuadOptions,
        [this](int32_t cur, int32_t old) {
            switch (cur)
            {
            case 0:
                m_SamplesPerQuad = 1;
                break;
            case 1:
                m_SamplesPerQuad = 2;
                break;
            case 2:
                m_SamplesPerQuad = 4;
                break;
            }
        }
    );
}

void SSSRRenderModule::CreateSSSRContext()
{
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    m_InitializationParameters.flags = GetConfig()->InvertedDepth ? FFX_SSSR_ENABLE_DEPTH_INVERTED : 0;
    m_InitializationParameters.renderSize.width = resInfo.RenderWidth;
    m_InitializationParameters.renderSize.height = resInfo.RenderHeight;
    m_InitializationParameters.normalsHistoryBufferFormat = SDKWrapper::GetFfxSurfaceFormat(m_pNormal->GetFormat());
    CAULDRON_ASSERT(ffxSssrContextCreate(&m_Context, &m_InitializationParameters) == FFX_OK);
}

void SSSRRenderModule::ResetSSSRContext()
{
    // Destroy the SSSR context
    CAULDRON_ASSERT(ffxSssrContextDestroy(&m_Context) == FFX_OK);

    // Re-create the SSSR context
    CreateSSSRContext();
}

void SSSRRenderModule::OnResize(const ResolutionInfo& resInfo)
{
    if (!ModuleEnabled())
    {
        return;
    }

    // Need to recreate the SSSR context on resource resize
    ResetSSSRContext();
}

void SSSRRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    if (!m_pBrdfTexture || !m_pPrefilteredEnvironmentMap)
    {
        m_pBrdfTexture = GetScene()->GetBRDFLutTexture();
        m_pPrefilteredEnvironmentMap = GetScene()->GetIBLTexture(IBLTexture::Prefiltered);

        if (m_pBrdfTexture && m_pPrefilteredEnvironmentMap)
        {
            m_pParamSet->SetTextureSRV(m_pBrdfTexture, ViewDimension::Texture2D, 4);
        }
        return;
    }

    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX SSSR");
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    CameraComponent* pCamera = GetScene()->GetCurrentCamera();

    // All cauldron resources come into a render module in a generic read state (ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource)
    FfxSssrDispatchDescription dispatchParameters = {};
    dispatchParameters.commandList = SDKWrapper::ffxGetCommandList(pCmdList);
    dispatchParameters.color = SDKWrapper::ffxGetResource(m_pColorTarget->GetResource(), L"SSSR_InputColor", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.depth = SDKWrapper::ffxGetResource(m_pDepthTarget->GetResource(), L"SSSR_InputDepth", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.motionVectors = SDKWrapper::ffxGetResource(m_pMotionVectors->GetResource(), L"SSSR_InputMotionVectors", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.normal = SDKWrapper::ffxGetResource(m_pNormal->GetResource(), L"SSSR_InputNormal", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.materialParameters = SDKWrapper::ffxGetResource(m_pAoRoughnessMetallic->GetResource(), L"SSSR_InputAoRoughnessMetallic", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.environmentMap = SDKWrapper::ffxGetResource(m_pPrefilteredEnvironmentMap->GetResource(), L"SSSR_InputEnvironmentMapTexture", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.brdfTexture = SDKWrapper::ffxGetResource(m_pBrdfTexture->GetResource(), L"SSSR_InputBRDFTexture", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.output = SDKWrapper::ffxGetResource(m_pOutput->GetResource(), L"SSSR_Output", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.normalUnPackMul = 2.0f;      // Cauldron's GBuffer stores normals in the [0, 1] range, SSSR expects them in the [-1, 1] range.
    dispatchParameters.normalUnPackAdd = -1.0f;     // Cauldron's GBuffer stores normals in the [0, 1] range, SSSR expects them in the [-1, 1] range.
    dispatchParameters.motionVectorScale.x = 1.0f;
    dispatchParameters.motionVectorScale.y = 1.0f;
    dispatchParameters.roughnessChannel = 1;
    dispatchParameters.isRoughnessPerceptual = false;
    dispatchParameters.renderSize.width = resInfo.RenderWidth;
    dispatchParameters.renderSize.height = resInfo.RenderHeight;
    dispatchParameters.iblFactor = GetScene()->GetIBLFactor();
    dispatchParameters.temporalStabilityFactor = m_TemporalStabilityFactor;
    dispatchParameters.depthBufferThickness = m_DepthBufferThickness;
    dispatchParameters.roughnessThreshold = m_RoughnessThreshold;
    dispatchParameters.varianceThreshold = m_VarianceThreshold;
    dispatchParameters.maxTraversalIntersections = m_MaxTraversalIntersections;
    dispatchParameters.minTraversalOccupancy = m_MinTraversalOccupancy;
    dispatchParameters.mostDetailedMip = m_MostDetailedMip;
    dispatchParameters.samplesPerQuad = m_SamplesPerQuad;
    dispatchParameters.temporalVarianceGuidedTracingEnabled = m_TemporalVarianceGuidedTracingEnabled;

    memcpy(&dispatchParameters.invViewProjection,   &pCamera->GetInverseViewProjection(), sizeof(Mat4));
    memcpy(&dispatchParameters.projection,          &pCamera->GetProjection(), sizeof(Mat4));
    memcpy(&dispatchParameters.invProjection,       &pCamera->GetInverseProjection(), sizeof(Mat4));
    memcpy(&dispatchParameters.view,                &pCamera->GetView(), sizeof(Mat4));
    memcpy(&dispatchParameters.invView,             &pCamera->GetInverseView(), sizeof(Mat4));
    memcpy(&dispatchParameters.prevViewProjection,  &pCamera->GetPreviousViewProjection(), sizeof(Mat4));

    FfxErrorCode errorCode = ffxSssrContextDispatch(&m_Context, &dispatchParameters);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);

    const Barrier toRenderTargetBarrier = Barrier::Transition(m_pColorTarget->GetResource(), 
        ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource, 
        ResourceState::RenderTargetResource);
    ResourceBarrier(pCmdList, 1, &toRenderTargetBarrier);

    BeginRaster(pCmdList, 1, &m_pColorRasterView);
    
    // Allocate a dynamic constant buffer and set
    Vec4 cameraDirection = GetScene()->GetCurrentCamera()->GetDirection();
    cameraDirection.setW(0.0f);
    ApplyReflectionsConstants constantBuffer = { 
        cameraDirection,
        (uint32_t)m_ShowReflectionTarget, 
        (uint32_t)m_ApplyScreenSpaceReflections,
        m_SpecularReflectionsMultiplier
    };
    BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(ApplyReflectionsConstants), &constantBuffer);
    m_pParamSet->UpdateRootConstantBuffer(&bufferInfo, 0);
    m_pParamSet->Bind(pCmdList, m_pApplyReflectionsPipeline);

    Viewport vp = { 0.f, 0.f, resInfo.fDisplayWidth(), resInfo.fDisplayHeight(), 0.f, 1.f };
    SetViewport(pCmdList, &vp);
    Rect scissorRect = { 0, 0, resInfo.RenderWidth, resInfo.RenderHeight };
    SetScissorRects(pCmdList, 1, &scissorRect);
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

    SetPipelineState(pCmdList, m_pApplyReflectionsPipeline);
    DrawInstanced(pCmdList, 3);
    
    EndRaster(pCmdList);

    const Barrier toDefaultStateBarrier = Barrier::Transition(m_pColorTarget->GetResource(), 
        ResourceState::RenderTargetResource, 
        ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource);
    ResourceBarrier(pCmdList, 1, &toDefaultStateBarrier);
}
