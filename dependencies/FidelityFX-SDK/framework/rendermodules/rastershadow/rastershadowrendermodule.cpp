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

#include "rastershadowrendermodule.h"

#include "shaders/shadercommon.h"
#include "shaders/surfacerendercommon.h"

#include "core/framework.h"
#include "core/components/lightcomponent.h"
#include "core/components/meshcomponent.h"
#include "core/components/animationcomponent.h"
#include "core/scene.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rasterview.h"
#include "render/resourceview.h"
#include "render/rootsignature.h"
#include "render/shaderbuilderhelper.h"

#include <functional>

using namespace cauldron;

void RasterShadowRenderModule::Init(const json& initData)
{
    // Reserve space for the max number of supported textures (use a bindless approach to resource indexing)
    m_Textures.reserve(s_MaxTextureCount);

    // Reserve space for the max number of samplers
    m_Samplers.reserve(s_MaxSamplerCount);

    // Setup num splits according to config
    m_NumCascades = initData.value("NumCascades", m_NumCascades);

    // Root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::VertexAndPixel, 1);   // Camera Information
    signatureDesc.AddConstantBufferView(1, ShaderBindStage::VertexAndPixel, 1);   // Instance Information
    signatureDesc.AddConstantBufferView(2, ShaderBindStage::Pixel, 1);            // Texture Indices
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, s_MaxTextureCount); // Texture resource array

    // Create sampler set
    signatureDesc.AddSamplerSet(0, ShaderBindStage::Pixel, s_MaxSamplerCount);

    m_pRootSignature = RootSignature::CreateRootSignature(L"RasterShadowRenderModule_RootSignature", signatureDesc);

    // Create ParameterSet and assign the constant buffer parameters
    // We will add texture views as they are loaded
    m_pParameterSet = ParameterSet::CreateParameterSet(m_pRootSignature);
    m_pParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 0);
    m_pParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(InstanceInformation), 1);
    m_pParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(TextureIndices), 2);

    // Register for content change updates
    GetContentManager()->AddContentListener(this);

    SetModuleReady(true);
}

RasterShadowRenderModule::~RasterShadowRenderModule()
{
    GetContentManager()->RemoveContentListener(this);

    delete m_pRootSignature;
    delete m_pParameterSet;

    for (auto sampler : m_Samplers)
        delete sampler;

    // Release pipeline objects and clear all mappings
    for (auto& pipelineGroup : m_PipelineRenderGroups)
    {
        CauldronAssert(ASSERT_ERROR, pipelineGroup.m_RenderSurfaces.empty(), L"Not all pipeline surfaces have been removed. This ship is leaking.");
        delete pipelineGroup.m_Pipeline;
    }

    ShadowMapResourcePool* pShadowMapResourcePool = GetFramework()->GetShadowMapResourcePool();
    CauldronAssert(ASSERT_ERROR, m_ShadowMapInfos.size() == 0, L"Not all lights have been removed");
    for (auto shadowMapInfo : m_ShadowMapInfos)
    {
        for (auto pLightComponent : shadowMapInfo.LightComponents)
        {
            for (int i = 0; i < pLightComponent->GetShadowMapCount(); ++i)
            {
                if (shadowMapInfo.ShadowMapIndex == pLightComponent->GetShadowMapIndex(i))
                    pShadowMapResourcePool->ReleaseShadowMap(pLightComponent->GetShadowMapIndex(i), pLightComponent->GetShadowMapCellIndex(i));
            }
        }
    }

    m_Samplers.clear();
    m_PipelineRenderGroups.clear();
    m_ShadowMapInfos.clear();
}

void RasterShadowRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    GPUScopedProfileCapture rasterShadowMapMarker(pCmdList, L"RasterShadow");

    std::lock_guard<std::mutex> shadowLock(m_CriticalSection);

    // Need to check this each update in case it changes and we need to change the UI
    bool hasDirectionalLight = false;

    // Transition all the shadow maps for write
    // Render modules expect resources coming in/going out to be in a shader read state
    ShadowMapResourcePool* pShadowPool = GetFramework()->GetShadowMapResourcePool();
    std::vector<Barrier> barriers;
    for (uint32_t i = 0; i < pShadowPool->GetRenderTargetCount(); ++i)
    {
        barriers.push_back(Barrier::Transition(pShadowPool->GetRenderTarget(i)->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::DepthWrite));
    }
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    //Early instantiate to prevent realloc in loops.
    std::vector<BufferAddressInfo> vertexBuffers;
    std::vector<BufferAddressInfo> perObjectBufferInfos;
    std::vector<BufferAddressInfo> textureIndicesBufferInfos;

    for (auto shadowMapInfo : m_ShadowMapInfos)
    {
        CauldronAssert(ASSERT_ERROR, shadowMapInfo.ShadowMapIndex >= 0, L"RasterShadowRenderModule register a shadow casting light that doesn't have a render target");

        // no light component, exit
        if (shadowMapInfo.LightComponents.size() == 0)
            continue;

        const Texture* pShadowMapTarget = pShadowPool->GetRenderTarget(shadowMapInfo.ShadowMapIndex);
        CauldronAssert(ASSERT_CRITICAL, pShadowMapTarget != nullptr, L"Unable to get a shadow map");

        // Do clears
        ClearDepthStencil(pCmdList, &shadowMapInfo.pRasterView->GetResourceView(), 0);

        // Bind raster resources
        BeginRaster(pCmdList, 0, nullptr, shadowMapInfo.pRasterView);

        Rect scissorRect = { 0, 0, pShadowMapTarget->GetDesc().Width, pShadowMapTarget->GetDesc().Height };
        SetScissorRects(pCmdList, 1, &scissorRect);
        SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

        for (auto pLightComponent : shadowMapInfo.LightComponents)
        {
            for (int i = 0; i < pLightComponent->GetShadowMapCount(); ++i)
            {
                if (shadowMapInfo.ShadowMapIndex != pLightComponent->GetShadowMapIndex(i))
                    continue;

                SceneInformation sceneInfo;
                const wchar_t* shadowMapName = nullptr;
                switch (pLightComponent->GetType())
                {
                case LightType::Directional:
                    shadowMapName = L"Directional shadow map";
                    hasDirectionalLight = true;
                    break;
                case LightType::Spot:
                    shadowMapName = L"Spot shadow map";
                    break;
                case LightType::Point:
                    shadowMapName = L"Point shadow map";
                    break;
                default:
                    shadowMapName = L"Unknown shadow map";
                    break;
                }

                // only update the necessary
                if (pLightComponent->GetCascadesCount() <= 1)
                    sceneInfo.CameraInfo.ViewProjectionMatrix = pLightComponent->GetViewProjection();
                else
                    sceneInfo.CameraInfo.ViewProjectionMatrix = pLightComponent->GetShadowViewProjection(i);

                sceneInfo.MipLODBias = GetScene()->GetSceneInfo().MipLODBias;

                // Update necessary scene frame information
                BufferAddressInfo cameraBufferInfo =
                    GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SceneInformation), reinterpret_cast<const void*>(&sceneInfo));
                m_pParameterSet->UpdateRootConstantBuffer(&cameraBufferInfo, 0);

                // Set viewport, scissor, primitive topology once and move on
                Viewport vp = ShadowMapResourcePool::GetViewport(pLightComponent->GetShadowMapRect());
                SetViewport(pCmdList, &vp);

                // Render all surfaces by pipeline groupings
                for (auto& pipelineGroup : m_PipelineRenderGroups)
                {
                    // Set the pipeline to use for all render calls
                    SetPipelineState(pCmdList, pipelineGroup.m_Pipeline);

                    uint32_t activeCount = 0;

                    for (auto& pipelineSurfaceInfo : pipelineGroup.m_RenderSurfaces)
                        if (pipelineSurfaceInfo.pOwner->IsActive())
                            activeCount++;

                    perObjectBufferInfos.clear();
                    perObjectBufferInfos.resize(activeCount);
                    GetDynamicBufferPool()->BatchAllocateConstantBuffer(sizeof(InstanceInformation), activeCount, perObjectBufferInfos.data());
                    textureIndicesBufferInfos.clear();
                    textureIndicesBufferInfos.resize(activeCount);
                    GetDynamicBufferPool()->BatchAllocateConstantBuffer(sizeof(TextureIndices), activeCount, textureIndicesBufferInfos.data());
                    uint32_t currentSurface = 0;


                    for (auto& pipelineSurfaceInfo : pipelineGroup.m_RenderSurfaces)
                    {
                        // Make sure owner is active
                        if (pipelineSurfaceInfo.pOwner->IsActive())
                        {
                            const Surface* pSurface = pipelineSurfaceInfo.pSurface;
                            const Material* pMaterial = pSurface->GetMaterial();

                            // NOTE - We should enforce no scaling on transforms as we don't support scaled matrix transforms in the shader
                            InstanceInformation instanceInfo;
                            instanceInfo.WorldTransform = pipelineSurfaceInfo.pOwner->GetTransform();
                            instanceInfo.MaterialInfo.AlphaCutoff = pMaterial->GetAlphaCutOff();

                            BufferAddressInfo& perObjectBufferInfo = perObjectBufferInfos[currentSurface];
                            GetDynamicBufferPool()->InitializeConstantBuffer(perObjectBufferInfo, sizeof(InstanceInformation), &instanceInfo);

                            BufferAddressInfo& textureIndicesBufferInfo = textureIndicesBufferInfos[currentSurface];
                            GetDynamicBufferPool()->InitializeConstantBuffer(
                                textureIndicesBufferInfo, sizeof(TextureIndices), &pipelineSurfaceInfo.TextureIndices);

                            currentSurface++;


                            m_pParameterSet->UpdateRootConstantBuffer(&perObjectBufferInfo, 1);
                            m_pParameterSet->UpdateRootConstantBuffer(&textureIndicesBufferInfo, 2);

                            // Bind everything
                            m_pParameterSet->Bind(pCmdList, pipelineGroup.m_Pipeline);

                            vertexBuffers.clear();
                            for (uint32_t attribute = 0; attribute < static_cast<uint32_t>(VertexAttributeType::Count); ++attribute)
                            {
                                // Check if the attribute is present
                                if (pipelineGroup.m_UsedAttributes & (0x1 << attribute))
                                {
                                    vertexBuffers.emplace_back(pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(attribute)).pBuffer->GetAddressInfo());
                                }
                            }

                            // Skeletal Animation
                            if (pipelineSurfaceInfo.pOwner->HasComponent(AnimationComponentMgr::Get()))
                            {
                                const auto& data = pipelineSurfaceInfo.pOwner->GetComponent<const AnimationComponent>(AnimationComponentMgr::Get())->GetData();

                                if (data->m_skinId != -1)
                                {
                                    // Positions are stored at index 0
                                    // Normals are stored at index 1

                                    // Replace the vertices POSITION attribute with the Skinned POSITION attribute
                                    const uint32_t surfaceID = pSurface->GetSurfaceID();
                                    vertexBuffers[0]         = data->m_skinnedPositions[surfaceID].pBuffer->GetAddressInfo();
                                }
                            }

                            // Set vertex/index buffers
                            SetVertexBuffers(pCmdList, 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data());

                            BufferAddressInfo addressInfo = pSurface->GetIndexBuffer().pBuffer->GetAddressInfo();
                            SetIndexBuffer(pCmdList, &addressInfo);

                            // And draw
                            DrawIndexedInstanced(pCmdList, pSurface->GetIndexBuffer().Count);
                        }
                    }
                }
            }
        }

        // Done drawing, unbind
        EndRaster(pCmdList);
    }

    // Transition all the shadow maps back to expected state
    barriers.clear();
    for (uint32_t i = 0; i < pShadowPool->GetRenderTargetCount(); ++i)
    {
        barriers.push_back(Barrier::Transition(pShadowPool->GetRenderTarget(i)->GetResource(), ResourceState::DepthWrite, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    }
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    // Update the UI state
    UpdateUIState(hasDirectionalLight);
}

void RasterShadowRenderModule::OnNewContentLoaded(ContentBlock* pContentBlock)
{
    MeshComponentMgr* pMeshComponentManager = MeshComponentMgr::Get();
    LightComponentMgr* pLightComponentManager = LightComponentMgr::Get();

    std::lock_guard<std::mutex> lock(m_CriticalSection);

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

                    // Push surface render information
                    PipelineSurfaceRenderInfo surfaceRenderInfo;
                    surfaceRenderInfo.pOwner = pComponent->GetOwner();
                    surfaceRenderInfo.pSurface = pSurface;

                    int32_t samplerIndex;
                    if (pMaterial->HasPBRInfo())
                    {
                        surfaceRenderInfo.TextureIndices.AlbedoTextureIndex = AddTexture(pMaterial, TextureClass::Albedo, samplerIndex);
                        surfaceRenderInfo.TextureIndices.AlbedoSamplerIndex = samplerIndex;
                    }

                    // Assign to the correct pipeline render group (will create a new pipeline group if needed)
                    m_PipelineRenderGroups[GetPipelinePermutationID(pSurface)].m_RenderSurfaces.push_back(surfaceRenderInfo);
                }
            }
            else if (pComponent->GetManager() == pLightComponentManager)
            {
                LightComponent* pLightComponent = reinterpret_cast<LightComponent*>(pComponent);
                CauldronAssert(ASSERT_CRITICAL, pLightComponent->GetShadowMapIndex() == -1, L"A shadow map has already been set on this light.");
                switch (pLightComponent->GetType())
                {
                case LightType::Directional:
                    pLightComponent->SetupCascades(m_NumCascades, m_CascadeSplitPoints, m_MoveLightTexelSize);
                    CreateShadowMapInfo(pLightComponent, ShadowMapResolution::Full);
                    break;
                case LightType::Spot:
                    CreateShadowMapInfo(pLightComponent, ShadowMapResolution::Half);
                    break;
                case LightType::Point:
                    // not supported
                    break;
                default:
                    break;
                }
            }
        }
    }

    {
        // Update the parameter set with loaded texture entries
        CauldronAssert(ASSERT_CRITICAL, m_Textures.size() <= s_MaxTextureCount, L"Too many textures.");
        for (uint32_t i = 0; i < m_Textures.size(); ++i)
            m_pParameterSet->SetTextureSRV(m_Textures[i].pTexture, ViewDimension::Texture2D, i);

        // Update sampler bindings as well
        CauldronAssert(ASSERT_CRITICAL, m_Samplers.size() <= s_MaxSamplerCount, L"Too many samplers.");
        for (uint32_t i = 0; i < m_Samplers.size(); ++i)
            m_pParameterSet->SetSampler(m_Samplers[i], i);
    }
}

void RasterShadowRenderModule::OnContentUnloaded(ContentBlock* pContentBlock)
{
    for (auto* pEntityData : pContentBlock->EntityDataBlocks)
    {
        for (auto* pComponent : pEntityData->Components)
        {
            if (pComponent->GetManager() == MeshComponentMgr::Get())
            {
                const Mesh* pMesh = reinterpret_cast<MeshComponent*>(pComponent)->GetData().pMesh;
                const Entity* pOwner = pComponent->GetOwner();

                const size_t numSurfaces = pMesh->GetNumSurfaces();
                for (uint32_t i = 0; i < numSurfaces; ++i)
                {
                    const Surface* pSurface = pMesh->GetSurface(i);

                    // We're going to be modifying the pipeline groups, so make sure no one else is using them
                    std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);

                    // Find what list the surface is in
                    for (auto& pipelineGroup : m_PipelineRenderGroups)
                    {
                        bool surfaceFound = false;
                        for (auto& surfaceItr = pipelineGroup.m_RenderSurfaces.begin(); surfaceItr != pipelineGroup.m_RenderSurfaces.end(); ++surfaceItr)
                        {
                            if (surfaceItr->pOwner == pOwner && surfaceItr->pSurface == pSurface)
                            {
                                // Don't keep looking
                                surfaceFound = true;

                                // Remove the texture entries
                                RemoveTexture(surfaceItr->TextureIndices.AlbedoTextureIndex);

                                // Remove it from the list
                                pipelineGroup.m_RenderSurfaces.erase(surfaceItr);
                                break;
                            }
                        }

                        // Don't need to check any other pipeline groups if we've already found it
                        if (surfaceFound)
                            break;
                    }
                }
            }
            else if (pComponent->GetManager() == LightComponentMgr::Get())
            {
                LightComponent* pLightComponent = reinterpret_cast<LightComponent*>(pComponent);
                std::lock_guard<std::mutex> pipelineLock(m_CriticalSection);
                DestroyShadowMapInfo(pLightComponent);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// Content loading helpers

uint32_t RasterShadowRenderModule::GetPipelinePermutationID(const Surface* pSurface) // uint32_t vertexAttributeFlags, const Material* pMaterial)
{
    // RasterShadow shader should be optimized based on what the model provides
    //   - It only needs the Position and Color0 attributes

    // Those are the only attributes we can accept
    uint32_t usedAttributes = VertexAttributeFlag_Position | VertexAttributeFlag_Color0;

    // only keep the available attributes of the surface
    const uint32_t surfaceAttributes = pSurface->GetVertexAttributes();
    usedAttributes = usedAttributes & surfaceAttributes;
    DefineList defineList;

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

    if (pMaterial->HasPBRInfo())
    {
        if (pMaterial->HasPBRMetalRough())
        {
            defineList.insert(std::make_pair(L"MATERIAL_METALLICROUGHNESS", L""));
            AddTextureToDefineList(defineList, usedAttributes, surfaceAttributes, pMaterial, TextureClass::Albedo, L"ID_albedoTexture", L"ID_albedoTexCoord");
            AddTextureToDefineList(defineList,
                usedAttributes,
                surfaceAttributes,
                pMaterial,
                TextureClass::MetalRough,
                L"ID_metallicRoughnessTexture",
                L"ID_metallicRoughnessTexCoord");
        }
        else if (pMaterial->HasPBRSpecGloss())
        {
            defineList.insert(std::make_pair(L"MATERIAL_SPECULARGLOSSINESS", L""));
            AddTextureToDefineList(defineList, usedAttributes, surfaceAttributes, pMaterial, TextureClass::Albedo, L"ID_albedoTexture", L"ID_albedoTexCoord");
            AddTextureToDefineList(defineList,
                usedAttributes,
                surfaceAttributes,
                pMaterial,
                TextureClass::SpecGloss,
                L"ID_specularGlossinessTexture",
                L"ID_specularGlossinessTexCoord");
        }
    }

    if (pMaterial->GetBlendMode() != MaterialBlend::Opaque)
    {
        defineList.insert(std::make_pair(L"DEF_alphaMode_MASK", L""));

        if (pMaterial->GetBlendMode() == MaterialBlend::Mask)
        {
            defineList.insert(std::make_pair(L"DEF_alphaCutoff", std::to_wstring(pMaterial->GetAlphaCutOff())));
        }
        else if (pMaterial->GetBlendMode() == MaterialBlend::AlphaBlend)
        {
            defineList.insert(std::make_pair(L"DEF_alphaCutoff", L"0.99"));
        }
    }
    defineList.insert(std::make_pair(L"NO_WORLDPOS", L"")); // no need for the vert4ext shader to output world pos

    // Get the defines for attributes that make up the surface vertices
    Surface::GetVertexAttributeDefines(usedAttributes, defineList);

    // compute hash
    uint64_t hash = static_cast<uint64_t>(Hash(defineList, usedAttributes, pSurface));

    // See if we've already built this pipeline
    for (uint32_t i = 0; i < m_PipelineRenderGroups.size(); ++i)
    {
        if (m_PipelineRenderGroups[i].m_PipelineHash == hash)
            return i;
    }

    // If we didn't find the pipeline already, create a new one

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pRootSignature);

    // Setup the shaders to build on the pipeline object
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"transformVS.hlsl", L"MainVS", ShaderModel::SM6_0, &defineList));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"rastershadowps.hlsl", L"MainPS", ShaderModel::SM6_0, &defineList));

    // Setup remaining information and build
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
    psoDesc.AddRenderTargetFormats(0, nullptr, GetFramework()->GetShadowMapResourcePool()->GetShadowMapTextureFormat());

    RasterDesc rasterDesc;
    rasterDesc.CullingMode = CullMode::None;    // To help avoid light leaks
    psoDesc.AddRasterStateDescription(&rasterDesc);

    std::vector<InputLayoutDesc> vertexAttributes;
    for (uint32_t attribute = 0; attribute < static_cast<uint32_t>(VertexAttributeType::Count); ++attribute)
    {
        // Check if the attribute is present
        if (usedAttributes & 0x1 << attribute)
            vertexAttributes.push_back(InputLayoutDesc(static_cast<VertexAttributeType>(attribute), pSurface->GetVertexBuffer(static_cast<VertexAttributeType>(attribute)).ResourceDataFormat, (uint32_t)vertexAttributes.size(), 0));
    }
    psoDesc.AddInputLayout(vertexAttributes);

    DepthDesc depthDesc;
    depthDesc.DepthEnable = true;
    depthDesc.StencilEnable = false;
    depthDesc.DepthWriteEnable = true;
    depthDesc.DepthFunc = ComparisonFunc::Less;
    psoDesc.AddDepthState(&depthDesc);

    PipelineObject*              pPipelineObj = PipelineObject::CreatePipelineObject(L"RasterShadowRenderModule_PipelineObj", psoDesc);

    PipelineRenderGroup pipelineGroup;
    pipelineGroup.m_Pipeline = pPipelineObj;
    pipelineGroup.m_PipelineHash = hash;
    pipelineGroup.m_UsedAttributes = usedAttributes;
    m_PipelineRenderGroups.push_back(pipelineGroup);

    return static_cast<uint32_t>(m_PipelineRenderGroups.size() - 1);
}

// Add texture index info and return the index to the texture in the texture array
int32_t RasterShadowRenderModule::AddTexture(const Material* pMaterial, const TextureClass textureClass, int32_t& textureSamplerIndex)
{
    const cauldron::TextureInfo* pTextureInfo = pMaterial->GetTextureInfo(textureClass);
    if (pTextureInfo != nullptr)
    {
        // Check if the texture's sampler is already one we have, and if not add it
        for (textureSamplerIndex = 0; textureSamplerIndex < m_Samplers.size(); ++textureSamplerIndex)
        {
            if (m_Samplers[textureSamplerIndex]->GetDesc() == pTextureInfo->TexSamplerDesc)
                break;  // found
        }

        // If we didn't find the sampler, add it
        if (textureSamplerIndex == m_Samplers.size())
        {
            Sampler* pSampler = Sampler::CreateSampler(L"RasterShadowSampler", pTextureInfo->TexSamplerDesc);
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
            else if (firstFreeIndex < 0 && boundTexture.count == 0)
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

void RasterShadowRenderModule::RemoveTexture(int32_t index)
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

void RasterShadowRenderModule::CreateShadowMapInfo(LightComponent* pLightComponent, ShadowMapResolution resolution)
{
    LightComponentData& lightData = pLightComponent->GetData();

    lightData.ShadowResolution = g_ShadowMapTextureSize / static_cast<uint32_t>(resolution);

    for (int i = 0; i < pLightComponent->GetShadowMapCount(); ++i)
    {
        ShadowMapResourcePool* pResourcePool = GetFramework()->GetShadowMapResourcePool();
        ShadowMapResourcePool::ShadowMapView view = pResourcePool->GetNewShadowMap(resolution);
        CauldronAssert(ASSERT_WARNING, view.index >= 0, L"Unable to get a shadow map texture from the pool.");

        // find if the shadow map info already exists
        auto iter = m_ShadowMapInfos.begin();
        for (; iter < m_ShadowMapInfos.end(); ++iter)
        {
            if (iter->ShadowMapIndex == view.index)
                break;
        }
        if (iter == m_ShadowMapInfos.end())
        {
            // create a new entry
            ShadowMapInfo shadowMapInfo;
            shadowMapInfo.ShadowMapIndex = view.index;

            // Setup raster view
            shadowMapInfo.pRasterView = GetRasterViewAllocator()->RequestRasterView(pResourcePool->GetRenderTarget(view.index), ViewDimension::Texture2D);

            m_ShadowMapInfos.push_back(shadowMapInfo);

            // get the correct iterator for this entry
            iter = std::prev(m_ShadowMapInfos.end());
        }

        if (std::find(iter->LightComponents.begin(), iter->LightComponents.end(), pLightComponent) == iter->LightComponents.end())
            iter->LightComponents.push_back(pLightComponent);

        lightData.ShadowMapIndex[i] = view.index;
        lightData.ShadowMapCellIndex[i] = view.cellIndex;
        lightData.ShadowMapRect[i] = view.rect;
    }
}

void RasterShadowRenderModule::DestroyShadowMapInfo(cauldron::LightComponent* pLightComponent)
{
    for (int i = 0; i < pLightComponent->GetShadowMapCount(); ++i)
    {
        if (pLightComponent->GetShadowMapIndex(i) >= 0)
        {
            ShadowMapResourcePool* pResourcePool = GetFramework()->GetShadowMapResourcePool();
            pResourcePool->ReleaseShadowMap(pLightComponent->GetShadowMapIndex(i), pLightComponent->GetShadowMapCellIndex(i));

            // remove shadow map info
            for (auto iter = m_ShadowMapInfos.begin(); iter != m_ShadowMapInfos.end(); ++iter)
            {
                if (iter->ShadowMapIndex == pLightComponent->GetShadowMapIndex(i))
                {
                    auto lightIter = iter->LightComponents.begin();
                    for (; lightIter < iter->LightComponents.end(); ++lightIter)
                    {
                        if (*lightIter == pLightComponent)
                            break;
                    }
                    if (lightIter != iter->LightComponents.end())
                    {
                        iter->LightComponents.erase(lightIter);
                        if (iter->LightComponents.size() == 0)
                            m_ShadowMapInfos.erase(iter);
                        break;
                    }
                }
            }
        }
    }
}

void RasterShadowRenderModule::UpdateUIState(bool hasDirectional)
{
    // If we have the UI setup already
    if (m_UISection && !m_UISection->GetElements().empty())
    {
        // If we no longer have directional light, hide the UI
        if (m_DirUIShowing && !hasDirectional)
        {
            m_UISection->Show(false);
            m_DirUIShowing = false;
        }

        // If we have have directional light again, show it again
        else if (hasDirectional && !m_DirUIShowing)
        {
            m_UISection->Show(true);
            m_DirUIShowing = true;
        }
    }

    // If we don't have the UI setup yet, and we have a directional light, setup the UI
    else if (hasDirectional)
    {
        m_UISection = GetUIManager()->RegisterUIElements("Shadow");

        // Init cascade enables to number of cascades
        m_CascadeSplitPointsEnabled[0] = m_NumCascades > 1;
        m_CascadeSplitPointsEnabled[1] = m_NumCascades > 2;
        m_CascadeSplitPointsEnabled[2] = m_NumCascades > 3;

        // Setup cascades number
        static bool enabled = true;
#if defined(_VK)
        // There is a known issue with native VK backend holding on to resource handles
        // when it shouldn't be, so don't allow changing of slice count on VK for now.
        // This will be in the "Known Issues" section of the documentation
        enabled = false;
#endif // #if defined(_VK)
        m_UISection->RegisterUIElement<UISlider<int32_t>>(
            "Cascades Number",
            m_NumCascades,
            1, 4,
            enabled,
            [this](int32_t cur, int32_t old) {
                for (int i = 0; i < _countof(m_CascadeSplitPointsEnabled); ++i)
                    m_CascadeSplitPointsEnabled[i] = (m_NumCascades > i + 1);
                UpdateCascades();
            });

        // Setup cascade split points
        m_UISection->RegisterUIElement<UISlider<float>>(
            "Cascade Split Points 0",
            m_CascadeSplitPoints[0],
            0.0f, 100.0f,
            m_CascadeSplitPointsEnabled[0],
            [this](int32_t cur, int32_t old) { UpdateCascades(); },
            true, false, "%.2f%%");
        m_UISection->RegisterUIElement<UISlider<float>>(
            "Cascade Split Points 1",
            m_CascadeSplitPoints[1],
            0.0f, 100.0f,
            m_CascadeSplitPointsEnabled[1],
            [this](int32_t cur, int32_t old) { UpdateCascades(); },
            true, false, "%.2f%%");
        m_UISection->RegisterUIElement<UISlider<float>>(
            "Cascade Split Points 2",
            m_CascadeSplitPoints[2],
            0.0f, 100.0f,
            m_CascadeSplitPointsEnabled[2],
            [this](int32_t cur, int32_t old) { UpdateCascades(); },
            true, false, "%.2f%%");

        // bMoveLightTexelSize
        m_UISection->RegisterUIElement<UICheckBox>(
            "Camera Pixel Align",
            m_MoveLightTexelSize,
            [this](bool cur, bool old) {
                for (int i = 0; i < _countof(m_CascadeSplitPointsEnabled); ++i)
                    m_CascadeSplitPointsEnabled[i] = (m_NumCascades > i + 1);
                UpdateCascades();
            });

        m_DirUIShowing = true;
    }
}

void RasterShadowRenderModule::UpdateCascades()
{
    // Find all the directional lights
    std::set<LightComponent*> directionalLightComponents;
    for (auto iter = m_ShadowMapInfos.begin(); iter != m_ShadowMapInfos.end(); ++iter)
    {
        for (auto lightIter = iter->LightComponents.begin(); lightIter < iter->LightComponents.end(); ++lightIter)
        {
            LightComponent* pLightComponent = const_cast<LightComponent*>(*lightIter);
            if (pLightComponent->GetType() == LightType::Directional)
            {
                directionalLightComponents.insert(pLightComponent);
            }
        }
    }

    // Traverse directional lights
    for (auto lightIter = directionalLightComponents.begin(); lightIter != directionalLightComponents.end(); ++lightIter)
    {
        LightComponent* pLightComponent = *lightIter;
        if (pLightComponent->GetCascadesCount() != (m_NumCascades))
        {
            // If need to modify the Cascade Number, destroy ShadowMaps and recreate
            DestroyShadowMapInfo(pLightComponent);
            pLightComponent->SetupCascades(m_NumCascades, m_CascadeSplitPoints, m_MoveLightTexelSize);
            CreateShadowMapInfo(pLightComponent, ShadowMapResolution::Full);
        }
        else
        {
            // Just setup
            pLightComponent->SetupCascades(m_NumCascades, m_CascadeSplitPoints, m_MoveLightTexelSize);
        }
    }

    // Any of these changes need to force the camera to be dirtry
    GetScene()->GetCurrentCamera()->SetDirty();
}
