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

#include "lpmrendermodule.h"

#include "core/backend_interface.h"
#include "core/contentmanager.h"
#include "core/framework.h"
#include "core/loaders/textureloader.h"
#include "core/scene.h"
#include "core/uimanager.h"
#include "render/device.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rasterview.h"
#include "render/renderdefines.h"
#include "render/swapchain.h"

using namespace cauldron;
using namespace std::experimental;

void LPMRenderModule::Init(const json& initData)
{
    //////////////////////////////////////////////////////////////////////////
    // Resource setup

    // Fetch needed resources
    m_pInputColor = GetFramework()->GetRenderTexture(L"HDR11Color");
    CauldronAssert(ASSERT_CRITICAL, m_pInputColor != nullptr, L"Couldn't find the input texture for the tone mapper");

    // Get the proper post tone map color target (these are the same now)
    m_pOutputColor = GetFramework()->GetRenderTexture(L"SwapChainProxy");

    //////////////////////////////////////////////////////////////////////////
    // Build UI

    m_Shoulder         = 1;
    m_SoftGap          = 0.0f;
    m_HdrMax           = 1847.0f;
    m_LpmExposure      = GetScene()->GetSceneExposure();
    m_Contrast         = 0.3f;
    m_ShoulderContrast = 1.0f;
    m_Saturation[0]    = 0.0f;
    m_Saturation[1]    = 0.0f;
    m_Saturation[2]    = 0.0f;
    m_Crosstalk[0]     = 1.0f;
    m_Crosstalk[1]     = 1.0f / 2.0f;
    m_Crosstalk[2]     = 1.0f / 32.0f;
    m_ColorSpace       = ColorSpace::ColorSpace_REC709;
    m_DisplayMode      = GetSwapChain()->GetSwapChainDisplayMode();

    // Build UI options
    UISection* uiSection = GetUIManager()->RegisterUIElements("LPM Tonemapper", UISectionType::Sample);

    // Setup LPM preset options
    uiSection->RegisterUIElement<UISlider<float>>("Soft Gap", m_SoftGap, 0.0f, 0.5f);
    uiSection->RegisterUIElement<UISlider<float>>("HDR Max", m_HdrMax, 6.0f, 2048.0f);
    uiSection->RegisterUIElement<UISlider<float>>("LPM Exposure", m_LpmExposure, 1.0f, 16.0f,
        [](float cur, float old) {
            GetScene()->SetSceneExposure(cur);
        });
    uiSection->RegisterUIElement<UISlider<float>>("Contrast", m_Contrast, 0.0f, 1.0f);
    uiSection->RegisterUIElement<UISlider<float>>("Shoulder Contrast", m_ShoulderContrast, 1.0f, 1.5f);
    uiSection->RegisterUIElement<UISlider<float>>("Saturation Red", m_Saturation[0], -1.0f, 1.0f);
    uiSection->RegisterUIElement<UISlider<float>>("Saturation Green", m_Saturation[1], -1.0f, 1.0f);
    uiSection->RegisterUIElement<UISlider<float>>("Saturation Blue", m_Saturation[2], -1.0f, 1.0f);
    uiSection->RegisterUIElement<UISlider<float>>("Crosstalk Red", m_Crosstalk[0], 0.0f, 1.0f);
    uiSection->RegisterUIElement<UISlider<float>>("Crosstalk Green", m_Crosstalk[1], 0.0f, 1.0f);
    uiSection->RegisterUIElement<UISlider<float>>("Crosstalk Blue", m_Crosstalk[2], 0.0f, 1.0f);

    InitFfxContext();

    GetFramework()->ConfigureRuntimeShaderRecompiler(
        [this](void) {
            DestroyFfxContext();
        },
        [this](void) {
            InitFfxContext();
        });
    //////////////////////////////////////////////////////////////////////////
    // Finish up init

    // Fetch needed resource
    m_pRenderTarget = m_pInputColor;
    CauldronAssert(ASSERT_CRITICAL, m_pRenderTarget != nullptr, L"Couldn't get the LPM fullscreen render target when initializing LPMRenderModule.");

    m_pRasterView   = GetRasterViewAllocator()->RequestRasterView(m_pRenderTarget, ViewDimension::Texture2D);

    // Initialize common resources that aren't pipeline dependent
    m_LinearSamplerDesc.Filter        = FilterFunc::MinMagLinearMipPoint;
    m_LinearSamplerDesc.MaxLOD        = std::numeric_limits<float>::max();
    m_LinearSamplerDesc.MaxAnisotropy = 1;

    // Using AllowRenderTarget + AllowUnorderedAccess on the same resource is usually frowned upon for performance reasons, but we are doing it here in the interest of re-using a resource
    TextureLoadCompletionCallbackFn completionCallback = [this](const std::vector<const Texture*>& textures, void* additionalParams = nullptr) { this->TextureLoadComplete(textures, additionalParams); };
    filesystem::path texturePath = L"..\\media\\Textures\\LPM\\LuxoDoubleChecker_EXR_ARGB_16F_1.dds";
    GetContentManager()->LoadTexture(TextureLoadInfo(texturePath, false, 1.f, ResourceFlags::None), completionCallback);
}

void LPMRenderModule::InitFfxContext()
{
    // Setup FidelityFX interface.
    const size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(FFX_LPM_CONTEXT_COUNT);
    void* scratchBuffer = calloc(scratchBufferSize, 1u);
    FfxErrorCode errorCode = SDKWrapper::ffxGetInterface(&m_InitializationParameters.backendInterface, GetDevice(), scratchBuffer, scratchBufferSize, FFX_LPM_CONTEXT_COUNT);
    CauldronAssert(ASSERT_CRITICAL, errorCode == FFX_OK, L"Couldn't initialize the FidelityFX SDK backend interface.");
    CauldronAssert(ASSERT_CRITICAL, m_InitializationParameters.backendInterface.fpGetSDKVersion(&m_InitializationParameters.backendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
        L"FidelityFX LPM 2.1 sample requires linking with a 1.1.2 version SDK backend");
    CauldronAssert(ASSERT_CRITICAL, ffxLpmGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 4, 0),
                       L"FidelityFX LPM 2.1 sample requires linking with a 1.4 version FidelityFX LPM library");
                       
    m_InitializationParameters.backendInterface.fpRegisterConstantBufferAllocator(&m_InitializationParameters.backendInterface, SDKWrapper::ffxAllocateConstantBuffer);

    // Create the LPM context
    ffxLpmContextCreate(&m_LPMContext, &m_InitializationParameters);
}

LPMRenderModule::~LPMRenderModule()
{
    DestroyFfxContext();
    
    delete m_pRootSignature;
    delete m_pPipelineObj;
    delete m_pParameters;
}

void LPMRenderModule::DestroyFfxContext()
{
    // Flush anything out of the pipes before destroying the context
    GetDevice()->FlushAllCommandQueues();

    // Destroy the LPM context
    FfxErrorCode errorCode = ffxLpmContextDestroy(&m_LPMContext);
    CauldronAssert(ASSERT_CRITICAL, errorCode == FFX_OK, L"Errors occurred while destroying the FfxLpmContext.");

    // Destroy the FidelityFX interface memory
    if (m_InitializationParameters.backendInterface.scratchBuffer != nullptr)
    {
        free(m_InitializationParameters.backendInterface.scratchBuffer);
        m_InitializationParameters.backendInterface.scratchBuffer = nullptr;
    }
}

void LPMRenderModule::TextureLoadComplete(const std::vector<const Texture*>& textureList, void*)
{
    m_pTexture = textureList[0];

    ShaderBindStage shaderStage = ShaderBindStage::Pixel;
    // Create root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddStaticSamplers(0, shaderStage, 1, &m_LinearSamplerDesc);
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1);

    m_pRootSignature = RootSignature::CreateRootSignature(L"LPM_FullscreenPS_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pRootSignature);

    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"fullscreen.hlsl", L"FullscreenVS", ShaderModel::SM6_0, nullptr));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"copytexture.hlsl", L"CopyTexturePS", ShaderModel::SM6_0, nullptr));

    // Setup remaining information and build
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
    psoDesc.AddRasterFormats(m_pRenderTarget->GetFormat());

    m_pPipelineObj = PipelineObject::CreatePipelineObject(L"LPM_FullscreenPS_PipelineObj", psoDesc);

    m_pParameters = ParameterSet::CreateParameterSet(m_pRootSignature);

    // Set our texture to the right parameter slot
    m_pParameters->SetTextureSRV(m_pTexture, ViewDimension::Texture2D, 0);

    // That's all we need for now
    SetModuleReady(true);
}

void LPMRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    if (!ModuleReady())
        return;

    // Barrier the color target to render
    Barrier rtBarrier = Barrier::Transition(m_pRenderTarget->GetResource(), 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
        ResourceState::RenderTargetResource);
    ResourceBarrier(pCmdList, 1, &rtBarrier);

    BeginRaster(pCmdList, 1, &m_pRasterView);

    // Bind all the parameters
    m_pParameters->Bind(pCmdList, m_pPipelineObj);

     // Set pipeline and draw
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

    Viewport vp = {0.f, 0.f, resInfo.fDisplayWidth(), resInfo.fDisplayHeight(), 0.f, 1.f};
    SetViewport(pCmdList, &vp);
    Rect scissorRect = {0, 0, resInfo.RenderWidth, resInfo.RenderHeight};
    SetScissorRects(pCmdList, 1, &scissorRect);

    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);
    SetPipelineState(pCmdList, m_pPipelineObj);

    DrawInstanced(pCmdList, 3);

    EndRaster(pCmdList);

    rtBarrier = Barrier::Transition(m_pRenderTarget->GetResource(), 
        ResourceState::RenderTargetResource, 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
    ResourceBarrier(pCmdList, 1, &rtBarrier);

    GPUScopedProfileCapture sampleMarker(pCmdList, L"LPM");
    const HDRMetadata&      displayModeMetadata = GetFramework()->GetSwapChain()->GetHDRMetaData();

    FfxLpmDispatchDescription dispatchParameters = {};
    dispatchParameters.commandList               = SDKWrapper::ffxGetCommandList(pCmdList);
    dispatchParameters.shoulder                  = m_Shoulder;
    dispatchParameters.softGap                   = m_SoftGap;
    dispatchParameters.hdrMax                    = m_HdrMax;
    dispatchParameters.lpmExposure               = m_LpmExposure;
    dispatchParameters.contrast                  = m_Contrast;
    dispatchParameters.shoulderContrast          = m_ShoulderContrast;
    dispatchParameters.saturation[0]             = m_Saturation[0];
    dispatchParameters.saturation[1]             = m_Saturation[1];
    dispatchParameters.saturation[2]             = m_Saturation[2];
    dispatchParameters.crosstalk[0]              = m_Crosstalk[0];
    dispatchParameters.crosstalk[1]              = m_Crosstalk[1];
    dispatchParameters.crosstalk[2]              = m_Crosstalk[2];
    dispatchParameters.colorSpace                = static_cast<FfxLpmColorSpace>(m_ColorSpace);
    dispatchParameters.displayMode               = static_cast<FfxLpmDisplayMode>(m_DisplayMode);
    dispatchParameters.displayRedPrimary[0]      = displayModeMetadata.RedPrimary[0];
    dispatchParameters.displayRedPrimary[1]      = displayModeMetadata.RedPrimary[1];
    dispatchParameters.displayGreenPrimary[0]    = displayModeMetadata.GreenPrimary[0];
    dispatchParameters.displayGreenPrimary[1]    = displayModeMetadata.GreenPrimary[1];
    dispatchParameters.displayBluePrimary[0]     = displayModeMetadata.BluePrimary[0];
    dispatchParameters.displayBluePrimary[1]     = displayModeMetadata.BluePrimary[1];
    dispatchParameters.displayWhitePoint[0]      = displayModeMetadata.WhitePoint[0];
    dispatchParameters.displayWhitePoint[1]      = displayModeMetadata.WhitePoint[1];
    dispatchParameters.displayMinLuminance       = displayModeMetadata.MinLuminance;
    dispatchParameters.displayMaxLuminance       = displayModeMetadata.MaxLuminance;

    // All cauldron resources come into a render module in a generic read state (ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource)
    dispatchParameters.inputColor  = SDKWrapper::ffxGetResource(m_pInputColor->GetResource(), L"Lpm_InputColor", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParameters.outputColor = SDKWrapper::ffxGetResource(m_pOutputColor->GetResource(), L"Lpm_OutputColor", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    FfxErrorCode errorCode = ffxLpmContextDispatch(&m_LPMContext, &dispatchParameters);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);
}

void LPMRenderModule::OnResize(const ResolutionInfo& resInfo)
{
    if (!ModuleEnabled())
        return;

    // Refresh
    FfxErrorCode errorCode = ffxLpmContextDestroy(&m_LPMContext);
    CauldronAssert(ASSERT_CRITICAL, errorCode == FFX_OK, L"Errors occurred while destroying the FfxLpmContext.");
    errorCode = ffxLpmContextCreate(&m_LPMContext, &m_InitializationParameters);
    CauldronAssert(ASSERT_CRITICAL, errorCode == FFX_OK, L"Couldn't initialize the FidelityFX SDK backend interface.");
}
