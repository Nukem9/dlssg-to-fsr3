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

#pragma once

#include "misc/helpers.h"
#include "misc/math.h"
#include "core/components/cameracomponent.h"
#include "core/components/lightcomponent.h"
#include "shaders/shadercommon.h"
#include "render/rtresources.h"

#include <vector>

namespace cauldron
{
    struct ContentBlock;
    class Entity;

    /**
     * @enum class IBLTexture
     *
     * Type of image-based lighting texture.
     *
     * @ingroup CauldronCore
     */
    enum class IBLTexture : uint32_t
    {
        Irradiance = 0,         ///< Texture represents irradiance data
        Prefiltered,            ///< Texture represents pre-filtered image-based lighting
        Count
    };

    /**
     * @class BoundingBox
     *
     * Represents an axis-aligned bounding box that completely encapsulates the scene.
     *
     * @ingroup CauldronCore
     */
    class BoundingBox
    {
    public:
        /**
         * @brief   Constructor with default behavior.
         */
        BoundingBox() = default;

        /**
         * @brief   Grows the bounding box according to the passed in vector.
         */
        void Grow(Vec4 point);

        /**
         * @brief   Queries if the bounding box is empty.
         */
        bool IsEmpty() const { return m_Empty; }

        /**
         * @brief   Resets the bounding box.
         */
        void Reset();

        /**
         * @brief   Returns the minimum component vector of the bounding box.
         */
        Vec4 GetMin() const { return m_Min; }

        /**
         * @brief   Returns the maximum component vector of the bounding box.
         */
        Vec4 GetMax() const { return m_Max; }

        /**
         * @brief   Calculates and returns the center of the bounding box.
         */
        Vec4 GetCenter() const { return 0.5f * (m_Min + m_Max); };

        /**
         * @brief   Returns the radius of the bounding box.
         */
        Vec4 GetRadius() const { return m_Max - GetCenter(); };

    private:
        bool m_Empty = true;
        Vec4 m_Min;
        Vec4 m_Max;
    };

    /**
     * @class Scene
     *
     * Scene representation for the graphics framework. All loaded entities (whether active or not) are
     * part of the scene and can be queried/interacted with.
     *
     * @ingroup CauldronCore
     */
    class Scene
    {
    public:

        /**
         * @brief   Constructor with default behavior.
         */
        Scene();

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~Scene();

        /**
         * @brief   Sets the currently active camera for the scene.
         */
        void SetCurrentCamera(const Entity* pCameraEntity);

        /**
         * @brief   Gets the currently set active camera for the scene.
         */
        const CameraComponent* GetCurrentCamera() const { return m_pCurrentCamera; }
        CameraComponent* GetCurrentCamera() { return m_pCurrentCamera; }

        /**
         * @brief   Sets the BRDF lookup texture for the scene.
         */
        void SetBRDFLutTexture(const Texture* pTexture) { m_pBRDFTexture = pTexture; }

        /**
         * @brief   Gets the BRDF lookup texture for the scene.
         */
        const Texture* GetBRDFLutTexture() const { return m_pBRDFTexture; }

        /**
         * @brief   Sets the scene's image-based lighting (IBL) texture.
         */
        void SetIBLTexture(const Texture* pTexture, IBLTexture textureType) { m_pIBLTexture[static_cast<uint32_t>(textureType)] = pTexture; }

        /**
         * @brief   Gets the scene's image-based lighting (IBL) texture.
         */
        const Texture* GetIBLTexture(IBLTexture textureType) const { return m_pIBLTexture[static_cast<uint32_t>(textureType)]; }

        /**
         * @brief   Sets the global mip LOD bias to use when rendering geometry.
         */
        void SetMipLODBias(float mipLODBias) { m_SceneInformation.MipLODBias = mipLODBias; }

        /**
         * @brief   Adds all entities in a loaded <c><i>ContentBlock</i></c> to the scene. Also sets
         *          a new active camera if one was provided and updates the scene bounding box to encapsulate
         *          all newly loaded entities.
         */
        void AddContentBlockEntities(const ContentBlock* pContentBlock);

        /**
         * @brief   Removes all entities in a loaded <c><i>ContentBlock</i></c> from the scene. Also resets
         *          camera to default if current active camera is being unloaded and recomputes the scene
         *          bounding box.
         */
        void RemoveContentBlockEntities(const ContentBlock* pContentBlock);

        /**
         * @brief   Initializes the scene and triggers the creation of default scene content (camera, lighting).
         */
        void InitScene();

        /**
         * @brief   Schedules the loading of all default scene content (via <c><i>TaskManager</i></c>).
         */
        void InitSceneContent();

        /**
         * @brief   Updates the scene for the frame. Updates lighting constants and transforms.
         */
        void UpdateScene(double deltaTime);

        /**
         * @brief   Shuts down the scene when the graphics framework shuts down.
         */
        void TerminateScene();

        /**
         * @brief   Returns whether the scene is fully initialized and ready for processing or not.
         */
        bool IsReady() const { return m_SceneReady; }

        /**
         * @brief   Gets the <c><i>SceneInformation</i></c> for the current frame.
         */
        const SceneInformation& GetSceneInfo() const { return m_SceneInformation; }

        /**
         * @brief   Gets the <c><i>SceneLightingInformation</i></c> for the current frame.
         */
        const SceneLightingInformation& GetSceneLightInfo() const { return m_SceneLightInformation; }

        /**
         * @brief   Gets the list of scene entities for traversal.
         */
        const std::vector<const Entity*>& GetSceneEntities() const { return m_SceneEntities; }

        /**
         * @brief   Gets the scene's <c><i>BoundingBox</i></c>.
         */
        const BoundingBox& GetBoundingBox() const { return m_BoundingBox; }

        /**
         * @brief   Gets the <c><i>ASManager</i></c> for the scene.
         */
        ASManager* GetASManager() { return m_ASManager; }

        /**
         * @brief   Sets the scene's <c><i>ScreenSpaceShadowTexture</i></c>.
         */
        void SetScreenSpaceShadowTexture(const Texture* pTexture) { m_pScreenSpaceShadowTexture = pTexture; }

        /**
         * @brief   Gets the scene's <c><i>ScreenSpaceShadowTexture</i></c>.
         */
        const Texture* GetScreenSpaceShadowTexture() const { return m_pScreenSpaceShadowTexture; }

        /**
         * @brief   Set the scene's exposure value.
         */
        void  SetSceneExposure(float value) { m_Exposure = value; }

        /**
         * @brief   Set the scene's IBL factor.
         */
        void  SetIBLFactor(float value) { m_IBLFactor = value; }

        /**
         * @brief   Set the scene's specular IBL factor.
         */
        void  SetSpecularIBLFactor(float value) { m_SpecularIBLFactor = value; }

        /**
         * @brief   Get the scene's exposure value.
         */
        float GetSceneExposure() { return m_Exposure; }

        /**
         * @brief   Get the scene's IBL factor.
         */
        float GetIBLFactor() { return m_IBLFactor; }

        /**
         * @brief   Get the scene's specular IBL factor.
         */
        float GetSpecularIBLFactor() { return m_SpecularIBLFactor; }

        /**
         * @brief   Queries if the scene's bounding box was updated in AddContentBlockEntities (is reset by UpdateScene each frame).
         */
        bool IsBoundingBoxUpdated() { return m_BoundingBoxUpdated; }

        /**
         * @brief   Get the Skydome's procedurally generated light's time of day
         */
        int32_t& GetSkydomeHour() { return m_SkydomeLightHour; }

        /**
         * @brief   Get the Skydome's procedurally generated light's time of day
         */
        int32_t& GetSkydomeMinute() { return m_SkydomeLightMinute; }


    private:
        // No Copy, No Move
        NO_COPY(Scene);
        NO_MOVE(Scene);

        void UpdateSceneBoundingBox(const ContentBlock* pContentBlock);
        void UpdateSceneBoundingBox(const Entity* pEntity);
        void RecomputeSceneBoundingBox();

        std::vector<const Entity*>  m_SceneEntities;

        // Default camera to use as a backup
        Entity*                     m_pDefaultPerspCamera = nullptr;

        // Default light to use as a backup
        Entity*                     m_pDefaultLight = nullptr;

        CameraComponent*      m_pCurrentCamera = nullptr;

        SceneInformation            m_SceneInformation = {};
        SceneLightingInformation    m_SceneLightInformation = {};

        BoundingBox                 m_BoundingBox;

        // IBL Texture
        const Texture* m_pIBLTexture[IBLTexture::Count] = { nullptr };
        // BRDF Texture
        const Texture* m_pBRDFTexture = nullptr;
        // ScreenSpace ShadowTexture
        const Texture* m_pScreenSpaceShadowTexture = nullptr;

        // Scene Exposure
        float m_Exposure = 1.f;
        float m_IBLFactor = 0.55f;
        float m_SpecularIBLFactor = 1.0f;

        // Skydome Light Settings
        int32_t m_SkydomeLightHour      = 12;
        int32_t m_SkydomeLightMinute    = 0;

        // Acceleration Structure Manager
        ASManager* m_ASManager = nullptr;

        std::atomic_bool            m_SceneReady = false;

        bool m_BoundingBoxUpdated = true;
    };

} // namespace cauldron
