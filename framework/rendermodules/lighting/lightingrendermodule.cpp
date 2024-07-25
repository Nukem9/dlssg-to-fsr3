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

#include "lightingRenderModule.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/shadowmapresourcepool.h"
#include "core/contentmanager.h"
#include "core/framework.h"
#include "core/loaders/textureloader.h"
#include "core/scene.h"
#include "core/uimanager.h"

#include "render/parameterset.h"

#include "shaders/surfacerendercommon.h"

using namespace cauldron;
using namespace std::experimental;

constexpr uint32_t g_NumThreadX = 8;
constexpr uint32_t g_NumThreadY = 8;

void LightingRenderModule::Init(const json& initData)
{
    // Init resources
    m_pDiffuseTexture = GetFramework()->GetRenderTexture(L"GBufferAlbedoRT");
    m_pNormalTexture = GetFramework()->GetRenderTexture(L"GBufferNormalRT");
    m_pAoRoughnessMetallicTexture = GetFramework()->GetRenderTexture(L"GBufferAoRoughnessMetallicRT");
    m_pDepthTexture = GetFramework()->GetRenderTexture(L"GBufferDepth");

    // Root Signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1); // scene information
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);  // scene lighting information
    signatureDesc.AddConstantBufferView(2, ShaderBindStage::Compute, 1); // IBL factor
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1); // diffuse
    signatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1); // normal
    signatureDesc.AddTextureSRVSet(2, ShaderBindStage::Compute, 1); // specular roughness
    signatureDesc.AddTextureSRVSet(3, ShaderBindStage::Compute, 1); // depth
    signatureDesc.AddTextureSRVSet(4, ShaderBindStage::Compute, 1); // brdfTexture
    signatureDesc.AddTextureSRVSet(5, ShaderBindStage::Compute, 1); // irradianceCube
    signatureDesc.AddTextureSRVSet(6, ShaderBindStage::Compute, 1); // prefilteredCube
    signatureDesc.AddTextureSRVSet(7, ShaderBindStage::Compute, MAX_SHADOW_MAP_TEXTURES_COUNT); // shadow maps
    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);  // ColorTarget output

    SamplerDesc pointSampler; // default is enough
    pointSampler.Filter = FilterFunc::MinMagMipPoint;
    std::vector<SamplerDesc> samplers;
    samplers.push_back(pointSampler);
    signatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, static_cast<uint32_t>(samplers.size()), samplers.data());

    static bool s_InvertedDepth = GetConfig()->InvertedDepth;

    SamplerDesc comparisonSampler;
    comparisonSampler.Comparison = s_InvertedDepth ? ComparisonFunc::GreaterEqual : ComparisonFunc::LessEqual;
    comparisonSampler.Filter = FilterFunc::ComparisonMinMagLinearMipPoint;
    comparisonSampler.MaxAnisotropy = 1;
    samplers.clear();
    samplers.push_back(comparisonSampler);
    signatureDesc.AddStaticSamplers(1, ShaderBindStage::Compute, static_cast<uint32_t>(samplers.size()), samplers.data());

    // Setup samplers for brdfTexture, irradianceCube and prefilteredCube
    {
        SamplerDesc prefilteredCubeSampler;
        prefilteredCubeSampler.AddressW = AddressMode::Wrap;
        prefilteredCubeSampler.Filter = FilterFunc::MinMagMipLinear;
        prefilteredCubeSampler.MaxAnisotropy = 1;

        samplers.clear();
        samplers.push_back(prefilteredCubeSampler);
        signatureDesc.AddStaticSamplers(2, ShaderBindStage::Compute, static_cast<uint32_t>(samplers.size()), samplers.data());
        signatureDesc.AddStaticSamplers(4, ShaderBindStage::Compute, static_cast<uint32_t>(samplers.size()), samplers.data());


        SamplerDesc irradianceCubeSampler;
        irradianceCubeSampler.Filter = FilterFunc::MinMagMipPoint;
        irradianceCubeSampler.AddressW = AddressMode::Wrap;
        irradianceCubeSampler.Filter = FilterFunc::MinMagMipPoint;
        irradianceCubeSampler.MaxAnisotropy = 1;
        samplers.clear();
        samplers.push_back(irradianceCubeSampler);
        signatureDesc.AddStaticSamplers(3, ShaderBindStage::Compute, static_cast<uint32_t>(samplers.size()), samplers.data());
    }

    m_pRootSignature = RootSignature::CreateRootSignature(L"LightingRenderModule_RootSignature", signatureDesc);

    m_pRenderTarget = GetFramework()->GetColorTargetForCallback(GetName());
    CauldronAssert(ASSERT_CRITICAL, m_pRenderTarget != nullptr, L"Couldn't find or create the render target of PBRLightingRenderModule.");

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pRootSignature);

    DefineList defineList;
    defineList.insert(std::make_pair(L"NUM_THREAD_X", std::to_wstring(g_NumThreadX)));
    defineList.insert(std::make_pair(L"NUM_THREAD_Y", std::to_wstring(g_NumThreadY)));
    defineList.insert(std::make_pair(L"DEF_SSAO", std::to_wstring(1)));

    // Setup the shaders to build on the pipeline object
    std::wstring shaderPath = L"lighting.hlsl";
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"MainCS", ShaderModel::SM6_0, &defineList));

    m_pPipelineObj = PipelineObject::CreatePipelineObject(L"LightingRenderModule_PipelineObj", psoDesc);


    // Create parameter set to bind constant buffer and texture
    m_pParameters = ParameterSet::CreateParameterSet(m_pRootSignature);

    // Update necessary scene frame information
    m_pParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 0);
    m_pParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneLightingInformation), 1);
    m_pParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(LightingCBData), 2);

    m_pParameters->SetTextureSRV(m_pDiffuseTexture,             ViewDimension::Texture2D, 0);
    m_pParameters->SetTextureSRV(m_pNormalTexture,              ViewDimension::Texture2D, 1);
    m_pParameters->SetTextureSRV(m_pAoRoughnessMetallicTexture, ViewDimension::Texture2D, 2);
    m_pParameters->SetTextureSRV(m_pDepthTexture,               ViewDimension::Texture2D, 3);

    m_pParameters->SetTextureUAV(m_pRenderTarget,               ViewDimension::Texture2D, 0);

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
    {
        m_pParameters->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, 7 + i);
    }

    m_IBLFactor = GetConfig()->StartupContent.IBLFactor;
    // Register UI for Tone mapping as part of post processing
    UISection* uiSection = GetUIManager()->RegisterUIElements("Lighting");
    if (uiSection)
    {
        uiSection->RegisterUIElement<UISlider<float>>(
            "IBLFactor", m_IBLFactor, 0.0f, 1.0f,
            [this](float cur, float old) {
                GetScene()->SetIBLFactor(cur);
            });
    }

    // We are now ready for use
    SetModuleReady(true);
}

LightingRenderModule::~LightingRenderModule()
{
    delete m_pRootSignature;
    delete m_pPipelineObj;
    delete m_pParameters;
}

void LightingRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    if(GetScene()->GetBRDFLutTexture())
    {
        m_pParameters->SetTextureSRV(GetScene()->GetBRDFLutTexture(), ViewDimension::Texture2D, 4);
    }
    if(GetScene()->GetIBLTexture(IBLTexture::Irradiance))
    {
        m_pParameters->SetTextureSRV(GetScene()->GetIBLTexture(IBLTexture::Irradiance), ViewDimension::TextureCube, 5);
    }
    if(GetScene()->GetIBLTexture(IBLTexture::Prefiltered))
    {
        m_pParameters->SetTextureSRV(GetScene()->GetIBLTexture(IBLTexture::Prefiltered), ViewDimension::TextureCube, 6);
    }

    if (GetScene()->GetScreenSpaceShadowTexture())
    {
        // Store screenSpaceShadowTexture at index 0 in the shadow maps array
        m_pParameters->SetTextureSRV(GetScene()->GetScreenSpaceShadowTexture(), ViewDimension::Texture2D, 7);
    }
    else
    {
        ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
        if (pShadowMapResourcePool->GetRenderTargetCount() > m_ShadowMapCount)
        {
            CauldronAssert(ASSERT_CRITICAL,
                           pShadowMapResourcePool->GetRenderTargetCount() <= MAX_SHADOW_MAP_TEXTURES_COUNT,
                           L"Lighting Render Module can only support up to %d shadow maps. There are currently %d shadow maps",
                           MAX_SHADOW_MAP_TEXTURES_COUNT,
                           pShadowMapResourcePool->GetRenderTargetCount());
            for (uint32_t i = m_ShadowMapCount; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
            {
                m_pParameters->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, 7 + i);
            }
        }
    }

    GPUScopedProfileCapture sampleMarker(pCmdList, L"Lighting");

    // Render modules expect resources coming in/going out to be in a shader read state
    Barrier barrier = Barrier::Transition(m_pRenderTarget->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess);
    ResourceBarrier(pCmdList, 1, &barrier);

    // Update necessary scene frame information
    BufferAddressInfo sceneBuffers[2];
    sceneBuffers[0] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneInfo()));
    sceneBuffers[1] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneLightingInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneLightInfo()));
    m_pParameters->UpdateRootConstantBuffer(&sceneBuffers[0], 0);
    m_pParameters->UpdateRootConstantBuffer(&sceneBuffers[1], 1);

    // Allocate a dynamic constant buffers and set
    m_LightingConstantData.IBLFactor = GetScene()->GetIBLFactor();
    m_LightingConstantData.SpecularIBLFactor = GetScene()->GetSpecularIBLFactor();
    BufferAddressInfo bufferInfo  = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(LightingCBData), &m_LightingConstantData);

    // Update constant buffers
    m_pParameters->UpdateRootConstantBuffer(&bufferInfo, 2);

    // Bind everything
    m_pParameters->Bind(pCmdList, m_pPipelineObj);
    SetPipelineState(pCmdList, m_pPipelineObj);

    // Scale the work accordingly
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    uint32_t dispatchWidth = 0;
    uint32_t dispatchHeight = 0;
    if (GetFramework()->GetUpscalingState() == UpscalerState::PreUpscale)
    {
        dispatchWidth = resInfo.RenderWidth;
        dispatchHeight = resInfo.RenderHeight;
    }
    else
    {
        dispatchWidth = resInfo.UpscaleWidth;
        dispatchHeight = resInfo.UpscaleHeight;
    }

    const uint32_t numGroupX = DivideRoundingUp(dispatchWidth, g_NumThreadX);
    const uint32_t numGroupY = DivideRoundingUp(dispatchHeight, g_NumThreadY);
    Dispatch(pCmdList, numGroupX, numGroupY, 1);

    // Render modules expect resources coming in/going out to be in a shader read state
    barrier = Barrier::Transition(m_pRenderTarget->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
    ResourceBarrier(pCmdList, 1, &barrier);
}
