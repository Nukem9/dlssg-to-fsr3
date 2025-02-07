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

#include "vrsrendermodule.h"

#include "shaders/surfacerendercommon.h"
#include "render/device.h"
#include "render/dynamicresourcepool.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rasterview.h"
#include "render/uploadheap.h"
#include "core/backend_interface.h"
#include "core/framework.h"
#include "core/components/meshcomponent.h"
#include "core/loaders/textureloader.h"
#include "core/scene.h"
#include "core/uimanager.h"
#include "render/parameterset.h"
#include "render/shaderbuilderhelper.h"

#define FFX_CPP
#include <FidelityFX/gpu/vrs/ffx_variable_shading.h>

using namespace cauldron;

struct VRSOverlayInformation
{
    FfxUInt32  width, height;
    FfxUInt32  tileSize;
};

VRSRenderModule::VRSRenderModule() :
    RenderModule(L"VRSRenderModule")
{
}

VRSRenderModule::~VRSRenderModule()
{
    DestroyFfxContext();
    
    delete m_pOverlayRootSignature;
    delete m_pOverlayPipelineObj;
    delete m_pOverlayParameters;

    GetContentManager()->RemoveContentListener(this);

    if (m_GenerateMotionVectors)
    {
        delete m_pMotionVectorsRootSignature;
        delete m_pMotionVectorsParameterSet;

        for (auto& pipeline : m_PipelineHashObjects)
        {
            delete pipeline.m_Pipeline;
        }
        m_PipelineHashObjects.clear();
        m_MotionVectorsRenderSurfaces.clear();
    }
}

void VRSRenderModule::Init(const json& initData)
{
    // VRS Tier support
    if (GetFramework()->GetDevice()->FeatureSupported(DeviceFeature::VRSTier2))
    {
        m_VRSTierSupported = 2;
        GetFramework()->GetDevice()->GetFeatureInfo(DeviceFeature::VRSTier2, &m_FeatureInfoVRS);
    }
    else if (GetFramework()->GetDevice()->FeatureSupported(DeviceFeature::VRSTier1))
    {
        m_VRSTierSupported = 1;
        GetFramework()->GetDevice()->GetFeatureInfo(DeviceFeature::VRSTier1, &m_FeatureInfoVRS);
    }

    BuildUI();

    if (m_VRSTierSupported > 0)
    {
        m_pColorTarget = GetFramework()->GetColorTargetForCallback(GetName());

        int width  = GetFramework()->GetResolutionInfo().RenderWidth;
        int height = GetFramework()->GetResolutionInfo().RenderHeight;

        // History color buffer
        TextureDesc historyColorDesc = m_pColorTarget->GetDesc();
        historyColorDesc.Width       = width;
        historyColorDesc.Height      = height;
        historyColorDesc.Name        = L"HistoryColorBuffer";
        m_pHistoryColorBuffer        = GetDynamicResourcePool()->CreateRenderTexture(
            &historyColorDesc, [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
                desc.Width  = renderingWidth;
                desc.Height = renderingHeight;
            });
        CauldronAssert(ASSERT_ERROR, m_pHistoryColorBuffer, L"Could not create history color texture");

        // VRS Image
        uint32_t vrsImageWidth, vrsImageHeight;
        ffxVrsGetImageSizeFromeRenderResolution(&vrsImageWidth, &vrsImageHeight, width, height, m_FeatureInfoVRS.MaxTileSize[0]);
        TextureDesc vrsImageDesc = TextureDesc::Tex2D(
            L"VRSImage", ResourceFormat::R8_UINT, vrsImageWidth, vrsImageHeight, 1, 1, ResourceFlags::AllowShadingRate | ResourceFlags::AllowUnorderedAccess);
        m_pVRSTexture = GetDynamicResourcePool()->CreateTexture(
            &vrsImageDesc,
            static_cast<ResourceState>(ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource),
            [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
                cauldron::FeatureInfo_VRS featureInfoVRS;
                GetFramework()->GetDevice()->GetFeatureInfo(DeviceFeature::VRSTier2, &featureInfoVRS);
                uint32_t shadingRateImageTileSize = featureInfoVRS.MaxTileSize[0];
                ffxVrsGetImageSizeFromeRenderResolution(&desc.Width, &desc.Height, renderingWidth, renderingHeight, shadingRateImageTileSize);
            });
        CauldronAssert(ASSERT_ERROR, m_pVRSTexture, L"Could not create the VRS image texture");

        // Callbacks
        ExecuteCallback callbackDrawOverlay      = [this](double deltaTime, CommandList* pCmdList) { this->DrawOverlayCallback(deltaTime, pCmdList); };
        ExecutionTuple  callbackDrawOverlayTuple = std::make_pair(L"VRSRenderModule::DrawOverlayCallback", std::make_pair(this, callbackDrawOverlay));
        GetFramework()->RegisterExecutionCallback(L"ToneMappingRenderModule", false, callbackDrawOverlayTuple);

        ExecuteCallback callbackCopyColorBuffer = [this](double deltaTime, CommandList* pCmdList) { this->CopyColorBufferCallback(deltaTime, pCmdList); };
        ExecutionTuple  callbackCopyColorBufferTuple =
            std::make_pair(L"VRSRenderModule::CopyColorBufferCallback", std::make_pair(this, callbackCopyColorBuffer));
        GetFramework()->RegisterExecutionCallback(L"ToneMappingRenderModule", false, callbackCopyColorBufferTuple);

        m_GenerateMotionVectors = (GetFramework()->GetConfig()->MotionVectorGeneration == "VRSRenderModule");
        if (m_GenerateMotionVectors)
        {
            ExecuteCallback callbackGenerateMotionVectors = [this](double deltaTime, CommandList* pCmdList) {
                this->GenerateMotionVectorsCallback(deltaTime, pCmdList);
            };
            ExecutionTuple callbackGenerateMotionVectorsTuple =
                std::make_pair(L"VRSRenderModule::GenerateMotionVectorsCallback", std::make_pair(this, callbackGenerateMotionVectors));
            GetFramework()->RegisterExecutionCallback(L"VRSRenderModule", true, callbackGenerateMotionVectorsTuple);

            InitMotionVectors(initData);
        }

        InitOverlay(initData);

        InitFfxBackend();

        GetFramework()->ConfigureRuntimeShaderRecompiler(
            [this]() { DestroyFfxContext(); }, [this](void) { InitFfxContext(); });
    }

    UpdateVRSInfo();

    SetModuleReady(true);
}

void VRSRenderModule::InitFfxBackend()
{
    // Initialize the FFX backend
    const size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(FFX_VRS_CONTEXT_COUNT);
    void*        scratchBuffer     = calloc(scratchBufferSize, 1u);
    FfxErrorCode errorCode         = SDKWrapper::ffxGetInterface(&m_InitializationParameters.backendInterface, GetDevice(), scratchBuffer, scratchBufferSize, FFX_VRS_CONTEXT_COUNT);
    CAULDRON_ASSERT(errorCode == FFX_OK);
    CauldronAssert(ASSERT_CRITICAL, m_InitializationParameters.backendInterface.fpGetSDKVersion(&m_InitializationParameters.backendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
        L"FidelityFX VRS 2.1 sample requires linking with a 1.1.2 version SDK backend");
    CauldronAssert(ASSERT_CRITICAL, ffxVrsGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 2, 0),
                       L"FidelityFX VRS 2.1 sample requires linking with a 1.2 version FidelityFX VRS library");
                       
    m_InitializationParameters.backendInterface.fpRegisterConstantBufferAllocator(&m_InitializationParameters.backendInterface, SDKWrapper::ffxAllocateConstantBuffer);
}

void VRSRenderModule::InitFfxContext()
{
    InitFfxBackend();

    UpdateVRSContext(true);
}

void VRSRenderModule::DestroyFfxContext()
{
    // Flush anything out of the pipes before destroying the context
    GetDevice()->FlushAllCommandQueues();

    UpdateVRSContext(false);

    // Destroy the FidelityFX interface memory
    if (m_InitializationParameters.backendInterface.scratchBuffer != nullptr)
    {
        free(m_InitializationParameters.backendInterface.scratchBuffer);
        m_InitializationParameters.backendInterface.scratchBuffer = nullptr;
    }
}

void VRSRenderModule::OnResize(const ResolutionInfo& resInfo)
{
    if (!ModuleEnabled())
    {
        return;
    }

    UpdateVRSContext(false);
    UpdateVRSContext(true);
}

void VRSRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    if (!m_EnableVariableShading)
        return;

    if (m_ShadingRateCombinerIndex != 0)
    {
        ExecuteVRSImageGen(deltaTime, pCmdList);
    }
}

void VRSRenderModule::BuildUI()
{
    // UI
    UISection* uiSection = GetUIManager()->RegisterUIElements("Variable Shading", UISectionType::Sample);

    if (m_VRSTierSupported == 0)
    {
        uiSection->RegisterUIElement<UIText>("GPU does not support VRS!");
        return;
    }

    uiSection->RegisterUIElement<UICheckBox>(
        "Enable Variable Shading",
        m_EnableVariableShading,
        [this](bool cur, bool old) {
            ToggleVariableShading();
        }
    );

    std::vector<const char*> comboOptions;
    const char* shadingRateOptions[] = {"1x1", "1x2", "1x4", "2x1", "2x2", "2x4", "4x1", "4x2", "4x4"};
    auto GetFirstSetBitPos    = [](int32_t n) { return log2(n & -n) + 1; };
    for (uint32_t i = 0; i < (m_FeatureInfoVRS.NumShadingRates); ++i)
    {
        ShadingRate shadingRate = m_FeatureInfoVRS.ShadingRates[i];

        uint32_t x = (uint32_t)GetFirstSetBitPos(static_cast<uint32_t>(shadingRate) >> SHADING_RATE_SHIFT) - 1;
        uint32_t y = (uint32_t)GetFirstSetBitPos(static_cast<uint32_t>(shadingRate) & (uint32_t)(ShadingRate1D::ShadingRate1D_1X | ShadingRate1D::ShadingRate1D_2X | ShadingRate1D::ShadingRate1D_4X)) - 1;

        comboOptions.push_back(shadingRateOptions[x * SHADING_RATE_SHIFT + y]);
    }

    uiSection->RegisterUIElement<UICombo>(
        "PerDraw VRS",
        (int32_t&)m_ShadingRateIndex,
        comboOptions,
        m_VariableShadingEnabled,
        [this](int32_t cur, int32_t old) {
            SelectBaseShadingRate();
        }
    );

    if (m_VRSTierSupported == 2)
    {
        uiSection->RegisterUIElement<UICheckBox>(
            "ShadingRateImage Enabled",
            m_EnableShadingRateImage,
            m_VariableShadingEnabled,
            [this](bool cur, bool old) {
                ToggleShadingRateImage();
            }
        );

        const char* combinerOptions[] = {"Passthrough", "Override", "Min", "Max", "Sum", "Mul"};
        comboOptions.clear();
        m_AvailableCombiners.clear();
        for (int32_t i = 0; i < _countof(combinerOptions); ++i)
        {
            if ((uint32_t)m_FeatureInfoVRS.Combiners & (1u << i))
            {
                comboOptions.push_back(combinerOptions[i]);
                m_AvailableCombiners.push_back(static_cast<ShadingRateCombiner>(1u << i));
            }
        }

        uiSection->RegisterUIElement<UICombo>(
            "ShadingRateImage Combiner",
            (int32_t&)m_ShadingRateCombinerIndex,
            std::move(comboOptions),
            m_ShadingRateImageEnabled,
            [this](int32_t cur, int32_t old) {
                SelectCombiner();
            }
        );

        uiSection->RegisterUIElement<UISlider<float>>("VRS variance Threshold", m_VRSThreshold, 0.f, 0.1f, m_ShadingRateImageEnabled);
        uiSection->RegisterUIElement<UISlider<float>>("VRS Motion Factor", m_VRSMotionFactor, 0.f, 0.1f, m_ShadingRateImageEnabled);

        uiSection->RegisterUIElement<UICheckBox>(
            "ShadingRateImage Overlay",
            m_DrawOverlay,
            m_ShadingRateImageEnabled,
            [this](bool cur, bool old) {
                ToggleOverlay();
            }
        );
    }
}

void VRSRenderModule::InitOverlay(const json& initData)
{
    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Pixel, 1);
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1);

    m_pOverlayRootSignature = RootSignature::CreateRootSignature(L"VRSOverlay_RootSignature", signatureDesc);

    // Fetch needed resources
    m_pOverlayRenderTarget = GetFramework()->GetRenderTexture(L"SwapChainProxy"); // This occurs after tone mapping, so goes to swapchain proxy
    CauldronAssert(ASSERT_CRITICAL, m_pOverlayRenderTarget != nullptr, L"Couldn't find or create the render target for VRS Overlay.");

    m_pOverlayRasterView = GetRasterViewAllocator()->RequestRasterView(m_pOverlayRenderTarget, ViewDimension::Texture2D);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pOverlayRootSignature);

    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"VrsOverlay.hlsl", L"mainVS", ShaderModel::SM6_0, nullptr));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"VrsOverlay.hlsl", L"mainPS", ShaderModel::SM6_0, nullptr));

    // Setup remaining information and build
    // Setup blend and depth states
    BlendDesc blendDesc = {
        true, Blend::SrcAlpha, Blend::InvSrcAlpha, BlendOp::Add, Blend::One, Blend::InvSrcAlpha, BlendOp::Add, static_cast<uint32_t>(ColorWriteMask::All)};
    std::vector<BlendDesc> blends;
    blends.push_back(blendDesc);
    psoDesc.AddBlendStates(blends, false, false);
    DepthDesc depthDesc;
    psoDesc.AddDepthState(&depthDesc);  // Use defaults

    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
    psoDesc.AddRasterFormats(m_pOverlayRenderTarget->GetFormat());

    m_pOverlayPipelineObj = PipelineObject::CreatePipelineObject(L"VRSOverlay_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pOverlayParameters = ParameterSet::CreateParameterSet(m_pOverlayRootSignature);
    m_pOverlayParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(VRSOverlayInformation), 0);
}

void VRSRenderModule::InitMotionVectors(const json& initData)
{
    m_pMotionVectors = GetFramework()->GetRenderTexture(L"GBufferMotionVectorRT");
    m_pDepthTarget   = GetFramework()->GetRenderTexture(L"GBufferDepth");

    m_pMotionVectorsRasterView = GetRasterViewAllocator()->RequestRasterView(m_pMotionVectors, ViewDimension::Texture2D);
    m_pDepthRasterView         = GetRasterViewAllocator()->RequestRasterView(m_pDepthTarget, ViewDimension::Texture2D);

    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::VertexAndPixel, 1);  // Frame Information
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::VertexAndPixel, 1);  // Instance Information

    m_pMotionVectorsRootSignature = RootSignature::CreateRootSignature(L"MotionVectorsPass_RootSignature", signatureDesc);

    // Create ParameterSet and assign the constant buffer parameters
    // We will add texture views as they are loaded
    m_pMotionVectorsParameterSet = ParameterSet::CreateParameterSet(m_pMotionVectorsRootSignature);
    m_pMotionVectorsParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 0);
    m_pMotionVectorsParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(InstanceInformation), 1);

    // Register for content change updates
    GetContentManager()->AddContentListener(this);
}

void VRSRenderModule::CopyColorBufferCallback(double deltaTime, cauldron::CommandList* pCmdList)
{
    if (!m_EnableShadingRateImage || m_ShadingRateCombinerIndex == 0) // ShadingRateCombiner_Passthrough
        return;

    GPUScopedProfileCapture sampleMarker(pCmdList, L"VRS_CopyColor");

    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(m_pColorTarget->GetResource(), 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
        ResourceState::CopySource));
    barriers.push_back(Barrier::Transition(m_pHistoryColorBuffer->GetResource(), 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
        ResourceState::CopyDest));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    // Copy the color render target before we apply translucency
    TextureCopyDesc copyColor = TextureCopyDesc(m_pColorTarget->GetResource(), m_pHistoryColorBuffer->GetResource());
    CopyTextureRegion(pCmdList, &copyColor);

    barriers.clear();
    barriers.push_back(Barrier::Transition(m_pColorTarget->GetResource(), 
        ResourceState::CopySource, 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(m_pHistoryColorBuffer->GetResource(), 
        ResourceState::CopyDest, 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void VRSRenderModule::DrawOverlayCallback(double deltaTime, cauldron::CommandList* pCmdList)
{
    if (!m_DrawOverlay || m_ShadingRateCombinerIndex == 0)  // ShadingRateCombiner_Passthrough
        return;

    m_pOverlayParameters->SetTextureSRV(m_pVRSTexture, ViewDimension::Texture2D, 0);
    GPUScopedProfileCapture sampleMarker(pCmdList, L"VRS_DrawOverlay");

    // Render modules expect resources coming in/going out to be in a shader read state
    Barrier rtBarrier = Barrier::Transition(m_pOverlayRenderTarget->GetResource(), 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
        ResourceState::RenderTargetResource);
    ResourceBarrier(pCmdList, 1, &rtBarrier);

    BeginRaster(pCmdList, 1, &m_pOverlayRasterView);

    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

    uint32_t rtWidth, rtHeight;
    rtWidth  = resInfo.DisplayWidth;
    rtHeight = resInfo.DisplayHeight;

    VRSOverlayInformation constantData = {rtWidth, rtHeight, m_FeatureInfoVRS.MaxTileSize[0]};
    BufferAddressInfo    bufferInfo   = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(constantData), &constantData);
    m_pOverlayParameters->UpdateRootConstantBuffer(&bufferInfo, 0);

    // Bind all parameters
    m_pOverlayParameters->Bind(pCmdList, m_pOverlayPipelineObj);

    Viewport vp = {0.f, 0.f, resInfo.fDisplayWidth(), resInfo.fDisplayHeight(), 0.f, 1.f};
    SetViewport(pCmdList, &vp);

    Rect scissorRect = {0, 0, rtWidth, rtHeight};
    SetScissorRects(pCmdList, 1, &scissorRect);

    // Set pipeline and draw
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);
    SetPipelineState(pCmdList, m_pOverlayPipelineObj);

    DrawInstanced(pCmdList, 3, 1, 0, 0);

    EndRaster(pCmdList);

    // Render modules expect resources coming in/going out to be in a shader read state
    rtBarrier = Barrier::Transition(m_pOverlayRenderTarget->GetResource(), 
        ResourceState::RenderTargetResource, 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
    ResourceBarrier(pCmdList, 1, &rtBarrier);
}

void VRSRenderModule::GenerateMotionVectorsCallback(double deltaTime, cauldron::CommandList* pCmdList)
{
    if (!m_GenerateMotionVectors)
        return;

    GPUScopedProfileCapture motionVectorsMarker(pCmdList, L"VRS_MotionVectors");

    // Render modules expect resources coming in/going out to be in a shader read state
    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(
        m_pMotionVectors->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::RenderTargetResource));
    barriers.push_back(Barrier::Transition(
        m_pDepthTarget->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::DepthWrite));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ClearRenderTarget(pCmdList, &m_pMotionVectorsRasterView->GetResourceView(), clearColor);
    ClearDepthStencil(pCmdList, &m_pDepthRasterView->GetResourceView(), 0);

    // Bind raster resources
    BeginRaster(pCmdList, 1, &m_pMotionVectorsRasterView, m_pDepthRasterView);

    // Update necessary scene frame information
    BufferAddressInfo sceneInfoBufferInfo;
    sceneInfoBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneInfo()));
    m_pMotionVectorsParameterSet->UpdateRootConstantBuffer(&sceneInfoBufferInfo, 0);

    // Set viewport, scissor, primitive topology once and move on (set based on upscaler state)
    UpscalerState         upscaleState = GetFramework()->GetUpscalingState();
    const ResolutionInfo& resInfo      = GetFramework()->GetResolutionInfo();

    uint32_t width, height;
    if (upscaleState == UpscalerState::None || upscaleState == UpscalerState::PostUpscale)
    {
        width  = resInfo.DisplayWidth;
        height = resInfo.DisplayHeight;
    }
    else
    {
        width  = resInfo.RenderWidth;
        height = resInfo.RenderHeight;
    }

    Viewport vp = {0.f, 0.f, (float)width, (float)height, 0.f, 1.f};
    SetViewport(pCmdList, &vp);
    Rect scissorRect = {0, 0, width, height};
    SetScissorRects(pCmdList, 1, &scissorRect);
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);
    cauldron::PipelineObject*   currentPipeline = nullptr;  // optimization
    for (const auto& renderSurface : m_MotionVectorsRenderSurfaces)
    {
        if (currentPipeline != renderSurface.m_Pipeline)
        {
            SetPipelineState(pCmdList, renderSurface.m_Pipeline);
            currentPipeline = renderSurface.m_Pipeline;
        }

        if (renderSurface.m_RenderSurface.pOwner->IsActive())
        {
            InstanceInformation instanceInfo;
            instanceInfo.WorldTransform = renderSurface.m_RenderSurface.pOwner->GetTransform();
            instanceInfo.PrevWorldTransform = renderSurface.m_RenderSurface.pOwner->GetPrevTransform();

            instanceInfo.MaterialInfo.EmissiveFactor = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            instanceInfo.MaterialInfo.AlbedoFactor   = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
            instanceInfo.MaterialInfo.PBRParams      = Vec4(0.0f, 0.0f, 0.0f, 0.0f);

            const Surface* pSurface = renderSurface.m_RenderSurface.pSurface;

            // Update root constants
            BufferAddressInfo perObjectBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(InstanceInformation), &instanceInfo);
            m_pMotionVectorsParameterSet->UpdateRootConstantBuffer(&perObjectBufferInfo, 1);

            // Bind for rendering
            m_pMotionVectorsParameterSet->Bind(pCmdList, renderSurface.m_Pipeline);

            std::vector<BufferAddressInfo> vertexBuffers;
            uint32_t                       surfaceAttributes = pSurface->GetVertexAttributes();
            // Check if the attribute is present - should always be the case
            if (surfaceAttributes & VertexAttributeFlag_Position)
            {
                vertexBuffers.push_back(pSurface->GetVertexBuffer(VertexAttributeType::Position).pBuffer->GetAddressInfo());
            }
                
            
            // Set vertex/index buffers
            SetVertexBuffers(pCmdList, 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data());

            BufferAddressInfo addressInfo = pSurface->GetIndexBuffer().pBuffer->GetAddressInfo();
            SetIndexBuffer(pCmdList, &addressInfo);

            // And draw
            DrawIndexedInstanced(pCmdList, pSurface->GetIndexBuffer().Count);
        }
    }

    // Done drawing, unbind
    EndRaster(pCmdList);

    // Render modules expect resources coming in/going out to be in a shader read state
    barriers.clear();
    barriers.push_back(Barrier::Transition(
        m_pMotionVectors->GetResource(), ResourceState::RenderTargetResource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(
        m_pDepthTarget->GetResource(), ResourceState::DepthWrite, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void VRSRenderModule::ToggleVariableShading()
{
    if (!m_EnableVariableShading)
    {
        m_EnableShadingRateImage = false;
        m_DrawOverlay            = false;
        m_ShadingRateCombinerIndex = 0;
        m_ShadingRateIndex         = 0;

        m_VariableShadingEnabled = false;
        m_ShadingRateImageEnabled = false;
    }
    else
    {
        m_VariableShadingEnabled = true;
    }

    UpdateVRSInfo();
    UpdateVRSContext(m_EnableVariableShading);
}

void VRSRenderModule::ToggleShadingRateImage()
{
    m_ShadingRateImageEnabled = !m_ShadingRateImageEnabled;

    if (!m_EnableShadingRateImage)
        m_DrawOverlay = false;

    m_ShadingRateCombinerIndex = m_EnableShadingRateImage ? 1 : 0;

    UpdateVRSInfo();
}

void VRSRenderModule::ToggleOverlay()
{
    if (!m_EnableShadingRateImage)
        m_DrawOverlay = false;
}

void VRSRenderModule::SelectBaseShadingRate()
{
    ShadingRate shadingRate = m_FeatureInfoVRS.ShadingRates[m_ShadingRateIndex];
    auto        GetFirstSetBitPos = [](int32_t n) { return log2(n & -n) + 1; };

    uint32_t x = (uint32_t)GetFirstSetBitPos(static_cast<uint32_t>(shadingRate) >> SHADING_RATE_SHIFT) - 1;
    uint32_t y = (uint32_t)GetFirstSetBitPos(static_cast<uint32_t>(shadingRate) & (uint32_t)(ShadingRate1D::ShadingRate1D_1X | ShadingRate1D::ShadingRate1D_2X | ShadingRate1D::ShadingRate1D_4X)) - 1;

    bool isAdditionalShadingRate = (x + y > 2);

    if (isAdditionalShadingRate != m_AllowAdditionalShadingRates)
    {
        m_AllowAdditionalShadingRates = isAdditionalShadingRate;
        UpdateVRSContext(false);
        UpdateVRSContext(true);
    }

    UpdateVRSInfo();
}

void VRSRenderModule::SelectCombiner()
{
    if (m_ShadingRateCombinerIndex == 0)
        m_DrawOverlay = false;

    UpdateVRSInfo();
}

void VRSRenderModule::UpdateVRSInfo()
{
    VariableShadingRateInfo info;

    info.Combiners[0]            = ShadingRateCombiner::ShadingRateCombiner_Passthrough;
    info.Combiners[1]            = m_AvailableCombiners.empty() ? ShadingRateCombiner::ShadingRateCombiner_Passthrough : m_AvailableCombiners.at(m_ShadingRateCombinerIndex);
    info.pShadingRateImage       = m_pVRSTexture;
    info.BaseShadingRate         = m_FeatureInfoVRS.ShadingRates[m_ShadingRateIndex];
    info.ShadingRateTileWidth    = m_FeatureInfoVRS.MaxTileSize[0];
    info.ShadingRateTileHeight   = m_FeatureInfoVRS.MaxTileSize[1];
    info.VariableShadingMode     = m_EnableShadingRateImage ? VariableShadingMode::VariableShadingMode_Image
                                                            : (m_EnableVariableShading ? VariableShadingMode::VariableShadingMode_Per_Draw
                                                                                       : VariableShadingMode::VariableShadingMode_None);
    GetDevice()->SetVRSInfo(info);
}

void VRSRenderModule::UpdateVRSContext(bool enabled)
{
    if (!enabled && m_ContextCreated)
    {
        // Flush anything out of the pipes before destroying the context
        GetDevice()->FlushAllCommandQueues();

        if (m_ContextCreated)
            ffxVrsContextDestroy(&m_VRSContext);

        m_ContextCreated = false;
    }
    else if (enabled && !m_ContextCreated)
    {
        if (m_AllowAdditionalShadingRates)
            m_InitializationParameters.flags |= FFX_VRS_ALLOW_ADDITIONAL_SHADING_RATES;

        m_InitializationParameters.shadingRateImageTileSize = m_FeatureInfoVRS.MaxTileSize[0];
        ffxVrsContextCreate(&m_VRSContext, &m_InitializationParameters);

        m_ContextCreated = true;
    }
}

void VRSRenderModule::ExecuteVRSImageGen(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"VRS_ImageGen");

    // Render modules expect resources coming in/going out to be in a shader read state
    Barrier barrier = Barrier::Transition(m_pVRSTexture->GetResource(), 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
        ResourceState::UnorderedAccess);
    ResourceBarrier(pCmdList, 1, &barrier);

    uint32_t width  = GetFramework()->GetResolutionInfo().RenderWidth;
    uint32_t height = GetFramework()->GetResolutionInfo().RenderHeight;

    FfxVrsDispatchDescription dispatchParameters = {};
    dispatchParameters.commandList               = SDKWrapper::ffxGetCommandList(pCmdList);
    dispatchParameters.output                    = SDKWrapper::ffxGetResource(m_pVRSTexture->GetResource(), L"VRSImage", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    dispatchParameters.historyColor        = SDKWrapper::ffxGetResource(m_pHistoryColorBuffer->GetResource(), L"HistoryColorBuffer", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.motionVectors       = SDKWrapper::ffxGetResource(m_pMotionVectors->GetResource(), L"VRSMotionVectorsTarget", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.motionFactor        = m_VRSMotionFactor;
    dispatchParameters.varianceCutoff      = m_VRSThreshold;
    dispatchParameters.tileSize            = m_FeatureInfoVRS.MaxTileSize[0];
    dispatchParameters.renderSize          = {width, height};
    dispatchParameters.motionVectorScale.x = -1.f;
    dispatchParameters.motionVectorScale.y = -1.f;

    // Disabled until remaining things are fixes
    FfxErrorCode errorCode = ffxVrsContextDispatch(&m_VRSContext, &dispatchParameters);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);

    barrier = Barrier::Transition(m_pVRSTexture->GetResource(), 
        ResourceState::UnorderedAccess, 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
    ResourceBarrier(pCmdList, 1, &barrier);
}

void VRSRenderModule::OnNewContentLoaded(ContentBlock* pContentBlock)
{
    MeshComponentMgr* pMeshComponentManager = MeshComponentMgr::Get();

    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);

    // For each new Mesh, create a GBufferComponent that will map mesh/material information for more efficient rendering at run time
    for (auto* pEntityData : pContentBlock->EntityDataBlocks)
    {
        for (auto* pComponent : pEntityData->Components)
        {
            if (pComponent->GetManager() == pMeshComponentManager)
            {
                const Mesh*  pMesh       = reinterpret_cast<MeshComponent*>(pComponent)->GetData().pMesh;
                const size_t numSurfaces = pMesh->GetNumSurfaces();
                for (uint32_t i = 0; i < numSurfaces; ++i)
                {
                    const Surface*  pSurface  = pMesh->GetSurface(i);
                    const Material* pMaterial = pSurface->GetMaterial();

                    // Push surface render information
                    PipelineSurfaceRenderInfo surfaceRenderInfo;
                    surfaceRenderInfo.pOwner   = pComponent->GetOwner();
                    surfaceRenderInfo.pSurface = pSurface;

                    // Create pipeline or retrieve already created
                    uint32_t pipeHashIndex = CreatePipelineObject(pSurface);

                    // setup MotionVectorsRenderData
                    MotionVectorsRenderData renderData;
                    renderData.m_Pipeline      = m_PipelineHashObjects.at(pipeHashIndex).m_Pipeline;
                    renderData.m_RenderSurface = surfaceRenderInfo;
                    m_MotionVectorsRenderSurfaces.push_back(renderData);
                }
            }
        }
    }
}

void VRSRenderModule::OnContentUnloaded(ContentBlock* pContentBlock)
{
}

//////////////////////////////////////////////////////////////////////////
// Content loading helpers

uint32_t VRSRenderModule::CreatePipelineObject(const Surface* pSurface)
{
    // VRS shader should be optimized based on what the model provides
    // It only needs the Position attributes
    
    // so we should accept by default all the attributes except the texcoords
    uint32_t usedAttributes = VertexAttributeFlag_Position;

    // only keep the available attributes of the surface
    const uint32_t surfaceAttributes = pSurface->GetVertexAttributes();
    usedAttributes                   = usedAttributes & surfaceAttributes;

    CauldronAssert(ASSERT_CRITICAL, usedAttributes != 0, L"Encountered a surface that has no position attribute.");

    DefineList      defineList;
    const Material* pMaterial = pSurface->GetMaterial();

    // defines in the shaders

    // ID_skinningMatrices  - todo

    // ID_normalTexCoord
    // ID_emissiveTexCoord
    // ID_occlusionTexCoord
    // ID_albedoTexCoord
    // ID_metallicRoughnessTexCoord

    // ID_normalTexture
    // ID_emissiveTexture
    // ID_occlusionTexture
    // ID_albedoTexture
    // ID_metallicRoughnessTexture

    defineList.insert(std::make_pair(L"HAS_MOTION_VECTORS", L"1"));

    // compute hash
    uint64_t hash = static_cast<uint64_t>(Hash(defineList, usedAttributes, pSurface));

    // See if we've already built this pipeline
    for (uint32_t i = 0; i < m_PipelineHashObjects.size(); ++i)
    {
        if (m_PipelineHashObjects[i].m_PipelineHash == hash)
            return i;
    }

    // If we didn't find the pipeline already, create a new one

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pMotionVectorsRootSignature);

    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"transformVS.hlsl", L"MainVS", ShaderModel::SM6_0, &defineList));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"motionvectorsps.hlsl", L"MainPS", ShaderModel::SM6_0, &defineList));

    // Setup remaining information and build
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);

    psoDesc.AddRasterFormats(m_pMotionVectors->GetFormat(), m_pDepthTarget->GetFormat());

    std::vector<BlendDesc> blendDesc;
    psoDesc.AddBlendStates(blendDesc, false, false);

    RasterDesc rasterDesc;
    rasterDesc.CullingMode = CullMode::None;
    psoDesc.AddRasterStateDescription(&rasterDesc);

    // Set input layout
    std::vector<InputLayoutDesc> vertexAttributes;
    vertexAttributes.push_back(InputLayoutDesc(
        VertexAttributeType::Position, pSurface->GetVertexBuffer(VertexAttributeType::Position).ResourceDataFormat, static_cast<uint32_t>(vertexAttributes.size()), 0));
    psoDesc.AddInputLayout(vertexAttributes);

    DepthDesc depthDesc;
    depthDesc.DepthEnable = true;
    depthDesc.StencilEnable = false;

    depthDesc.DepthWriteEnable = true;
    depthDesc.DepthFunc = ComparisonFunc::Less;

    psoDesc.AddDepthState(&depthDesc);

    PipelineObject* pPipelineObj = PipelineObject::CreatePipelineObject(L"MotionVectorsRenderPass_PipelineObj", psoDesc);

    // Ok, this is a new pipeline, add it to the PipelineHashObject vector
    PipelineHashObject pipelineHashObject;
    pipelineHashObject.m_Pipeline     = pPipelineObj;
    pipelineHashObject.m_PipelineHash = hash;
    m_PipelineHashObjects.push_back(pipelineHashObject);

    return static_cast<uint32_t>(m_PipelineHashObjects.size() - 1);
}
