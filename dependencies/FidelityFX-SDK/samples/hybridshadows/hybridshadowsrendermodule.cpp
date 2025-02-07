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

#include "hybridshadowsrendermodule.h"

#include "core/backend_interface.h"
#include "core/framework.h"
#include "core/uimanager.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/renderdefines.h"
#include "../thirdparty/samplercpp/samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp.cpp"

#include "core/loaders/textureloader.h"
#include "render/dynamicresourcepool.h"
#include "render/profiler.h"
#include "render/indirectworkload.h"

#include "core/scene.h"
#include "core/contentmanager.h"

using namespace std;
using namespace cauldron;

static const Texture* CreateBlueNoiseTexture()
{
    byte blueNoise[128][128][4] = {};

    for (int x = 0; x < 128; ++x)
    {
        for (int y = 0; y < 128; ++y)
        {
            float const f0 = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(x, y, 0, 0);
            float const f1 = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(x, y, 0, 1);
            float const f2 = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(x, y, 0, 2);
            float const f3 = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(x, y, 0, 3);

            blueNoise[x][y][0] = static_cast<byte>(f0 * UCHAR_MAX);
            blueNoise[x][y][1] = static_cast<byte>(f1 * UCHAR_MAX);
            blueNoise[x][y][2] = static_cast<byte>(f2 * UCHAR_MAX);
            blueNoise[x][y][3] = static_cast<byte>(f3 * UCHAR_MAX);
        }
    }

    const TextureDesc texDesc = TextureDesc::Tex2D(L"BlueNoise Texture", ResourceFormat::RGBA8_UNORM, 128, 128, 1, 1);

    MemTextureDataBlock* pDataBlock = new MemTextureDataBlock(reinterpret_cast<char*>(blueNoise));

    Texture* pBlueNoiseTexture = Texture::CreateContentTexture(&texDesc);
    CauldronAssert(ASSERT_CRITICAL, pBlueNoiseTexture != nullptr, L"Could not create the texture %ls", texDesc.Name.c_str());

    pBlueNoiseTexture->CopyData(pDataBlock);

    return std::move(pBlueNoiseTexture);
}
static Vec3 CreateTangentVector(Vec3 normal)
{
    Vec3 up = abs(normal.getZ()) < 0.99999f ? Vec3(0.0, 0.0, 1.0) : Vec3(1.0, 0.0, 0.0);

    return normalize(cross(up, normal));
}
static float ComputeSunSizeLightSpace(const Vec3& lightDirection, float sunSize, const Mat4& lightViewMatrix)
{
    const Vec3 coneVec = normalize(lightDirection) + CreateTangentVector(lightDirection) * sunSize;
    const Vec3 lightSpaceConeVec = (lightViewMatrix * Vec4(coneVec, 0)).getXYZ();

    float length = math::length(Vec2(lightSpaceConeVec.getX(), lightSpaceConeVec.getY())) / lightSpaceConeVec.getZ();

    return length;
}

// Tile size is (8x4)
constexpr uint32_t k_tileSizeX = 8;
constexpr uint32_t k_tileSizeY = 4;
static constexpr float AMD_PI = 3.1415926535897932384626433832795f;
static constexpr float GOLDEN_RATIO = 1.6180339887f;

void HybridShadowsRenderModule::CreateCopyDepthPipeline()
{
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

    // Create resource
    TextureDesc copyDepthTextureDesc =
        TextureDesc::Tex2D(L"FidelityFXShadowDenoiser_CopyDepth", ResourceFormat::R32_FLOAT, resInfo.RenderWidth, resInfo.RenderHeight, 1, 1, ResourceFlags::AllowUnorderedAccess);
    m_pCopyDepth = GetDynamicResourcePool()->CreateTexture(&copyDepthTextureDesc, ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource,
        [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
            desc.Width  = renderingWidth;
            desc.Height = renderingHeight;
        });


    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);

    m_pCopyDepthRootSignature = RootSignature::CreateRootSignature(L"CopyDepth_RootSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pCopyDepthRootSignature);

    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"copydepth.hlsl", L"main", ShaderModel::SM6_0, nullptr));

    m_pCopyDepthPipelineObj = PipelineObject::CreatePipelineObject(L"CopyDepth_PipelineObj", psoDesc);

    // Create parameter set to bind constant buffer and texture
    m_pCopyDepthParameters = ParameterSet::CreateParameterSet(m_pCopyDepthRootSignature);
    m_pCopyDepthParameters->SetTextureSRV(m_pDepthTarget, ViewDimension::Texture2D, 0);
    m_pCopyDepthParameters->SetTextureUAV(m_pCopyDepth, ViewDimension::Texture2D, 0);
}


void HybridShadowsRenderModule::CreateResources()
{
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    const uint32_t xTiles    = DivideRoundingUp(resInfo.RenderWidth, k_tileSizeX);
    const uint32_t yTiles    = DivideRoundingUp(resInfo.RenderHeight, k_tileSizeY);
    const uint32_t tileCount = xTiles * yTiles;
    const size_t   tileSize  = sizeof(uint32_t) * 4;

    BufferDesc workQueueCountDesc{};
    workQueueCountDesc.Type   = BufferType::Data;
    workQueueCountDesc.Flags  = static_cast<ResourceFlags>(ResourceFlags::AllowUnorderedAccess | ResourceFlags::AllowIndirect);
    workQueueCountDesc.Size   = sizeof(uint32_t) * 3;
    workQueueCountDesc.Stride = sizeof(uint32_t);
    workQueueCountDesc.Name   = L"FidelityFXClassifier_WorkQueueCount";
    m_pWorkQueueCount         = GetDynamicResourcePool()->CreateBuffer(&workQueueCountDesc, ResourceState::CopyDest);

    BufferDesc workQueueDesc{};
    workQueueDesc.Type   = BufferType::Data;
    workQueueDesc.Flags  = ResourceFlags::AllowUnorderedAccess;
    workQueueDesc.Size   = tileSize * tileCount;
    workQueueDesc.Stride = tileSize;
    workQueueDesc.Name   = L"FidelityFXClassifier_WorkQueue";
    m_pWorkQueue         = GetDynamicResourcePool()->CreateBuffer(
        &workQueueDesc,
        ResourceState::UnorderedAccess,
        [](BufferDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
            const uint32_t xTiles    = DivideRoundingUp(renderingWidth,  k_tileSizeX);
            const uint32_t yTiles    = DivideRoundingUp(renderingHeight, k_tileSizeY);
            const uint32_t tileCount = xTiles * yTiles;
            const size_t   tileSize  = sizeof(uint32_t) * 4;

            desc.Size = tileSize * tileCount;
        });

    // RayHitTexture
    TextureDesc rayHitTextureDesc =
        TextureDesc::Tex2D(
        L"FidelityFXClassifier_RayHitTexture",
        ResourceFormat::R32_UINT,
        xTiles,
        yTiles,
        1, 1, ResourceFlags::AllowUnorderedAccess);

    m_pRayHitTexture  = GetDynamicResourcePool()->CreateTexture(
        &rayHitTextureDesc,
        ResourceState::UnorderedAccess,
        [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
            desc.Width  = DivideRoundingUp(renderingWidth,  k_tileSizeX);
            desc.Height = DivideRoundingUp(renderingHeight, k_tileSizeY);
        });

    // ShadowMaskOutput
    TextureDesc shadowMaskOutputDesc = TextureDesc::Tex2D(L"FidelityFX_HybridShadows_ShadowMaskOutput",
                                                          ResourceFormat::RGBA8_UNORM,
                                                          resInfo.RenderWidth,
                                                          resInfo.RenderHeight,
                                                          1,
                                                          1,
                                                          ResourceFlags::AllowUnorderedAccess);
    m_pShadowMaskOutput = GetDynamicResourcePool()->CreateTexture(
        &shadowMaskOutputDesc,
        ResourceState::UnorderedAccess,
        [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
            desc.Width  = renderingWidth;
            desc.Height = renderingHeight;
        });

    GetScene()->SetScreenSpaceShadowTexture(m_pShadowMaskOutput);
}

void HybridShadowsRenderModule::CreateDebugTilesPipeline()
{
    // Root Signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);          // CB
    signatureDesc.AddBufferSRVSet(0, ShaderBindStage::Compute, 1);                // WorkTiles
    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);               // ColorOutput
    m_pDebugTilesRootSignature = RootSignature::CreateRootSignature(L"FidelityFX_HybridShadows_DebugTilesSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pDebugTilesRootSignature);
    std::wstring shaderPath = L"debugtiles.hlsl";
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"MainCS", ShaderModel::SM6_5, nullptr));

    std::vector<const wchar_t*> pAdditionalParameters;
    pAdditionalParameters.push_back(L"-enable-16bit-types");
    m_pDebugTilesPipelineObj = PipelineObject::CreatePipelineObject(L"FidelityFX_HybridShadows_DebugTilesPipelineObj", psoDesc, &pAdditionalParameters);

    // Create parameter set to bind constant buffer and texture
    m_pDebugTilesParameters = ParameterSet::CreateParameterSet(m_pDebugTilesRootSignature);

    // Update necessary scene frame information
    m_pDebugTilesParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(DebugTilesConstantBuffer), 0);
    m_pDebugTilesParameters->SetBufferSRV(m_pWorkQueue, 0);
    m_pDebugTilesParameters->SetTextureUAV(m_pColorOutput, ViewDimension::Texture2D, 0);
}

void HybridShadowsRenderModule::CreateRayTracingPipelines()
{
    m_pBlueNoise = CreateBlueNoiseTexture();

    // Root Signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);          // CB
    signatureDesc.AddBufferSRVSet(0, ShaderBindStage::Compute, 1);                // WorkTiles
    signatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);               // Depth
    signatureDesc.AddTextureSRVSet(2, ShaderBindStage::Compute, 1);               // Normals
    signatureDesc.AddTextureSRVSet(3, ShaderBindStage::Compute, 1);               // BlueNoise
    signatureDesc.AddRTAccelerationStructureSet(4, ShaderBindStage::Compute, 1);  // AccellerationStructure
    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);               // rayHit
    m_pRayTracingRootSignature = RootSignature::CreateRootSignature(L"FidelityFX_HybridShadows_RayTracingSignature", signatureDesc);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pRayTracingRootSignature);
    std::wstring shaderPath = L"traceshadows.hlsl";
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"TraceOpaqueOnly", ShaderModel::SM6_5, nullptr));

    std::vector<const wchar_t*> pAdditionalParameters;
    pAdditionalParameters.push_back(L"-enable-16bit-types");
    m_pRayTracingPipelineObj = PipelineObject::CreatePipelineObject(L"FidelityFX_HybridShadows_RayTracingPipelineObj", psoDesc, &pAdditionalParameters);

    // Create parameter set to bind constant buffer and texture
    m_pRayTracingParameters = ParameterSet::CreateParameterSet(m_pRayTracingRootSignature);

    // Update necessary scene frame information
    m_pRayTracingParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(RTConstantBuffer), 0);
    m_pRayTracingParameters->SetBufferSRV(m_pWorkQueue, 0);
    m_pRayTracingParameters->SetTextureSRV(m_pDepthTarget, ViewDimension::Texture2D, 1);
    m_pRayTracingParameters->SetTextureSRV(m_pNormalTarget, ViewDimension::Texture2D, 2);
    m_pRayTracingParameters->SetTextureSRV(m_pBlueNoise, ViewDimension::Texture2D, 3);  // BlueNoise
    m_pRayTracingParameters->SetTextureUAV(m_pRayHitTexture, ViewDimension::Texture2D, 0);

    // Resolve raytracing pass
    {
        RootSignatureDesc signatureDesc;
        signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);  // rayHit
        signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);  // ColorOutput
        m_pResolveRayTracingRootSignature = RootSignature::CreateRootSignature(L"FidelityFX_HybridShadows_ResolveRayTracingSignature", signatureDesc);

        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pResolveRayTracingRootSignature);
        std::wstring shaderPath = L"resolveraytracing.hlsl";
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"main", ShaderModel::SM6_5, nullptr));

        std::vector<const wchar_t*> pAdditionalParameters;
        pAdditionalParameters.push_back(L"-enable-16bit-types");
        m_pResolveRayTracingPipelineObj =
            PipelineObject::CreatePipelineObject(L"FidelityFX_HybridShadows_ResolveRayTracingPipelineObj", psoDesc, &pAdditionalParameters);

        m_pResolveRayTracingParameters = ParameterSet::CreateParameterSet(m_pResolveRayTracingRootSignature);
        m_pResolveRayTracingParameters->SetTextureSRV(m_pRayHitTexture, ViewDimension::Texture2D, 0);
        m_pResolveRayTracingParameters->SetTextureUAV(m_pShadowMaskOutput, ViewDimension::Texture2D, 0);
    }
}

void HybridShadowsRenderModule::CreateDebugRayTracingPipeline()
{
    // Root Signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1); // screenSpace RT texture
    signatureDesc.AddTextureUAVSet(1, ShaderBindStage::Compute, 1);  // color
    m_pDebugRayTracingRootSignature = RootSignature::CreateRootSignature(L"FidelityFX_HybridShadows_DebugRayTracingSignature", signatureDesc);

    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pDebugRayTracingRootSignature);
    std::wstring shaderPath = L"copydebugraytracing.hlsl";
    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"main", ShaderModel::SM6_0, nullptr));

    m_pDebugRayTracingPipelineObj =
        PipelineObject::CreatePipelineObject(L"FidelityFX_HybridShadows_DebugRayTracingPipelineObj", psoDesc, nullptr);

    m_pDebugRayTracingParameters = ParameterSet::CreateParameterSet(m_pDebugRayTracingRootSignature);
    m_pDebugRayTracingParameters->SetTextureUAV(m_pShadowMaskOutput, ViewDimension::Texture2D, 0);
    m_pDebugRayTracingParameters->SetTextureUAV(m_pColorOutput, ViewDimension::Texture2D, 1);
}

void HybridShadowsRenderModule::Init(const json&)
{
    CauldronAssert(ASSERT_CRITICAL, !GetFramework()->GetConfig()->MotionVectorGeneration.empty(), L"Error : HybridShadowsRenderModule requires MotionVectorGeneration be set");

    // Config Asserts
    CauldronAssert(ASSERT_CRITICAL, GetDevice()->FeatureSupported(DeviceFeature::RT_1_1), L"Error : HybridShadowsRenderModule requires RT1.1");
    CauldronAssert(ASSERT_CRITICAL, GetDevice()->FeatureSupported(DeviceFeature::FP16), L"Error : HybridShadowsRenderModule requires FP16");

    // Fetch needed resource
    m_pDepthTarget   = GetFramework()->GetRenderTexture(L"DepthTarget");
    m_pNormalTarget  = GetFramework()->GetRenderTexture(L"GBufferNormalRT");
    m_pColorOutput   = GetFramework()->GetColorTargetForCallback(GetName());
    m_pMotionVectors = GetFramework()->GetRenderTexture(L"GBufferMotionVectorRT");

    // Create Resources for Effect
    CreateResources();

    // UI elements
    UISection* uiSection = GetUIManager()->RegisterUIElements("Hybrid Shadows", UISectionType::Sample);
    uiSection->RegisterUIElement<UICheckBox>("Run Hybrid Shadows", m_bRunHybridShadows);
    uiSection->RegisterUIElement<UICheckBox>("Use Denoiser", m_bUseDenoiser);
    uiSection->RegisterUIElement<UISlider<float>>("Sun solid angle", m_sunSolidAngle, 0.f, 1.f, nullptr, true, false, "%.2f deg");

    const static std::vector<const char*> debugOptions = {"Disabled", "Show RayTraced Tiles", "Show Ray minT", "Show Ray maxT", "Show Ray length", "Show RayTracing Texture"};
    uiSection->RegisterUIElement<UICombo>("Debug Mode", m_DebugMode, debugOptions);

    uiSection->RegisterUIElement<UISlider<int32_t>>("TileCutOff", (int&)m_TileCutoff, 0, 32);
    uiSection->RegisterUIElement<UISlider<float>>("PCF offset", m_blockerOffset, 0.0f, 0.008f);

    uiSection->RegisterUIElement<UICheckBox>("Reject Lit Pixels for Ray Tracing", m_bRejectLitPixels);
    uiSection->RegisterUIElement<UICheckBox>("Use Shadow Maps to determine RayT", m_bUseCascadesForRayT);    

    m_pIndirectWorkLoad = IndirectWorkload::CreateIndirectWorkload(IndirectCommandType::Dispatch);
    CreateCopyDepthPipeline();
    CreateRayTracingPipelines();
    CreateDebugTilesPipeline();
    CreateDebugRayTracingPipeline();

    InitEffect();

    GetFramework()->ConfigureRuntimeShaderRecompiler(
        [this](void) {
            DestroyEffect();
        },
        [this](void) {
            InitEffect();
        });


    //////////////////////////////////////////////////////////////////////////
    // Register additional execution callbacks during the frame

    // Register a post-lighting callback for tile debug pass
    ExecuteCallback callbackTileDebug      = [this](double deltaTime, CommandList* pCmdList) { this->TileDebugCallback(deltaTime, pCmdList); };
    ExecutionTuple  callbackTileDebugTuple = std::make_pair(L"HybridShadowsRenderModule::TileDebugCallback", std::make_pair(this, callbackTileDebug));
    GetFramework()->RegisterExecutionCallback(L"TranslucencyRenderModule", false, callbackTileDebugTuple);

    SetModuleReady(true);
    SetModuleEnabled(true);
}

void HybridShadowsRenderModule::TileDebugCallback(double deltaTime, cauldron::CommandList* pCmdList)
{
    if (m_DebugMode == 0 || !m_bRunHybridShadows)
        return;

    const ResolutionInfo& resinfo = GetFramework()->GetResolutionInfo();

    if (m_DebugMode == 5)
    {
        GPUScopedProfileCapture sampleMarker(pCmdList, L"Debug-RayTracing (HybridShadows)");

        {
            std::vector<Barrier> barriers;
            barriers.push_back(Barrier::Transition(
                m_pColorOutput->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess));
            barriers.push_back(Barrier::Transition(m_pWorkQueueCount->GetResource(), ResourceState::CopyDest, ResourceState::IndirectArgument));
            ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
        }

        // Bind everything
        m_pDebugRayTracingParameters->Bind(pCmdList, m_pDebugRayTracingPipelineObj);
        SetPipelineState(pCmdList, m_pDebugRayTracingPipelineObj);


        Dispatch(pCmdList, DivideRoundingUp(resinfo.RenderWidth, 8), DivideRoundingUp(resinfo.RenderHeight, 8), 1);
        {
            std::vector<Barrier> barriers;
            barriers.push_back(Barrier::Transition(
                m_pColorOutput->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
            barriers.push_back(Barrier::Transition(m_pWorkQueueCount->GetResource(), ResourceState::IndirectArgument, ResourceState::CopyDest));
            ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
        }
        return;
    }

    GPUScopedProfileCapture sampleMarker(pCmdList, L"Debug-Tiles (HybridShadows)");

    {
        std::vector<Barrier> barriers;
        barriers.push_back(Barrier::Transition(
            m_pColorOutput->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess));
        barriers.push_back(Barrier::Transition(
            m_pWorkQueueCount->GetResource(), ResourceState::CopyDest, ResourceState::IndirectArgument));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
    }

    static DebugTilesConstantBuffer dtConstantBuffer{};
    dtConstantBuffer.debugMode = m_DebugMode;

    BufferAddressInfo cb = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(DebugTilesConstantBuffer), &dtConstantBuffer);
    m_pDebugTilesParameters->UpdateRootConstantBuffer(&cb, 0);

    // Bind everything
    m_pDebugTilesParameters->SetBufferSRV(m_pWorkQueue, 0);
    m_pDebugTilesParameters->Bind(pCmdList, m_pDebugTilesPipelineObj);
    SetPipelineState(pCmdList, m_pDebugTilesPipelineObj);

    ExecuteIndirect(pCmdList, m_pIndirectWorkLoad, m_pWorkQueueCount, 1, 0);

    {
        std::vector<Barrier> barriers;
        barriers.push_back(Barrier::Transition(
            m_pColorOutput->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
        barriers.push_back(Barrier::Transition(m_pWorkQueueCount->GetResource(), ResourceState::IndirectArgument, ResourceState::CopyDest));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
    }
}

HybridShadowsRenderModule::~HybridShadowsRenderModule()
{
    if (ModuleEnabled())
    {
        EnableModule(false);

        // Destroy the context
        UpdateEffectContext(false);

        // Destroy the FidelityFX interface memory
        free(m_ClassifierCtxDesc.backendInterface.scratchBuffer);

        delete m_pDebugRayTracingPipelineObj;
        delete m_pDebugRayTracingRootSignature;
        delete m_pDebugRayTracingParameters;
        delete m_pIndirectWorkLoad;
        delete m_pRayTracingRootSignature;
        delete m_pRayTracingPipelineObj;
        delete m_pRayTracingParameters;
        delete m_pResolveRayTracingRootSignature;
        delete m_pResolveRayTracingPipelineObj;
        delete m_pResolveRayTracingParameters;
        delete m_pDebugTilesRootSignature;
        delete m_pDebugTilesPipelineObj;
        delete m_pDebugTilesParameters;
        delete m_pCopyDepthRootSignature;
        delete m_pCopyDepthPipelineObj;
        delete m_pCopyDepthParameters;

        delete m_pBlueNoise;
    }

}

void HybridShadowsRenderModule::InitEffect()
{
    // Setup FidelityFX interface.
    {
        const size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(FFX_CLASSIFIER_CONTEXT_COUNT + FFX_DENOISER_CONTEXT_COUNT);
        void*        scratchBuffer     = calloc(scratchBufferSize, 1u);
        FfxErrorCode errorCode         = SDKWrapper::ffxGetInterface(
                                                 &m_SDKInterface,
                                                 GetDevice(),
                                                 scratchBuffer,
                                                 scratchBufferSize,
                                                 FFX_CLASSIFIER_CONTEXT_COUNT + FFX_DENOISER_CONTEXT_COUNT);
        CAULDRON_ASSERT(errorCode == FFX_OK);
        CauldronAssert(ASSERT_CRITICAL, m_SDKInterface.fpGetSDKVersion(&m_SDKInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
            L"FidelityFX HybridShadows 2.1 sample requires linking with a 1.1.2 version SDK backend");
        CauldronAssert(ASSERT_CRITICAL, ffxClassifierGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 3, 0),
                           L"FidelityFX HybridShadows 2.1 sample requires linking with a 1.3 version FidelityFX Classifier library");
        CauldronAssert(ASSERT_CRITICAL, ffxDenoiserGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 3, 0),
                           L"FidelityFX HybridShadows 2.1 sample requires linking with a 1.3 version FidelityFX Denoiser library");
                           
        m_SDKInterface.fpRegisterConstantBufferAllocator(&m_SDKInterface, SDKWrapper::ffxAllocateConstantBuffer);
    }

    m_ClassifierCtxDesc.backendInterface = m_SDKInterface;
    m_DenoiserCtxDesc.backendInterface = m_SDKInterface;

    // Create the context
    UpdateEffectContext(true);
}

void HybridShadowsRenderModule::DestroyEffect()
{
    // Destroy the context
    UpdateEffectContext(false);

    // Destroy the FidelityFX interface memory
    if (m_ClassifierCtxDesc.backendInterface.scratchBuffer != nullptr)
    {
        free(m_ClassifierCtxDesc.backendInterface.scratchBuffer);
        m_ClassifierCtxDesc.backendInterface.scratchBuffer = nullptr;
    }
}

 void HybridShadowsRenderModule::UpdateEffectContext(bool enabled)
 {
     const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

     if (enabled)
     {
         // Classifier
         m_ClassifierCtxDesc.flags = 0;
         m_ClassifierCtxDesc.flags |= FFX_CLASSIFIER_SHADOW;

         switch (m_ClassificationMode)
         {
         case ClassificationMode::ClassifyByNormals:
            m_ClassifierCtxDesc.flags |= FFX_CLASSIFIER_CLASSIFY_BY_NORMALS;
            break;
         case ClassificationMode::ClassifyByCascades:
            m_ClassifierCtxDesc.flags |= FFX_CLASSIFIER_CLASSIFY_BY_CASCADES;
            break;
         default:
            CauldronCritical(L"Invalid Classification Mode");
         }
         m_ClassifierCtxDesc.flags |= GetConfig()->InvertedDepth ? FFX_CLASSIFIER_ENABLE_DEPTH_INVERTED : 0;

         m_ClassifierCtxDesc.resolution.width  = resInfo.RenderWidth;
         m_ClassifierCtxDesc.resolution.height = resInfo.RenderHeight;

         ffxClassifierContextCreate(&m_ClassifierContext, &m_ClassifierCtxDesc);

         // Denoiser
         m_DenoiserCtxDesc.flags = 0;
         m_DenoiserCtxDesc.flags = FFX_DENOISER_SHADOWS;
         m_DenoiserCtxDesc.flags |= GetConfig()->InvertedDepth ? FFX_CLASSIFIER_ENABLE_DEPTH_INVERTED : 0;

         m_DenoiserCtxDesc.windowSize.width  = resInfo.RenderWidth;
         m_DenoiserCtxDesc.windowSize.height = resInfo.RenderHeight;

         ffxDenoiserContextCreate(&m_DenoiserContext, &m_DenoiserCtxDesc);
     }
     else
     {
         // Flush anything out of the pipes before destroying the context
         GetDevice()->FlushAllCommandQueues();

         ffxClassifierContextDestroy(&m_ClassifierContext);
         ffxDenoiserContextDestroy(&m_DenoiserContext);
     }
 }

void HybridShadowsRenderModule::ResolveRayTracingToShadowTexture(cauldron::CommandList* pCmdList)
 {
     const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

     {
         std::vector<Barrier> preResolveBarriers;
         preResolveBarriers.push_back(Barrier::Transition(
             m_pRayHitTexture->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
     }

     m_pResolveRayTracingParameters->Bind(pCmdList, m_pResolveRayTracingPipelineObj);
     SetPipelineState(pCmdList, m_pResolveRayTracingPipelineObj);

     Dispatch(pCmdList, DivideRoundingUp(resInfo.RenderWidth, k_tileSizeX), DivideRoundingUp(resInfo.RenderHeight, k_tileSizeY), 1);

     {
         std::vector<Barrier> postResolveBarriers;
         postResolveBarriers.push_back(Barrier::Transition(
             m_pRayHitTexture->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess));
     }
 }

void HybridShadowsRenderModule::RunRayTracingShadowDispatch(const FfxClassifierShadowDispatchDescription& shadowClassifierDispatchParams,
                                                            const float sunSize,
                                                             cauldron::CommandList*                        pCmdList)
 {
    GPUScopedProfileCapture sampleMarker(pCmdList, L"Trace Shadow Rays");
    {
        std::vector<Barrier> preTracebarriers;
        preTracebarriers.push_back(Barrier::Transition(m_pWorkQueueCount->GetResource(), ResourceState::UnorderedAccess, ResourceState::IndirectArgument));
        ResourceBarrier(pCmdList, (uint32_t)preTracebarriers.size(), preTracebarriers.data());
    }
    m_pRayTracingParameters->SetAccelerationStructure(GetScene()->GetASManager()->GetTLAS(), 4);

    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

    m_RTConstantBuffer.textureSize = Vec4((float)resInfo.RenderWidth, (float)resInfo.RenderHeight, 1.f / resInfo.RenderWidth, 1.f / resInfo.RenderHeight);
    memcpy(&m_RTConstantBuffer.lightDir, &shadowClassifierDispatchParams.lightDir, sizeof(Vec3));
    static uint32_t frame = 0;

    m_RTConstantBuffer.pixelThickness_bUseCascadesForRayT_noisePhase_sunSize[0] = 1e-4f;
    m_RTConstantBuffer.pixelThickness_bUseCascadesForRayT_noisePhase_sunSize[1] = shadowClassifierDispatchParams.bUseCascadesForRayT;
    m_RTConstantBuffer.pixelThickness_bUseCascadesForRayT_noisePhase_sunSize[2] = (frame++ & 0xff) * GOLDEN_RATIO;
    m_RTConstantBuffer.pixelThickness_bUseCascadesForRayT_noisePhase_sunSize[3] = sunSize;

    memcpy(&m_RTConstantBuffer.viewToWorld, &shadowClassifierDispatchParams.viewToWorld, sizeof(Mat4));
    BufferAddressInfo cb = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(RTConstantBuffer), &m_RTConstantBuffer);
    m_pRayTracingParameters->UpdateRootConstantBuffer(&cb, 0);

    // Bind everything
    m_pRayTracingParameters->SetBufferSRV(m_pWorkQueue, 0);
    m_pRayTracingParameters->Bind(pCmdList, m_pRayTracingPipelineObj);
    SetPipelineState(pCmdList, m_pRayTracingPipelineObj);

    ExecuteIndirect(pCmdList, m_pIndirectWorkLoad, m_pWorkQueueCount, 1, 0);

    {
        std::vector<Barrier> postTracebarriers;
        postTracebarriers.push_back(Barrier::Transition(m_pWorkQueueCount->GetResource(), ResourceState::IndirectArgument, ResourceState::CopyDest));
        ResourceBarrier(pCmdList, (uint32_t)postTracebarriers.size(), postTracebarriers.data());
    }
}

void HybridShadowsRenderModule::RunCopyDepth(cauldron::CommandList* pCmdList)
{
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

    {
        std::vector<Barrier> barriers;
        barriers.push_back(Barrier::Transition(
            m_pCopyDepth->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
    }

    m_pCopyDepthParameters->Bind(pCmdList, m_pCopyDepthPipelineObj);

    SetPipelineState(pCmdList, m_pCopyDepthPipelineObj);

    Dispatch(pCmdList, DivideRoundingUp(resInfo.RenderWidth, 64u), DivideRoundingUp(resInfo.RenderHeight, 64u), 1);

    {
        std::vector<Barrier> barriers;
        barriers.push_back(Barrier::Transition(
            m_pCopyDepth->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
    }
}

void HybridShadowsRenderModule::RunFfxShadowDenoiser(cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture               sampleMarker(pCmdList, L"FidelityFX Shadow Denoiser");

    static uint32_t                       frameIndex = 0;
    FfxDenoiserShadowsDispatchDescription denoiserDispatchDescription{};
    denoiserDispatchDescription.commandList = SDKWrapper::ffxGetCommandList(pCmdList);
    denoiserDispatchDescription.hitMaskResults =
        SDKWrapper::ffxGetResource(m_pRayHitTexture->GetResource(), L"FidelityFX_ShadowDenoiser_RayHit", FFX_RESOURCE_STATE_GENERIC_READ);
    denoiserDispatchDescription.depth =
        SDKWrapper::ffxGetResource(m_pCopyDepth->GetResource(), L"FidelityFX_ShadowDenoiser_InputDepth", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchDescription.velocity =
        SDKWrapper::ffxGetResource(m_pMotionVectors->GetResource(), L"FidelityFX_ShadowDenoiser_InputVelocity", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchDescription.normal =
        SDKWrapper::ffxGetResource(m_pNormalTarget->GetResource(), L"FidelityFX_ShadowDenoiser_InputNormals", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    denoiserDispatchDescription.shadowMaskOutput =
        SDKWrapper::ffxGetResource(m_pShadowMaskOutput->GetResource(), L"FidelityFX_ShadowDenoiser_ShadowMaskOutput", FFX_RESOURCE_STATE_UNORDERED_ACCESS);

    // Cauldron's GBuffer stores normals in the [0, 1] range, the FidelityFX Shadow Denoiser expects them in the [-1, 1] range.
    denoiserDispatchDescription.normalsUnpackMul =  2.f;
    denoiserDispatchDescription.normalsUnpackAdd = -1.f;

    const CameraComponent* pCam = GetScene()->GetCurrentCamera();
    memcpy(&denoiserDispatchDescription.eye, &pCam->GetCameraPos(), sizeof(Vec3));
    denoiserDispatchDescription.frameIndex = frameIndex;
    memcpy(&denoiserDispatchDescription.projectionInverse, &pCam->GetInverseProjection(), sizeof(Mat4));
    const auto reprojMat = pCam->GetProjection() * (pCam->GetPreviousView() * pCam->GetInverseViewProjection());
    memcpy(&denoiserDispatchDescription.reprojectionMatrix, &reprojMat, sizeof(Mat4));
    memcpy(&denoiserDispatchDescription.viewProjectionInverse, &pCam->GetInverseViewProjection(), sizeof(Mat4));
    denoiserDispatchDescription.depthSimilaritySigma = 1.f;

    float mvScale[2] = {1.f, 1.f};
    memcpy(denoiserDispatchDescription.motionVectorScale, mvScale, sizeof(mvScale));

    CAULDRON_ASSERT(FFX_OK == ffxDenoiserContextDispatchShadows(&m_DenoiserContext, &denoiserDispatchDescription));
    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);

    frameIndex++;
}

void HybridShadowsRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
     if (!m_bRunHybridShadows)
     {
         GetScene()->SetScreenSpaceShadowTexture(nullptr);
         return;
     }
     GetScene()->SetScreenSpaceShadowTexture(m_pShadowMaskOutput);

     static float sunSize;
     sunSize = tanf(0.5f * (m_sunSolidAngle * (2 * AMD_PI) / 360.0f));


    GPUScopedProfileCapture sampleMarker(pCmdList, L"FidelityFX Hybrid Shadows");
    FfxClassifierShadowDispatchDescription shadowClassifierDispatchParams{};
    {
         GPUScopedProfileCapture sampleMarker(pCmdList, L"FidelityFX Classifier");
         const CameraComponent*  pCamera = GetScene()->GetCurrentCamera();

         // Init WorkQueueCount
         constexpr uint32_t offsets[3] = {sizeof(uint32_t) * 0, sizeof(uint32_t) * 1, sizeof(uint32_t) * 2};
         constexpr uint32_t values[3] =  {0, 1, 1};
         WriteBufferImmediate(pCmdList, m_pWorkQueueCount->GetResource(), 3, offsets, values);

         Barrier barrier = Barrier::Transition(m_pWorkQueueCount->GetResource(), ResourceState::CopyDest, ResourceState::UnorderedAccess);
         ResourceBarrier(pCmdList, 1, &barrier);

         shadowClassifierDispatchParams.commandList    = SDKWrapper::ffxGetCommandList(pCmdList);
         shadowClassifierDispatchParams.depth          = SDKWrapper::ffxGetResource(m_pDepthTarget->GetResource(), L"FidelityFXClassifier_InputDepth", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
         shadowClassifierDispatchParams.normals        = SDKWrapper::ffxGetResource(m_pNormalTarget->GetResource(), L"FidelityFXClassifier_InputNormals", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
         shadowClassifierDispatchParams.workQueue      = SDKWrapper::ffxGetResource(m_pWorkQueue->GetResource(), L"FidelityFXClassifier_WorkQueue", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
         shadowClassifierDispatchParams.workQueueCount = SDKWrapper::ffxGetResource(m_pWorkQueueCount->GetResource(), L"FidelityFXClassifier_WorkQueueCount", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
         shadowClassifierDispatchParams.rayHitTexture  = SDKWrapper::ffxGetResource(m_pRayHitTexture->GetResource(), L"FidelityFXClassifier_RayHit", FFX_RESOURCE_STATE_UNORDERED_ACCESS);

         // Cauldron's GBuffer stores normals in the [0, 1] range, the FidelityFX Classifier expects them in the [-1, 1] range.
         shadowClassifierDispatchParams.normalsUnPackMul =  2.f;
         shadowClassifierDispatchParams.normalsUnPackAdd = -1.f;

         shadowClassifierDispatchParams.tileCutOff          = m_TileCutoff;
         shadowClassifierDispatchParams.bRejectLitPixels    = m_bRejectLitPixels;
         shadowClassifierDispatchParams.bUseCascadesForRayT = m_bUseCascadesForRayT && m_ClassificationMode == ClassificationMode::ClassifyByCascades;
         shadowClassifierDispatchParams.blockerOffset       = m_blockerOffset;

         memcpy(&shadowClassifierDispatchParams.viewToWorld, &pCamera->GetInverseViewProjection(), sizeof(Mat4));

         const std::vector<Component*>& lightingComponents = LightComponentMgr::Get()->GetComponentList();
         for (auto* pComp : lightingComponents)
         {
            // Skip inactive lights
            if (!pComp->GetOwner()->IsActive())
                continue;

            const LightComponent* pLightComp = static_cast<LightComponent*>(pComp);

            // Process only a light if it has cascades and is a directional light
            if (pLightComp->GetCascadesCount() <= 0 || pLightComp->GetType() != LightType::Directional)
            {
                Barrier barrier2 = Barrier::Transition(m_pWorkQueueCount->GetResource(), ResourceState::UnorderedAccess, ResourceState::CopyDest);
                ResourceBarrier(pCmdList, 1, &barrier2);
                return;
            }

            memcpy(&shadowClassifierDispatchParams.lightDir, &-pLightComp->GetDirection(), sizeof(Vec3));
            memcpy(&shadowClassifierDispatchParams.lightView, &pLightComp->GetView(), sizeof(Mat4));
            memcpy(&shadowClassifierDispatchParams.inverseLightView, &pLightComp->GetInverseView(), sizeof(Mat4));

            ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
            CauldronAssert(ASSERT_CRITICAL, pLightComp->GetCascadesCount() <= 4, L"HybridShadowsRenderModule does not support lights with more than 4 cascades");
            shadowClassifierDispatchParams.cascadeCount = pLightComp->GetCascadesCount();

            for (uint32_t cascade = 0; cascade < shadowClassifierDispatchParams.cascadeCount; ++cascade)
            {
                int shadowMapIndex                 = pLightComp->GetShadowMapIndex(cascade);
                shadowClassifierDispatchParams.shadowMaps[cascade] = SDKWrapper::ffxGetResource(pShadowMapResourcePool->GetRenderTarget(shadowMapIndex)->GetResource(),
                                                                    L"FidelityFXClassifier_ShadowMap",
                                                                    FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

                const Vec4  scaleAndOffset       = pShadowMapResourcePool->GetTransformation(pLightComp->GetShadowMapRect(cascade));
                const Mat4 matTextureScale       = Mat4::scale(Vec3(scaleAndOffset.getX(), scaleAndOffset.getY(), 1.0f));
                const Mat4 matTextureTranslation = Mat4::translation(Vec3(scaleAndOffset.getZ(), scaleAndOffset.getW(), 0.f));
                const Mat4 mShadowTexture        = matTextureTranslation * matTextureScale * pLightComp->GetShadowProjection(cascade);
                memcpy(&shadowClassifierDispatchParams.cascadeScale[cascade],
                       &Vec4(mShadowTexture.getCol0().getX(), mShadowTexture.getCol1().getY(), mShadowTexture.getCol2().getZ(), 1.0f),
                       sizeof(Vec4));
                memcpy(&shadowClassifierDispatchParams.cascadeOffset[cascade],
                       &Vec4(mShadowTexture.getCol3().getX(), mShadowTexture.getCol3().getY(), mShadowTexture.getCol3().getZ(), 0.0f),
                       sizeof(Vec4));
            }

            shadowClassifierDispatchParams.cascadeSize       = (float)pLightComp->GetShadowResolution();
            shadowClassifierDispatchParams.sunSizeLightSpace = ComputeSunSizeLightSpace(pLightComp->GetDirection(), sunSize, pLightComp->GetView());

            // Only process one light
            break;
         }

         CAULDRON_ASSERT(FFX_OK == ffxClassifierContextShadowDispatch(&m_ClassifierContext, &shadowClassifierDispatchParams));
         // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
         SetAllResourceViewHeaps(pCmdList);
    }


    RunRayTracingShadowDispatch(shadowClassifierDispatchParams, sunSize, pCmdList);

    if (m_bUseDenoiser)
    {
        // Copy depth to a R32 texture
        RunCopyDepth(pCmdList);

        // Denoise RT output and write it to the Shadow Texture
        RunFfxShadowDenoiser(pCmdList);
    }
    else
    {
        // Write RT output to the shadow texture directly
        GPUScopedProfileCapture sampleMarker(pCmdList, L"Write to Shadow Texture");
        ResolveRayTracingToShadowTexture(pCmdList);
    }
}

void HybridShadowsRenderModule::OnResize(const ResolutionInfo& resInfo)
{
     if (!ModuleEnabled())
        return;

     // Need to recreate the context on resource resize
     UpdateEffectContext(false);  // Destroy
     UpdateEffectContext(true);   // Re-create
}
