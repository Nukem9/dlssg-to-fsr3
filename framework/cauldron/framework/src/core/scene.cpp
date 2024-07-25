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

#include "core/scene.h"
#include "core/contentloader.h"
#include "core/contentmanager.h"
#include "core/framework.h"
#include "core/components/lightcomponent.h"
#include "core/components/meshcomponent.h"

#include "misc/assert.h"
#include "misc/math.h"

#include "render/device.h"

using namespace std::experimental;

namespace cauldron
{
    void BoundingBox::Grow(Vec4 point)
    {
        if (point.getW()) {
            point /= point.getW();
        }

        if (m_Empty)
        {
            m_Min = point;
            m_Max = point;
            m_Empty = false;
        }
        else
        {
            m_Min = MinPerElement(point, m_Min);
            m_Max = MaxPerElement(point, m_Max);
        }
    }

    void BoundingBox::Reset()
    {
        m_Empty = true;
        m_Min = m_Max = Vec4(0, 0, 0, 0);
    }

    Scene::Scene()
    {
        m_SceneInformation.MipLODBias = 0.f;
    }

    Scene::~Scene()
    {
        delete m_ASManager;
    }

    void Scene::InitScene()
    {
        if (GetConfig()->BuildRayTracingAccelerationStructure)
        {
            CauldronAssert(ASSERT_CRITICAL, GetDevice()->FeatureSupported(DeviceFeature::RT_1_0), L"Error: Building acceleration structure requires RT 1.0 or above capabilities");

            m_ASManager = ASManager::CreateASManager();
        }

        // Set exposure according to what was specified
        m_Exposure = GetConfig()->StartupContent.SceneExposure;
    }

    void Scene::InitSceneContent()
    {
        // Need to create our content on a background thread so proper notifiers can be called
        std::function<void(void*)> createContent = [this](void*)
        {
            // Load all IBL and BRDF data for scene
            TextureLoadCompletionCallbackFn CompletionCallback =
                [this](const std::vector<const Texture*>& textures, void* additionalParams = nullptr)
            {
                if (!GetScene()->GetIBLTexture(IBLTexture::Prefiltered))
                    GetScene()->SetIBLTexture(textures[0], IBLTexture::Prefiltered);
                if (!GetScene()->GetIBLTexture(IBLTexture::Irradiance))
                    GetScene()->SetIBLTexture(textures[1], IBLTexture::Irradiance);
                GetScene()->SetBRDFLutTexture(textures[2]);
            };
            filesystem::path specPath = GetConfig()->StartupContent.SpecularIBL;
            filesystem::path diffPath = GetConfig()->StartupContent.DiffuseIBL;
            filesystem::path brdfPath = L"..\\media\\Textures\\BRDF\\BrdfLut.dds";
            std::vector<TextureLoadInfo> texInfo;
            texInfo.push_back(TextureLoadInfo(specPath));
            texInfo.push_back(TextureLoadInfo(diffPath));
            texInfo.push_back(TextureLoadInfo(brdfPath));
            GetContentManager()->LoadTextures(texInfo, CompletionCallback);

            float IBLFactor = GetConfig()->StartupContent.IBLFactor;
            GetScene()->SetIBLFactor(IBLFactor);

            ContentBlock* pContentBlock = new ContentBlock();

            // Memory backing camera creation
            EntityDataBlock* pCameraDataBlock = new EntityDataBlock();
            pContentBlock->EntityDataBlocks.push_back(pCameraDataBlock);
            pCameraDataBlock->pEntity = new Entity(L"StaticScenePerspectiveCamera");
            m_pDefaultPerspCamera = pCameraDataBlock->pEntity;
            CauldronAssert(ASSERT_CRITICAL, m_pDefaultPerspCamera, L"Could not allocate default perspective camera entity");

            // Use the same matrix setup as Cauldron 1.4 (note that Cauldron kept view-matrix native transforms, and our entity needs the inverse of that)
            Mat4 transform = LookAtMatrix(Vec4(5.13694048f, 1.89175785f, -1.40289795f, 0.f), Vec4(0.703276634f, 1.02280307f, 0.218072295f, 0.f), Vec4(0.f, 1.f, 0.f, 0.f));
            transform = InverseMatrix(transform);
            m_pDefaultPerspCamera->SetTransform(transform);

            // Setup default camera parameters
            CameraComponentData defaultPerspCameraCompData;
            defaultPerspCameraCompData.Name = L"StaticScenePerspectiveCamera";
            defaultPerspCameraCompData.Perspective.AspectRatio = GetFramework()->GetAspectRatio();
            defaultPerspCameraCompData.Perspective.Yfov = CAULDRON_PI2 / defaultPerspCameraCompData.Perspective.AspectRatio;

            CameraComponentData* pCameraComponentData = new CameraComponentData(defaultPerspCameraCompData);
            pCameraDataBlock->ComponentsData.push_back(pCameraComponentData);
            CameraComponent* pCameraComponent = CameraComponentMgr::Get()->SpawnCameraComponent(m_pDefaultPerspCamera, pCameraComponentData);
            pCameraDataBlock->Components.push_back(pCameraComponent);
            m_pCurrentCamera = pCameraComponent;

            // Memory backing light creation
            EntityDataBlock* pLightDataBlock = new EntityDataBlock();
            pContentBlock->EntityDataBlocks.push_back(pLightDataBlock);
            pLightDataBlock->pEntity = new Entity(L"DefaultSpotLight");
            m_pDefaultLight = pLightDataBlock->pEntity;
            CauldronAssert(ASSERT_CRITICAL, m_pDefaultLight, L"Could not allocate default spotlight entity");

            // Use the same light setup as Cauldron 1.4 (note that Cauldron kept view-matrix native transforms, and our entity needs the inverse of that)
            Vec4 from = PolarToVector(CAULDRON_PI2, 0.58f) * 3.5f;
            transform = LookAtMatrix(from, Vec4(0, 0, 0, 0), Vec4(0, 1, 0, 0));
            transform = InverseMatrix(transform);
            m_pDefaultLight->SetTransform(transform);

            // Setup default light parameters
            LightComponentData defaultLightCompoData;
            defaultLightCompoData.Name = L"DefaultSpotLight";   // Only need to set the name as the other parameters default to directional
            defaultLightCompoData.Type = LightType::Spot;
            defaultLightCompoData.Intensity = 10.f;
            defaultLightCompoData.Range = 15;
            defaultLightCompoData.SpotOuterConeAngle = CAULDRON_PI4;
            defaultLightCompoData.SpotInnerConeAngle = CAULDRON_PI4 * 0.9f;
            defaultLightCompoData.ShadowResolution = 1024;

            LightComponentData* pLightComponentData = new LightComponentData(defaultLightCompoData);
            pLightDataBlock->ComponentsData.push_back(pLightComponentData);
            LightComponent* pLightComponent = LightComponentMgr::Get()->SpawnLightComponent(m_pDefaultLight, pLightComponentData);
            pLightDataBlock->Components.push_back(pLightComponent);

            GetContentManager()->StartManagingContent(L"SceneDefaultEntities", pContentBlock, false);

            // Scene is now ready
            m_SceneReady = true;
        };

        // Scene content creation is fire and forget as it'll be one of the first things we do before loading actual scene data
        Task createContentTask(createContent, nullptr);
        GetTaskManager()->AddTask(createContentTask);
    }

    void Scene::TerminateScene()
    {
        // No more current camera
        m_pCurrentCamera = nullptr;

        // Count Entities in the Scene to check for leaks
        CauldronAssert(ASSERT_ERROR, m_SceneEntities.empty(), L"Not all entities were removed from scene.");
        m_SceneEntities.clear();
    }

    void Scene::UpdateScene(double deltaTime)
    {
        // If we have more than 1 light component, set the default light to off
        if (LightComponentMgr::Get()->GetComponentCount() > 1 && m_pDefaultLight->IsActive())
            m_pDefaultLight->SetActive(false);

        // Update scene information for the frame based on the current camera
        m_SceneInformation.CameraInfo.ViewMatrix                = m_pCurrentCamera->GetView();
        m_SceneInformation.CameraInfo.ProjectionMatrix          = m_pCurrentCamera->GetProjection();
        m_SceneInformation.CameraInfo.ViewProjectionMatrix      = m_pCurrentCamera->GetViewProjection();
        m_SceneInformation.CameraInfo.InvViewMatrix             = m_pCurrentCamera->GetInverseView();
        m_SceneInformation.CameraInfo.InvProjectionMatrix       = m_pCurrentCamera->GetInverseProjection();
        m_SceneInformation.CameraInfo.InvViewProjectionMatrix   = m_pCurrentCamera->GetInverseViewProjection();
        m_SceneInformation.CameraInfo.PrevViewMatrix            = m_pCurrentCamera->GetPreviousView();
        m_SceneInformation.CameraInfo.PrevViewProjectionMatrix  = m_pCurrentCamera->GetPreviousViewProjection();

        m_SceneInformation.CameraInfo.CurrJitter[0] = m_pCurrentCamera->GetProjectionJittered().getCol2().getX();
        m_SceneInformation.CameraInfo.CurrJitter[1] = m_pCurrentCamera->GetProjectionJittered().getCol2().getY();
        m_SceneInformation.CameraInfo.PrevJitter[0] = m_pCurrentCamera->GetPrevProjectionJittered().getCol2().getX();
        m_SceneInformation.CameraInfo.PrevJitter[1] = m_pCurrentCamera->GetPrevProjectionJittered().getCol2().getY();

        m_SceneInformation.CameraInfo.CameraPos = Vec4(m_pCurrentCamera->GetOwner()->GetTransform().getTranslation(), 0.f);

        // Update Upscaler Info
        UpscalerState    upscaleState = GetFramework()->GetUpscalingState();
        const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
        uint32_t rtWidth, rtHeight;
        float    widthScale, heightScale;
        GetFramework()->GetUpscaledRenderInfo(rtWidth, rtHeight, widthScale, heightScale);
        m_SceneInformation.UpscalerInfo.FullScreenScaleRatio = Vec4((float)rtWidth, (float)rtHeight, widthScale, heightScale);

        // Update lighting information for the scene
        m_SceneLightInformation.LightCount = 0;     // Reset light count
        m_SceneLightInformation.bUseScreenSpaceShadowMap  = GetScreenSpaceShadowTexture() != nullptr;
        const std::vector<Component*>& lightingComponents = LightComponentMgr::Get()->GetComponentList();
        for (auto* pComp : lightingComponents)
        {
            // Skip inactive lights
            if (!pComp->GetOwner()->IsActive())
                continue;

            static bool s_InvertedDepth = GetConfig()->InvertedDepth;
            const LightComponent* pLightComp = static_cast<LightComponent*>(pComp);

            LightInformation lightInfo;
            lightInfo.DirectionRange          = Vec4(pLightComp->GetDirection(), pLightComp->GetRange());
            lightInfo.ColorIntensity          = Vec4(pLightComp->GetColor(), pLightComp->GetIntensity());
            lightInfo.PosDepthBias            = Vec4(pLightComp->GetOwner()->GetTransform().getTranslation(), (s_InvertedDepth ? -1.0f: 1.0f ) * pLightComp->GetDepthBias());
            lightInfo.InnerConeCos            = pLightComp->GetInnerAngle();
            lightInfo.OuterConeCos            = pLightComp->GetOuterAngle();
            lightInfo.Type                    = static_cast<int>(pLightComp->GetType());
            lightInfo.NumCascades             = pLightComp->GetCascadesCount();
            if (pLightComp->GetCascadesCount() <= 1)
            {
                lightInfo.LightViewProj[0]           = pLightComp->GetViewProjection();
                lightInfo.ShadowMapIndex[0]          = pLightComp->GetShadowMapIndex();
                lightInfo.ShadowMapTransformation[0] = ShadowMapResourcePool::GetTransformation(pLightComp->GetShadowMapRect());
            }
            else
            {
                for (int i = 0; i < pLightComp->GetCascadesCount(); ++i)
                {
                    lightInfo.LightViewProj[i]           = pLightComp->GetShadowViewProjection(i);
                    lightInfo.ShadowMapIndex[i]          = pLightComp->GetShadowMapIndex(i);
                    lightInfo.ShadowMapTransformation[i] = ShadowMapResourcePool::GetTransformation(pLightComp->GetShadowMapRect(i));
                }
            }

            m_SceneLightInformation.LightInfo[m_SceneLightInformation.LightCount++] = lightInfo;
        }

        m_BoundingBoxUpdated = false;
    }

    void Scene::AddContentBlockEntities(const ContentBlock* pContentBlock)
    {
        for (auto* pEntityDataBlock : pContentBlock->EntityDataBlocks)
            m_SceneEntities.push_back(pEntityDataBlock->pEntity);

        // If the content block specified a new active camera, set it now
        if (pContentBlock->ActiveCamera)
            SetCurrentCamera(pContentBlock->ActiveCamera);

        UpdateSceneBoundingBox(pContentBlock);
    }

    void Scene::RemoveContentBlockEntities(const ContentBlock* pContentBlock)
    {
        // Gate that there are actually entities created in this block (might not be the case if something happened on load
        if (pContentBlock->EntityDataBlocks.empty())
            return;

        // Check if the current default camera is in this block, and if so reset to a default camera
        if (pContentBlock->ActiveCamera && pContentBlock->ActiveCamera == m_pCurrentCamera->GetOwner())
            SetCurrentCamera(nullptr);

        // Find where the first entry from the content block is
        for (auto iter = m_SceneEntities.begin(); iter != m_SceneEntities.end(); ++iter)
        {
            // Get the first entity in the data block
            if (*iter == pContentBlock->EntityDataBlocks[0]->pEntity)
            {
                m_SceneEntities.erase(iter, iter + pContentBlock->EntityDataBlocks.size());
                break;
            }
        }

        RecomputeSceneBoundingBox();
    }

    void Scene::SetCurrentCamera(const Entity* pCameraEntity)
    {
        // If we called set current camera with nullptr, set to the default
        if (!pCameraEntity)
            pCameraEntity = m_pDefaultPerspCamera;

        CameraComponent* pCameraComp = static_cast<CameraComponent*>(CameraComponentMgr::Get()->GetComponent(pCameraEntity));
        CauldronAssert(ASSERT_ERROR, pCameraComp, L"Could not find a camera component on Entity %ls", pCameraEntity->GetName());

        // Swap 'em if we got 'em
        if (pCameraComp)
        {
            m_pCurrentCamera->GetOwner()->SetActive(false);
            m_pCurrentCamera = pCameraComp;
            m_pCurrentCamera->GetOwner()->SetActive(true);
        }
    }

    void Scene::UpdateSceneBoundingBox(const ContentBlock* pContentBlock)
    {
        const MeshComponentMgr* pMeshComponentManager = MeshComponentMgr::Get();
        for (auto pEntityBlock : pContentBlock->EntityDataBlocks)
        {
            UpdateSceneBoundingBox(pEntityBlock->pEntity);
        }
        m_BoundingBoxUpdated = true;
    }

    void Scene::UpdateSceneBoundingBox(const Entity* pEntity)
    {
        const MeshComponent* pMeshComponent = pEntity->GetComponent<const MeshComponent>(MeshComponentMgr::Get());
        if (pMeshComponent != nullptr)
        {
            Mat4        transform = pEntity->GetTransform();
            const Mesh* pMesh     = pMeshComponent->GetData().pMesh;
            for (uint32_t i = 0; i < pMesh->GetNumSurfaces(); ++i)
            {
                const Surface* pSurface = pMesh->GetSurface(i);
                Vec4           center   = pSurface->Center();
                Vec4           radius   = pSurface->Radius();

                m_BoundingBox.Grow(transform * (center + Vec4(-radius.getX(), -radius.getY(), -radius.getZ(), 0.0f)));
                m_BoundingBox.Grow(transform * (center + Vec4(-radius.getX(), -radius.getY(), radius.getZ(), 0.0f)));
                m_BoundingBox.Grow(transform * (center + Vec4(-radius.getX(), radius.getY(), -radius.getZ(), 0.0f)));
                m_BoundingBox.Grow(transform * (center + Vec4(-radius.getX(), radius.getY(), radius.getZ(), 0.0f)));
                m_BoundingBox.Grow(transform * (center + Vec4(radius.getX(), -radius.getY(), -radius.getZ(), 0.0f)));
                m_BoundingBox.Grow(transform * (center + Vec4(radius.getX(), -radius.getY(), radius.getZ(), 0.0f)));
                m_BoundingBox.Grow(transform * (center + Vec4(radius.getX(), radius.getY(), -radius.getZ(), 0.0f)));
                m_BoundingBox.Grow(transform * (center + Vec4(radius.getX(), radius.getY(), radius.getZ(), 0.0f)));
            }
        }

        for (const auto& child : pEntity->GetChildren())
        {
            UpdateSceneBoundingBox(child);
        }
    }

    void Scene::RecomputeSceneBoundingBox()
    {
        // update all so reset the bounding box
        m_BoundingBox.Reset();

        const MeshComponentMgr* pMeshComponentManager = MeshComponentMgr::Get();
        for (auto pEntity : m_SceneEntities)
        {
            UpdateSceneBoundingBox(pEntity);
        }
    }

} // namespace cauldron
