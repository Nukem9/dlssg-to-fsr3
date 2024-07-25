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

#include "animatedtexturesrendermodule.h"
#include "core/framework.h"
#include "core/contentmanager.h"
#include "core/scene.h"
#include "core/loaders/textureloader.h"
#include "render/pipelineobject.h"
#include "render/rasterview.h"
#include "render/parameterset.h"
#include "render/profiler.h"

using namespace cauldron;

struct ConstantBufferData
{
    Mat4  CurrentViewProjection;
    Mat4  PreviousViewProjection;
    Vec2  CameraJitterCompensation;
    float ScrollFactor;
    float RotationFactor;
    int   Mode;
    float pad[3];
};

void AnimatedTexturesRenderModule::Init(const json& initData)
{
    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::VertexAndPixel, 1);
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1);
    SamplerDesc samplerDesc = {FilterFunc::Anisotropic, AddressMode::Wrap, AddressMode::Wrap, AddressMode::Wrap};
    signatureDesc.AddStaticSamplers(0, ShaderBindStage::Pixel, 1, &samplerDesc);

    m_pRootSignature = RootSignature::CreateRootSignature(L"AnimatedTextures_RootSignature", signatureDesc);

    // Fetch needed resources
    m_pRenderTarget    = GetFramework()->GetColorTargetForCallback(GetName());
    m_pMotionVectors   = GetFramework()->GetRenderTexture(L"GBufferMotionVectorRT");
    m_pReactiveMask    = GetFramework()->GetRenderTexture(L"ReactiveMask");
    m_pCompositionMask = GetFramework()->GetRenderTexture(L"TransCompMask");
    m_pDepthTarget     = GetFramework()->GetRenderTexture(L"DepthTarget");
    CauldronAssert(ASSERT_CRITICAL,
                   m_pRenderTarget && m_pMotionVectors && m_pReactiveMask && m_pCompositionMask && m_pDepthTarget,
                   L"Could not get one of the needed resources for AnimatedTexturesRenderModule.");

    m_pRasterViews[0] = GetRasterViewAllocator()->RequestRasterView(m_pRenderTarget, ViewDimension::Texture2D);
    m_pRasterViews[1] = GetRasterViewAllocator()->RequestRasterView(m_pMotionVectors, ViewDimension::Texture2D);
    m_pRasterViews[2] = GetRasterViewAllocator()->RequestRasterView(m_pReactiveMask, ViewDimension::Texture2D);
    m_pRasterViews[3] = GetRasterViewAllocator()->RequestRasterView(m_pCompositionMask, ViewDimension::Texture2D);
    m_pRasterViews[4] = GetRasterViewAllocator()->RequestRasterView(m_pDepthTarget, ViewDimension::Texture2D);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pRootSignature);

    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"AnimatedTexture.hlsl", L"VSMain", ShaderModel::SM6_0, nullptr));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"AnimatedTexture.hlsl", L"PSMain", ShaderModel::SM6_0, nullptr));

    BlendDesc              blendDesc;
    std::vector<BlendDesc> blends;
    blends.push_back(blendDesc);
    blendDesc.RenderTargetWriteMask = (uint32_t)ColorWriteMask::Red + (uint32_t)ColorWriteMask::Green;
    blends.push_back(blendDesc);
    blendDesc.RenderTargetWriteMask = 0;
    blends.push_back(blendDesc);
    blendDesc.RenderTargetWriteMask = (uint32_t)ColorWriteMask::Red;
    blends.push_back(blendDesc);
    psoDesc.AddBlendStates(blends, false, true);

    // Setup remaining information and build
    DepthDesc depthDesc{};
    depthDesc.DepthEnable = true;
    depthDesc.DepthFunc = ComparisonFunc::LessEqual;
    psoDesc.AddDepthState(&depthDesc);  // Use defaults

    RasterDesc rasterDesc{};
    rasterDesc.CullingMode = CullMode::None;
    psoDesc.AddRasterStateDescription(&rasterDesc);
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
    psoDesc.AddRasterFormats({m_pRenderTarget->GetFormat(), m_pMotionVectors->GetFormat(), m_pReactiveMask->GetFormat(), m_pCompositionMask->GetFormat()},
                             ResourceFormat::D32_FLOAT);

    m_pPipelineObj = PipelineObject::CreatePipelineObject(L"AnimatedTextures_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pParameters = ParameterSet::CreateParameterSet(m_pRootSignature);
    m_pParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(ConstantBufferData), 0);

    // Load the texture data from which to create the texture
    std::function<void(const std::vector<const Texture*>&, void*)> CompletionCallback =
        [this](const std::vector<const Texture*>& textures, void* additionalParams) { this->TextureLoadComplete(textures, additionalParams); };

    std::vector<TextureLoadInfo> texturesToLoad;
    texturesToLoad.push_back(TextureLoadInfo(L"..\\media\\Textures\\AnimatedTextures\\lion.jpg"));
    texturesToLoad.push_back(TextureLoadInfo(L"..\\media\\Textures\\AnimatedTextures\\checkerboard.dds"));
    texturesToLoad.push_back(TextureLoadInfo(L"..\\media\\Textures\\AnimatedTextures\\composition_text.dds"));
    GetContentManager()->LoadTextures(texturesToLoad, CompletionCallback);

    // Register UI
    m_pUISection = GetUIManager()->RegisterUIElements("Animated Textures");
    m_pUISection->RegisterUIElement<UISlider<float>>("Animation Speed", m_Speed, 0.0f, 3.0f);
}

AnimatedTexturesRenderModule::~AnimatedTexturesRenderModule()
{
    delete m_pRootSignature;
    delete m_pPipelineObj;
    delete m_pParameters;
}

void AnimatedTexturesRenderModule::TextureLoadComplete(const std::vector<const Texture*>& textureList, void*)
{
    // Only the one texture
    m_Textures = textureList;

    // We are now ready for use
    SetModuleReady(true);
}

void AnimatedTexturesRenderModule::Execute(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"AnimatedTextures RM");

    m_scrollFactor += (float)deltaTime * 1.0f * m_Speed;
    m_rotationFactor += (float)deltaTime * 2.0f * m_Speed;
    m_flipTimer += (float)deltaTime * 1.0f;

    if (m_scrollFactor > 10.0f)
        m_scrollFactor -= 10.0f;

    const float twoPI = 6.283185307179586476925286766559f;

    if (m_rotationFactor > twoPI)
        m_rotationFactor -= twoPI;

    size_t textureIndex = std::min((size_t)floorf(m_flipTimer * 0.33333f), m_Textures.size() - 1);
    if (m_flipTimer > 9.0f)
        m_flipTimer = 0.0f;

    const auto&        camInfo = GetScene()->GetSceneInfo().CameraInfo;
    auto               cam     = GetScene()->GetCurrentCamera();
    ConstantBufferData cbData;
    cbData.CurrentViewProjection    = camInfo.ViewProjectionMatrix;
    cbData.PreviousViewProjection   = camInfo.PrevViewProjectionMatrix;
    Vec4 jitterComp                 = cam->GetPrevProjectionJittered().getCol2() - cam->GetProjectionJittered().getCol2();
    cbData.CameraJitterCompensation = Vec2(jitterComp.getX(), jitterComp.getY());
    cbData.ScrollFactor             = m_scrollFactor;
    cbData.RotationFactor           = m_rotationFactor;
    cbData.Mode                     = (int)textureIndex;

    // Set our texture to the right parameter slot
    m_pParameters->SetTextureSRV(m_Textures[textureIndex], ViewDimension::Texture2D, 0);

    BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(cbData), &cbData);
    m_pParameters->UpdateRootConstantBuffer(&bufferInfo, 0);

    // Render modules expect resources coming in/going out to be in a shader read state
    std::vector<Barrier> barriers{};
    barriers.push_back(Barrier::Transition(
        m_pRenderTarget->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::RenderTargetResource));
    barriers.push_back(Barrier::Transition(
        m_pMotionVectors->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::RenderTargetResource));
    barriers.push_back(Barrier::Transition(
        m_pReactiveMask->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::RenderTargetResource));
    barriers.push_back(Barrier::Transition(
        m_pCompositionMask->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::RenderTargetResource));
    barriers.push_back(Barrier::Transition(
        m_pDepthTarget->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::DepthRead));
    ResourceBarrier(pCmdList, (uint32_t)barriers.size(), barriers.data());
    barriers.clear();

    BeginRaster(pCmdList, 4, m_pRasterViews.data(), m_pRasterViews[4]);

    UpscalerState         upscaleState = GetFramework()->GetUpscalingState();
    const ResolutionInfo& resInfo      = GetFramework()->GetResolutionInfo();

    uint32_t width, height;
    if (upscaleState == UpscalerState::None || upscaleState == UpscalerState::PostUpscale)
    {
        width  = resInfo.UpscaleWidth;
        height = resInfo.UpscaleHeight;
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

    // Bind all parameters
    m_pParameters->Bind(pCmdList, m_pPipelineObj);

    // Set pipeline and draw
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleStrip);
    SetPipelineState(pCmdList, m_pPipelineObj);

    DrawInstanced(pCmdList, 4, 2, 0, 0);

    EndRaster(pCmdList);

    // Render modules expect resources coming in/going out to be in a shader read state
    barriers.push_back(Barrier::Transition(
        m_pRenderTarget->GetResource(), ResourceState::RenderTargetResource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(
        m_pMotionVectors->GetResource(), ResourceState::RenderTargetResource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(
        m_pReactiveMask->GetResource(), ResourceState::RenderTargetResource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(
        m_pCompositionMask->GetResource(), ResourceState::RenderTargetResource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(
        m_pDepthTarget->GetResource(), ResourceState::DepthRead, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    ResourceBarrier(pCmdList, (uint32_t)barriers.size(), barriers.data());
}
