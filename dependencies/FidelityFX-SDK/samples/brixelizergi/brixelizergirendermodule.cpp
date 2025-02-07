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

#include "brixelizergirendermodule.h"

#include <functional>
#include <float.h>

#include "core/backend_interface.h"
#include "core/components/meshcomponent.h"
#include "core/components/animationcomponent.h"
#include "core/scene.h"
#include "core/loaders/textureloader.h"
#include "render/device.h"
#include "render/dynamicresourcepool.h"
#include "render/profiler.h"
#include "render/rootsignature.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "shaders/surfacerendercommon.h"
#include "shaders/lightingcommon.h"

using namespace std::experimental;
using namespace cauldron;

constexpr uint32_t g_NumNoiseTextures = 16;

struct GIConstants
{
    float DiffuseGIFactor;
    float SpecularGIFactor;
    int   MultiBounce;
    int   pad1;
};

void BrixelizerGIRenderModule::Init(const json& initData)
{
    m_PrevProjection = Mat4::Matrix4(0.0f);

    m_pColorTarget        = GetFramework()->GetColorTargetForCallback(GetName());
    m_pDiffuseTexture     = GetFramework()->GetRenderTexture(L"GBufferAlbedoRT");
    m_pDepthBuffer        = GetFramework()->GetRenderTexture(L"DepthTarget");
    m_pNormalTarget       = GetFramework()->GetRenderTexture(L"GBufferNormalRT");
    m_pVelocityBuffer     = GetFramework()->GetRenderTexture(L"GBufferMotionVectorRT");
    m_pRoughnessTarget    = GetFramework()->GetRenderTexture(L"GBufferAoRoughnessMetallicRT");
    m_pHistoryLitOutput   = GetFramework()->GetRenderTexture(L"HistoryLitOutput");
    m_pHistoryDepth       = GetFramework()->GetRenderTexture(L"HistoryDepth");
    m_pHistoryNormals     = GetFramework()->GetRenderTexture(L"HistoryNormals");
    m_pDiffuseGI          = GetFramework()->GetRenderTexture(L"DiffuseGI");
    m_pSpecularGI         = GetFramework()->GetRenderTexture(L"SpecularGI");
    m_pDebugVisualization = GetFramework()->GetRenderTexture(L"DebugVisualization");
    m_pLitOutputCopy      = GetFramework()->GetRenderTexture(L"LitOutputCopy");

    // Create SDF atlas texture
    {
        TextureDesc desc      = {};
        desc.Format           = ResourceFormat::R8_UNORM;
        desc.Flags            = ResourceFlags::AllowUnorderedAccess;
        desc.Width            = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
        desc.Height           = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
        desc.DepthOrArraySize = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;
        desc.Dimension        = TextureDimension::Texture3D;
        desc.MipLevels        = 1;
        desc.Name             = L"Brixelizer SDF Atlas";

        m_pSdfAtlas = GetDynamicResourcePool()->CreateTexture(&desc, ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource);
    }

    // Create brick AABBs buffer
    {
        BufferDesc desc = {};
        desc.Type       = BufferType::Data;
        desc.Flags      = ResourceFlags::AllowUnorderedAccess;
        desc.Size       = FFX_BRIXELIZER_BRICK_AABBS_SIZE;
        desc.Alignment  = 0;
        desc.Stride     = FFX_BRIXELIZER_BRICK_AABBS_STRIDE;
        desc.Name       = L"Brixelizer Brick AABB List";

        m_pBrickAABBs = GetDynamicResourcePool()->CreateBuffer(&desc, ResourceState::CommonResource);
    }

    // Create cascade AABB trees
    for (uint32_t i = 0; i < _countof(m_pCascadeAABBTrees); ++i)
    {
        wchar_t name[64] = {};
        swprintf(name, _countof(name), L"Brixelizer Cascade[%u] AABB Tree", i);

        BufferDesc desc = {};
        desc.Type       = BufferType::Data;
        desc.Flags      = ResourceFlags::AllowUnorderedAccess;
        desc.Size       = FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE;
        desc.Alignment  = 0;
        desc.Stride     = FFX_BRIXELIZER_CASCADE_AABB_TREE_STRIDE;
        desc.Name       = name;

        m_pCascadeAABBTrees[i] = GetDynamicResourcePool()->CreateBuffer(&desc, ResourceState::CommonResource);
    }

    // Create cascade brick maps
    for (uint32_t i = 0; i < _countof(m_pCascadeBrickMaps); ++i)
    {
        wchar_t name[64] = {};
        swprintf(name, _countof(name), L"Brixelizer Cascade[%u] Brick Map", i);

        BufferDesc desc = {};
        desc.Type       = BufferType::Data;
        desc.Flags      = ResourceFlags::AllowUnorderedAccess;
        desc.Size       = FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE;
        desc.Alignment  = 0;
        desc.Stride     = FFX_BRIXELIZER_CASCADE_BRICK_MAP_STRIDE;
        desc.Name       = name;

        m_pCascadeBrickMaps[i] = GetDynamicResourcePool()->CreateBuffer(&desc, ResourceState::CommonResource);
    }

    // Create scratch buffer
    {
        BufferDesc desc     = {};
        desc.Type           = BufferType::Data;
        desc.Flags          = ResourceFlags::AllowUnorderedAccess;
        desc.Size           = GPU_SCRATCH_BUFFER_SIZE;
        desc.Alignment      = 0;
        desc.Name           = L"Scratch Buffer";
        m_pGpuScratchBuffer = GetDynamicResourcePool()->CreateBuffer(&desc, ResourceState::CommonResource);
    }

    // Create example shader
    {
        SamplerDesc sdfAtlasSampler;
        sdfAtlasSampler.Filter   = FilterFunc::MinMagLinearMipPoint;
        sdfAtlasSampler.AddressU = AddressMode::Wrap;
        sdfAtlasSampler.AddressV = AddressMode::Wrap;
        sdfAtlasSampler.AddressW = AddressMode::Wrap;

        RootSignatureDesc rootSigDesc;
        rootSigDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
        rootSigDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);

        rootSigDesc.AddStaticSamplers(0, ShaderBindStage::Compute, 1, &sdfAtlasSampler);

        rootSigDesc.AddBufferSRVSet(0, ShaderBindStage::Compute, 1);
        rootSigDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);
        rootSigDesc.AddBufferSRVSet(2, ShaderBindStage::Compute, 24);
        rootSigDesc.AddBufferSRVSet(26, ShaderBindStage::Compute, 24);

        rootSigDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);

        m_pExampleRootSignature = RootSignature::CreateRootSignature(L"BrixelizerExamplePass_RootSignature", rootSigDesc);

        m_pExampleParameterSet = ParameterSet::CreateParameterSet(m_pExampleRootSignature);
        m_pExampleParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(BrixelizerExampleConstants), 0);
        m_pExampleParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(FfxBrixelizerContextInfo), 1);

        DefineList defineList;
        defineList.insert(std::make_pair(L"FFX_GPU", L"1"));
        defineList.insert(std::make_pair(L"FFX_HLSL", L"1"));
        defineList.insert(std::make_pair(L"FFX_HALF", L"1"));

        PipelineDesc pipelineDesc;
        pipelineDesc.SetRootSignature(m_pExampleRootSignature);
        pipelineDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"brixelizergiexample.hlsl", L"MainCS", ShaderModel::SM6_0, &defineList));

        m_pExamplePipeline = PipelineObject::CreatePipelineObject(L"BrixelizerExamplePass_PipelineObj", pipelineDesc);
    }

    // Create copy lit output shader
    {
        RootSignatureDesc rootSigDesc;
        rootSigDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);

        rootSigDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);

        m_pPassThroughRootSignature = RootSignature::CreateRootSignature(L"BrixelizerGICopyHistoryPass_RootSignature", rootSigDesc);

        m_pPassThroughParameterSet = ParameterSet::CreateParameterSet(m_pPassThroughRootSignature);

        DefineList defineList;

        PipelineDesc pipelineDesc;
        pipelineDesc.SetRootSignature(m_pPassThroughRootSignature);
        pipelineDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"copytexture.hlsl", L"CopyTextureCS", ShaderModel::SM6_0, &defineList));

        m_pPassThroughPipeline = PipelineObject::CreatePipelineObject(L"BrixelizerGICopyHistoryPass_PipelineObj", pipelineDesc);
    }

    // Deferred Lighting
    {
        // Root Signature
        RootSignatureDesc signatureDesc;
        signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);                         // scene information
        signatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);                         // scene lighting information
        signatureDesc.AddConstantBufferView(2, ShaderBindStage::Compute, 1);                         // IBL factor
        signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);                              // diffuse
        signatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);                              // normal
        signatureDesc.AddTextureSRVSet(2, ShaderBindStage::Compute, 1);                              // specular roughness
        signatureDesc.AddTextureSRVSet(3, ShaderBindStage::Compute, 1);                              // depth
        signatureDesc.AddTextureSRVSet(4, ShaderBindStage::Compute, 1);                              // brdfTexture
        signatureDesc.AddTextureSRVSet(5, ShaderBindStage::Compute, 1);                              // irradianceCube
        signatureDesc.AddTextureSRVSet(6, ShaderBindStage::Compute, 1);                              // prefilteredCube
        signatureDesc.AddTextureSRVSet(7, ShaderBindStage::Compute, MAX_SHADOW_MAP_TEXTURES_COUNT);  // shadow maps
        signatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);                              // ColorTarget output
        signatureDesc.AddTextureUAVSet(1, ShaderBindStage::Compute, 1);                              // DiffuseColorTarget output

        SamplerDesc pointSampler;  // default is enough
        pointSampler.Filter = FilterFunc::MinMagMipPoint;
        std::vector<SamplerDesc> samplers;
        samplers.push_back(pointSampler);
        signatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, static_cast<uint32_t>(samplers.size()), samplers.data());

        static bool s_InvertedDepth = GetConfig()->InvertedDepth;

        SamplerDesc comparisonSampler;
        comparisonSampler.Comparison    = s_InvertedDepth ? ComparisonFunc::GreaterEqual : ComparisonFunc::LessEqual;
        comparisonSampler.Filter        = FilterFunc::ComparisonMinMagLinearMipPoint;
        comparisonSampler.MaxAnisotropy = 1;
        samplers.clear();
        samplers.push_back(comparisonSampler);
        signatureDesc.AddStaticSamplers(1, ShaderBindStage::Compute, static_cast<uint32_t>(samplers.size()), samplers.data());

        // Setup samplers for brdfTexture, irradianceCube and prefilteredCube
        SamplerDesc brdfSampler;
        brdfSampler.AddressW      = AddressMode::Wrap;
        brdfSampler.Filter        = FilterFunc::MinMagMipLinear;
        brdfSampler.MaxAnisotropy = 1;

        samplers.clear();
        samplers.push_back(brdfSampler);
        signatureDesc.AddStaticSamplers(2, ShaderBindStage::Compute, static_cast<uint32_t>(samplers.size()), samplers.data());

        m_pDeferredLightingRootSignature = RootSignature::CreateRootSignature(L"LightingRenderModule_RootSignature", signatureDesc);

        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pDeferredLightingRootSignature);

        DefineList defineList;

        // Setup the shaders to build on the pipeline object
        std::wstring shaderPath = L"lightinggi.hlsl";
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderPath.c_str(), L"MainCS", ShaderModel::SM6_0, &defineList));

        m_pDeferredLightingPipeline = PipelineObject::CreatePipelineObject(L"LightingRenderModule_PipelineObj", psoDesc);

        // Create parameter set to bind constant buffer and texture
        m_pDeferredLightingParameterSet = ParameterSet::CreateParameterSet(m_pDeferredLightingRootSignature);

        // Update necessary scene frame information
        m_pDeferredLightingParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 0);
        m_pDeferredLightingParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneLightingInformation), 1);
        m_pDeferredLightingParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(GIConstants), 2);

        m_pDeferredLightingParameterSet->SetTextureSRV(m_pDiffuseTexture, ViewDimension::Texture2D, 0);
        m_pDeferredLightingParameterSet->SetTextureSRV(m_pNormalTarget, ViewDimension::Texture2D, 1);
        m_pDeferredLightingParameterSet->SetTextureSRV(m_pRoughnessTarget, ViewDimension::Texture2D, 2);
        m_pDeferredLightingParameterSet->SetTextureSRV(m_pDepthBuffer, ViewDimension::Texture2D, 3);
        m_pDeferredLightingParameterSet->SetTextureSRV(m_pDiffuseGI, ViewDimension::Texture2D, 5);
        m_pDeferredLightingParameterSet->SetTextureSRV(m_pSpecularGI, ViewDimension::Texture2D, 6);

        m_pDeferredLightingParameterSet->SetTextureUAV(m_pColorTarget, ViewDimension::Texture2D, 0);
        m_pDeferredLightingParameterSet->SetTextureUAV(m_pLitOutputCopy, ViewDimension::Texture2D, 1);

        ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
        for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
        {
            m_pDeferredLightingParameterSet->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, 7 + i);
        }
    }
    
    // Setup Cauldron FidelityFX interface.
    const size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(2);
    void*        scratchBuffer     = malloc(scratchBufferSize);
    memset(scratchBuffer, 0, scratchBufferSize);
    FfxErrorCode errorCode         = SDKWrapper::ffxGetInterface(&m_InitializationParameters.backendInterface, GetDevice(), scratchBuffer, scratchBufferSize, 2);
    CauldronAssert(ASSERT_CRITICAL, errorCode == FFX_OK, L"Could not initialize the FidelityFX SDK backend");

    if (errorCode == FFX_OK) {
        CauldronAssert(ASSERT_CRITICAL, m_InitializationParameters.backendInterface.fpGetSDKVersion(&m_InitializationParameters.backendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
                           L"FidelityFX Brixelizer GI sample requires linking with a 1.1.2 version SDK backend.");
    }

    CreateBrixelizerContext();
    CreateBrixelizerGIContext();

    GetContentManager()->AddContentListener(this);

    SetModuleEnabled(true);

    UISection* uiSection = GetUIManager()->RegisterUIElements("FFX Brixelizer GI", UISectionType::Sample);
    InitUI(uiSection);

    TextureLoadCompletionCallbackFn CompletionCallback = [this](const std::vector<const Texture*>& textures, void* additionalParams = nullptr) {
        this->TextureLoadComplete(textures, additionalParams);
    };

    // Load all noise textures.
    for (int i = 0; i < g_NumNoiseTextures; i++)
    {
        filesystem::path noiseTexturePath = filesystem::path("../media/Textures/Noise/LDR_RG01_" + std::to_string(i) + ".png");
        GetContentManager()->LoadTexture(TextureLoadInfo(noiseTexturePath), CompletionCallback);
    }

    GetFramework()->ConfigureRuntimeShaderRecompiler(
        // Pre reload callback
        [this]() {
            DeleteInstances();
            DeleteBrixelizerGIContext();
            DeleteBrixelizerContext();
        },
        // Post reload callback
        [this]() {
            CreateBrixelizerContext();
            CreateBrixelizerGIContext();
            m_Buffers.clear();
            FlushInstances(true);
        });
}

BrixelizerGIRenderModule::~BrixelizerGIRenderModule()
{
    DeleteBrixelizerGIContext();
    DeleteBrixelizerContext();

    free(m_InitializationParameters.backendInterface.scratchBuffer);

    delete m_pExampleRootSignature;
    delete m_pExampleParameterSet;
    delete m_pExamplePipeline;

    delete m_pPassThroughRootSignature;
    delete m_pPassThroughPipeline;

    delete m_pPassThroughParameterSet;

    delete m_pDeferredLightingRootSignature;
    delete m_pDeferredLightingParameterSet;
    delete m_pDeferredLightingPipeline;
}

void BrixelizerGIRenderModule::EnableModule(bool enabled)
{
    RenderModule::EnableModule(enabled);
    UpdateUIElementVisibility();
}

void BrixelizerGIRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);

    GPUScopedProfileCapture sampleMarker(pCmdList, L"Brixelizer GI");

    // Recreate the Brixelizer context if voxel size has been changed.
    if (m_InitializationParameters.cascadeDescs[1].voxelSize != m_MeshUnitSize * m_CascadeSizeRatio)
        RecreateBrixelizerContext();

    if (m_InitColorHistory) {
        DeferredLighting(pCmdList, false);

        CopyHistoryResources(pCmdList);

        m_InitColorHistory = false;
    }

    UpdateUIElementVisibility();

    UpdateConfig();

    // Create the dynamic instances every frame.
    FlushInstances(false);

    // Dispatch Brixelizer workloads.
    UpdateBrixelizerContext(pCmdList);

    // Restore the Cauldron resource view heaps.
    SetAllResourceViewHeaps(pCmdList);

    UpdateBrixelizerGIContext(pCmdList);

    SetAllResourceViewHeaps(pCmdList);

    CopyHistoryResources(pCmdList);

    if (m_OutputMode != OutputMode::DebugVisualization)
    {
        DeferredLighting(pCmdList, m_EnableGI);

        // Dispatch the Brixelizer Example Shader.
        if (m_OutputMode == OutputMode::ExampleShader)
            DispatchExampleShader(pCmdList);
        else
            VisualizeGIDebug(pCmdList);
    }

    ++m_FrameIndex;
}

void BrixelizerGIRenderModule::OnResize(const cauldron::ResolutionInfo& resInfo)
{
    RecreateBrixelizerContext();
    m_InitColorHistory = true;
}

void BrixelizerGIRenderModule::OnNewContentLoaded(ContentBlock* pContentBlock)
{
    MeshComponentMgr* pMeshComponentManager = MeshComponentMgr::Get();

    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);

    for (EntityDataBlock* pEntityData : pContentBlock->EntityDataBlocks)
    {
        for (Component* pComponent : pEntityData->Components)
        {
            if (pComponent->GetManager() == pMeshComponentManager)
            {
                const Mesh*  pMesh       = reinterpret_cast<MeshComponent*>(pComponent)->GetData().pMesh;
                const size_t numSurfaces = pMesh->GetNumSurfaces();
                for (uint32_t i = 0; i < numSurfaces; ++i)
                {
                    const Surface* pSurface = pMesh->GetSurface(i);

                    if (pSurface->HasTranslucency())
                    {
                        continue;
                    }

                    Entity* entity = pComponent->GetOwner();

                    BrixelizerInstanceInfo instanceInfo = {};

                    FfxBrixelizerInstanceDescription desc = {};

                    instanceInfo.entity     = entity;
                    instanceInfo.surface    = pSurface;
                    instanceInfo.instanceID = FFX_BRIXELIZER_INVALID_ID;
                    instanceInfo.isDynamic  = entity->HasComponent(AnimationComponentMgr::Get());

                    m_Instances.push_back(instanceInfo);
                }
            }
        }
    }

    FlushInstances(true);
}

void BrixelizerGIRenderModule::OnContentUnloaded(ContentBlock* pContentBlock)
{
    MeshComponentMgr* pMeshComponentManager = MeshComponentMgr::Get();

    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);

    std::vector<FfxBrixelizerInstanceID> instanceIDs;

    for (EntityDataBlock* pEntityData : pContentBlock->EntityDataBlocks)
    {
        for (Component* pComponent : pEntityData->Components)
        {
            if (pComponent->GetManager() == pMeshComponentManager)
            {
                const Mesh*  pMesh       = reinterpret_cast<MeshComponent*>(pComponent)->GetData().pMesh;
                const size_t numSurfaces = pMesh->GetNumSurfaces();
                for (uint32_t i = 0; i < numSurfaces; ++i)
                {
                    const Surface* pSurface = pMesh->GetSurface(i);

                    if (pSurface->HasTranslucency())
                    {
                        continue;
                    }

                    Entity* entity = pComponent->GetOwner();

                    for (uint32_t i = 0; i < m_Instances.size(); ++i)
                    {
                        BrixelizerInstanceInfo* instance = &m_Instances[i];
                        if (instance->entity == entity)
                        {
                            if (!instance->isDynamic && instance->instanceID != FFX_BRIXELIZER_INVALID_ID)
                            {
                                instanceIDs.push_back(instance->instanceID);
                            }
                            m_Instances[i] = m_Instances[m_Instances.size() - 1];
                            m_Instances.pop_back();
                            break;
                        }
                    }
                }
            }
        }
    }

    if (instanceIDs.size())
    {
        FfxErrorCode errorCode = ffxBrixelizerDeleteInstances(&m_BrixelizerContext, instanceIDs.data(), (uint32_t)instanceIDs.size());
        CauldronAssert(AssertLevel::ASSERT_ERROR, errorCode == FFX_OK, L"Failed call to ffxBrixelizerDeleteInstances.");
    }
}

void BrixelizerGIRenderModule::FlushInstances(bool flushStaticInstances)
{
    std::vector<FfxBrixelizerInstanceDescription> instanceDescs;

    for (uint32_t i = 0; i < m_Instances.size(); ++i)
    {
        BrixelizerInstanceInfo* info = &m_Instances[i];

        if ((!flushStaticInstances && !info->isDynamic) || (info->instanceID != FFX_BRIXELIZER_INVALID_ID))
        {
            continue;
        }

        const Mat4& transform  = info->entity->GetTransform();
        const Vec4  c          = info->surface->Center();
        const Vec4  r          = info->surface->Radius();
        const Vec4  aabbMinVec = c - r;
        const Vec4  aabbMaxVec = c + r;
        const Vec4  extents    = aabbMaxVec - aabbMinVec;

        const Vec4 aabbCorners[8] = {
            aabbMinVec + Vec4(0.0f, 0.0f, 0.0f, 0.0f),
            aabbMinVec + Vec4(extents.getX(), 0.0f, 0.0f, 0.0f),
            aabbMinVec + Vec4(0.0f, 0.0f, extents.getZ(), 0.0f),
            aabbMinVec + Vec4(extents.getX(), 0.0f, extents.getZ(), 0.0f),
            aabbMinVec + Vec4(0.0f, extents.getY(), 0.0f, 0.0f),
            aabbMinVec + Vec4(extents.getX(), extents.getY(), 0.0f, 0.0f),
            aabbMinVec + Vec4(0.0f, extents.getY(), extents.getZ(), 0.0f),
            aabbMinVec + Vec4(extents.getX(), extents.getY(), extents.getZ(), 0.0f),
        };

        Vec4 minExtents = Vec4(INFINITY, INFINITY, INFINITY, INFINITY);
        Vec4 maxExtents = Vec4(-INFINITY, -INFINITY, -INFINITY, -INFINITY);

        for (uint32_t i = 0; i < 8; i++)
        {
            minExtents = MinPerElement(minExtents, transform * aabbCorners[i]);
            maxExtents = MaxPerElement(maxExtents, transform * aabbCorners[i]);
        }

        const VertexBufferInformation* vertexBufferInfo = NULL;
        if (info->isDynamic)
        {
            const AnimationComponentData* data = info->entity->GetComponent<const AnimationComponent>(AnimationComponentMgr::Get())->GetData();
            if (data->m_skinId != -1)
            {
                vertexBufferInfo = &data->m_skinnedPositions[info->surface->GetSurfaceID()];
            }
        }
        if (!vertexBufferInfo)
        {
            vertexBufferInfo = &info->surface->GetVertexBuffer(VertexAttributeType::Position);
        }

        const IndexBufferInformation& indexBufferInfo = info->surface->GetIndexBuffer();

        Buffer* vertexBuffer = vertexBufferInfo->pBuffer;
        Buffer* indexBuffer  = indexBufferInfo.pBuffer;

        uint32_t vertexBufferIndex = GetBufferIndex(vertexBuffer);
        uint32_t indexBufferIndex  = GetBufferIndex(indexBuffer);

        FfxBrixelizerInstanceDescription instanceDesc = {};

        for (uint32_t i = 0; i < 3; ++i)
        {
            instanceDesc.aabb.min[i] = minExtents[i];
            instanceDesc.aabb.max[i] = maxExtents[i];
        }
        
        for (uint32_t row = 0; row < 3; ++row) {
            for (uint32_t col = 0; col < 4; ++col) {
                instanceDesc.transform[row*4 + col] = transform.getCol(col)[row];
            }
        }

        instanceDesc.indexFormat       = indexBufferInfo.IndexFormat == ResourceFormat::R16_UINT ? FFX_INDEX_TYPE_UINT16 : FFX_INDEX_TYPE_UINT32;
        instanceDesc.indexBuffer       = indexBufferIndex;
        instanceDesc.indexBufferOffset = 0;
        instanceDesc.triangleCount     = indexBufferInfo.Count / 3;

        instanceDesc.vertexBuffer = vertexBufferIndex;
        CauldronAssert(AssertLevel::ASSERT_ERROR,
                       vertexBufferInfo->AttributeDataFormat == AttributeFormat::Vec3,
                       L"Unexpected vertex buffer format submitted to Brixelizer");
        instanceDesc.vertexStride       = 3 * sizeof(float);
        instanceDesc.vertexBufferOffset = 0;
        instanceDesc.vertexCount        = vertexBufferInfo->Count;
        instanceDesc.vertexFormat       = FFX_SURFACE_FORMAT_R32G32B32_FLOAT;

        instanceDesc.outInstanceID = &info->instanceID;
        instanceDesc.flags         = info->isDynamic ? FFX_BRIXELIZER_INSTANCE_FLAG_DYNAMIC : FFX_BRIXELIZER_INSTANCE_FLAG_NONE;

        instanceDescs.push_back(instanceDesc);
    }

    FfxErrorCode errorCode = ffxBrixelizerCreateInstances(&m_BrixelizerContext, instanceDescs.data(), static_cast<uint32_t>(instanceDescs.size()));
    CauldronAssert(AssertLevel::ASSERT_ERROR, errorCode == FFX_OK, L"Failed call to ffxBrixelizerCreateInstances.");
}

void BrixelizerGIRenderModule::DeleteInstances()
{
    std::vector<FfxBrixelizerInstanceID> instanceIDs;
    for (BrixelizerInstanceInfo& instance : m_Instances)
    {
        if (instance.instanceID != FFX_BRIXELIZER_INVALID_ID)
        {
            instanceIDs.push_back(instance.instanceID);
            instance.instanceID = FFX_BRIXELIZER_INVALID_ID;
        }
    }

    if (instanceIDs.size())
    {
        FfxErrorCode errorCode = ffxBrixelizerDeleteInstances(&m_BrixelizerContext, instanceIDs.data(), (uint32_t)instanceIDs.size());
        CauldronAssert(AssertLevel::ASSERT_ERROR, errorCode == FFX_OK, L"Failed call to ffxBrixelizerDeleteInstances.");
    }
}

void BrixelizerGIRenderModule::CreateBrixelizerContext()
{
    m_InitializationParameters.sdfCenter[0] = 0.0f;
    m_InitializationParameters.sdfCenter[1] = 0.0f;
    m_InitializationParameters.sdfCenter[2] = 0.0f;
    m_InitializationParameters.flags        = FFX_BRIXELIZER_CONTEXT_FLAG_ALL_DEBUG;
    m_InitializationParameters.numCascades  = NUM_BRIXELIZER_CASCADES;

    float voxelSize = m_MeshUnitSize;
    for (uint32_t i = 0; i < m_InitializationParameters.numCascades; ++i)
    {
        FfxBrixelizerCascadeDescription* cascadeDesc = &m_InitializationParameters.cascadeDescs[i];
        cascadeDesc->flags                    = (FfxBrixelizerCascadeFlag)(FFX_BRIXELIZER_CASCADE_STATIC | FFX_BRIXELIZER_CASCADE_DYNAMIC);
        cascadeDesc->voxelSize                = voxelSize;
        voxelSize *= m_CascadeSizeRatio;
    }

    m_InitializationParameters.backendInterface = m_InitializationParameters.backendInterface;

    FfxErrorCode errorCode = ffxBrixelizerContextCreate(&m_InitializationParameters, &m_BrixelizerContext);
    CauldronAssert(AssertLevel::ASSERT_ERROR, errorCode == FFX_OK, L"Failed to create Brixelizer context.");

    if (errorCode == FFX_OK) {
        CauldronAssert(ASSERT_CRITICAL, ffxBrixelizerGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 0, 0),
                           L"FidelityFX Brixelizer GI sample requires linking with a 1.0 version Brixelizer library.");
    }
}

void BrixelizerGIRenderModule::DeleteBrixelizerContext()
{
    FfxErrorCode errorCode = ffxBrixelizerContextDestroy(&m_BrixelizerContext);
    CauldronAssert(AssertLevel::ASSERT_ERROR, errorCode == FFX_OK, L"Failed to delete Brixelizer context.");
}

void BrixelizerGIRenderModule::RecreateBrixelizerContext()
{
    GetFramework()->GetDevice()->FlushAllCommandQueues();

    DeleteInstances();
    DeleteBrixelizerGIContext();
    DeleteBrixelizerContext();
    CreateBrixelizerContext();
    CreateBrixelizerGIContext();
    m_Buffers.clear();
    FlushInstances(true);
}

void BrixelizerGIRenderModule::UpdateBrixelizerContext(cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture updateMarker(pCmdList, L"Brixelizer Update");

    // Transition all resources to resource state expected by Brixelizer
    {
        std::vector<Barrier> barriers;

        if (m_pGpuScratchBuffer->GetResource()->GetCurrentResourceState() != ResourceState::UnorderedAccess)
            barriers.push_back(Barrier::Transition(m_pGpuScratchBuffer->GetResource(), m_pGpuScratchBuffer->GetResource()->GetCurrentResourceState(), ResourceState::UnorderedAccess));

        if (m_pColorTarget->GetResource()->GetCurrentResourceState() == ResourceState::UnorderedAccess)
            barriers.push_back(Barrier::Transition(m_pColorTarget->GetResource(), m_pColorTarget->GetResource()->GetCurrentResourceState(), ResourceState::UnorderedAccess));
        
        barriers.push_back(Barrier::Transition(m_pSdfAtlas->GetResource(), m_pSdfAtlas->GetResource()->GetCurrentResourceState(), ResourceState::UnorderedAccess));
        barriers.push_back(Barrier::Transition(m_pBrickAABBs->GetResource(), m_pBrickAABBs->GetResource()->GetCurrentResourceState(), ResourceState::UnorderedAccess));

        for (const Buffer* aabbTree : m_pCascadeAABBTrees)
            barriers.push_back(Barrier::Transition(aabbTree->GetResource(), aabbTree->GetResource()->GetCurrentResourceState(), ResourceState::UnorderedAccess));

        for (const Buffer* brickMap : m_pCascadeBrickMaps)
            barriers.push_back(Barrier::Transition(brickMap->GetResource(), brickMap->GetResource()->GetCurrentResourceState(), ResourceState::UnorderedAccess));

        ResourceBarrier(pCmdList, (uint32_t)barriers.size(), barriers.data());
    }

    FfxBrixelizerStats stats = {};

    // Fill out the Brixelizer update description.
    FfxBrixelizerUpdateDescription updateDesc = {};

    // Pass in the externally created output resources as FfxResource objects.
    updateDesc.resources.sdfAtlas =
        SDKWrapper::ffxGetResource(m_pSdfAtlas->GetResource(), (wchar_t*)m_pSdfAtlas->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    updateDesc.resources.brickAABBs =
        SDKWrapper::ffxGetResource(m_pBrickAABBs->GetResource(), (wchar_t*)m_pBrickAABBs->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_UNORDERED_ACCESS);

    for (uint32_t i = 0; i < FFX_BRIXELIZER_MAX_CASCADES; ++i)
    {
        updateDesc.resources.cascadeResources[i].aabbTree = SDKWrapper::ffxGetResource(m_pCascadeAABBTrees[i]->GetResource(), (wchar_t*)m_pCascadeAABBTrees[i]->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_UNORDERED_ACCESS);
        updateDesc.resources.cascadeResources[i].brickMap = SDKWrapper::ffxGetResource(m_pCascadeBrickMaps[i]->GetResource(), (wchar_t*)m_pCascadeBrickMaps[i]->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    updateDesc.frameIndex              = m_FrameIndex;
    updateDesc.debugVisualizationDesc  = nullptr;
    updateDesc.populateDebugAABBsFlags = FFX_BRIXELIZER_POPULATE_AABBS_NONE;
    updateDesc.maxReferences           = 32 * (1 << 20);
    updateDesc.maxBricksPerBake        = 1 << 14;
    updateDesc.triangleSwapSize        = 300 * (1 << 20);
    updateDesc.outStats                = &stats;

    for (uint32_t i = 0; i < 3; ++i)
        updateDesc.sdfCenter[i] = m_SdfCenter[i];

    FfxBrixelizerDebugVisualizationDescription debugVisDesc = {};

    if (m_OutputMode == OutputMode::DebugVisualization)
        SetupDebugVisualization(updateDesc, debugVisDesc);

    FfxResource ffxGpuScratchBuffer = SDKWrapper::ffxGetResource(m_pGpuScratchBuffer->GetResource(), L"Scratch Buffer", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    ffxGpuScratchBuffer.description.stride = sizeof(uint32_t);

    size_t scratchBufferSize        = 0;
    updateDesc.outScratchBufferSize = &scratchBufferSize;
    ffxBrixelizerBakeUpdate(&m_BrixelizerContext, &updateDesc, &m_BrixelizerBakedUpdateDesc);
    CauldronAssert(
        AssertLevel::ASSERT_ERROR, scratchBufferSize < GPU_SCRATCH_BUFFER_SIZE, L"Required Brixelizer scratch memory size larger than available GPU buffer.");

    ffxBrixelizerUpdate(&m_BrixelizerContext, &m_BrixelizerBakedUpdateDesc, ffxGpuScratchBuffer, SDKWrapper::ffxGetCommandList(pCmdList));

    // Transition all resources to resource state expected after Brixelizer
    {
        std::vector<Barrier> barriers;

        barriers.push_back(Barrier::Transition(m_pSdfAtlas->GetResource(), m_pSdfAtlas->GetResource()->GetCurrentResourceState(), ResourceState::NonPixelShaderResource));
        barriers.push_back(Barrier::Transition(m_pBrickAABBs->GetResource(), m_pBrickAABBs->GetResource()->GetCurrentResourceState(), ResourceState::NonPixelShaderResource));

        for (const Buffer* aabbTree : m_pCascadeAABBTrees)
            barriers.push_back(Barrier::Transition(aabbTree->GetResource(), aabbTree->GetResource()->GetCurrentResourceState(), ResourceState::NonPixelShaderResource));

        for (const Buffer* brickMap : m_pCascadeBrickMaps)
            barriers.push_back(Barrier::Transition(brickMap->GetResource(), brickMap->GetResource()->GetCurrentResourceState(), ResourceState::NonPixelShaderResource));

        ResourceBarrier(pCmdList, (uint32_t)barriers.size(), barriers.data());
    }

    // Draw Brixelizer stats
    {
        if (m_ResetStats)
        {
            m_ResetStats           = false;
            m_MaxStaticTriangles   = 0;
            m_MaxStaticReferences  = 0;
            m_MaxStaticBricks      = 0;
            m_MaxDynamicTriangles  = 0;
            m_MaxDynamicReferences = 0;
            m_MaxDynamicBricks     = 0;
        }

        uint64_t freeBricks    = stats.contextStats.freeBricks;
        m_MaxStaticBricks      = std::max(m_MaxStaticBricks, (uint64_t)stats.staticCascadeStats.bricksAllocated);
        m_MaxStaticReferences  = std::max(m_MaxStaticReferences, (uint64_t)stats.staticCascadeStats.referencesAllocated);
        m_MaxStaticTriangles   = std::max(m_MaxStaticTriangles, (uint64_t)stats.staticCascadeStats.trianglesAllocated);
        m_MaxDynamicBricks     = std::max(m_MaxDynamicBricks, (uint64_t)stats.dynamicCascadeStats.bricksAllocated);
        m_MaxDynamicReferences = std::max(m_MaxDynamicReferences, (uint64_t)stats.dynamicCascadeStats.referencesAllocated);
        m_MaxDynamicTriangles  = std::max(m_MaxDynamicTriangles, (uint64_t)stats.dynamicCascadeStats.trianglesAllocated);

        char buffer[1024] = {};

        snprintf(buffer, _countof(buffer), "Free Bricks:             %8llu", freeBricks);
        m_FreeBricksTextElement->SetDesc(buffer);
        snprintf(buffer, _countof(buffer), "Max Static Bricks:       %8llu", m_MaxStaticBricks);
        m_StaticBricksTextElement->SetDesc(buffer);
        snprintf(buffer, _countof(buffer), "Max Static Triangles:    %8llu", m_MaxStaticTriangles);
        m_StaticTrianglesTextElement->SetDesc(buffer);
        snprintf(buffer, _countof(buffer), "Max Static References:   %8llu", m_MaxStaticReferences);
        m_StaticReferencesTextElement->SetDesc(buffer);
        snprintf(buffer, _countof(buffer), "Max Dynamic Bricks:      %8llu", m_MaxDynamicBricks);
        m_DynamicBricksTextElement->SetDesc(buffer);
        snprintf(buffer, _countof(buffer), "Max Dynamic Triangles:   %8llu", m_MaxDynamicTriangles);
        m_DynamicTrianglesTextElement->SetDesc(buffer);
        snprintf(buffer, _countof(buffer), "Max Dynamic References:  %8llu", m_MaxDynamicReferences);
        m_DynamicReferencesTextElement->SetDesc(buffer);
    }
}

void BrixelizerGIRenderModule::SetupDebugVisualization(FfxBrixelizerUpdateDescription& updateDesc, FfxBrixelizerDebugVisualizationDescription& debugVisDesc)
{
    ResolutionInfo   resInfo           = GetFramework()->GetResolutionInfo();
    CameraComponent* camera            = GetScene()->GetCurrentCamera();
    const Mat4&      inverseView       = camera->GetInverseView();
    const Mat4&      inverseProjection = camera->GetInverseProjection();

    memcpy(&debugVisDesc.inverseViewMatrix, &inverseView, sizeof(debugVisDesc.inverseViewMatrix));
    memcpy(&debugVisDesc.inverseProjectionMatrix, &inverseProjection, sizeof(debugVisDesc.inverseProjectionMatrix));

    switch (m_DebugVisOutputType)
    {
    case DebugVisOutputType::Distance:
        debugVisDesc.debugState = FFX_BRIXELIZER_TRACE_DEBUG_MODE_DISTANCE;
        break;
    case DebugVisOutputType::UVW:
        debugVisDesc.debugState = FFX_BRIXELIZER_TRACE_DEBUG_MODE_UVW;
        break;
    case DebugVisOutputType::Iterations:
        debugVisDesc.debugState = FFX_BRIXELIZER_TRACE_DEBUG_MODE_ITERATIONS;
        break;
    case DebugVisOutputType::Gradient:
        debugVisDesc.debugState = FFX_BRIXELIZER_TRACE_DEBUG_MODE_GRAD;
        break;
    case DebugVisOutputType::BrickID:
        debugVisDesc.debugState = FFX_BRIXELIZER_TRACE_DEBUG_MODE_BRICK_ID;
        break;
    case DebugVisOutputType::CascadeID:
        debugVisDesc.debugState = FFX_BRIXELIZER_TRACE_DEBUG_MODE_CASCADE_ID;
        break;
    default:
        CauldronAssert(AssertLevel::ASSERT_ERROR, false, L"Unknown debug visualization output type.");
        break;
    }

    uint32_t cascadeIndexOffset = 0;
    switch (m_CascadeType)
    {
    case CascadeType::Static:
        cascadeIndexOffset = 0;
        break;
    case CascadeType::Dynamic:
        cascadeIndexOffset = NUM_BRIXELIZER_CASCADES;
        break;
    case CascadeType::Merged:
        cascadeIndexOffset = 2 * NUM_BRIXELIZER_CASCADES;
        break;
    default:
        CauldronAssert(AssertLevel::ASSERT_ERROR, false, L"Unknown cascade type.");
        break;
    }

    debugVisDesc.startCascadeIndex = cascadeIndexOffset + m_StartCascadeIdx;
    debugVisDesc.endCascadeIndex   = cascadeIndexOffset + m_EndCascadeIdx;

    debugVisDesc.tMin         = m_TMin;
    debugVisDesc.tMax         = m_TMax;
    debugVisDesc.sdfSolveEps  = m_SdfSolveEps;
    debugVisDesc.renderWidth  = resInfo.RenderWidth;
    debugVisDesc.renderHeight = resInfo.RenderHeight;
    debugVisDesc.output       = SDKWrapper::ffxGetResource(m_pColorTarget->GetResource(), L"Color Target", FFX_RESOURCE_STATE_UNORDERED_ACCESS);

    FfxBrixelizerPopulateDebugAABBsFlags populateDebugAABBFlags = FFX_BRIXELIZER_POPULATE_AABBS_NONE;

    if (m_ShowStaticInstanceAABBs)
        populateDebugAABBFlags = (FfxBrixelizerPopulateDebugAABBsFlags)(populateDebugAABBFlags | FFX_BRIXELIZER_POPULATE_AABBS_STATIC_INSTANCES);

    if (m_ShowDynamicInstanceAABBs)
        populateDebugAABBFlags = (FfxBrixelizerPopulateDebugAABBsFlags)(populateDebugAABBFlags | FFX_BRIXELIZER_POPULATE_AABBS_DYNAMIC_INSTANCES);

    if (m_ShowCascadeAABBs)
        populateDebugAABBFlags = (FfxBrixelizerPopulateDebugAABBsFlags)(populateDebugAABBFlags | FFX_BRIXELIZER_POPULATE_AABBS_CASCADE_AABBS);

    if (m_ShowAABBTreeIndex != -1)
        debugVisDesc.cascadeDebugAABB[2 * NUM_BRIXELIZER_CASCADES + m_ShowAABBTreeIndex] = FFX_BRIXELIZER_CASCADE_DEBUG_AABB_AABB_TREE;

    updateDesc.debugVisualizationDesc  = &debugVisDesc;
    updateDesc.populateDebugAABBsFlags = populateDebugAABBFlags;
}

void BrixelizerGIRenderModule::DispatchExampleShader(cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture exampleMarker(pCmdList, L"Brixelizer Example");

    const ResourceState currentState = m_pColorTarget->GetResource()->GetCurrentResourceState();

    if (currentState != ResourceState::UnorderedAccess)
    {
        Barrier barrier = currentState == ResourceState::UnorderedAccess
                              ? Barrier::UAV(m_pColorTarget->GetResource())
                              : Barrier::Transition(m_pColorTarget->GetResource(), currentState, ResourceState::UnorderedAccess);

        ResourceBarrier(pCmdList, 1, &barrier);
    }

    FfxBrixelizerContextInfo contextInfo = {};
    FfxErrorCode             error       = ffxBrixelizerGetContextInfo(&m_BrixelizerContext, &contextInfo);
    CauldronAssert(AssertLevel::ASSERT_ERROR, error == FFX_OK, L"Failed to get Brixelizer context info.");

    BrixelizerExampleConstants constants = {};

    constants.SolveEpsilon = m_SdfSolveEps;
    constants.TMin         = m_TMin;
    constants.TMax         = m_TMax;
    constants.State        = (uint32_t)m_ExampleOutputType;

    CameraComponent* camera            = GetScene()->GetCurrentCamera();
    const Mat4&      inverseView       = camera->GetInverseView();
    const Mat4&      inverseProjection = camera->GetInverseProjection();

    memcpy(&constants.InvView, &inverseView, sizeof(constants.InvView));
    memcpy(&constants.InvProj, &inverseProjection, sizeof(constants.InvProj));

    // Always use the merged cascades for the example shader.
    uint32_t offset = 2 * NUM_BRIXELIZER_CASCADES;

    constants.StartCascadeID = m_StartCascadeIdx + offset;
    constants.EndCascadeID   = m_EndCascadeIdx + offset;

    if (m_ShowBrickOutlines)
        constants.Flags |= BRIXELIZER_EXAMPLE_SHOW_BRICK_OUTLINES;

    constants.Alpha = m_Alpha;

    BufferAddressInfo exampleConstantBuffer = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(constants), &constants);
    BufferAddressInfo contextConstantBuffer = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(contextInfo), &contextInfo);

    m_pExampleParameterSet->UpdateRootConstantBuffer(&exampleConstantBuffer, 0);
    m_pExampleParameterSet->UpdateRootConstantBuffer(&contextConstantBuffer, 1);

    // Bind the resources needed by Brixelizer.
    m_pExampleParameterSet->SetBufferSRV(m_pBrickAABBs, 0);
    m_pExampleParameterSet->SetTextureSRV(m_pSdfAtlas, ViewDimension::Texture3D, 1);

    for (uint32_t i = 0; i < _countof(m_pCascadeAABBTrees); ++i)
        m_pExampleParameterSet->SetBufferSRV(m_pCascadeAABBTrees[i], 2 + i);

    for (uint32_t i = 0; i < _countof(m_pCascadeBrickMaps); ++i)
        m_pExampleParameterSet->SetBufferSRV(m_pCascadeBrickMaps[i], 26 + i);

    m_pExampleParameterSet->SetTextureUAV(m_pColorTarget, ViewDimension::Texture2D, 0);

    m_pExampleParameterSet->Bind(pCmdList, m_pExamplePipeline);
    SetPipelineState(pCmdList, m_pExamplePipeline);

    TextureDesc colorTargetDesc = m_pColorTarget->GetDesc();
    Dispatch(pCmdList, (colorTargetDesc.Width + 7) / 8, (colorTargetDesc.Height + 7) / 8, 1);

    // Render modules expect resources coming in/going out to be in a shader read state
    {
        Barrier barriers[] = {Barrier::Transition(m_pColorTarget->GetResource(), ResourceState::UnorderedAccess, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource)};
        ResourceBarrier(pCmdList, 1, barriers);
    }
}

void BrixelizerGIRenderModule::CreateBrixelizerGIContext()
{
    TextureDesc colorTargetDesc = m_pColorTarget->GetDesc();

    FfxBrixelizerGIContextDescription desc = {};

    desc.flags              = FFX_BRIXELIZER_GI_FLAG_DEPTH_INVERTED;
    desc.internalResolution = FFX_BRIXELIZER_GI_INTERNAL_RESOLUTION_50_PERCENT;
    desc.displaySize        = {colorTargetDesc.Width, colorTargetDesc.Height};
    desc.backendInterface   = m_InitializationParameters.backendInterface;

    FfxErrorCode errorCode = ffxBrixelizerGIContextCreate(&m_BrixelizerGIContext, &desc);
    CauldronAssert(AssertLevel::ASSERT_ERROR, errorCode == FFX_OK, L"Failed to create Brixelizer GI context.");

    if (errorCode == FFX_OK) {
        CauldronAssert(ASSERT_CRITICAL, ffxBrixelizerGIGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 0, 0),
                           L"FidelityFX Brixelizer GI sample requires linking with a 1.0 version Brixelizer GI library.");
    }
}

void BrixelizerGIRenderModule::DeleteBrixelizerGIContext()
{
    FfxErrorCode errorCode = ffxBrixelizerGIContextDestroy(&m_BrixelizerGIContext);
    CauldronAssert(AssertLevel::ASSERT_ERROR, errorCode == FFX_OK, L"Failed to delete Brixelizer GI context.");
}

void BrixelizerGIRenderModule::RecreateBrixelizerGIContext()
{
    GetFramework()->GetDevice()->FlushAllCommandQueues();
    DeleteBrixelizerGIContext();
    CreateBrixelizerGIContext();
}

void BrixelizerGIRenderModule::UpdateBrixelizerGIContext(cauldron::CommandList* pCmdList)
{
    CameraComponent* camera = GetScene()->GetCurrentCamera();

    const Mat4& view       = camera->GetView();
    const Mat4& projection = camera->GetProjection();
    const Mat4& prevView   = camera->GetPreviousView();

    {
        GPUScopedProfileCapture marker(pCmdList, L"Brixelizer GI Update");

        const ResourceState diffuseGIState    = m_pDiffuseGI->GetResource()->GetCurrentResourceState();
        const ResourceState specularGIState   = m_pSpecularGI->GetResource()->GetCurrentResourceState();

        {
            Barrier barriers[] = {Barrier::Transition(m_pDiffuseGI->GetResource(), diffuseGIState, ResourceState::NonPixelShaderResource),
                                  Barrier::Transition(m_pSpecularGI->GetResource(), specularGIState, ResourceState::NonPixelShaderResource)};
            ResourceBarrier(pCmdList, 2, barriers);
        }

        m_pEnivornmentMap = GetScene()->GetIBLTexture(IBLTexture::Prefiltered);

        const SceneLightingInformation& lightingInfo = GetScene()->GetSceneLightInfo();

        memcpy(&m_GIDispatchDesc.view,           &view,             sizeof(m_GIDispatchDesc.view));
        memcpy(&m_GIDispatchDesc.projection,     &projection,       sizeof(m_GIDispatchDesc.projection));
        memcpy(&m_GIDispatchDesc.prevView,       &prevView,         sizeof(m_GIDispatchDesc.prevView));
        memcpy(&m_GIDispatchDesc.prevProjection, &m_PrevProjection, sizeof(m_GIDispatchDesc.prevProjection));
        m_PrevProjection = projection;

        memcpy(&m_GIDispatchDesc.cameraPosition, &camera->GetCameraPos(), sizeof(m_GIDispatchDesc.cameraPosition));

        m_GIDispatchDesc.startCascade        = m_StartCascadeIdx + (2 * NUM_BRIXELIZER_CASCADES);
        m_GIDispatchDesc.endCascade          = m_EndCascadeIdx + (2 * NUM_BRIXELIZER_CASCADES);
        m_GIDispatchDesc.rayPushoff          = m_RayPushoff;
        m_GIDispatchDesc.sdfSolveEps         = m_SdfSolveEps;
        m_GIDispatchDesc.specularRayPushoff  = m_RayPushoff;
        m_GIDispatchDesc.specularSDFSolveEps = m_SdfSolveEps;
        m_GIDispatchDesc.tMin                = m_TMin;
        m_GIDispatchDesc.tMax                = m_TMax;

        m_GIDispatchDesc.normalsUnpackMul        = 2.0f;
        m_GIDispatchDesc.normalsUnpackAdd        = -1.0f;
        m_GIDispatchDesc.isRoughnessPerceptual   = false;
        m_GIDispatchDesc.roughnessChannel        = 1;
        m_GIDispatchDesc.roughnessThreshold      = 0.9f;
        m_GIDispatchDesc.environmentMapIntensity = 0.1f;
        m_GIDispatchDesc.motionVectorScale       = { 1.0f, 1.0f};    

        m_GIDispatchDesc.depth         = SDKWrapper::ffxGetResource(m_pDepthBuffer->GetResource(), L"Depth");
        m_GIDispatchDesc.normal        = SDKWrapper::ffxGetResource(m_pNormalTarget->GetResource(), L"Normal");
        m_GIDispatchDesc.roughness     = SDKWrapper::ffxGetResource(m_pRoughnessTarget->GetResource(), L"Roughness");
        m_GIDispatchDesc.motionVectors = SDKWrapper::ffxGetResource(m_pVelocityBuffer->GetResource(), L"MotionVectors");

        m_GIDispatchDesc.historyDepth     = SDKWrapper::ffxGetResource(m_pHistoryDepth->GetResource(), L"HistoryDepth");
        m_GIDispatchDesc.historyNormal    = SDKWrapper::ffxGetResource(m_pHistoryNormals->GetResource(), L"HistoryNormal");
        m_GIDispatchDesc.prevLitOutput    = SDKWrapper::ffxGetResource(m_pHistoryLitOutput->GetResource(), L"PrevLitOutput");
        m_GIDispatchDesc.noiseTexture     = SDKWrapper::ffxGetResource(m_NoiseTextures[m_FrameIndex % g_NumNoiseTextures]->GetResource(), L"NoiseTexture");
        m_GIDispatchDesc.environmentMap   = SDKWrapper::ffxGetResource(m_pEnivornmentMap->GetResource(), L"EnvironmentMap");

        m_GIDispatchDesc.sdfAtlas    = SDKWrapper::ffxGetResource(m_pSdfAtlas->GetResource(), (wchar_t*)m_pSdfAtlas->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_COMPUTE_READ);
        m_GIDispatchDesc.bricksAABBs = SDKWrapper::ffxGetResource(m_pBrickAABBs->GetResource(), (wchar_t*)m_pBrickAABBs->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_COMPUTE_READ);
        
        for (uint32_t i = 0; i < FFX_BRIXELIZER_MAX_CASCADES; ++i)
        {
            m_GIDispatchDesc.cascadeAABBTrees[i] = SDKWrapper::ffxGetResource(m_pCascadeAABBTrees[i]->GetResource(), (wchar_t*)m_pCascadeAABBTrees[i]->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_COMPUTE_READ);
            m_GIDispatchDesc.cascadeBrickMaps[i] = SDKWrapper::ffxGetResource(m_pCascadeBrickMaps[i]->GetResource(), (wchar_t*)m_pCascadeBrickMaps[i]->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_COMPUTE_READ);
        }

        m_GIDispatchDesc.outputDiffuseGI = SDKWrapper::ffxGetResource(m_pDiffuseGI->GetResource(), L"OutputDiffuseGI");
        m_GIDispatchDesc.outputSpecularGI = SDKWrapper::ffxGetResource(m_pSpecularGI->GetResource(), L"OutputSpecularGI");
        
        FfxErrorCode error = ffxBrixelizerGetRawContext(&m_BrixelizerContext, &m_GIDispatchDesc.brixelizerContext);
        CauldronAssert(AssertLevel::ASSERT_ERROR, error == FFX_OK, L"Failed to get Brixelizer context pointer.");

        ffxBrixelizerGIContextDispatch(&m_BrixelizerGIContext, &m_GIDispatchDesc, SDKWrapper::ffxGetCommandList(pCmdList));    

        {
            Barrier barriers[] = {Barrier::Transition(m_pDiffuseGI->GetResource(), ResourceState::NonPixelShaderResource, diffuseGIState),
                                  Barrier::Transition(m_pSpecularGI->GetResource(), ResourceState::NonPixelShaderResource, specularGIState)};
            ResourceBarrier(pCmdList, 2, barriers);
        }
    }

    if (m_OutputMode == OutputMode::RadianceCache || m_OutputMode == OutputMode::IrradianceCache)
    {
        GPUScopedProfileCapture marker(pCmdList, L"Brixelizer GI Debug Visualization");

        const ResourceState debugVisState = m_pDiffuseGI->GetResource()->GetCurrentResourceState();

        {
            Barrier barriers[] = {Barrier::Transition(m_pDebugVisualization->GetResource(), debugVisState, ResourceState::UnorderedAccess)};
            ResourceBarrier(pCmdList, 1, barriers);
        }

        FfxBrixelizerGIDebugDescription debug_desc = {};

        memcpy(&debug_desc.view, &view, sizeof(debug_desc.view));
        memcpy(&debug_desc.projection, &projection, sizeof(debug_desc.projection));

        TextureDesc desc = m_pColorTarget->GetDesc();
        debug_desc.outputSize[0]    = desc.Width;
        debug_desc.outputSize[1]    = desc.Height;
        debug_desc.normalsUnpackMul = 2.0f;
        debug_desc.normalsUnpackAdd = -1.0f;

        if (m_OutputMode == OutputMode::RadianceCache)
            debug_desc.debugMode = FFX_BRIXELIZER_GI_DEBUG_MODE_RADIANCE_CACHE;
        else if (m_OutputMode == OutputMode::IrradianceCache)
            debug_desc.debugMode = FFX_BRIXELIZER_GI_DEBUG_MODE_IRRADIANCE_CACHE;

        debug_desc.startCascade  = m_StartCascadeIdx + (2 * NUM_BRIXELIZER_CASCADES);
        debug_desc.endCascade    = m_EndCascadeIdx + (2 * NUM_BRIXELIZER_CASCADES);
        debug_desc.depth         = SDKWrapper::ffxGetResource(m_pDepthBuffer->GetResource(), L"Depth");
        debug_desc.normal        = SDKWrapper::ffxGetResource(m_pNormalTarget->GetResource(), L"Normal");

        debug_desc.sdfAtlas    = SDKWrapper::ffxGetResource(m_pSdfAtlas->GetResource(), (wchar_t*)m_pSdfAtlas->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_COMPUTE_READ);
        debug_desc.bricksAABBs = SDKWrapper::ffxGetResource(m_pBrickAABBs->GetResource(), (wchar_t*)m_pBrickAABBs->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_COMPUTE_READ);

        for (uint32_t i = 0; i < FFX_BRIXELIZER_MAX_CASCADES; ++i)
        {
            debug_desc.cascadeAABBTrees[i] = SDKWrapper::ffxGetResource(m_pCascadeAABBTrees[i]->GetResource(), (wchar_t*)m_pCascadeAABBTrees[i]->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_COMPUTE_READ);
            debug_desc.cascadeBrickMaps[i] = SDKWrapper::ffxGetResource(m_pCascadeBrickMaps[i]->GetResource(), (wchar_t*)m_pCascadeBrickMaps[i]->GetDesc().Name.c_str(), FFX_RESOURCE_STATE_COMPUTE_READ);
        }

        debug_desc.outputDebug = SDKWrapper::ffxGetResource(m_pDebugVisualization->GetResource(), L"OutputDebugVisualization", FFX_RESOURCE_STATE_UNORDERED_ACCESS);

        FfxErrorCode error = ffxBrixelizerGetRawContext(&m_BrixelizerContext, &debug_desc.brixelizerContext);
        CauldronAssert(AssertLevel::ASSERT_ERROR, error == FFX_OK, L"Failed to get Brixelizer context pointer.");

        ffxBrixelizerGIContextDebugVisualization(&m_BrixelizerGIContext, &debug_desc, SDKWrapper::ffxGetCommandList(pCmdList));

        {
            Barrier barriers[] = {Barrier::Transition(m_pDebugVisualization->GetResource(), ResourceState::UnorderedAccess, debugVisState)};
            ResourceBarrier(pCmdList, 1, barriers);
        }
    }
}

void BrixelizerGIRenderModule::CopyHistoryResource(cauldron::CommandList*   pCmdList,
                                                   const cauldron::Texture* pInput,
                                                   const cauldron::Texture* pOutput,
                                                   std::wstring             name)
{
    std::wstring markerString = L"Copy History Resource:" + name;

    GPUScopedProfileCapture marker(pCmdList, markerString.c_str());

    TextureCopyDesc desc(pInput->GetResource(), pOutput->GetResource());
    CopyTextureRegion(pCmdList, &desc);
}

void BrixelizerGIRenderModule::CopyHistoryResources(cauldron::CommandList* pCmdList)
{
    const ResourceState historyDepthState = m_pHistoryDepth->GetResource()->GetCurrentResourceState();
    const ResourceState historyNormalsState = m_pHistoryNormals->GetResource()->GetCurrentResourceState();
    const ResourceState historyLitOutputState = m_pHistoryLitOutput->GetResource()->GetCurrentResourceState();
    const ResourceState depthState = m_pDepthBuffer->GetResource()->GetCurrentResourceState();
    const ResourceState normalsState = m_pNormalTarget->GetResource()->GetCurrentResourceState();
    const ResourceState litOutputState = m_pLitOutputCopy->GetResource()->GetCurrentResourceState();

    {
        Barrier barriers[] = {
            Barrier::Transition(m_pHistoryDepth->GetResource(), historyDepthState, ResourceState::CopyDest),
            Barrier::Transition(m_pHistoryNormals->GetResource(), historyNormalsState, ResourceState::CopyDest),
            Barrier::Transition(m_pHistoryLitOutput->GetResource(), historyLitOutputState, ResourceState::CopyDest),
            Barrier::Transition(m_pDepthBuffer->GetResource(), depthState, ResourceState::CopySource),
            Barrier::Transition(m_pNormalTarget->GetResource(), normalsState, ResourceState::CopySource),
            Barrier::Transition(m_pLitOutputCopy->GetResource(), litOutputState, ResourceState::CopySource),
        };
        ResourceBarrier(pCmdList, _countof(barriers), barriers);
    }

    CopyHistoryResource(pCmdList, m_pDepthBuffer, m_pHistoryDepth, L"Depth");
    CopyHistoryResource(pCmdList, m_pNormalTarget, m_pHistoryNormals, L"Normals");
    CopyHistoryResource(pCmdList, m_pLitOutputCopy, m_pHistoryLitOutput, L"LitOutput");

    {
        Barrier barriers[] = {
            Barrier::Transition(m_pHistoryDepth->GetResource(), ResourceState::CopyDest, historyDepthState),
            Barrier::Transition(m_pHistoryNormals->GetResource(), ResourceState::CopyDest, historyNormalsState),
            Barrier::Transition(m_pHistoryLitOutput->GetResource(), ResourceState::CopyDest, historyLitOutputState),
            Barrier::Transition(m_pDepthBuffer->GetResource(), ResourceState::CopySource, depthState),
            Barrier::Transition(m_pNormalTarget->GetResource(), ResourceState::CopySource, normalsState),
            Barrier::Transition(m_pLitOutputCopy->GetResource(), ResourceState::CopySource, litOutputState),
        };
        ResourceBarrier(pCmdList, _countof(barriers), barriers);
    }
}

void BrixelizerGIRenderModule::DeferredLighting(cauldron::CommandList* pCmdList, bool enableGI)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"Deferred Lighting");

    const ResourceState currentState     = m_pColorTarget->GetResource()->GetCurrentResourceState();
    const ResourceState copyCurrentState = m_pLitOutputCopy->GetResource()->GetCurrentResourceState();

    if (currentState != ResourceState::UnorderedAccess)
    {
        Barrier barriers[] = {currentState == ResourceState::UnorderedAccess ? Barrier::UAV(m_pColorTarget->GetResource()) : Barrier::Transition(m_pColorTarget->GetResource(), currentState, ResourceState::UnorderedAccess),
                              Barrier::Transition(m_pLitOutputCopy->GetResource(), copyCurrentState, ResourceState::UnorderedAccess)
        };
        
        ResourceBarrier(pCmdList, 2, barriers);
    }

    if (GetScene()->GetBRDFLutTexture())
    {
        m_pDeferredLightingParameterSet->SetTextureSRV(GetScene()->GetBRDFLutTexture(), ViewDimension::Texture2D, 4);
    }

    if (GetScene()->GetScreenSpaceShadowTexture())
    {
        // Store screenSpaceShadowTexture at index 0 in the shadow maps array
        m_pDeferredLightingParameterSet->SetTextureSRV(GetScene()->GetScreenSpaceShadowTexture(), ViewDimension::Texture2D, 7);
    }
    else
    {
        ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
        for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
        {
            m_pDeferredLightingParameterSet->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, 7 + i);
        }
    }

    // Update necessary scene frame information
    BufferAddressInfo sceneBuffers[2];
    sceneBuffers[0] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneInfo()));
    sceneBuffers[1] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneLightingInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneLightInfo()));
    m_pDeferredLightingParameterSet->UpdateRootConstantBuffer(&sceneBuffers[0], 0);
    m_pDeferredLightingParameterSet->UpdateRootConstantBuffer(&sceneBuffers[1], 1);

    // Allocate a dynamic constant buffers and set
    GIConstants giConstants = {};
    giConstants.DiffuseGIFactor  = enableGI ? m_DiffuseGIFactor : 0.0f;
    giConstants.SpecularGIFactor = enableGI ? m_SpecularGIFactor : 0.0f;
    giConstants.MultiBounce      = m_FrameIndex == 0 ? 0 : static_cast<int>(m_MultiBounce);

    BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(giConstants), &giConstants);

    // Update constant buffers
    m_pDeferredLightingParameterSet->UpdateRootConstantBuffer(&bufferInfo, 2);

    // Bind everything
    m_pDeferredLightingParameterSet->Bind(pCmdList, m_pDeferredLightingPipeline);
    SetPipelineState(pCmdList, m_pDeferredLightingPipeline);

    // Scale the work accordingly
    const ResolutionInfo& resInfo        = GetFramework()->GetResolutionInfo();
    uint32_t              dispatchWidth  = 0;
    uint32_t              dispatchHeight = 0;
    if (GetFramework()->GetUpscalingState() == UpscalerState::PreUpscale)
    {
        dispatchWidth  = resInfo.RenderWidth;
        dispatchHeight = resInfo.RenderHeight;
    }
    else
    {
        dispatchWidth  = resInfo.DisplayWidth;
        dispatchHeight = resInfo.DisplayHeight;
    }

    const uint32_t numGroupX = DivideRoundingUp(dispatchWidth, 8);
    const uint32_t numGroupY = DivideRoundingUp(dispatchHeight, 8);
    Dispatch(pCmdList, numGroupX, numGroupY, 1);

    // Render modules expect resources coming in/going out to be in a shader read state
    {
        Barrier barriers[] = {Barrier::Transition(m_pColorTarget->GetResource(), ResourceState::UnorderedAccess, currentState),
                              Barrier::Transition(m_pLitOutputCopy->GetResource(), ResourceState::UnorderedAccess, copyCurrentState)
        };
        ResourceBarrier(pCmdList, 2, barriers);
    }
}

void BrixelizerGIRenderModule::VisualizeGIDebug(cauldron::CommandList* pCmdList)
{
    const cauldron::Texture* currentOutput = nullptr;

    if (m_OutputMode == OutputMode::DiffuseGI)
        currentOutput = m_pDiffuseGI;
    else if (m_OutputMode == OutputMode::SpecularGI)
        currentOutput = m_pSpecularGI;
    else if (m_OutputMode == OutputMode::RadianceCache || m_OutputMode == OutputMode::IrradianceCache)
        currentOutput = m_pDebugVisualization;

    if (currentOutput)
    {
        const ResourceState currentState = currentOutput->GetResource()->GetCurrentResourceState();

        if (currentState != ResourceState::NonPixelShaderResource)
        {
            Barrier barriers[] = {Barrier::Transition(currentOutput->GetResource(), currentState, ResourceState::NonPixelShaderResource)};
            ResourceBarrier(pCmdList, 1, barriers);
        }

        m_pPassThroughParameterSet->SetTextureSRV(currentOutput, ViewDimension::Texture2D, 0);
        m_pPassThroughParameterSet->SetTextureUAV(m_pColorTarget, ViewDimension::Texture2D, 0);

        m_pPassThroughParameterSet->Bind(pCmdList, m_pPassThroughPipeline);
        SetPipelineState(pCmdList, m_pPassThroughPipeline);

        TextureDesc desc = m_pColorTarget->GetDesc();
        Dispatch(pCmdList, (desc.Width + 7) / 8, (desc.Height + 7) / 8, 1);

        if (currentState != ResourceState::NonPixelShaderResource)
        {
            Barrier barriers[] = {Barrier::Transition(currentOutput->GetResource(), ResourceState::NonPixelShaderResource, currentState)};
            ResourceBarrier(pCmdList, 1, barriers);
        }
    }
}

void BrixelizerGIRenderModule::TextureLoadComplete(const std::vector<const cauldron::Texture*>& textureList, void*)
{
    std::lock_guard<std::mutex> noiseTextureLock(m_TextureLoadCallbackMutex);

    for (auto texture : textureList)
        m_NoiseTextures.push_back(texture);

    if (m_NoiseTextures.size() == g_NumNoiseTextures)
        SetModuleReady(true);
}

void BrixelizerGIRenderModule::InitUI(cauldron::UISection* uiSection)
{
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UIText>("Static Settings"));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Mesh Unit Size", m_MeshUnitSize, 1e-3f, 1.0f));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Cascade Size Ratio", m_CascadeSizeRatio, 1.1f, 3.0f));

    m_StaticUIElements.emplace_back(uiSection->RegisterUIElement<UIText>("Dynamic Settings"));
    static std::vector<const char*> outputModeComboOptions = {"None", "Example Shader", "Debug Visualization", "Diffuse GI", "Specular GI", "Radiance Cache", "Irradiance Cache"};
    m_StaticUIElements.emplace_back(uiSection->RegisterUIElement<UICombo>("Output Mode", (int32_t&)m_OutputMode, std::move(outputModeComboOptions)));
    static std::vector<const char*> exampleOutputTypeComboOptions = {"Distance", "UVW", "Iterations", "Gradient", "Brick ID"};
    m_ExampleUIElements.emplace_back(uiSection->RegisterUIElement<UICombo>("Output Type", (int32_t&)m_ExampleOutputType, std::move(exampleOutputTypeComboOptions)));
    static std::vector<const char*> debugOutputTypeComboOptions = {"Distance", "UVW", "Iterations", "Gradient", "Brick ID", "Cascade ID"};
    m_DebugUIElements.emplace_back(uiSection->RegisterUIElement<UICombo>("Output Type", (int32_t&)m_DebugVisOutputType, std::move(debugOutputTypeComboOptions)));
    static std::vector<const char*> cascadeTypeComboOptions = {"Static", "Dynamic", "Merged"};
    m_DebugUIElements.emplace_back(uiSection->RegisterUIElement<UICombo>("Cascade Type", (int32_t&)m_CascadeType, std::move(cascadeTypeComboOptions)));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<int32_t>>("Start Cascade", (int32_t&)m_StartCascadeIdx, 0, NUM_BRIXELIZER_CASCADES - 1));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<int32_t>>("End Cascade", (int32_t&)m_EndCascadeIdx, 0, NUM_BRIXELIZER_CASCADES - 1));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("SDF Solve Epsilon", m_SdfSolveEps, 1e-6f, 1.0f));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UICheckBox>("SDF Center Follow Camera", m_SdfCenterFollowCamera));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("TMin", m_TMin, 0.0f, 10.0f));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("TMax", m_TMax, 0.0f, 10000.0f));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Ray Pushoff", m_RayPushoff, 0.0f, 10.0f));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Diffuse GI Factor", m_DiffuseGIFactor, 0.0f, 10.0f));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Specular GI Factor", m_SpecularGIFactor, 0.0f, 10.0f));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UICheckBox>("Enable GI", m_EnableGI));
    m_CommonUIElements.emplace_back(uiSection->RegisterUIElement<UICheckBox>("Multi-Bounce", m_MultiBounce));
    m_DebugUIElements.emplace_back(uiSection->RegisterUIElement<UICheckBox>("Show Static Instance AABBs", m_ShowStaticInstanceAABBs));
    m_DebugUIElements.emplace_back(uiSection->RegisterUIElement<UICheckBox>("Show Dynamic Instance AABBs", m_ShowDynamicInstanceAABBs));
    m_DebugUIElements.emplace_back(uiSection->RegisterUIElement<UICheckBox>("Show Cascade AABBs", m_ShowCascadeAABBs));
    m_DebugUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<int32_t>>("Show AABB Tree Index", m_ShowAABBTreeIndex, -1, NUM_BRIXELIZER_CASCADES - 1));
    m_ExampleUIElements.emplace_back(uiSection->RegisterUIElement<UICheckBox>("Show Brick Outlines", m_ShowBrickOutlines));
    m_ExampleUIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Alpha", m_Alpha, 0.0f, 1.0f));

    m_StaticUIElements.emplace_back(uiSection->RegisterUIElement<UIText>("Performance"));
    m_StaticUIElements.emplace_back(uiSection->RegisterUIElement<UICheckBox>("Reset Stats", m_ResetStats));

    m_FreeBricksTextElement        = uiSection->RegisterUIElement<UIText>("");
    m_StaticBricksTextElement      = uiSection->RegisterUIElement<UIText>("");
    m_StaticTrianglesTextElement   = uiSection->RegisterUIElement<UIText>("");
    m_StaticReferencesTextElement  = uiSection->RegisterUIElement<UIText>("");
    m_DynamicBricksTextElement     = uiSection->RegisterUIElement<UIText>("");
    m_DynamicTrianglesTextElement  = uiSection->RegisterUIElement<UIText>("");
    m_DynamicReferencesTextElement = uiSection->RegisterUIElement<UIText>("");

    m_StaticUIElements.emplace_back(m_FreeBricksTextElement);
    m_StaticUIElements.emplace_back(m_StaticBricksTextElement);
    m_StaticUIElements.emplace_back(m_StaticTrianglesTextElement);
    m_StaticUIElements.emplace_back(m_StaticReferencesTextElement);
    m_StaticUIElements.emplace_back(m_DynamicBricksTextElement);
    m_StaticUIElements.emplace_back(m_DynamicTrianglesTextElement);
    m_StaticUIElements.emplace_back(m_DynamicReferencesTextElement);

    UpdateUIElementVisibility();
}

void BrixelizerGIRenderModule::UpdateUIElementVisibility()
{
    bool enabled = ModuleEnabled();
    if (!enabled)
    {
        for (auto uiElement : m_StaticUIElements)
            uiElement->Show(false);

        for (auto uiElement : m_CommonUIElements)
            uiElement->Show(false);

        for (auto uiElement : m_DebugUIElements)
            uiElement->Show(false);

        for (auto uiElement : m_ExampleUIElements)
            uiElement->Show(false);

        return;
    }

    for (auto uiElement : m_StaticUIElements)
        uiElement->Show(true);

    for (auto uiElement : m_CommonUIElements)
        uiElement->Show(true);

    for (auto uiElement : m_DebugUIElements)
        uiElement->Show(m_OutputMode == OutputMode::DebugVisualization);

    for (auto uiElement : m_ExampleUIElements)
        uiElement->Show(m_OutputMode == OutputMode::ExampleShader);
}

void BrixelizerGIRenderModule::UpdateConfig()
{
    // Make sure the values set from the UI are valid.
    m_EndCascadeIdx   = std::max(m_EndCascadeIdx, m_StartCascadeIdx);
    m_StartCascadeIdx = std::min(m_StartCascadeIdx, m_EndCascadeIdx);
    m_TMax            = std::max(m_TMin, m_TMax);
    m_TMin            = std::min(m_TMin, m_TMax);

    if (m_SdfCenterFollowCamera)
    {
        // Update the Brixelizer SDF center position to follow the camera.
        Vec3 cameraPos = GetScene()->GetCurrentCamera()->GetCameraPos();
        m_SdfCenter[0] = cameraPos.getX();
        m_SdfCenter[1] = cameraPos.getY();
        m_SdfCenter[2] = cameraPos.getZ();
    }
}

uint32_t BrixelizerGIRenderModule::GetBufferIndex(const cauldron::Buffer* buffer)
{
    for (BrixelizerBufferInfo& bufferInfo : m_Buffers)
    {
        if (bufferInfo.buffer == buffer)
            return bufferInfo.index;
    }

    BufferDesc         bufferDesc = buffer->GetDesc();
    const GPUResource* resource   = buffer->GetResource();

    wchar_t name[256] = {};
    _snwprintf_s(name, _countof(name), L"Vertex Buffer (\"%s\")", bufferDesc.Name.c_str());
    FfxResource ffxResource = SDKWrapper::ffxGetResource(resource, name, FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    uint32_t                bufferIndex          = 0;
    FfxBrixelizerBufferDescription brixelizerBufferDesc = {};
    brixelizerBufferDesc.buffer                  = ffxResource;
    brixelizerBufferDesc.outIndex                = &bufferIndex;
    ffxBrixelizerRegisterBuffers(&m_BrixelizerContext, &brixelizerBufferDesc, 1);

    BrixelizerBufferInfo info;
    info.index  = bufferIndex;
    info.buffer = buffer;
    m_Buffers.push_back(info);

    return info.index;
}
