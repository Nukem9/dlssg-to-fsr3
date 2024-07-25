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

#include "translucencyrendermodule.h"

#include "core/framework.h"
#include "core/components/cameracomponent.h"
#include "core/components/meshcomponent.h"
#include "core/components/particlespawnercomponent.h"
#include "core/components/animationcomponent.h"
#include "core/scene.h"
#include "render/device.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rasterview.h"
#include "render/resourceview.h"
#include "render/rootsignature.h"
#include "render/sampler.h"
#include "render/shaderbuilderhelper.h"
#include "render/shadowmapresourcepool.h"
#include "render/indirectworkload.h"
#include "shaders/lightingcommon.h"

#include <functional>

using namespace cauldron;

void TranslucencyRenderModule::Init(const json& initData)
{
    m_VariableShading = initData.value("VariableShading", m_VariableShading);

    m_pColorRenderTarget = GetFramework()->GetColorTargetForCallback(GetName());
    m_pDepthTarget = GetFramework()->GetRenderTexture(L"DepthTarget");

    m_RasterViews.resize(2);
    m_RasterViews[0] = GetRasterViewAllocator()->RequestRasterView(m_pColorRenderTarget, ViewDimension::Texture2D);
    m_RasterViews[1] = GetRasterViewAllocator()->RequestRasterView(m_pDepthTarget, ViewDimension::Texture2D);

    // Reserve space for the max number of supported textures (use a bindless approach to resource indexing)
    m_Textures.reserve(MAX_TEXTURES_COUNT);

    // Reserve space for the max number of samplers
    m_Samplers.reserve(MAX_SAMPLERS_COUNT);

    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::VertexAndPixel, 1); // Frame Information
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::VertexAndPixel, 1); // Instance Information
    signatureDesc.AddConstantBufferView(2, ShaderBindStage::Pixel, 1);          // Texture Indices
    signatureDesc.AddConstantBufferView(3, ShaderBindStage::Pixel, 1);          // LightingCBData
    signatureDesc.AddConstantBufferView(4, ShaderBindStage::Pixel, 1);          // SceneLightingInformation
    // IBL
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1);                              // brdfTexture +
    signatureDesc.AddTextureSRVSet(1, ShaderBindStage::Pixel, 1);                              // diffuseCube +
    signatureDesc.AddTextureSRVSet(2, ShaderBindStage::Pixel, 1);                              // specularCube+
    signatureDesc.AddTextureSRVSet(3, ShaderBindStage::Pixel, MAX_SHADOW_MAP_TEXTURES_COUNT);  // shadow maps
    // AllTextures
    signatureDesc.AddTextureSRVSet(3 + MAX_SHADOW_MAP_TEXTURES_COUNT, ShaderBindStage::Pixel, MAX_TEXTURES_COUNT);  // Texture resource array

    // Create sampler set
    signatureDesc.AddSamplerSet(4, ShaderBindStage::Pixel, MAX_SAMPLERS_COUNT);

    // Setup samplers for brdfTexture, irradianceCube and prefilteredCube
    std::vector<SamplerDesc> samplers;
    static bool              s_InvertedDepth = GetConfig()->InvertedDepth;
    SamplerDesc comparisonSampler;
    comparisonSampler.Comparison = s_InvertedDepth ? ComparisonFunc::GreaterEqual : ComparisonFunc::LessEqual;
    comparisonSampler.Filter = FilterFunc::ComparisonMinMagLinearMipPoint;
    comparisonSampler.MaxAnisotropy = 1;
    samplers.push_back(comparisonSampler);
    signatureDesc.AddStaticSamplers(3, ShaderBindStage::Pixel, (uint32_t)samplers.size(), samplers.data());

    {
        SamplerDesc prefilteredCubeSampler;
        prefilteredCubeSampler.AddressW = AddressMode::Wrap;
        prefilteredCubeSampler.Filter = FilterFunc::MinMagMipLinear;
        prefilteredCubeSampler.MaxAnisotropy = 1;

        samplers.clear();
        samplers.push_back(prefilteredCubeSampler);
        signatureDesc.AddStaticSamplers(0, ShaderBindStage::Pixel, (uint32_t)samplers.size(), samplers.data());
        signatureDesc.AddStaticSamplers(2, ShaderBindStage::Pixel, (uint32_t)samplers.size(), samplers.data());

        SamplerDesc irradianceCubeSampler;
        irradianceCubeSampler.Filter = FilterFunc::MinMagMipPoint;
        irradianceCubeSampler.AddressW = AddressMode::Wrap;
        irradianceCubeSampler.Filter = FilterFunc::MinMagMipPoint;
        irradianceCubeSampler.MaxAnisotropy = 1;
        samplers.clear();
        samplers.push_back(irradianceCubeSampler);
        signatureDesc.AddStaticSamplers(1, ShaderBindStage::Pixel, (uint32_t)samplers.size(), samplers.data());
    }

    m_pRootSignature = RootSignature::CreateRootSignature(L"TranslucencyPass_RootSignature", signatureDesc);

    // Create ParameterSet and assign the constant buffer parameters
    // We will add texture views as they are loaded
    m_pParameterSet = ParameterSet::CreateParameterSet(m_pRootSignature);
    m_pParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 0);
    m_pParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(InstanceInformation), 1);
    m_pParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(TextureIndices), 2);
    m_pParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(LightingCBData), 3);
    m_pParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneLightingInformation), 4);

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    for (uint32_t i = 0; i < pShadowMapResourcePool->GetRenderTargetCount(); ++i)
    {
        m_pParameterSet->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, 3 + i);
    }

    // Init for particles rendering
    {
        RootSignatureDesc signatureDesc;
        signatureDesc.AddBufferSRVSet(0, ShaderBindStage::Vertex, 1);
        signatureDesc.AddBufferSRVSet(1, ShaderBindStage::Vertex, 1);
        signatureDesc.AddBufferSRVSet(2, ShaderBindStage::Vertex, 1);
        signatureDesc.AddBufferSRVSet(3, ShaderBindStage::Vertex, 1);
        signatureDesc.AddTextureSRVSet(4, ShaderBindStage::Pixel, 1);
        signatureDesc.AddTextureSRVSet(5, ShaderBindStage::Pixel, 1);  // t5 - depth texture

        signatureDesc.AddConstantBufferView(0, ShaderBindStage::VertexAndPixel, 1);  // b0
        signatureDesc.AddConstantBufferView(1, ShaderBindStage::VertexAndPixel, 1);

        SamplerDesc samplerDesc = {FilterFunc::MinMagLinearMipPoint, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp};
        signatureDesc.AddStaticSamplers(0, ShaderBindStage::Pixel, 1, &samplerDesc);

        m_pParticlesRenderRootSignature = RootSignature::CreateRootSignature(L"ParticleRenderPass_RootSignature", signatureDesc);

        m_pParticlesRenderParameters = ParameterSet::CreateParameterSet(m_pParticlesRenderRootSignature);
        m_pParticlesRenderParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(RenderingConstantBuffer), 0);
        //m_pParticlesRenderParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(IndirectCommand), 1);
    }

    // Register for content change updates
    GetContentManager()->AddContentListener(this);

    SetModuleReady(true);
}

TranslucencyRenderModule::~TranslucencyRenderModule()
{
    GetContentManager()->RemoveContentListener(this);

    delete m_pRootSignature;
    delete m_pParameterSet;

    // Clear out raster views
    m_RasterViews.clear();

    for (auto& pipeline : m_PipelineHashObjects)
    {
        delete pipeline.m_Pipeline;
    }
    m_PipelineHashObjects.clear();
    m_TranslucentRenderSurfaces.clear();

    // Release samplers
    for (auto samplerIter : m_Samplers)
    {
        delete samplerIter;
    }
    m_Samplers.clear();

    if (m_pParticlesRenderRootSignature)
        delete m_pParticlesRenderRootSignature;

    if (m_pParticlesRenderParameters)
        delete m_pParticlesRenderParameters;

    if (m_pIndirectWorkload)
        delete m_pIndirectWorkload;

    for (auto& pipeline : m_pParticlesRenderPipelineHashObjects)
    {
        delete pipeline.m_Pipeline;
    }
    m_pParticlesRenderPipelineHashObjects.clear();
}

void TranslucencyRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    // Don't do any of this if there is nothing to actually render yet
    if (m_TranslucentRenderSurfaces.empty() && m_RenderParticleSpawners.empty())
        return;

    std::lock_guard<std::mutex> paramsLock(m_CriticalSection);  // There's a lot going on here dependent on what's loaded. Prevent race condition with loading content
    GPUScopedProfileCapture translucencyMarker(pCmdList, L"Translucency");

    if (GetScene()->GetBRDFLutTexture())
    {
        m_pParameterSet->SetTextureSRV(GetScene()->GetBRDFLutTexture(), ViewDimension::Texture2D, 0);
    }
    if (GetScene()->GetIBLTexture(IBLTexture::Irradiance))
    {
        m_pParameterSet->SetTextureSRV(GetScene()->GetIBLTexture(IBLTexture::Irradiance), ViewDimension::TextureCube, 1);
    }
    if (GetScene()->GetIBLTexture(IBLTexture::Prefiltered))
    {
        m_pParameterSet->SetTextureSRV(GetScene()->GetIBLTexture(IBLTexture::Prefiltered), ViewDimension::TextureCube, 2);
    }

    if (GetScene()->GetScreenSpaceShadowTexture())
    {
        // Store screenSpaceShadowTexture at index 0 in the shadow maps array
        m_pParameterSet->SetTextureSRV(GetScene()->GetScreenSpaceShadowTexture(), ViewDimension::Texture2D, 3);
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
                m_pParameterSet->SetTextureSRV(pShadowMapResourcePool->GetRenderTarget(i), ViewDimension::Texture2D, 3 + i);
            }
        }
    }

    // Allocate a dynamic constant buffers and set
    m_LightingConstantData.IBLFactor            = GetScene()->GetIBLFactor();
    m_LightingConstantData.SpecularIBLFactor    = GetScene()->GetSpecularIBLFactor();
    BufferAddressInfo bufferInfo                = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(LightingCBData), &m_LightingConstantData);

    // Update constant buffers
    m_pParameterSet->UpdateRootConstantBuffer(&bufferInfo, 3);

    // Render modules expect resources coming in/going out to be in a shader read state
    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(m_pColorRenderTarget->GetResource(),    ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::RenderTargetResource));
    barriers.push_back(Barrier::Transition(m_pDepthTarget->GetResource(),
                                           ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
                                           ResourceState::DepthRead | ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    if (m_OptionalTransparencyOptions.OptionalTargets.size())
    {
        for (auto iter : m_OptionalTransparencyOptions.OptionalTargets)
            barriers.push_back(Barrier::Transition(iter.first->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::RenderTargetResource));
    }
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    auto processSpawnerInBarrier = [&](ParticlesRenderData& renderParticles) {
        const ParticleSystem* pParticleSystem = renderParticles.m_RenderParticles.pParticleSystem;

        if (!pParticleSystem->m_RenderReady)
        {
            renderParticles.m_ReadyForFrame = false;
            return;
        }

        renderParticles.m_ReadyForFrame = true;

        Barrier barriers[1] = {
            Barrier::Transition(pParticleSystem->m_pIndirectArgsBuffer->GetResource(), ResourceState::UnorderedAccess, ResourceState::IndirectArgument)};
        ResourceBarrier(pCmdList, 1, barriers);
    };

    auto iterSpawners = m_RenderParticleSpawners.begin();
    while (iterSpawners != m_RenderParticleSpawners.end())
    {
        ParticlesRenderData& renderParticles = *iterSpawners;
        processSpawnerInBarrier(renderParticles);
        iterSpawners++;
    }

    // Bind raster resources
    BeginRaster(pCmdList,
                uint32_t(m_RasterViews.size() - 1),
                m_RasterViews.data(),
                m_RasterViews[m_RasterViews.size() - 1],
                m_VariableShading ? GetDevice()->GetVRSInfo() : nullptr);

    // Update necessary scene frame information
    BufferAddressInfo sceneInfoBufferInfo[2];
    sceneInfoBufferInfo[0] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneInfo()));
    m_pParameterSet->UpdateRootConstantBuffer(&sceneInfoBufferInfo[0], 0);

    sceneInfoBufferInfo[1] = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneLightingInformation), reinterpret_cast<const void*>(&GetScene()->GetSceneLightInfo()));
    m_pParameterSet->UpdateRootConstantBuffer(&sceneInfoBufferInfo[1], 4);

    // Set viewport, scissor, primitive topology once and move on (set based on upscaler state)
    UpscalerState upscaleState = GetFramework()->GetUpscalingState();
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

    uint32_t width, height;
    if (upscaleState == UpscalerState::None || upscaleState == UpscalerState::PostUpscale)
    {
        width = resInfo.UpscaleWidth;
        height = resInfo.UpscaleHeight;
    }
    else
    {
        width = resInfo.RenderWidth;
        height = resInfo.RenderHeight;
    }

    SetViewportScissorRect(pCmdList, 0, 0, width, height, 0.f, 1.f);
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

    // Preprocess to sort depth
    for (auto& renderSurface : m_TranslucentRenderSurfaces)
    {
        // Depth gather
        const Vectormath::Matrix4 wvp    = GetScene()->GetSceneInfo().CameraInfo.ViewProjectionMatrix * renderSurface.m_RenderSurface.pOwner->GetTransform();
        const Vectormath::Vector4 center = renderSurface.m_RenderSurface.pSurface->Center();
        renderSurface.m_depth = (wvp * center).getW();
    }
    // Sort translucent object from further away to closes to the camera, this is needed for correct color blending
    std::sort(m_TranslucentRenderSurfaces.begin(), m_TranslucentRenderSurfaces.end());

    for (auto& spawner : m_RenderParticleSpawners)
    {
        // Depth gather
        const Vectormath::Matrix4 vp       = GetScene()->GetSceneInfo().CameraInfo.ViewProjectionMatrix * spawner.m_RenderParticles.pOwner->GetTransform();
        const Vectormath::Vector4 position = Vectormath::Vector4(spawner.m_RenderParticles.pParticleSystem->GetPosition(), 1.0f);
        spawner.m_depth                         = (vp * position).getW();
    }
    // Sort translucent object from further away to closes to the camera, this is needed for correct color blending
    std::sort(m_RenderParticleSpawners.begin(), m_RenderParticleSpawners.end());

    // Render translucent surfaces
    cauldron::PipelineObject* currentPipeline = nullptr; // optimization

    auto processSurface = [&](const TranslucentRenderData& renderSurface)
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

            const Surface*  pSurface  = renderSurface.m_RenderSurface.pSurface;
            const Material* pMaterial = pSurface->GetMaterial();

            instanceInfo.MaterialInfo.AlphaCutoff = pMaterial->GetAlphaCutOff();

            // update the perObjectConstantData
            if (pMaterial->HasPBRInfo())
            {
                instanceInfo.MaterialInfo.EmissiveFactor = pMaterial->GetEmissiveColor();

                Vec4 albedo                               = pMaterial->GetAlbedoColor();
                instanceInfo.MaterialInfo.AlbedoFactor    = albedo;

                if (pMaterial->HasPBRMetalRough() || pMaterial->HasPBRSpecGloss())
                    instanceInfo.MaterialInfo.PBRParams = pMaterial->GetPBRInfo();
            }

            // Update root constants
            BufferAddressInfo perObjectBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(InstanceInformation), &instanceInfo);
            BufferAddressInfo textureIndicesBufferInfo =
                GetDynamicBufferPool()->AllocConstantBuffer(sizeof(TextureIndices), &renderSurface.m_RenderSurface.TextureIndicies);
            m_pParameterSet->UpdateRootConstantBuffer(&perObjectBufferInfo, 1);
            m_pParameterSet->UpdateRootConstantBuffer(&textureIndicesBufferInfo, 2);

            // Bind for rendering
            m_pParameterSet->Bind(pCmdList, renderSurface.m_Pipeline);

            std::vector<BufferAddressInfo> vertexBuffers;
            for (uint32_t attribute = 0; attribute < static_cast<uint32_t>(VertexAttributeType::Count); ++attribute)
            {
                // Check if the attribute is present
                if (renderSurface.m_UsedAttributes & (0x1 << attribute))
                {
                    vertexBuffers.push_back(pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(attribute)).pBuffer->GetAddressInfo());
                }
            }

            // Skeletal Animation
            if (renderSurface.m_RenderSurface.pOwner->HasComponent(AnimationComponentMgr::Get()))
            {
                const auto& data = renderSurface.m_RenderSurface.pOwner->GetComponent<const AnimationComponent>(AnimationComponentMgr::Get())->GetData();

                if (data->m_skinId != -1)
                {
                    // Positions are stored at index 0
                    // Normals are stored at index 1

                    // Replace the vertices POSITION attribute with the Skinned POSITION attribute
                    // Replace the vertices NORMAL   attribute with the Skinned NORMAL   attribute
                    const uint32_t surfaceID = pSurface->GetSurfaceID();
                    vertexBuffers[0]         = data->m_skinnedPositions[surfaceID].pBuffer->GetAddressInfo();
                    vertexBuffers[1]         = data->m_skinnedNormals[surfaceID].pBuffer->GetAddressInfo();
                }
            }

            // Set vertex/index buffers
            SetVertexBuffers(pCmdList, 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data());

            BufferAddressInfo addressInfo = pSurface->GetIndexBuffer().pBuffer->GetAddressInfo();
            SetIndexBuffer(pCmdList, &addressInfo);

            // And draw
            DrawIndexedInstanced(pCmdList, pSurface->GetIndexBuffer().Count);
        }
    };

    auto processSpawner = [&](const ParticlesRenderData& renderParticles)
    {
        const ParticleSystem* pParticleSystem = renderParticles.m_RenderParticles.pParticleSystem;

        if (!renderParticles.m_ReadyForFrame)
            return;

        if (currentPipeline != renderParticles.m_Pipeline)
        {
            SetPipelineState(pCmdList, renderParticles.m_Pipeline);
            currentPipeline = renderParticles.m_Pipeline;
        }

        //GPUScopedProfileCapture GPUParticleSimMarker(pCmdList, L"rasterization");

        m_pParticlesRenderParameters->SetBufferSRV(pParticleSystem->m_pParticleBufferA, 0);
        m_pParticlesRenderParameters->SetBufferSRV(pParticleSystem->m_pPackedViewSpaceParticlePositions, 1);
        m_pParticlesRenderParameters->SetBufferSRV(pParticleSystem->m_pAliveCountBuffer, 2);
        m_pParticlesRenderParameters->SetBufferSRV(pParticleSystem->m_pAliveIndexBuffer, 3);
        m_pParticlesRenderParameters->SetTextureSRV(pParticleSystem->m_pAtlas, ViewDimension::Texture2D, 4);
        m_pParticlesRenderParameters->SetTextureSRV(m_pDepthTarget, ViewDimension::Texture2D, 5);

        const CameraInformation& cameraInfo = GetScene()->GetSceneInfo().CameraInfo;

        RenderingConstantBuffer renderingConstants = {};
        renderingConstants.m_Projection            = cameraInfo.ProjectionMatrix;
        renderingConstants.m_ProjectionInv         = cameraInfo.InvProjectionMatrix;
        renderingConstants.m_SunColor              = Vec4(0.8f, 0.8f, 0.7f, 0);
        renderingConstants.m_AmbientColor          = Vec4(0.2f, 0.2f, 0.3f, 0);
        renderingConstants.m_SunDirectionVS        = cameraInfo.ViewMatrix * Vec4(0.7f, 0.7f, 0, 0);
        renderingConstants.m_ScreenWidth           = resInfo.RenderWidth;
        renderingConstants.m_ScreenHeight          = resInfo.RenderHeight;
        //renderingConstants.m_SunColor *= fLightMod;
        //renderingConstants.m_AmbientColor *= fLightMod;

        // Update root constants
        BufferAddressInfo renderingBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(RenderingConstantBuffer), &renderingConstants);
        m_pParticlesRenderParameters->UpdateRootConstantBuffer(&renderingBufferInfo, 0);

        // Bind for rendering
        m_pParticlesRenderParameters->Bind(pCmdList, renderParticles.m_Pipeline);

        // Set vertex/index buffers
        BufferAddressInfo addressInfo = pParticleSystem->m_pIndexBuffer->GetAddressInfo();
        SetIndexBuffer(pCmdList, &addressInfo);

        SetVertexBuffers(pCmdList, 0, 0, nullptr);
        SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

        ExecuteIndirect(pCmdList, m_pIndirectWorkload, pParticleSystem->m_pIndirectArgsBuffer, 1, 0);
    };

    auto iterSurfaces = m_TranslucentRenderSurfaces.begin();
    iterSpawners = m_RenderParticleSpawners.begin();
    while (iterSurfaces != m_TranslucentRenderSurfaces.end() && iterSpawners != m_RenderParticleSpawners.end())
    {
        if (iterSurfaces->m_depth > iterSpawners->m_depth)
        {
            const TranslucentRenderData& renderSurface = *iterSurfaces;
            processSurface(renderSurface);
            iterSurfaces++;
        }
        else
        {
            const ParticlesRenderData& renderParticles = *iterSpawners;
            processSpawner(renderParticles);
            iterSpawners++;
        }
    }
    while (iterSurfaces != m_TranslucentRenderSurfaces.end())
    {
        const TranslucentRenderData& renderSurface = *iterSurfaces;
        processSurface(renderSurface);
        iterSurfaces++;
    }
    while (iterSpawners != m_RenderParticleSpawners.end())
    {
        const ParticlesRenderData& renderParticles = *iterSpawners;
        processSpawner(renderParticles);
        iterSpawners++;
    }

    // Done drawing, unbind
    EndRaster(pCmdList, m_VariableShading ? GetDevice()->GetVRSInfo() : nullptr);

    // finish transitions
    auto processSpawnerOutBarrier = [&](ParticlesRenderData& renderParticles) {
        const ParticleSystem* pParticleSystem = renderParticles.m_RenderParticles.pParticleSystem;

        if (!renderParticles.m_ReadyForFrame)
            return;

        Barrier barriers[1] = {
            Barrier::Transition(pParticleSystem->m_pIndirectArgsBuffer->GetResource(), ResourceState::IndirectArgument, ResourceState::UnorderedAccess)};
        ResourceBarrier(pCmdList, 1, barriers);

        renderParticles.m_ReadyForFrame = false;
    };
    iterSpawners = m_RenderParticleSpawners.begin();
    while (iterSpawners != m_RenderParticleSpawners.end())
    {
        ParticlesRenderData& renderParticles = *iterSpawners;
        processSpawnerOutBarrier(renderParticles);
        iterSpawners++;
    }

    // Render modules expect resources coming in/going out to be in a shader read state
    barriers.clear();
    barriers.push_back(Barrier::Transition(m_pColorRenderTarget->GetResource(),    ResourceState::RenderTargetResource,    ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(m_pDepthTarget->GetResource(),
                                           ResourceState::DepthRead | ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
                                           ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    if (m_OptionalTransparencyOptions.OptionalTargets.size())
    {
        for (auto iter : m_OptionalTransparencyOptions.OptionalTargets)
            barriers.push_back(Barrier::Transition(iter.first->GetResource(), ResourceState::RenderTargetResource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    }
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void TranslucencyRenderModule::OnNewContentLoaded(ContentBlock* pContentBlock)
{
    MeshComponentMgr* pMeshComponentManager = MeshComponentMgr::Get();
    ParticleSpawnerComponentMgr* pParticleSpawnerComponentManager = ParticleSpawnerComponentMgr::Get();

    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);

    // For each new Mesh, create a GBufferComponent that will map mesh/material information for more efficient rendering at run time
    for (auto* pEntityData : pContentBlock->EntityDataBlocks)
    {
        for (auto* pComponent : pEntityData->Components)
        {
            if (pComponent->GetManager() == pMeshComponentManager)
            {
                const Mesh* pMesh = reinterpret_cast<MeshComponent*>(pComponent)->GetData().pMesh;
                const size_t numSurfaces = pMesh->GetNumSurfaces();
                for (uint32_t i = 0; i < numSurfaces; ++i)
                {
                    const Surface* pSurface = pMesh->GetSurface(i);
                    const Material* pMaterial = pSurface->GetMaterial();

                    // TranslucencyRenderModule only handles translucenct object, so skip this surface if it's not translucent
                    if (!pSurface->HasTranslucency())
                        continue;

                    // Push surface render information
                    PipelineSurfaceRenderInfo surfaceRenderInfo;
                    surfaceRenderInfo.pOwner = pComponent->GetOwner();
                    surfaceRenderInfo.pSurface = pSurface;

                    int32_t samplerIndex;
                    if (pMaterial->HasPBRInfo())
                    {
                        surfaceRenderInfo.TextureIndicies.AlbedoTextureIndex = AddTexture(pMaterial, TextureClass::Albedo, samplerIndex);
                        surfaceRenderInfo.TextureIndicies.AlbedoSamplerIndex = samplerIndex;
                        if (pMaterial->HasPBRMetalRough())
                        {
                            surfaceRenderInfo.TextureIndicies.MetalRoughSpecGlossTextureIndex = AddTexture(pMaterial, TextureClass::MetalRough, samplerIndex);
                            surfaceRenderInfo.TextureIndicies.MetalRoughSpecGlossSamplerIndex = samplerIndex;
                        }
                        else if (pMaterial->HasPBRSpecGloss())
                        {
                            surfaceRenderInfo.TextureIndicies.MetalRoughSpecGlossTextureIndex = AddTexture(pMaterial, TextureClass::SpecGloss, samplerIndex);
                            surfaceRenderInfo.TextureIndicies.MetalRoughSpecGlossSamplerIndex = samplerIndex;
                        }
                    }

                    surfaceRenderInfo.TextureIndicies.NormalTextureIndex = AddTexture(pMaterial, TextureClass::Normal, samplerIndex);
                    surfaceRenderInfo.TextureIndicies.NormalSamplerIndex = samplerIndex;
                    surfaceRenderInfo.TextureIndicies.EmissiveTextureIndex = AddTexture(pMaterial, TextureClass::Emissive, samplerIndex);
                    surfaceRenderInfo.TextureIndicies.EmissiveSamplerIndex = samplerIndex;
                    surfaceRenderInfo.TextureIndicies.OcclusionTextureIndex = AddTexture(pMaterial, TextureClass::Occlusion, samplerIndex);
                    surfaceRenderInfo.TextureIndicies.OcclusionSamplerIndex = samplerIndex;

                    // Create pipeline or retrieve already created
                    uint32_t pipeHashIndex = CreatePipelineObject(pSurface);

                    // setup TranslucentRenderData
                    TranslucentRenderData renderData;
                    renderData.m_Pipeline       = m_PipelineHashObjects.at(pipeHashIndex).m_Pipeline;
                    renderData.m_UsedAttributes = m_PipelineHashObjects.at(pipeHashIndex).m_UsedAttributes;
                    renderData.m_RenderSurface  = surfaceRenderInfo;
                    m_TranslucentRenderSurfaces.push_back(renderData);
                }
            }
            else if (pComponent->GetManager() == pParticleSpawnerComponentManager)
            {
                ParticleSystem* pParticleSystem = reinterpret_cast<ParticleSpawnerComponent*>(pComponent)->GetParticleSystem();

                // Push surface render information
                PipelineParticlesRenderInfo particlesRenderInfo;
                particlesRenderInfo.pOwner = pComponent->GetOwner();
                particlesRenderInfo.pParticleSystem = pParticleSystem;

                unsigned int reactiveFlags = 0;
                for (int i = 0; i < pParticleSystem->m_Emitters.size(); ++i)
                {
                    ParticleSystem::Emitter& emitter = pParticleSystem->m_Emitters[i];
                    reactiveFlags |= ((emitter.Flags & EmitterDesc::EF_Reactive) ? 1 : 0) << i;
                }

                // See if we've already built this pipeline
                uint32_t index = 0;
                for (; index < m_pParticlesRenderPipelineHashObjects.size(); ++index)
                {
                    if (m_pParticlesRenderPipelineHashObjects[index].m_PipelineHash == reactiveFlags)
                        break;
                }

                if (index == m_pParticlesRenderPipelineHashObjects.size())
                {
                    RasterDesc rasterDesc;
                    rasterDesc.CullingMode = CullMode::None;

                    DepthDesc depthDesc;
                    depthDesc.DepthEnable      = true;
                    depthDesc.DepthWriteEnable = false;
                    depthDesc.DepthFunc        = ComparisonFunc::LessEqual;

                    std::vector<ResourceFormat> rtFormats = {m_pColorRenderTarget->GetFormat()};

                    // Add additional targets
                    if (m_OptionalTransparencyOptions.OptionalTargets.size())
                    {
                        for (const auto& targetPair : m_OptionalTransparencyOptions.OptionalTargets)
                        {
                            // Add the format
                            rtFormats.push_back(targetPair.first->GetFormat());
                        }
                    }

                    std::vector<BlendDesc> blendDescs = {{true,
                                                          Blend::SrcAlpha,
                                                          Blend::InvSrcAlpha,
                                                          BlendOp::Add,
                                                          Blend::InvSrcAlpha,
                                                          Blend::Zero,
                                                          BlendOp::Add,
                                                          static_cast<uint32_t>(ColorWriteMask::All)}};

                    // Add additional blends
                    if (m_OptionalTransparencyOptions.OptionalTargets.size())
                    {
                        for (const auto& targetPair : m_OptionalTransparencyOptions.OptionalTargets)
                            blendDescs.push_back(targetPair.second);
                    }

                    // Setup the pipeline object
                    PipelineDesc psoDesc;
                    psoDesc.SetRootSignature(m_pParticlesRenderRootSignature);

                    DefineList defineList;

                    if (reactiveFlags != 0)
                    {
                        defineList.insert(std::make_pair(L"REACTIVE_FLAGS", std::to_wstring(reactiveFlags).c_str()));

                        // Add additional output/export support if needed
                        if (!m_OptionalTransparencyOptions.OptionalAdditionalOutputs.empty())
                            defineList.insert(
                                std::make_pair(L"ADDITIONAL_TRANSLUCENT_OUTPUTS", m_OptionalTransparencyOptions.OptionalAdditionalOutputs.c_str()));

                        if (!m_OptionalTransparencyOptions.OptionalAdditionalExports.empty())
                            defineList.insert(
                                std::make_pair(L"ADDITIONAL_TRANSLUCENT_EXPORTS", m_OptionalTransparencyOptions.OptionalAdditionalExports.c_str()));
                    }

                    psoDesc.AddRasterStateDescription(&rasterDesc);
                    psoDesc.AddDepthState(&depthDesc);
                    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
                    psoDesc.AddRasterFormats(rtFormats, m_pDepthTarget->GetFormat());
                    psoDesc.AddBlendStates(blendDescs, false, true);

                    // Setup the shaders to build on the pipeline object
                    std::wstring shaderPath = L"ParticleRender.hlsl";
                    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(shaderPath.c_str(), L"VS_StructuredBuffer", ShaderModel::SM6_0, &defineList));
                    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(shaderPath.c_str(), L"PS_Billboard", ShaderModel::SM6_0, &defineList));

                    std::wstring piplineObjName = L"ParticleRenderPass_PipelineObj_" + std::to_wstring(reactiveFlags);
                    cauldron::PipelineObject* pPipelineObj = PipelineObject::CreatePipelineObject(piplineObjName.c_str(), psoDesc);

                    // Ok, this is a new pipeline, add it to the PipelineHashObject vector
                    PipelineHashObject pipelineHashObject;
                    pipelineHashObject.m_Pipeline     = pPipelineObj;
                    pipelineHashObject.m_PipelineHash = reactiveFlags;
                    m_pParticlesRenderPipelineHashObjects.push_back(pipelineHashObject);
                }

                // Create indirect workload when the first spawner is loaded
                if (m_RenderParticleSpawners.empty())
                {
                    m_pIndirectWorkload = IndirectWorkload::CreateIndirectWorkload(IndirectCommandType::DrawIndexed);
                }

                // setup ParticlesRenderData
                ParticlesRenderData renderData;
                renderData.m_Pipeline        = m_pParticlesRenderPipelineHashObjects.at(index).m_Pipeline;
                renderData.m_RenderParticles = particlesRenderInfo;
                m_RenderParticleSpawners.push_back(renderData);
            }
        }
    }

    {
        // Update the parameter set with loaded texture entries
        CauldronAssert(ASSERT_CRITICAL, m_Textures.size() <= MAX_TEXTURES_COUNT, L"Too many textures.");
        // Shadow maps are bound at t3 and there are MAX_SHADOW_MAP_TEXTURES_COUNT of them. Textures are bound afterwards.
        for (uint32_t i = 0; i < m_Textures.size(); ++i)
            m_pParameterSet->SetTextureSRV(m_Textures[i].pTexture, ViewDimension::Texture2D, i + 3 + MAX_SHADOW_MAP_TEXTURES_COUNT);

        // Update sampler bindings as well
        CauldronAssert(ASSERT_CRITICAL, m_Samplers.size() <= MAX_SAMPLERS_COUNT, L"Too many samplers.");
        for (uint32_t i = 0; i < m_Samplers.size(); ++i)
            m_pParameterSet->SetSampler(m_Samplers[i], i+4);
    }
}

void TranslucencyRenderModule::OnContentUnloaded(ContentBlock* pContentBlock)
{
    // We're going to be modifying the pipeline groups, so make sure no one else is using them
    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);
    for (auto& renderData : m_TranslucentRenderSurfaces)
    {
        RemoveTexture(renderData.m_RenderSurface.TextureIndicies.AlbedoTextureIndex);
        RemoveTexture(renderData.m_RenderSurface.TextureIndicies.MetalRoughSpecGlossTextureIndex);
        RemoveTexture(renderData.m_RenderSurface.TextureIndicies.NormalTextureIndex);
        RemoveTexture(renderData.m_RenderSurface.TextureIndicies.EmissiveTextureIndex);
        RemoveTexture(renderData.m_RenderSurface.TextureIndicies.OcclusionTextureIndex);
    }
}

void TranslucencyRenderModule::AddOptionalTransparencyOptions(const OptionalTransparencyOptions& options)
{
    // Copy any additional target/blend pairs to add to pipeline descriptions
    if (options.OptionalTargets.size())
    {
        for (const auto& targetPair : options.OptionalTargets)
        {
            m_OptionalTransparencyOptions.OptionalTargets.push_back(targetPair);

            // Push depth raster view to the end
            m_RasterViews.push_back(m_RasterViews[m_RasterViews.size() - 1]);

            // Create a raster view for rendering and re-assign to before last entry
            m_RasterViews[m_RasterViews.size() - 2] = GetRasterViewAllocator()->RequestRasterView(targetPair.first, ViewDimension::Texture2D);
        }
    }

    // Append any additional outputs
    if (!options.OptionalAdditionalOutputs.empty())
        m_OptionalTransparencyOptions.OptionalAdditionalOutputs.append((options.OptionalAdditionalOutputs + L"\n").c_str());

    // Append any additional exports
    if (!options.OptionalAdditionalExports.empty())
        m_OptionalTransparencyOptions.OptionalAdditionalExports.append((options.OptionalAdditionalExports + L"\n").c_str());
}

//////////////////////////////////////////////////////////////////////////
// Content loading helpers

uint32_t TranslucencyRenderModule::CreatePipelineObject(const Surface* pSurface)
{
    // Translucency shader should be optimized based on what the model provides
    //   - The used attributes are AT MOST what the model has
    //   - Material model (metallic+roughhness or specular+glossiness) is a material property but it doesn't guarantee that all the data is available
    //   - some textures can be missing, hence are not in the define list
    //   - when some textures are missing, texcoord attributes can be removed.
    //     - POSITION have to be present
    //     - NORMAL, TANGENT and COLOR# are always used if present
    //     - TEXCOORD# depends on which textures are using them. If there is no texture, they should be removed

    uint32_t usedAttributes =
        VertexAttributeFlag_Position | VertexAttributeFlag_Normal | VertexAttributeFlag_Tangent | VertexAttributeFlag_Color0 | VertexAttributeFlag_Color1;

    // only keep the available attributes of the surface
    const uint32_t surfaceAttributes = pSurface->GetVertexAttributes();
    usedAttributes = usedAttributes & surfaceAttributes;

    DefineList defineList;
    const Material* pMaterial = pSurface->GetMaterial();

    // defines in the shaders

    // ID_skinningMatrices - TODO

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
    if (pMaterial->HasPBRInfo())
    {
        if (pMaterial->HasPBRMetalRough())
        {
            defineList.insert(std::make_pair(L"MATERIAL_METALLICROUGHNESS", L""));
            AddTextureToDefineList(defineList, usedAttributes, surfaceAttributes, pMaterial, TextureClass::Albedo, L"ID_albedoTexture", L"ID_albedoTexCoord");
            AddTextureToDefineList(defineList, usedAttributes, surfaceAttributes, pMaterial, TextureClass::MetalRough, L"ID_metallicRoughnessTexture", L"ID_metallicRoughnessTexCoord");
        }
        else if (pMaterial->HasPBRSpecGloss())
        {
            defineList.insert(std::make_pair(L"MATERIAL_SPECULARGLOSSINESS", L""));
            AddTextureToDefineList(defineList, usedAttributes, surfaceAttributes, pMaterial, TextureClass::Albedo, L"ID_albedoTexture", L"ID_albedoTexCoord");
            AddTextureToDefineList(defineList, usedAttributes, surfaceAttributes, pMaterial, TextureClass::SpecGloss, L"ID_specularGlossinessTexture", L"ID_specularGlossinessTexCoord");
        }
    }
    AddTextureToDefineList(defineList, usedAttributes, surfaceAttributes, pMaterial, TextureClass::Normal, L"ID_normalTexture", L"ID_normalTexCoord");
    AddTextureToDefineList(defineList, usedAttributes, surfaceAttributes, pMaterial, TextureClass::Emissive, L"ID_emissiveTexture", L"ID_emissiveTexCoord");
    AddTextureToDefineList(defineList, usedAttributes, surfaceAttributes, pMaterial, TextureClass::Occlusion, L"ID_occlusionTexture", L"ID_occlusionTexCoord");

    if (pMaterial->HasDoubleSided())
        defineList.insert(std::make_pair(L"ID_doublesided", L""));

    if (pMaterial->GetBlendMode() == MaterialBlend::Mask)
        defineList.insert(std::make_pair(L"ID_alphaMask", L""));

    // Add additional output/export support if needed
    if (!m_OptionalTransparencyOptions.OptionalAdditionalOutputs.empty())
        defineList.insert(std::make_pair(L"ADDITIONAL_TRANSLUCENT_OUTPUTS", m_OptionalTransparencyOptions.OptionalAdditionalOutputs.c_str()));

    if (!m_OptionalTransparencyOptions.OptionalAdditionalExports.empty())
        defineList.insert(std::make_pair(L"ADDITIONAL_TRANSLUCENT_EXPORTS", m_OptionalTransparencyOptions.OptionalAdditionalExports.c_str()));

    defineList.insert(std::make_pair(L"TRANS_ALL_TEXTURES_INDEX", L"t" + std::to_wstring(3 + MAX_SHADOW_MAP_TEXTURES_COUNT)));
    defineList.insert(std::make_pair(L"HAS_WORLDPOS", L""));

    // Get the defines for attributes that make up the surface vertices
    Surface::GetVertexAttributeDefines(usedAttributes, defineList);

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
    psoDesc.SetRootSignature(m_pRootSignature);

    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"transformVS.hlsl", L"MainVS", ShaderModel::SM6_0, &defineList));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel (L"translucencyps.hlsl", L"MainPS", ShaderModel::SM6_0, &defineList));

    // Setup remaining information and build
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
    std::vector<ResourceFormat> rtFormats = {
        m_pColorRenderTarget->GetFormat()
    };

    // Add additional targets
    if (m_OptionalTransparencyOptions.OptionalTargets.size())
    {
        for (const auto& targetPair : m_OptionalTransparencyOptions.OptionalTargets)
        {
            // Add the format
            rtFormats.push_back(targetPair.first->GetFormat());
        }
    }

    psoDesc.AddRasterFormats(rtFormats, m_pDepthTarget->GetFormat());

    std::vector<BlendDesc> blendDesc = {
        {true, Blend::SrcAlpha, Blend::InvSrcAlpha, BlendOp::Add, Blend::One, Blend::Zero, BlendOp::Add, static_cast<uint32_t>(ColorWriteMask::All)}
    };

    // Add additional blends
    if (m_OptionalTransparencyOptions.OptionalTargets.size())
    {
        for (const auto& targetPair : m_OptionalTransparencyOptions.OptionalTargets)
            blendDesc.push_back(targetPair.second);
    }
    psoDesc.AddBlendStates(blendDesc, false, (blendDesc.size() > 1));

    RasterDesc rasterDesc;
    rasterDesc.CullingMode = CullMode::None;
    psoDesc.AddRasterStateDescription(&rasterDesc);

    // Set input layout
    std::vector<InputLayoutDesc> vertexAttributes;
    for (uint32_t attribute = 0; attribute < static_cast<uint32_t>(VertexAttributeType::Count); ++attribute)
    {
        // Check if the attribute is present
        if (usedAttributes & (0x1 << attribute))
            vertexAttributes.push_back(InputLayoutDesc(static_cast<VertexAttributeType>(attribute), pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(attribute)).ResourceDataFormat, static_cast<uint32_t>(vertexAttributes.size()), 0));
    }
    psoDesc.AddInputLayout(vertexAttributes);

    DepthDesc depthDesc;
    depthDesc.DepthEnable = true;
    depthDesc.DepthWriteEnable = false;
    depthDesc.StencilEnable = false;
    depthDesc.DepthFunc = ComparisonFunc::Less;
    psoDesc.AddDepthState(&depthDesc);

    PipelineObject* pPipelineObj = PipelineObject::CreatePipelineObject(L"TranslucencyRenderPass_PipelineObj", psoDesc);

    // Ok, this is a new pipeline, add it to the PipelineHashObject vector
    PipelineHashObject pipelineHashObject;
    pipelineHashObject.m_Pipeline = pPipelineObj;
    pipelineHashObject.m_PipelineHash = hash;
    pipelineHashObject.m_UsedAttributes = usedAttributes;
    m_PipelineHashObjects.push_back(pipelineHashObject);

    return static_cast<uint32_t>(m_PipelineHashObjects.size() - 1);
}

// Add texture index info and return the index to the texture in the texture array
int32_t TranslucencyRenderModule::AddTexture(const Material* pMaterial, const TextureClass textureClass, int32_t& textureSamplerIndex)
{
    const cauldron::TextureInfo* pTextureInfo = pMaterial->GetTextureInfo(textureClass);
    if (pTextureInfo != nullptr)
    {
        // Check if the texture's sampler is already one we have, and if not add it
        for (textureSamplerIndex = 0; textureSamplerIndex < m_Samplers.size(); ++textureSamplerIndex)
        {
            if (m_Samplers[textureSamplerIndex]->GetDesc() == pTextureInfo->TexSamplerDesc)
                break; // found
        }

        // If we didn't find the sampler, add it
        if (textureSamplerIndex == m_Samplers.size())
        {
            Sampler* pSampler = Sampler::CreateSampler(L"TranslucencySampler", pTextureInfo->TexSamplerDesc);
            CauldronAssert(ASSERT_WARNING, pSampler, L"Could not create sampler for loaded content %ls", pTextureInfo->pTexture->GetDesc().Name.c_str());
            m_Samplers.push_back(pSampler);
        }

        // Find a slot for the texture
        int32_t firstFreeIndex = -1;
        for (int32_t i = 0; i < m_Textures.size(); ++i)
        {
            BoundTexture& boundTexture = m_Textures[i];

            // If this texture is already mapped, bump it's reference count
            if (pTextureInfo->pTexture == boundTexture.pTexture)
            {
                boundTexture.count += 1;
                return i;
            }

            // Try to re-use an existing entry that was released
            else if (firstFreeIndex < 0  && boundTexture.count == 0)
            {
                firstFreeIndex = i;
            }
        }

        // Texture wasn't found
        BoundTexture b = { pTextureInfo->pTexture, 1 };
        if (firstFreeIndex < 0)
        {
            m_Textures.push_back(b);
            return static_cast<int32_t>(m_Textures.size()) - 1;
        }
        else
        {
            m_Textures[firstFreeIndex] = b;
            return firstFreeIndex;
        }
    }
    return -1;
}

void TranslucencyRenderModule::RemoveTexture(int32_t index)
{
    if (index >= 0)
    {
        m_Textures[index].count -= 1;
        if (m_Textures[index].count == 0)
        {
            m_Textures[index].pTexture = nullptr;
        }
    }
}
