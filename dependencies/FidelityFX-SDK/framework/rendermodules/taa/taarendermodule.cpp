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

#include "taarendermodule.h"

#include "core/framework.h"
#include "core/scene.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rootsignature.h"
#include "shaders/taacommon.h"

using namespace cauldron;

// Used in a few places
static uint32_t s_Seed;

void TAARenderModule::Init(const json& initData)
{
    CauldronAssert(
        ASSERT_CRITICAL, !GetFramework()->GetConfig()->MotionVectorGeneration.empty(), L"Error : TAARendermodule requires MotionVectorGeneration be set");

    m_pColorBuffer = GetFramework()->GetColorTargetForCallback(GetName());
    m_pDepthBuffer = GetFramework()->GetRenderTexture(L"DepthTarget");
    m_pHistoryBuffer = GetFramework()->GetRenderTexture(L"TAAHistoryBufferTarget");
    m_pVelocityBuffer = GetFramework()->GetRenderTexture(L"GBufferMotionVectorRT");
    m_pTAAOutputBuffer = GetFramework()->GetRenderTexture(L"TAAOutputBufferTarget");

    InitTaa();
    InitPost();

    // UI
    // Register UI for Tone mapping as part of post processing
    m_UISection = GetUIManager()->RegisterUIElements("TAA");
    m_UISection->RegisterUIElement<UICheckBox>("Enable TAA", m_bEnableTaa);

    // We are now ready for use
    SetModuleReady(true);
}

TAARenderModule::~TAARenderModule()
{
    delete m_pTAARootSignature;
    delete m_pTAAParameters;
    delete m_pFirstPipelineObj;
    delete m_pTAAPipelineObj;
    delete m_pPostPipelineObj;
    delete m_pPostRootSignature;
    delete m_pPostParameters;
}

void TAARenderModule::Execute(double deltaTime, cauldron::CommandList* pCmdList)
{
    if (!m_bEnableTaa)
    {
        // Disable Jitter
        //GetScene()->GetCurrentCamera()->SetJitterValues({0, 0});
        

        m_bFirst = true;
        return;
    }
    /*else
    {
        Vec2 jitterValues = CalculateJitterOffsets(m_pTAAOutputBuffer->GetDesc().Width, m_pTAAOutputBuffer->GetDesc().Height, s_Seed);
        GetScene()->GetCurrentCamera()->SetJitterValues(jitterValues);
    }*/

    GPUScopedProfileCapture tonemappingMarker(pCmdList, L"TAA");

    // Render modules expect resources coming in/going out to be in a shader read state
    {
        Barrier barrier;
        barrier = Barrier::Transition(
            m_pTAAOutputBuffer->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess);
        ResourceBarrier(pCmdList, 1, &barrier);
    }

    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    TAACBData taaCB {};
    taaCB.RenderWidth = resInfo.RenderWidth;
    taaCB.RenderHeight = resInfo.RenderHeight;
    taaCB.DisplayWidth = resInfo.DisplayWidth;
    taaCB.DisplayHeight = resInfo.DisplayHeight;

    // Update taa CB info
    BufferAddressInfo upscaleInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(TAACBData), &taaCB);
    m_pTAAParameters->UpdateRootConstantBuffer(&upscaleInfo, 0);

    // bind all the parameters
    m_pTAAParameters->Bind(pCmdList, m_pFirstPipelineObj);

    // Set pipeline and dispatch
    if (m_bFirst)
    {
        SetPipelineState(pCmdList, m_pFirstPipelineObj);
        m_bFirst = false;
    }
    else
    {
        SetPipelineState(pCmdList, m_pTAAPipelineObj);
    }


    uint32_t ThreadGroupCountX = DivideRoundingUp(taaCB.RenderWidth, 16);
    uint32_t ThreadGroupCountY = DivideRoundingUp(taaCB.RenderHeight, 16);
    Dispatch(pCmdList, ThreadGroupCountX, ThreadGroupCountY, 1);


    GPUScopedProfileCapture sharpeningMarker(pCmdList, L"TAA Sharpening Pass");
    // Sharpen Pass
    {
        Barrier barrier[] = {
            Barrier::Transition(
                m_pTAAOutputBuffer->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource),
            Barrier::Transition(
                m_pColorBuffer->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess),
            Barrier::Transition(
                m_pHistoryBuffer->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess)
        };
        ResourceBarrier(pCmdList, 3, barrier);
    }

    // bind all the parameters
    m_pPostParameters->Bind(pCmdList, m_pPostPipelineObj);
    SetPipelineState(pCmdList, m_pPostPipelineObj);


    ThreadGroupCountX = DivideRoundingUp(taaCB.RenderWidth, 8);
    ThreadGroupCountY = DivideRoundingUp(taaCB.RenderHeight, 8);
    Dispatch(pCmdList, ThreadGroupCountX, ThreadGroupCountY, 1);

    {
        Barrier barrier[] = {
            Barrier::Transition(m_pColorBuffer->GetResource(),
                                ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource),
            Barrier::Transition(m_pHistoryBuffer->GetResource(),
                                ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource)};
        ResourceBarrier(pCmdList, 2, barrier);
    }
}

void TAARenderModule::EnableModule(bool enabled)
{
    // Set it enabled/disabled
    SetModuleEnabled(enabled);

    if (enabled)
    {
        // Set the jitter callback to use 
        CameraJitterCallback jitterCallback = [this](Vec2& values) {
                values = CalculateJitterOffsets(m_pTAAOutputBuffer->GetDesc().Width, m_pTAAOutputBuffer->GetDesc().Height, s_Seed);
            };
        CameraComponent::SetJitterCallbackFunc(jitterCallback);

        /*Vec2 jitterValues = CalculateJitterOffsets(m_pTAAOutputBuffer->GetDesc().Width, m_pTAAOutputBuffer->GetDesc().Height, s_Seed);
        GetScene()->GetCurrentCamera()->SetJitterValues(jitterValues);*/
        m_bFirst = true;
    }
    else
    {
        CameraComponent::SetJitterCallbackFunc(nullptr);
    }

    m_UISection->Show(enabled);
}

void TAARenderModule::OnResize(const ResolutionInfo& resInfo)
{
    m_bFirst = true;
}

void TAARenderModule::InitTaa()
{
    // root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);  // ColorBuffer
    signatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);  // DepthBuffer
    signatureDesc.AddTextureSRVSet(2, ShaderBindStage::Compute, 1);  // HistoryBuffer
    signatureDesc.AddTextureSRVSet(3, ShaderBindStage::Compute, 1);  // VelocityBuffer
    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);  // TaaOutputBuffer

    SamplerDesc pointSampler;
    pointSampler.Filter = FilterFunc::MinMagMipPoint;
    pointSampler.AddressW = AddressMode::Wrap;

    SamplerDesc linearSampler;
    linearSampler.AddressW = AddressMode::Wrap;

    signatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, 1, &pointSampler);
    signatureDesc.AddStaticSamplers(1, ShaderBindStage::Compute, 1, &linearSampler);


    // CBuffer for Render resolution
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);

    m_pTAARootSignature = RootSignature::CreateRootSignature(L"TAARenderModule_RootSignature", signatureDesc);

    // First TAA Pass
    {
        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pTAARootSignature);

        // Setup the shaders to build on the pipeline object
        std::wstring shaderPath = L"taa.hlsl";
        DefineList   defineList;

        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"FirstCS", ShaderModel::SM6_0, &defineList));

        m_pFirstPipelineObj = PipelineObject::CreatePipelineObject(L"TAAFirstRenderPass_PipelineObj", psoDesc);
    }

    // Main TAA Pass
    {
        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pTAARootSignature);

        // Setup the shaders to build on the pipeline object
        std::wstring shaderPath = L"taa.hlsl";
        DefineList   defineList;

        bool invertedDepth = GetFramework()->GetConfig()->InvertedDepth;
        if (invertedDepth)
        {
            defineList.insert(std::make_pair(L"INVERTED_DEPTH", L"1"));
        }
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"MainCS", ShaderModel::SM6_0, &defineList));

        m_pTAAPipelineObj = PipelineObject::CreatePipelineObject(L"TAAMainRenderPass_PipelineObj", psoDesc);
    }

    m_pTAAParameters = ParameterSet::CreateParameterSet(m_pTAARootSignature);

    // Set our texture to the right parameter slot
    m_pTAAParameters->SetTextureSRV(m_pColorBuffer, ViewDimension::Texture2D, 0);
    m_pTAAParameters->SetTextureSRV(m_pDepthBuffer, ViewDimension::Texture2D, 1);
    m_pTAAParameters->SetTextureSRV(m_pHistoryBuffer, ViewDimension::Texture2D, 2);
    m_pTAAParameters->SetTextureSRV(m_pVelocityBuffer, ViewDimension::Texture2D, 3);
    m_pTAAParameters->SetTextureUAV(m_pTAAOutputBuffer, ViewDimension::Texture2D, 0);
    m_pTAAParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(TAACBData), 0);
}

void TAARenderModule::InitPost()
{
    // root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);  // TaaOutputBuffer
    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);  // ColorBuffer
    signatureDesc.AddTextureUAVSet(1, ShaderBindStage::Compute, 1);  // HistoryBuffer

    m_pPostRootSignature = RootSignature::CreateRootSignature(L"TAARenderModule_Post_RootSignature", signatureDesc);

    // CS Pass
    {
        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pPostRootSignature);

        // Setup the shaders to build on the pipeline object
        std::wstring shaderPath = L"taaPost.hlsl";
        DefineList   defineList;
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"PostCS", ShaderModel::SM6_0, &defineList));

        m_pPostPipelineObj = PipelineObject::CreatePipelineObject(L"TAARenderPass_Post_PipelineObj", psoDesc);
    }

    m_pPostParameters = ParameterSet::CreateParameterSet(m_pPostRootSignature);

    m_pPostParameters->SetTextureSRV(m_pTAAOutputBuffer, ViewDimension::Texture2D, 0);
    m_pPostParameters->SetTextureUAV(m_pColorBuffer,     ViewDimension::Texture2D, 0);
    m_pPostParameters->SetTextureUAV(m_pHistoryBuffer,   ViewDimension::Texture2D, 1);
}


Vec2 TAARenderModule::CalculateJitterOffsets(uint32_t width, uint32_t height, uint32_t& sampleIndex)
{
    static const auto CalculateHaltonNumber = [](uint32_t index, uint32_t base) {
        float f = 1.0f, result = 0.0f;

        for (uint32_t i = index; i > 0;)
        {
            f /= static_cast<float>(base);
            result = result + f * static_cast<float>(i % base);
            i      = static_cast<uint32_t>(floorf(static_cast<float>(i) / static_cast<float>(base)));
        }

        return result;
    };

    sampleIndex = (sampleIndex + 1) % 16;  // 16x TAA

    float jitterX = CalculateHaltonNumber(sampleIndex + 1, 2) - 0.5f;
    float jitterY = CalculateHaltonNumber(sampleIndex + 1, 3) - 0.5f;

    jitterX /= static_cast<float>(width);
    jitterY /= static_cast<float>(height);
    return Vec2(jitterX, jitterY);
}
