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

#include "skydomerendermodule.h"
#include "core/contentmanager.h"
#include "core/framework.h"
#include "core/loaders/textureloader.h"
#include "core/uimanager.h"
#include "core/scene.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rasterview.h"
#include "render/resourceview.h"
#include "render/rootsignature.h"
#include "render/sampler.h"
#include "render/dynamicresourcepool.h"
#include "render/device.h"

using namespace std::experimental;
using namespace cauldron;

constexpr uint32_t g_NumThreadX = 8;
constexpr uint32_t g_NumThreadY = 8;
constexpr uint32_t g_EnvironmentCubeX = 512;
constexpr uint32_t g_EnvironmentCubeY = 512;
constexpr uint32_t g_IrradianceCubeX = 32;
constexpr uint32_t g_IrradianceCubeY = 32;
constexpr uint32_t g_PrefilteredCubeX    = 512;
constexpr uint32_t g_PrefilteredCubeY    = 512;
constexpr uint32_t g_PrefilterMipLevels  = 10;
constexpr uint32_t g_MaxPrefilterSamples = 64;


void SkyDomeRenderModule::Init(const json& initData)
{
    // Init the right version
    m_IsProcedural = initData.value("Procedural", m_IsProcedural);

    // Read in procedural defaults
    GetScene()->GetSkydomeHour() = initData.value("Hour", GetScene()->GetSkydomeHour());
    GetScene()->GetSkydomeMinute() = initData.value("Minute", GetScene()->GetSkydomeMinute());
    
    m_pProceduralConstantData.Rayleigh = initData.value("Rayleigh", 2.f);
    m_pProceduralConstantData.Turbidity = initData.value("Turbidity", 10.f);
    m_pProceduralConstantData.MieCoefficient = initData.value("Mie", 0.005f);
    m_pProceduralConstantData.Luminance = initData.value("Luminance", 3.5f);
    m_pProceduralConstantData.MieDirectionalG = initData.value("MieDir", 0.8f);

    // Get the render and depth targets
    m_pRenderTarget = GetFramework()->GetColorTargetForCallback(GetName());
    CauldronAssert(ASSERT_CRITICAL, m_pRenderTarget != nullptr, L"Couldn't find or create the render target of SkyDomeRenderModule.");
    m_pDepthTarget = GetFramework()->GetRenderTexture(L"DepthTarget");
    CauldronAssert(ASSERT_CRITICAL, m_pRenderTarget != nullptr, L"Couldn't find depth target for read-only needed for SkyDomeRenderModule.");

    if (m_IsProcedural)
        InitProcedural();
    InitSkyDome();
}

SkyDomeRenderModule::~SkyDomeRenderModule()
{
    delete m_pRootSignatureSkyDomeGeneration;
    delete m_pRootSignatureApplySkydome;
    delete m_pPipelineObjEnvironmentCube;
    delete m_pPipelineObjApplySkydome;
    delete m_pPipelineObjIrradianceCube;
    for (auto iter : m_pPipelineObjPrefilteredCube)
    {
        delete iter;
    }
    delete m_pParametersEnvironmentCube;
    delete m_pParametersApplySkydome;
    delete m_pParametersIrradianceCube;
    for (auto iter : m_pParametersPrefilteredCube)
    {
        delete iter;
    }
}

void SkyDomeRenderModule::InitSkyDome()
{
    // We are now ready for use
    SetModuleReady(false);

    // Get raster views
    m_pRasterViews.resize(2);
    m_pRasterViews[0] = GetRasterViewAllocator()->RequestRasterView(m_pRenderTarget, ViewDimension::Texture2D);
    m_pRasterViews[1] = GetRasterViewAllocator()->RequestRasterView(m_pDepthTarget, ViewDimension::Texture2D);

    // create sampler
    SamplerDesc samplerDesc;
    samplerDesc.AddressW = AddressMode::Wrap;

    // root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Vertex, 1); // b0 for UpscalerInformation included from upscaler.h
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::Vertex, 1); // b1 for SkydomeCBData included from skydomecommon.h
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1); //t0 for SkyTexture included from skydome.hlsl

    signatureDesc.AddStaticSamplers(0, ShaderBindStage::Pixel, 1, &samplerDesc);

    m_pRootSignatureApplySkydome = RootSignature::CreateRootSignature(L"SkyDomeRenderPass_RootSignature", signatureDesc);

    m_pParametersApplySkydome = ParameterSet::CreateParameterSet(m_pRootSignatureApplySkydome);
    m_pParametersApplySkydome->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SkydomeCBData), 1); // b1 for SkydomeCBData in skydomecommon.h

    if (m_pSkyTexture == nullptr)
    {
        // Load the texture data from which to create the texture
        TextureLoadCompletionCallbackFn CompletionCallback = [this](const std::vector<const Texture*>& textures, void* additionalParams = nullptr)
        {
            this->TextureLoadComplete(textures, additionalParams);
        };
        filesystem::path m_pSkyTexturePath = GetConfig()->StartupContent.SkyMap;
        GetContentManager()->LoadTexture(TextureLoadInfo(m_pSkyTexturePath), CompletionCallback);
    }

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pRootSignatureApplySkydome);
    
    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"skydome.hlsl", L"MainVS", ShaderModel::SM6_0, nullptr));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"skydome.hlsl", L"MainPS", ShaderModel::SM6_0, nullptr));

    // Setup remaining information and build
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);

    //attach render target and depth
    psoDesc.AddRasterFormats(m_pRenderTarget->GetFormat(), m_pDepthTarget->GetFormat());
    DepthDesc depthDesc;
    depthDesc.DepthEnable      = true;
    depthDesc.StencilEnable    = false;
    depthDesc.DepthWriteEnable = false;
    depthDesc.DepthFunc        = ComparisonFunc::LessEqual;
    psoDesc.AddDepthState(&depthDesc);

    m_pPipelineObjApplySkydome = PipelineObject::CreatePipelineObject(L"SkydomeRenderPass_PipelineObj", psoDesc);

    m_SkydomeConstantData.ClipToWorld = Mat4::identity();

}

void SkyDomeRenderModule::InitProcedural()
{
    SetModuleReady(false);

    // create sampler
    SamplerDesc samplerDesc;
    samplerDesc.AddressW = AddressMode::Wrap;

    // root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);  // b0 for UpscalerInformation included from upscaler.h
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);  // b1 for SkydomeCBData included from skydomecommon.h
    signatureDesc.AddConstantBufferView(2, ShaderBindStage::Compute, 1);  // b2 for ProceduralCBData included from skydomecommon.h
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);
    signatureDesc.AddBufferSRVSet(1, ShaderBindStage::Compute, 1);
    signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);

    signatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, 1, &samplerDesc);

    m_pRootSignatureSkyDomeGeneration = RootSignature::CreateRootSignature(L"SkyDomeProcRenderPass_RootSignature", signatureDesc);

    // EnvironmentCube
    {
        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pRootSignatureSkyDomeGeneration);

        DefineList defineList;
        defineList.insert(std::make_pair(L"NUM_THREAD_X", std::to_wstring(g_NumThreadX)));
        defineList.insert(std::make_pair(L"NUM_THREAD_Y", std::to_wstring(g_NumThreadY)));
        defineList.insert(std::make_pair(L"ENVIRONMENT_CUBE", L""));
        defineList.insert(std::make_pair(L"ENVIRONMENT_CUBE_X", std::to_wstring(g_EnvironmentCubeX)));
        defineList.insert(std::make_pair(L"ENVIRONMENT_CUBE_Y", std::to_wstring(g_EnvironmentCubeY)));

        // Setup the shaders to build on the pipeline object
        std::wstring ShaderPath = L"skydomeproc.hlsl";
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(ShaderPath.c_str(), L"MainCS", ShaderModel::SM6_0, &defineList));

        m_pPipelineObjEnvironmentCube = PipelineObject::CreatePipelineObject(L"SkydomeProcRenderPassEnvironmentCube_PipelineObj", psoDesc);

        m_pParametersEnvironmentCube = ParameterSet::CreateParameterSet(m_pRootSignatureSkyDomeGeneration);
        m_pParametersEnvironmentCube->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(UpscalerInformation), 0); // b0 for UpscalerInformation included from upscaler.h
        m_pParametersEnvironmentCube->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SkydomeCBData), 1); // b1 for SkydomeCBData included from skydomecommon.h
        m_pParametersEnvironmentCube->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(ProceduralCBData), 2); // b2 for ProceduralCBData included from skydomecommon.h

        if (m_pSkyTextureGenerated == nullptr)
        {
            TextureDesc textureDesc = TextureDesc::TexCube(std::wstring(L"EnvironmentCubeGenerated").c_str(),
                                                           ResourceFormat::RGBA16_FLOAT,
                                                           g_EnvironmentCubeX,
                                                           g_EnvironmentCubeY,
                                                           1,
                                                           0,
                                                           ResourceFlags::AllowUnorderedAccess);
            m_pSkyTextureGenerated  = GetDynamicResourcePool()->CreateTexture(&textureDesc, ResourceState::NonPixelShaderResource);

            InitSunlight();
        }

        m_pParametersEnvironmentCube->SetTextureUAV(m_pSkyTextureGenerated, ViewDimension::Texture2DArray, 0, -1, 6, 0);
    }

    // IrradianceCube
    {
        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pRootSignatureSkyDomeGeneration);

        DefineList defineList;
        defineList.insert(std::make_pair(L"NUM_THREAD_X", std::to_wstring(g_NumThreadX)));
        defineList.insert(std::make_pair(L"NUM_THREAD_Y", std::to_wstring(g_NumThreadY)));
        defineList.insert(std::make_pair(L"IRRADIANCE_CUBE", L""));
        defineList.insert(std::make_pair(L"IRRADIANCE_CUBE_X", std::to_wstring(g_IrradianceCubeX)));
        defineList.insert(std::make_pair(L"IRRADIANCE_CUBE_Y", std::to_wstring(g_IrradianceCubeY)));

        // Setup the shaders to build on the pipeline object
        std::wstring ShaderPath = L"skydomeproc.hlsl";
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(ShaderPath.c_str(), L"MainCS", ShaderModel::SM6_0, &defineList));

        m_pPipelineObjIrradianceCube = PipelineObject::CreatePipelineObject(L"SkydomeProcRenderPassIrradianceCube_PipelineObj", psoDesc);

        m_pParametersIrradianceCube = ParameterSet::CreateParameterSet(m_pRootSignatureSkyDomeGeneration);
        m_pParametersIrradianceCube->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(UpscalerInformation), 0); // b0 for UpscalerInformation included from upscaler.h
        m_pParametersIrradianceCube->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SkydomeCBData), 1); // b1 for SkydomeCBData included from skydomecommon.h
        m_pParametersIrradianceCube->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(ProceduralCBData), 2); // b2 for ProceduralCBData included from skydomecommon.h
        m_pParametersIrradianceCube->SetTextureSRV(m_pSkyTextureGenerated, ViewDimension::TextureCube, 0);

        if (m_pIrradianceCube == nullptr)
        {
            TextureDesc textureDesc = TextureDesc::TexCube(std::wstring(L"IrradianceCube").c_str(),
                                                           ResourceFormat::RGBA16_FLOAT,
                                                           g_IrradianceCubeX,
                                                           g_IrradianceCubeY,
                                                           1,
                                                           0,
                                                           ResourceFlags::AllowUnorderedAccess);
            m_pIrradianceCube       = GetDynamicResourcePool()->CreateTexture(&textureDesc, ResourceState::ShaderResource);

            GetScene()->SetIBLTexture(m_pIrradianceCube, IBLTexture::Irradiance);
        }

        if (m_pIrradianceCubeGenerated == nullptr)
        {
            TextureDesc textureDesc = TextureDesc::TexCube(std::wstring(L"IrradianceCubeGenerated").c_str(),
                                                           ResourceFormat::RGBA16_FLOAT,
                                                           g_IrradianceCubeX,
                                                           g_IrradianceCubeY,
                                                           1,
                                                           0,
                                                           ResourceFlags::AllowUnorderedAccess);
            m_pIrradianceCubeGenerated   = GetDynamicResourcePool()->CreateTexture(&textureDesc, ResourceState::NonPixelShaderResource);
        }

        m_pParametersIrradianceCube->SetTextureUAV(m_pIrradianceCubeGenerated, ViewDimension::Texture2DArray, 0, -1, 6, 0);
    }

    // PrefilteredCube
    {
        m_pPipelineObjPrefilteredCube.resize(g_PrefilterMipLevels);
        m_pParametersPrefilteredCube.resize(g_PrefilterMipLevels);
        for (int mip = 0; mip < g_PrefilterMipLevels; mip++)
        {
            uint32_t mip_width  = static_cast<uint32_t>(g_PrefilteredCubeX * std::pow(0.5, mip));
            uint32_t mip_height = static_cast<uint32_t>(g_PrefilteredCubeY * std::pow(0.5, mip));

            // Setup the pipeline object
            PipelineDesc psoDesc;
            psoDesc.SetRootSignature(m_pRootSignatureSkyDomeGeneration);

            DefineList defineList;
            defineList.insert(std::make_pair(L"NUM_THREAD_X", std::to_wstring(g_NumThreadX)));
            defineList.insert(std::make_pair(L"NUM_THREAD_Y", std::to_wstring(g_NumThreadY)));
            defineList.insert(std::make_pair(L"PREFILTERED_CUBE", L""));
            defineList.insert(std::make_pair(L"MIP_WIDTH", std::to_wstring(mip_width)));
            defineList.insert(std::make_pair(L"MIP_HEIGHT", std::to_wstring(mip_height)));
            defineList.insert(std::make_pair(L"SAMPLE_COUNT", std::to_wstring(32)));

            // Setup the shaders to build on the pipeline object
            std::wstring ShaderPath = L"skydomeproc.hlsl";
            psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(ShaderPath.c_str(), L"MainCS", ShaderModel::SM6_0, &defineList));

            m_pPipelineObjPrefilteredCube[mip] = PipelineObject::CreatePipelineObject(
                (std::wstring(L"SkydomeProcRenderPassPrefilteredCube[") + std::to_wstring(mip) + std::wstring(L"]_PipelineObj")).c_str(), psoDesc);

            m_pParametersPrefilteredCube[mip] = ParameterSet::CreateParameterSet(m_pRootSignatureSkyDomeGeneration);
            m_pParametersPrefilteredCube[mip]->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(UpscalerInformation), 0); // b0 for UpscalerInformation included from upscaler.h
            m_pParametersPrefilteredCube[mip]->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SkydomeCBData), 1); // b1 for SkydomeCBData included from skydomecommon.h
            m_pParametersPrefilteredCube[mip]->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(ProceduralCBData), 2); // b2 for ProceduralCBData included from skydomecommon.h
            m_pParametersPrefilteredCube[mip]->SetTextureSRV(m_pSkyTextureGenerated, ViewDimension::TextureCube, 0);
        }

        if (m_pPrefilteredCube == nullptr)
        {
            TextureDesc textureDesc = TextureDesc::TexCube(std::wstring(L"PrefilteredCube").c_str(),
                                                           ResourceFormat::RGBA16_FLOAT,
                                                           g_PrefilteredCubeX,
                                                           g_PrefilteredCubeY,
                                                           1,
                                                           g_PrefilterMipLevels,
                                                           ResourceFlags::AllowUnorderedAccess);
            m_pPrefilteredCube      = GetDynamicResourcePool()->CreateTexture(&textureDesc, ResourceState::ShaderResource);

            GetScene()->SetIBLTexture(m_pPrefilteredCube, IBLTexture::Prefiltered);
        }

        if (m_pPrefilteredCubeGenerated == nullptr)
        {
            TextureDesc textureDesc = TextureDesc::TexCube(std::wstring(L"PrefilteredCubeGenerated").c_str(),
                                                           ResourceFormat::RGBA16_FLOAT,
                                                           g_PrefilteredCubeX,
                                                           g_PrefilteredCubeY,
                                                           1,
                                                           g_PrefilterMipLevels,
                                                           ResourceFlags::AllowUnorderedAccess);
            m_pPrefilteredCubeGenerated  = GetDynamicResourcePool()->CreateTexture(&textureDesc, ResourceState::NonPixelShaderResource);
        }

        InitSampleDirections();
    }

    // initial values
    m_pProceduralConstantData.SunDirection = Vec3(1.0f, 0.05f, 0.0f);

    UISection* uiSection = GetUIManager()->RegisterUIElements("Procedural SkyDome");
    if (uiSection)
    {
        uiSection->RegisterUIElement<UISlider<int32_t>>("Hour", GetScene()->GetSkydomeHour(), 5, 19, [this](int32_t cur, int32_t old) {
            if (cur != old)
            {
                m_pShouldRunSkydomeGeneration = true;
            }
            });
        uiSection->RegisterUIElement<UISlider<int32_t>>("Minute", GetScene()->GetSkydomeMinute(), 0, 59, [this](int32_t cur, int32_t old) {
            if (cur != old)
            {
                m_pShouldRunSkydomeGeneration = true;
            }
            });
        uiSection->RegisterUIElement<UISlider<float>>("Rayleigh", m_pProceduralConstantData.Rayleigh, 0.0f, 10.0f, [this](float cur, float old) {
            if (cur != old)
            {
                m_pShouldRunSkydomeGeneration = true;
            }
            });
        uiSection->RegisterUIElement<UISlider<float>>("Turbidity", m_pProceduralConstantData.Turbidity, 0.0f, 25.0f, [this](float cur, float old) {
            if (cur != old)
            {
                m_pShouldRunSkydomeGeneration = true;
            }
            });
        uiSection->RegisterUIElement<UISlider<float>>("Mie Coefficient", m_pProceduralConstantData.MieCoefficient, 0.0f, 0.01f, [this](float cur, float old) {
            if (cur != old)
            {
                m_pShouldRunSkydomeGeneration = true;
            }
            });
        uiSection->RegisterUIElement<UISlider<float>>("Luminance", m_pProceduralConstantData.Luminance, 0.0f, 25.0f, [this](float cur, float old) {
            if (cur != old)
            {
                m_pShouldRunSkydomeGeneration = true;
            }
            });
        uiSection->RegisterUIElement<UISlider<float>>("Mie Directional G", m_pProceduralConstantData.MieDirectionalG, 0.0f, 1.0f, [this](float cur, float old) {
            if (cur != old)
            {
                m_pShouldRunSkydomeGeneration = true;
            }
            });

    }

}

float RadicalInverseVdc(uint32_t bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits * 2.3283064365386963e-10);  // / 0x100000000
}

Vec2 Hammersley(uint32_t i, uint32_t N)
{
    return Vec2(float(i) / float(N), RadicalInverseVdc(i));
}

void SkyDomeRenderModule::InitSampleDirections()
{
    m_pSampleDirections.resize(g_PrefilterMipLevels);
    for (int mip = 0; mip < g_PrefilterMipLevels; mip++)
    {
        BufferDesc bufferDescSampleDirections =
            BufferDesc::Data(std::wstring(L"SampleDirections[" + std::to_wstring(mip) + L"]").c_str(), sizeof(Vec4) * g_MaxPrefilterSamples, sizeof(Vec4));
        m_pSampleDirections[mip] = GetDynamicResourcePool()->CreateBuffer(&bufferDescSampleDirections, ResourceState::CopyDest);

        float roughness = (float)mip / (float)(g_PrefilterMipLevels - 1);

        std::vector<Vec4> samples;

        samples.resize(g_MaxPrefilterSamples);

        for (int i = 0; i < 32; i++)
        {
            Vec2  Xi = Hammersley(i, 32);
            float a  = roughness * roughness;

            float phi      = 2.0f * CAULDRON_PI * Xi.getX();
            float cosTheta = sqrt((1.0f - Xi.getY()) / (1.0f + (a * a - 1.0f) * Xi.getY()));
            float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

            // from spherical coordinates to cartesian coordinates - halfway vector
            Vec3 H;
            H.setX(cos(phi) * sinTheta);
            H.setY(sin(phi) * sinTheta);
            H.setZ(cosTheta);

            samples[i] = Vec4(H, 0.0f);
        }

        const_cast<Buffer*>(m_pSampleDirections[mip])->CopyData(samples.data(), sizeof(Vec4) * samples.size());
        // Once done, auto-enqueue a barrier for start of next frame so it's usable
        Barrier bufferTransition = Barrier::Transition(m_pSampleDirections[mip]->GetResource(), ResourceState::CopyDest, ResourceState::CommonResource);
        GetDevice()->ExecuteResourceTransitionImmediate(1, &bufferTransition);
    }
}

void SkyDomeRenderModule::InitSunlight()
{
    // Init data first as it's needed when continuing on the main thread
    m_pSunlightCompData.Name = L"SkyDome ProceduralSunlight";

    // Pull in luminance
    m_pSunlightCompData.Intensity = m_pProceduralConstantData.Luminance;
    m_pSunlightCompData.Color = Vec3(1.0f, 1.0f, 1.0f);
    m_pSunlightCompData.ShadowResolution = 1024;

    // Need to create our content on a background thread so proper notifiers can be called
    std::function<void(void*)> createContent = [this](void*)
    {
        ContentBlock* pContentBlock = new ContentBlock();

        // Memory backing light creation
        EntityDataBlock* pLightDataBlock = new EntityDataBlock();
        pContentBlock->EntityDataBlocks.push_back(pLightDataBlock);
        pLightDataBlock->pEntity = new Entity(m_pSunlightCompData.Name.c_str());
        m_pSunlight = pLightDataBlock->pEntity;
        CauldronAssert(ASSERT_CRITICAL, m_pSunlight, L"Could not allocate skydome sunlight entity");

        // Calculate transform
        Mat4 lookAt = Mat4::lookAt(Point3(m_pProceduralConstantData.SunDirection), Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
        Mat4 transform = InverseMatrix(lookAt);
        m_pSunlight->SetTransform(transform);

        LightComponentData* pLightComponentData = new LightComponentData(m_pSunlightCompData);
        pLightDataBlock->ComponentsData.push_back(pLightComponentData);
        m_pSunlightComponent = LightComponentMgr::Get()->SpawnLightComponent(m_pSunlight, pLightComponentData);
        pLightDataBlock->Components.push_back(m_pSunlightComponent);

        GetContentManager()->StartManagingContent(L"ProceduralSkydomeLightEntity", pContentBlock, false);

        // We are now ready for use
        SetModuleReady(true);
    };

    // Queue a task to create needed content after setup (but before run)
    Task createContentTask(createContent, nullptr);
    GetFramework()->AddContentCreationTask(createContentTask);
}

void SkyDomeRenderModule::UpdateSunDirection()
{
    // Parameters based on Japan 6/15
    float Lat = DEG_TO_RAD(35.0f); // Latitude
    float Decl = DEG_TO_RAD(23.0f + (17.0f / 60.0f)); // Sun declination at 6/15
    float Hour = DEG_TO_RAD((GetScene()->GetSkydomeHour() + static_cast<float>(GetScene()->GetSkydomeMinute() / 60.0f) - 12.0f) * 15.0f); // Hour angle at E135
    // Sin of solar altitude angle (2 / PI - solar zenith angle)
    float SinH = sinf(Lat) * sinf(Decl) + cosf(Lat) * cosf(Decl) * cosf(Hour);
    float CosH = sqrtf(1.0f - SinH * SinH);
    // Sin/Cos of solar azimuth angle
    float SinA = cosf(Decl) * sinf(Hour) / CosH;
    float CosA = (SinH * sinf(Lat) - sinf(Decl)) / (CosH * cosf(Decl));

    // Polar to Cartesian
    float x = CosA * CosH;
    float y = SinH;
    float z = SinA * CosH;

    // Update Proc shader parameters
    m_pProceduralConstantData.SunDirection = normalize(Vec3(x, y, z));

    // Update sunlight
    Mat4 lookAt = Mat4::lookAt(Point3(m_pProceduralConstantData.SunDirection), Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 transform = InverseMatrix(lookAt);

    m_pSunlight->SetTransform(transform);
    m_pSunlightComponent->GetData().Intensity = m_pProceduralConstantData.Luminance;
    m_pSunlightComponent->SetDirty();
}

void SkyDomeRenderModule::TextureLoadComplete(const std::vector<const Texture*>& textureList, void*)
{
    // First texture to be used as sky texture
    m_pSkyTexture = textureList[0];
    CauldronAssert(ASSERT_CRITICAL, m_pSkyTexture != nullptr, L"SkyDomeRenderModule: Required texture could not be loaded. Terminating sample. Did you run UpdateMedia.bat?");

    // Set our texture to the right parameter slot
    m_pParametersApplySkydome->SetTextureSRV(m_pSkyTexture, ViewDimension::TextureCube, 0); //t0 for SkyTexture included from skydome.hlsl

    // We are now ready for use
    SetModuleReady(true);
}

void SkyDomeRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    GPUScopedProfileCapture skydomeMarker(pCmdList, L"SkyDome");

    // Set viewport, scissor, primitive topology once and move on (set based on upscaler state)
    UpscalerState upscaleState = GetFramework()->GetUpscalingState();
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

    if (upscaleState == UpscalerState::None || upscaleState == UpscalerState::PostUpscale)
    {
        m_pWidth = resInfo.UpscaleWidth;
        m_pHeight = resInfo.UpscaleHeight;
    }
    else
    {
        m_pWidth = resInfo.RenderWidth;
        m_pHeight = resInfo.RenderHeight;
    }
    // write to dynamic constant buffer
    m_SkydomeConstantData.ClipToWorld = GetScene()->GetCurrentCamera()->GetInverseViewProjection();

    
    //run procedural skydome + IBL texture generation for first frame and when time of day changes
    if (m_IsProcedural)
    {
        if (m_pUpscalerInfo.FullScreenScaleRatio != GetScene()->GetSceneInfo().UpscalerInfo.FullScreenScaleRatio)
        {
            m_pShouldRunSkydomeGeneration = true;
        }
        // time of day changes also sets m_pShouldRunSkydomeGeneration to true 
        if (m_pShouldRunSkydomeGeneration && m_CubemapGenerateReady)
        {
            m_pComputeCmdList = GetDevice()->CreateCommandList(L"SkyDomeComputeCmdList", CommandQueue::Compute);
            m_pShouldRunSkydomeGeneration = false;
            m_CubemapGenerateReady.store(false);
            ExecuteSkydomeGeneration(m_pComputeCmdList);
            SetAllResourceViewHeaps(pCmdList);
        }
    }
    //always run PS to apply skydome cubemap
    ExecuteSkydomeRender(pCmdList);
}
void SkyDomeRenderModule::ExecuteSkydomeRender(cauldron::CommandList* pCmdList)
{
    // Render modules expect resources coming in/going out to be in a shader read state
    GPUScopedProfileCapture skydomeMarker(pCmdList, L"SkyDome Rendering");

    std::vector<Barrier> barriers;

    if (m_IsProcedural && m_CubemapGenerateReady && !m_CubemapCopyReady)
    {
        m_CubemapCopyReady.store(true);

        // Render modules expect resources coming in/going out to be in a shader read state
        barriers.push_back(Barrier::Transition(m_pSkyTextureGenerated->GetResource(), ResourceState::NonPixelShaderResource, ResourceState::CopySource));
        barriers.push_back(Barrier::Transition(
            m_pSkyTexture->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::CopyDest));

        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

        for (uint32_t array = 0; array < m_pSkyTextureGenerated->GetDesc().DepthOrArraySize; array++)
        {
            TextureCopyDesc skyTextureCopyDesc(m_pSkyTextureGenerated->GetResource(), m_pSkyTexture->GetResource(), array, 0);
            CopyTextureRegion(pCmdList, &skyTextureCopyDesc);
        }

        barriers.clear();
        barriers.push_back(Barrier::Transition(m_pSkyTextureGenerated->GetResource(), ResourceState::CopySource, ResourceState::NonPixelShaderResource));
        barriers.push_back(Barrier::Transition(
            m_pSkyTexture->GetResource(), ResourceState::CopyDest, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));

        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

        // Render modules expect resources coming in/going out to be in a shader read state
        barriers.clear();
        barriers.push_back(
            Barrier::Transition(m_pIrradianceCubeGenerated->GetResource(), ResourceState::NonPixelShaderResource, ResourceState::CopySource));
        barriers.push_back(Barrier::Transition(
            m_pIrradianceCube->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::CopyDest));

        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

        for (uint32_t array = 0; array < m_pIrradianceCubeGenerated->GetDesc().DepthOrArraySize; array++)
        {
            for (uint32_t mip = 0; mip < m_pIrradianceCubeGenerated->GetDesc().MipLevels; mip++)
            {
                TextureCopyDesc irradianceCubeCopyDesc(m_pIrradianceCubeGenerated->GetResource(), m_pIrradianceCube->GetResource(), array, mip);
                CopyTextureRegion(pCmdList, &irradianceCubeCopyDesc);
            }
        }

        barriers.clear();
        barriers.push_back(
            Barrier::Transition(m_pIrradianceCubeGenerated->GetResource(), ResourceState::CopySource, ResourceState::NonPixelShaderResource));
        barriers.push_back(Barrier::Transition(
            m_pIrradianceCube->GetResource(), ResourceState::CopyDest, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));

        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

        // Render modules expect resources coming in/going out to be in a shader read state
        barriers.clear();
        barriers.push_back(
            Barrier::Transition(m_pPrefilteredCubeGenerated->GetResource(), ResourceState::NonPixelShaderResource, ResourceState::CopySource));
        barriers.push_back(Barrier::Transition(
            m_pPrefilteredCube->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::CopyDest));

        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

        for (uint32_t array = 0; array < m_pPrefilteredCubeGenerated->GetDesc().DepthOrArraySize; array++)
        {
            for (uint32_t mip = 0; mip < m_pPrefilteredCubeGenerated->GetDesc().MipLevels; mip++)
            {
                TextureCopyDesc prefilteredCubeCopyDesc(m_pPrefilteredCubeGenerated->GetResource(), m_pPrefilteredCube->GetResource(), array, mip);
                CopyTextureRegion(pCmdList, &prefilteredCubeCopyDesc);
            }
        }

        barriers.clear();
        barriers.push_back(
            Barrier::Transition(m_pPrefilteredCubeGenerated->GetResource(), ResourceState::CopySource, ResourceState::NonPixelShaderResource));
        barriers.push_back(Barrier::Transition(
            m_pPrefilteredCube->GetResource(), ResourceState::CopyDest, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));

        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
    }

    // Set dynamic constant buffer
    BufferAddressInfo SkydomeBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SkydomeCBData), &m_SkydomeConstantData);
    m_pParametersApplySkydome->UpdateRootConstantBuffer(&SkydomeBufferInfo, 1); //b1 for SkydomeCBData included from skydomecommon.h

    // Render modules expect resources coming in/going out to be in a shader read state
    barriers.clear();
    barriers.push_back(Barrier::Transition(m_pRenderTarget->GetResource(),
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
        ResourceState::RenderTargetResource));
    barriers.push_back(Barrier::Transition(m_pDepthTarget->GetResource(),
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
        ResourceState::DepthRead | ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));

    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    //Sets render and depth target views
    BeginRaster(pCmdList, uint32_t(m_pRasterViews.size() - 1), m_pRasterViews.data(), m_pRasterViews[m_pRasterViews.size() - 1]);
    SetViewportScissorRect(pCmdList, 0, 0, m_pWidth, m_pHeight, 0.f, 1.f);
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

    // Bind all the parameters
    m_pParametersApplySkydome->Bind(pCmdList, m_pPipelineObjApplySkydome);
    SetPipelineState(pCmdList, m_pPipelineObjApplySkydome);

    DrawInstanced(pCmdList, 3);
    EndRaster(pCmdList);

    // Render modules expect resources coming in/going out to be in a shader read state
    barriers.clear();
    barriers.push_back(Barrier::Transition(
        m_pRenderTarget->GetResource(),
        ResourceState::RenderTargetResource,
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(m_pDepthTarget->GetResource(),
        ResourceState::DepthRead | ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));

    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}
void SkyDomeRenderModule::ExecuteSkydomeGeneration(cauldron::CommandList* pCmdList)
{
    SetAllResourceViewHeaps(pCmdList);

    UpdateSunDirection();
    // update local version of FullScreenScaleRatio for comparison later to know if it changed
    m_pUpscalerInfo.FullScreenScaleRatio = GetScene()->GetSceneInfo().UpscalerInfo.FullScreenScaleRatio;
    // Write to dynamic constant buffers and set
    BufferAddressInfo SkydomeBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SkydomeCBData), &m_SkydomeConstantData);
    BufferAddressInfo upscaleInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(UpscalerInformation), &m_pUpscalerInfo.FullScreenScaleRatio);
    BufferAddressInfo ProceduralBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(ProceduralCBData), &m_pProceduralConstantData);

    // EnvironmentCube
    {
        // Render modules expect resources coming in/going out to be in a shader read state
        std::vector<Barrier> barriers;
        barriers.push_back(Barrier::Transition(m_pSkyTextureGenerated->GetResource(), ResourceState::NonPixelShaderResource, ResourceState::UnorderedAccess));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

        m_pParametersEnvironmentCube->UpdateRootConstantBuffer(&upscaleInfo, 0);
        m_pParametersEnvironmentCube->UpdateRootConstantBuffer(&SkydomeBufferInfo, 1);
        m_pParametersEnvironmentCube->UpdateRootConstantBuffer(&ProceduralBufferInfo, 2);
        // Bind all the parameters
        m_pParametersEnvironmentCube->Bind(pCmdList, m_pPipelineObjEnvironmentCube);

        SetPipelineState(pCmdList, m_pPipelineObjEnvironmentCube);

        Dispatch(pCmdList, DivideRoundingUp(g_EnvironmentCubeX, g_NumThreadX), DivideRoundingUp(g_EnvironmentCubeY, g_NumThreadY), 6);

        // Render modules expect resources coming in/going out to be in a shader read state
        barriers.clear();
        barriers.push_back(Barrier::Transition(m_pSkyTextureGenerated->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
    }

    // IrradianceCube
    {
        // Render modules expect resources coming in/going out to be in a shader read state
        std::vector<Barrier> barriers;
        barriers.push_back(Barrier::Transition(m_pIrradianceCubeGenerated->GetResource(), ResourceState::NonPixelShaderResource, ResourceState::UnorderedAccess));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

        m_pParametersIrradianceCube->UpdateRootConstantBuffer(&upscaleInfo, 0);
        m_pParametersIrradianceCube->UpdateRootConstantBuffer(&SkydomeBufferInfo, 1);
        m_pParametersIrradianceCube->UpdateRootConstantBuffer(&ProceduralBufferInfo, 2);
        // Bind all the parameters
        m_pParametersIrradianceCube->Bind(pCmdList, m_pPipelineObjIrradianceCube);

        SetPipelineState(pCmdList, m_pPipelineObjIrradianceCube);

        Dispatch(pCmdList, DivideRoundingUp(g_IrradianceCubeX, g_NumThreadX), DivideRoundingUp(g_IrradianceCubeY, g_NumThreadY), 6);

        // Render modules expect resources coming in/going out to be in a shader read state
        barriers.clear();
        barriers.push_back(Barrier::Transition(m_pIrradianceCubeGenerated->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
    }

    // PrefilteredCube
    {
        // Render modules expect resources coming in/going out to be in a shader read state
        std::vector<Barrier> barriers;
        barriers.push_back(Barrier::Transition(m_pPrefilteredCubeGenerated->GetResource(), ResourceState::NonPixelShaderResource, ResourceState::UnorderedAccess));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

        for (int mip = 0; mip < g_PrefilterMipLevels; mip++)
        {
            uint32_t mipWidth  = static_cast<uint32_t>(g_PrefilteredCubeX * std::pow(0.5, mip));
            uint32_t mipHeight = static_cast<uint32_t>(g_PrefilteredCubeY * std::pow(0.5, mip));

            m_pParametersPrefilteredCube[mip]->UpdateRootConstantBuffer(&upscaleInfo, 0);
            m_pParametersPrefilteredCube[mip]->UpdateRootConstantBuffer(&SkydomeBufferInfo, 1);
            m_pParametersPrefilteredCube[mip]->UpdateRootConstantBuffer(&ProceduralBufferInfo, 2);

            m_pParametersPrefilteredCube[mip]->SetBufferSRV(m_pSampleDirections[mip], 1);
            m_pParametersPrefilteredCube[mip]->SetTextureUAV(m_pPrefilteredCubeGenerated, ViewDimension::Texture2DArray, 0, mip, 6, 0);

            // Bind all the parameters
            m_pParametersPrefilteredCube[mip]->Bind(pCmdList, m_pPipelineObjPrefilteredCube[mip]);

            SetPipelineState(pCmdList, m_pPipelineObjPrefilteredCube[mip]);

            Dispatch(pCmdList, DivideRoundingUp(mipWidth, g_NumThreadX), DivideRoundingUp(mipHeight, g_NumThreadY), 6);
        }

        // Render modules expect resources coming in/going out to be in a shader read state
        barriers.clear();
        barriers.push_back(Barrier::Transition(m_pPrefilteredCubeGenerated->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
    }

    CloseCmdList(pCmdList);

    std::vector<CommandList*> cmdLists;
    cmdLists.push_back(pCmdList);
    m_SignalValue = GetDevice()->ExecuteCommandLists(cmdLists, CommandQueue::Compute);

    std::function<void(void*)> waitOnQueue = [this](void*) 
    {
        GetDevice()->WaitOnQueue(this->m_SignalValue, CommandQueue::Compute);

        delete this->m_pComputeCmdList;

        this->m_CubemapGenerateReady.store(true);
        this->m_CubemapCopyReady.store(false);
    };
    Task waitOnQueueTask(waitOnQueue, nullptr);
    GetTaskManager()->AddTask(waitOnQueueTask);
}
