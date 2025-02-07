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

#include "tonemappingrendermodule.h"

#include "core/framework.h"
#include "core/scene.h"
#include "core/uimanager.h"
#include "render/profiler.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/rootsignature.h"
#include "render/color_conversion.h"
#include "render/swapchain.h"
#include "render/dynamicresourcepool.h"
#include "render/rasterview.h"


#define FFX_CPU
#include <FidelityFX/host/ffx_spd.h>
#include <FidelityFX/gpu/ffx_core.h>
#include <FidelityFX/gpu/spd/ffx_spd.h>

using namespace cauldron;

constexpr uint32_t g_NumThreadX = 8;
constexpr uint32_t g_NumThreadY = 8;

ToneMappingRenderModule::ToneMappingRenderModule() :
    RenderModule(L"ToneMappingRenderModule")
{
    GetFramework()->SetTonemapper(this);
}

ToneMappingRenderModule::ToneMappingRenderModule(const wchar_t* pName) :
    RenderModule(pName)
{
    GetFramework()->SetTonemapper(this);
}

void ToneMappingRenderModule::Init(const json& InitData)
{
    // Get the proper pre-tone map color target
    m_pRenderTargetIn = GetFramework()->GetRenderTexture(L"HDR11Color");
    CauldronAssert(ASSERT_CRITICAL, m_pRenderTargetIn != nullptr, L"Couldn't find the render target for the tone mapper input");
    
    // Get the proper post tone map color target
    m_pRenderTargetOut = GetFramework()->GetRenderTexture(L"SwapChainProxy");
    CauldronAssert(ASSERT_CRITICAL, m_pRenderTargetOut != nullptr, L"Couldn't find the render target for the tone mapper output");

    m_pDistortionField[0] = GetFramework()->GetRenderTexture(L"DistortionField0");
    m_pDistortionField[1] = GetFramework()->GetRenderTexture(L"DistortionField1");
    m_pDistortionFieldRasterView[0] = GetRasterViewAllocator()->RequestRasterView(m_pDistortionField[0], ViewDimension::Texture2D);
    m_pDistortionFieldRasterView[1] = GetRasterViewAllocator()->RequestRasterView(m_pDistortionField[1], ViewDimension::Texture2D);


    TextureDesc desc = TextureDesc::Tex2D(L"AutomaticExposureSpdAtomicCounter", ResourceFormat::R32_UINT, 1, 1, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pAutomaticExposureSpdAtomicCounter = GetDynamicResourcePool()->CreateRenderTexture(&desc);

    desc = TextureDesc::Tex2D(L"AutomaticExposureMipsShadingChange", ResourceFormat::R16_FLOAT, 80, 45, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pAutomaticExposureMipsShadingChange = GetDynamicResourcePool()->CreateRenderTexture(&desc);

    desc = TextureDesc::Tex2D(L"AutomaticExposureMips5", ResourceFormat::R16_FLOAT, 40, 22, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pAutomaticExposureMips5 = GetDynamicResourcePool()->CreateRenderTexture(&desc);

    desc = TextureDesc::Tex2D(L"AutomaticExposureValue", ResourceFormat::RG32_FLOAT, 1, 1, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pAutomaticExposureValue = GetDynamicResourcePool()->CreateRenderTexture(&desc);

    // Init auto exposure calculation
    // root signature
    // Auto exposure
    uint32_t workGroupOffset[2];
    uint32_t numWorkGroupsAndMips[2];
    uint32_t rectInfo[4] = {0, 0, m_pRenderTargetIn->GetDesc().Width, m_pRenderTargetIn->GetDesc().Height};
    ffxSpdSetup(m_DispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo);

    // Downsample
    m_AutoExposureSpdConstants.numWorkGroups    = numWorkGroupsAndMips[0];
    m_AutoExposureSpdConstants.mips             = numWorkGroupsAndMips[1];
    m_AutoExposureSpdConstants.workGroupOffset[0] = workGroupOffset[0];
    m_AutoExposureSpdConstants.workGroupOffset[1] = workGroupOffset[1];
    m_AutoExposureSpdConstants.renderSize[0]      = rectInfo[2];
    m_AutoExposureSpdConstants.renderSize[1]      = rectInfo[3];

    RootSignatureDesc autoExposureSignatureDesc;
    autoExposureSignatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    autoExposureSignatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);
    autoExposureSignatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);
    autoExposureSignatureDesc.AddTextureUAVSet(1, ShaderBindStage::Compute, 1);
    autoExposureSignatureDesc.AddTextureUAVSet(2, ShaderBindStage::Compute, 1);
    autoExposureSignatureDesc.AddTextureUAVSet(3, ShaderBindStage::Compute, 1);

     // Initialize common resources that aren't pipeline dependent
    m_LinearSamplerDesc.Filter        = FilterFunc::MinMagLinearMipPoint;
    m_LinearSamplerDesc.MaxLOD        = std::numeric_limits<float>::max();
    m_LinearSamplerDesc.MaxAnisotropy = 1;
    ShaderBindStage shaderStage       = ShaderBindStage::Compute;
    autoExposureSignatureDesc.AddStaticSamplers(0, shaderStage, 1, &m_LinearSamplerDesc);

    m_pAutoExposureSpdRootSignature = RootSignature::CreateRootSignature(L"AutoExposureSPDRenderPass_RootSignature", autoExposureSignatureDesc);

    // Setup the pipeline object
    PipelineDesc autoExposurePsoDesc;
    autoExposurePsoDesc.SetRootSignature(m_pAutoExposureSpdRootSignature);

    // Setup the shaders to build on the pipeline object
    std::wstring shaderPath = L"autoexposure.hlsl";
    DefineList   exposureDefineList;

    exposureDefineList.insert(std::make_pair(L"NUM_THREAD_X", std::to_wstring(256)));
    autoExposurePsoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"MainCS", ShaderModel::SM6_0, &exposureDefineList));

    m_pAutoExposureSpdPipelineObj = PipelineObject::CreatePipelineObject(L"AutomaticExposureRenderPass_PipelineObj", autoExposurePsoDesc);

    m_pAutoExposureSpdParameters = ParameterSet::CreateParameterSet(m_pAutoExposureSpdRootSignature);
    m_pAutoExposureSpdParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(AutoExposureSpdConstants), 0);

    // Set our texture to the right parameter slot
    m_pAutoExposureSpdParameters->SetTextureSRV(m_pRenderTargetIn, ViewDimension::Texture2D, 0);
    m_pAutoExposureSpdParameters->SetTextureUAV(m_pAutomaticExposureSpdAtomicCounter, ViewDimension::Texture2D, 0);
    m_pAutoExposureSpdParameters->SetTextureUAV(m_pAutomaticExposureMipsShadingChange, ViewDimension::Texture2D, 1);
    m_pAutoExposureSpdParameters->SetTextureUAV(m_pAutomaticExposureMips5, ViewDimension::Texture2D, 2);
    m_pAutoExposureSpdParameters->SetTextureUAV(m_pAutomaticExposureValue, ViewDimension::Texture2D, 3);

    {
        // Init build distortion field pipeline
        RootSignatureDesc buildDistortionFieldSignatureDesc;
        buildDistortionFieldSignatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
        buildDistortionFieldSignatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);
        m_pBuildDistortionFieldRootSignature = RootSignature::CreateRootSignature(L"BuildDistortionFieldRenderPass_RootSignature", buildDistortionFieldSignatureDesc);

        // Setup the pipeline object
        PipelineDesc buildDistortionFieldPsoDesc;
        buildDistortionFieldPsoDesc.SetRootSignature(m_pBuildDistortionFieldRootSignature);

        // Setup the shaders to build on the pipeline object
        shaderPath = L"builddistortionfield.hlsl";

        DefineList buildDistortionFieldDefineList;
        buildDistortionFieldDefineList.insert(std::make_pair(L"NUM_THREAD_X", std::to_wstring(g_NumThreadX)));
        buildDistortionFieldDefineList.insert(std::make_pair(L"NUM_THREAD_Y", std::to_wstring(g_NumThreadY)));
        buildDistortionFieldPsoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"MainCS", ShaderModel::SM6_0, &buildDistortionFieldDefineList));

        m_pBuildDistortionFieldPipelineObj = PipelineObject::CreatePipelineObject(L"BuildDistortionFieldRenderPass_PipelineObj", buildDistortionFieldPsoDesc);

        m_pBuildDistortionFieldParameters = ParameterSet::CreateParameterSet(m_pBuildDistortionFieldRootSignature);
        m_pBuildDistortionFieldParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(TonemapperCBData), 0);
    }
    // Init tonemapper
    // root signature
    RootSignatureDesc tonemapperSignatureDesc;
    tonemapperSignatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    tonemapperSignatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);
    tonemapperSignatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);
    tonemapperSignatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);

    m_pTonemapperRootSignature = RootSignature::CreateRootSignature(L"ToneMappingRenderPass_RootSignature", tonemapperSignatureDesc);

    // Setup the pipeline object
    PipelineDesc tonemapperPsoDesc;
    tonemapperPsoDesc.SetRootSignature(m_pTonemapperRootSignature);

    // Setup the shaders to build on the pipeline object
    shaderPath = L"tonemapping.hlsl";

    DefineList tonemapperDefineList;
    tonemapperDefineList.insert(std::make_pair(L"NUM_THREAD_X", std::to_wstring(g_NumThreadX)));
    tonemapperDefineList.insert(std::make_pair(L"NUM_THREAD_Y", std::to_wstring(g_NumThreadY)));
    tonemapperPsoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"MainCS", ShaderModel::SM6_0, &tonemapperDefineList));

    m_pTonemapperPipelineObj = PipelineObject::CreatePipelineObject(L"ToneMappingRenderPass_PipelineObj", tonemapperPsoDesc);

    m_pTonemapperParameters = ParameterSet::CreateParameterSet(m_pTonemapperRootSignature);
    m_pTonemapperParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(TonemapperCBData), 0);
    
    // Set our texture to the right parameter slot
    m_pTonemapperParameters->SetTextureSRV(m_pAutomaticExposureValue, ViewDimension::Texture2D, 0);
    m_pTonemapperParameters->SetTextureSRV(m_pRenderTargetIn, ViewDimension::Texture2D, 1);
    m_pTonemapperParameters->SetTextureUAV(m_pRenderTargetOut, ViewDimension::Texture2D, 0);

    // Register UI for Tone mapping as part of post processing
    UISection* uiSection = GetUIManager()->RegisterUIElements("Post Processing");
    if (uiSection)
    {
        std::vector<const char*> comboOptions = { "AMD Tonemapper", "DX11DSK", "Reinhard", "Uncharted2Tonemap", "ACES", "No Tonemapper" };
        uiSection->RegisterUIElement<UICombo>("Tone Mapper", (int32_t&)m_TonemapperConstantData.ToneMapper, std::move(comboOptions));

        m_TonemapperConstantData.Exposure = GetScene()->GetSceneExposure();
        uiSection->RegisterUIElement<UISlider<float>>(
            "Exposure",
            m_TonemapperConstantData.Exposure,
            0.f, 5.f,
            [this](float cur, float old) {
                GetScene()->SetSceneExposure(cur);
            }
        );
        uiSection->RegisterUIElement<UICheckBox>("AutoExposure", (bool&)m_TonemapperConstantData.UseAutoExposure);

        uiSection->RegisterUIElement<UICheckBox>("Lens Distortion Enable", (bool&)m_TonemapperConstantData.LensDistortionEnabled);
        uiSection->RegisterUIElement<UISlider<float>>("Lens Distortion Strength", m_TonemapperConstantData.LensDistortionStrength, -1.f, 1.f, (bool&)m_TonemapperConstantData.LensDistortionEnabled);
        uiSection->RegisterUIElement<UISlider<float>>("Lens Distortion Zoom", m_TonemapperConstantData.LensDistortionZoom, 0.f, 1.f, (bool&)m_TonemapperConstantData.LensDistortionEnabled);
    }

    // We are now ready for use
    SetModuleReady(true);
}

ToneMappingRenderModule::~ToneMappingRenderModule()
{
    delete m_pAutoExposureSpdRootSignature;
    delete m_pAutoExposureSpdPipelineObj;
    delete m_pAutoExposureSpdParameters;

    delete m_pBuildDistortionFieldRootSignature;
    delete m_pBuildDistortionFieldPipelineObj;
    delete m_pBuildDistortionFieldParameters;

    delete m_pTonemapperRootSignature;
    delete m_pTonemapperPipelineObj;
    delete m_pTonemapperParameters;
}

void ToneMappingRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    // If display mode is set to FSHDR_SCRGB or HDR10_SCRGB, the tonemapper will not run
    // the color target for the duration of the frame will be RGBA16_FLOAT (HDR16Color)
    if (GetFramework()->GetSwapChain()->GetSwapChainDisplayMode() == DisplayMode::DISPLAYMODE_FSHDR_SCRGB ||
        GetFramework()->GetSwapChain()->GetSwapChainDisplayMode() == DisplayMode::DISPLAYMODE_HDR10_SCRGB)
    {
        return;
    }

    {
        GPUScopedProfileCapture automaticExposureMarker(pCmdList, L"AutomaticExposure");
        std::array<Barrier, 4u> automaticExposureTransitionBarriers;
        automaticExposureTransitionBarriers[0] = Barrier::Transition(m_pAutomaticExposureSpdAtomicCounter->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::UnorderedAccess);
        automaticExposureTransitionBarriers[1] = Barrier::Transition(m_pAutomaticExposureMipsShadingChange->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::UnorderedAccess);
        automaticExposureTransitionBarriers[2] = Barrier::Transition(m_pAutomaticExposureMips5->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::UnorderedAccess);
        automaticExposureTransitionBarriers[3] = Barrier::Transition(m_pAutomaticExposureValue->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::UnorderedAccess);
        ResourceBarrier(pCmdList, static_cast<uint32_t>(automaticExposureTransitionBarriers.size()), automaticExposureTransitionBarriers.data());

        // Allocate a dynamic constant buffer and set
        BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(AutoExposureSpdConstants), &m_AutoExposureSpdConstants);
        m_pAutoExposureSpdParameters->UpdateRootConstantBuffer(&bufferInfo, 0);

        // bind all the parameters
        m_pAutoExposureSpdParameters->Bind(pCmdList, m_pAutoExposureSpdPipelineObj);

        // Set pipeline and dispatch
        SetPipelineState(pCmdList, m_pAutoExposureSpdPipelineObj);

        Dispatch(pCmdList, m_DispatchThreadGroupCountXY[0], m_DispatchThreadGroupCountXY[1], 1);
    }

    {
        GPUScopedProfileCapture tonemappingMarker(pCmdList, L"ToneMapping");

        // Render modules expect resources coming in/going out to be in a shader read state
        std::array<Barrier, 5u> tonemmaperTransitionBarriers;
        tonemmaperTransitionBarriers[0] = Barrier::Transition(m_pRenderTargetOut->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::UnorderedAccess);
        tonemmaperTransitionBarriers[1] = Barrier::Transition(m_pAutomaticExposureSpdAtomicCounter->GetResource(),
            ResourceState::UnorderedAccess,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
        tonemmaperTransitionBarriers[2] = Barrier::Transition(m_pAutomaticExposureMipsShadingChange->GetResource(),
            ResourceState::UnorderedAccess,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
        tonemmaperTransitionBarriers[3] = Barrier::Transition(m_pAutomaticExposureMips5->GetResource(),
            ResourceState::UnorderedAccess,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
        tonemmaperTransitionBarriers[4] = Barrier::Transition(m_pAutomaticExposureValue->GetResource(),
            ResourceState::UnorderedAccess,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
        ResourceBarrier(pCmdList, static_cast<uint32_t>(tonemmaperTransitionBarriers.size()), tonemmaperTransitionBarriers.data());

        m_TonemapperConstantData.MonitorDisplayMode = GetFramework()->GetSwapChain()->GetSwapChainDisplayMode();
        m_TonemapperConstantData.DisplayMaxLuminance = GetFramework()->GetSwapChain()->GetHDRMetaData().MaxLuminance;

        ResolutionInfo resInfo = GetFramework()->GetResolutionInfo();

        // assume symmetric letterbox
        m_TonemapperConstantData.LetterboxRectBase[0] = (resInfo.DisplayWidth - resInfo.UpscaleWidth) / 2;
        m_TonemapperConstantData.LetterboxRectBase[1] = (resInfo.DisplayHeight - resInfo.UpscaleHeight) / 2;

        m_TonemapperConstantData.LetterboxRectSize[0] = resInfo.UpscaleWidth;
        m_TonemapperConstantData.LetterboxRectSize[1] = resInfo.UpscaleHeight;

        // Scene dependent
        ColorSpace inputColorSpace = ColorSpace_REC709;

        // Display mode dependent
        // Both FSHDR_2084 and HDR10_2084 take rec2020 value
        // Difference is FSHDR needs to be gamut mapped using monitor primaries and then gamut converted to rec2020
        ColorSpace outputColorSpace = ColorSpace_REC2020;
        SetupGamutMapperMatrices(inputColorSpace, outputColorSpace, &m_TonemapperConstantData.ContentToMonitorRecMatrix);

        // Allocate a dynamic constant buffer and set
        BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(TonemapperCBData), &m_TonemapperConstantData);
        m_pTonemapperParameters->UpdateRootConstantBuffer(&bufferInfo, 0);

        // bind all the parameters
        m_pTonemapperParameters->Bind(pCmdList, m_pTonemapperPipelineObj);

        // Set pipeline and dispatch
        SetPipelineState(pCmdList, m_pTonemapperPipelineObj);

        const uint32_t numGroupX = DivideRoundingUp(m_pRenderTargetOut->GetDesc().Width, g_NumThreadX);
        const uint32_t numGroupY = DivideRoundingUp(m_pRenderTargetOut->GetDesc().Height, g_NumThreadY);
        Dispatch(pCmdList, numGroupX, numGroupY, 1);

        // Render modules expect resources coming in/going out to be in a shader read state
        Barrier barrier = Barrier::Transition(m_pRenderTargetOut->GetResource(),
            ResourceState::UnorderedAccess,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
        ResourceBarrier(pCmdList, 1, &barrier);
    }
        
    if (shouldClearRenderTargets)
    {
        GPUScopedProfileCapture distortionFieldMarker(pCmdList, L"Clear Distortion Field");
        std::vector<Barrier> barriers;
        barriers.push_back(Barrier::Transition(m_pDistortionField[0]->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::RenderTargetResource));
        barriers.push_back(Barrier::Transition(m_pDistortionField[1]->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::RenderTargetResource));
        ResourceBarrier(pCmdList, static_cast<uint32_t> (barriers.size()), barriers.data());

        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ClearRenderTarget(pCmdList, &m_pDistortionFieldRasterView[0]->GetResourceView(), clearColor);
        ClearRenderTarget(pCmdList, &m_pDistortionFieldRasterView[1]->GetResourceView(), clearColor);
        shouldClearRenderTargets = false;
            
        barriers.clear();
        barriers.push_back(Barrier::Transition(m_pDistortionField[0]->GetResource(),
            ResourceState::RenderTargetResource,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
        barriers.push_back(Barrier::Transition(m_pDistortionField[1]->GetResource(),
            ResourceState::RenderTargetResource,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
        ResourceBarrier(pCmdList, static_cast<uint32_t> (barriers.size()), barriers.data());
    }

    if (m_TonemapperConstantData.LensDistortionEnabled)
    {
        GPUScopedProfileCapture distortionFieldMarker(pCmdList, L"Build Distortion Field");

        Barrier barrierToWrite = Barrier::Transition(m_pDistortionField[m_curDoubleBufferedTextureIndex]->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::UnorderedAccess);
        ResourceBarrier(pCmdList, 1, &barrierToWrite);

        // Allocate a dynamic constant buffer and set
        BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(TonemapperCBData), &m_TonemapperConstantData);
        m_pBuildDistortionFieldParameters->UpdateRootConstantBuffer(&bufferInfo, 0);

        // bind all the parameters
        m_pBuildDistortionFieldParameters->SetTextureUAV(m_pDistortionField[m_curDoubleBufferedTextureIndex], ViewDimension::Texture2D, 0);
        m_pBuildDistortionFieldParameters->Bind(pCmdList, m_pBuildDistortionFieldPipelineObj);

        // Set pipeline and dispatch
        SetPipelineState(pCmdList, m_pBuildDistortionFieldPipelineObj);

        const uint32_t numGroupX = DivideRoundingUp(m_pDistortionField[m_curDoubleBufferedTextureIndex]->GetDesc().Width, g_NumThreadX);
        const uint32_t numGroupY = DivideRoundingUp(m_pDistortionField[m_curDoubleBufferedTextureIndex]->GetDesc().Height, g_NumThreadY);
        Dispatch(pCmdList, numGroupX, numGroupY, 1);

        Barrier barrierToRead = Barrier::Transition(m_pDistortionField[m_curDoubleBufferedTextureIndex]->GetResource(),
            ResourceState::UnorderedAccess,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
        ResourceBarrier(pCmdList, 1, &barrierToRead);
    }
}

void ToneMappingRenderModule::SetDoubleBufferedTextureIndex(uint32_t textureIndex)
{
    m_curDoubleBufferedTextureIndex = textureIndex;
}

void ToneMappingRenderModule::OnResize(const cauldron::ResolutionInfo& resInfo)
{
    if (!ModuleEnabled())
        return;
    shouldClearRenderTargets = true;
}
