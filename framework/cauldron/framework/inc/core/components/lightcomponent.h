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

#include "core/component.h"
#include "misc/math.h"
#include "render/shadowmapresourcepool.h"
#include "core/scene.h"

namespace cauldron
{
    class LightComponent;
    class BoundingBox;

    /**
     * @class LightComponentMgr
     *
     * Component manager class for <c><i>LightComponent</i></c>s.
     *
     * @ingroup CauldronComponent
     */

    class LightComponentMgr : public ComponentMgr
    {
    public:
        static const wchar_t* s_ComponentName;          ///< Component name

    public:

        /**
         * @brief   Constructor with default behavior.
         */
        LightComponentMgr();

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~LightComponentMgr();

        /**
         * @brief   Component creator.
         */
        virtual Component* SpawnComponent(Entity* pOwner, ComponentData* pData) override { return reinterpret_cast<Component*>(SpawnLightComponent(pOwner, pData)); }

        /**
         * @brief   Allocates a new <c><i>LightComponent</i></c> for the given entity.
         */
        LightComponent* SpawnLightComponent(Entity* pOwner, ComponentData* pData);

        /**
         * @brief   Gets the component type string ID.
         */
        virtual const wchar_t* ComponentType() const override { return s_ComponentName; }

        /**
         * @brief   Initializes the component manager.
         */
        virtual void Initialize() override;

        /**
         * @brief   Shuts down the component manager.
         */
        virtual void Shutdown() override;

        /**
         * @brief   Component manager instance accessor.
         */
        static LightComponentMgr* Get() { return s_pComponentManager; }

    private:
        static LightComponentMgr* s_pComponentManager;
    };

    /**
     * @enum LightType
     *
     * Supported light types by the <c><i>LightComponent</i></c>.
     *
     * @ingroup CauldronComponent
     */
    enum class LightType
    {
        Directional,    ///< Directional light type (parallel orthographic source)
        Spot,           ///< Spot light type (directed cone source)
        Point           ///< Point light type (radial source)
    };

    /**
     * @struct LightComponentData
     *
     * Initialization data structure for the <c><i>LightComponent</i></c>.
     *
     * @ingroup CauldronComponent
     */
    struct LightComponentData : public ComponentData
    {
        LightType            Type                    = LightType::Directional;  ///< Type of light this component represents
        Vec3                 Color                   = Vec3(1.0f, 1.0f, 1.0f);  ///< Light color
        float                SpotInnerConeAngle      = 0.0f;                    ///< Inner cone angle for spotlight representation
        float                SpotOuterConeAngle      = 1.0f;                    ///< Outer cone angle for spotlight representation
        float                Intensity               = 1.0f;                    ///< Light intensity
        float                Range                   = -1.f;                    ///< Light range (-1 indicates infinite point source limited by intensity)
        float                DepthBias               = 0.005f;                     ///< Depth bias to apply to light shadow maps
        int32_t              ShadowResolution        = 1024;                    ///< Light shadow map resolution
        std::vector<int>     ShadowMapIndex          = {-1};                    ///< Light shadow map index from the shadow pool
        std::vector<int32_t> ShadowMapCellIndex      = {-1};                    ///< Light shadow map cell index from the shadow pool
        std::vector<Rect>    ShadowMapRect           = {Rect()};                ///< Light shadow map rect from the shadow pool
        std::wstring         Name                    = L"";                     ///< Light name
    };

    /**
     * @class LightComponent
     *
     * Light component class. Implements lighting functionality for a given entity.
     *
     * @ingroup CauldronComponent
     */
    class LightComponent : public Component
    {
    public:

        /**
         * @brief   Constructor.
         */
        LightComponent(Entity* pOwner, ComponentData* pData, LightComponentMgr* pManager);

        /**
         * @brief   Destructor.
         */
        virtual ~LightComponent();

        /**
         * @brief   Component update. Updates only execute when the light is marked as dirty. This occurs if the
         *          light moves, or has any of it's core parameters changed.
         */
        virtual void Update(double deltaTime) override;

        /**
         * @brief   Component data accessor.
         */
        LightComponentData& GetData() { return *m_pData; }
        const LightComponentData& GetData() const { return *m_pData; }

        /**
         * @brief   Gets the light type.
         */
        LightType GetType() const { return m_pData->Type; }

        /**
         * @brief   Gets the light color.
         */
        Vec3 GetColor() const { return m_pData->Color; }

        /**
         * @brief   Gets the light intensity.
         */
        float GetIntensity() const { return m_pData->Intensity; }

        /**
         * @brief   Gets the light range.
         */
        float GetRange() const { return m_pData->Range; }

        /**
         * @brief   Gets the light depth bias.
         */
        float GetDepthBias() const { return m_pData->DepthBias; }

        /**
         * @brief   Gets the light inner cone angle.
         */
        float GetInnerAngle() const { return m_pData->SpotInnerConeAngle; }

        /**
         * @brief   Gets the light outer cone angle.
         */
        float GetOuterAngle() const { return m_pData->SpotOuterConeAngle; }

        /**
         * @brief   Gets the light shadow map resolution.
         */
        int32_t GetShadowResolution() const { return m_pData->ShadowResolution; }

        /**
         * @brief   Gets the light shadow map index.
         */
        int   GetShadowMapIndex(int index = 0) const { return m_pData->ShadowMapIndex[index]; }

        /**
         * @brief   Gets the light shadow map cell index.
         */
        int32_t GetShadowMapCellIndex(int index = 0) const { return m_pData->ShadowMapCellIndex[index]; }

        /**
         * @brief   Gets the light shadow map rect.
         */
        Rect  GetShadowMapRect(int index = 0) const { return m_pData->ShadowMapRect[index]; }

        /**
         * @brief   Gets the light projection matrix for the shadow.
         */
        const Mat4& GetShadowProjection(int index = 0) const { return m_ShadowProjectionMatrix[index]; }

        /**
         * @brief   Gets the light view projection matrix for the shadow.
         */
        const Mat4& GetShadowViewProjection(int index = 0) const { return m_ShadowViewProjectionMatrix[index]; }

        /**
         * @brief   Gets the light cascade count.
         */
        int GetCascadesCount() const { return m_NumCascades; }

        /**
         * @brief   Gets the light shadow map count. This 1 for anything besides directional lights. The count for
         *          directional lights is dependent on the number of cascades configured.
         */
        int GetShadowMapCount() const { return m_NumCascades == 0 ? 1 : m_NumCascades; }

        /**
         * @brief   Gets the light direction.
         */
        Vec3 GetDirection() const { return m_InvViewMatrix.getCol2().getXYZ(); }

        /**
         * @brief   Sets the light as dirty.
         */
        void SetDirty() { m_Dirty = true; }

        /**
         * @brief   Gets the light view matrix.
         */
        const Mat4& GetView() const { return m_ViewMatrix; }

        /**
         * @brief   Gets the light projection matrix.
         */
        const Mat4& GetProjection() const { return m_ProjectionMatrix; }

        /**
         * @brief   Gets the light view projection matrix.
         */
        const Mat4& GetViewProjection() const { return m_ViewProjectionMatrix; }

        /**
         * @brief   Gets the light inverse view matrix.
         */
        const Mat4& GetInverseView() const { return m_InvViewMatrix; }

        /**
         * @brief   Gets the light inverse projection matrix.
         */
        const Mat4& GetInverseProjection() const { return m_InvProjectionMatrix; }

        /**
         * @brief   Gets the light inverse view projection matrix.
         */
        const Mat4& GetInverseViewProjection() const { return m_InvViewProjectionMatrix; }

        /**
         * @brief   Sets up the light's shadow map cascades.
         */
        void SetupCascades(int numCascades, const std::vector<float>& cascadeSplitPoints, bool moveLightTexelSize);

    private:
        LightComponent() = delete;

        void CalculateCascadeShadowProjection(const Mat4&        cameraProjectionMatrix,
                                        const Mat4&        cameraViewMatrix,
                                        const Mat4&        lightViewMatrix,
                                        float              camNear,
                                        const cauldron::BoundingBox& sceneBoundingBox,
                                        int                numCascades,
                                        const float*       cascadeSplitPoints,
                                        float              width,
                                        bool               moveLightTexelSize);

    protected:

        LightComponentData* m_pData;
        bool                m_Dirty = true;    // Whether or not we need to recalculate everything

        // Core matrix information
        Mat4       m_ViewMatrix = Mat4::identity();
        Mat4       m_ProjectionMatrix = Mat4::identity();
        Mat4       m_ViewProjectionMatrix = Mat4::identity();

        // Inverses
        Mat4       m_InvViewMatrix = Mat4::identity();
        Mat4       m_InvProjectionMatrix = Mat4::identity();
        Mat4       m_InvViewProjectionMatrix = Mat4::identity();

        // CSM information
        int                m_NumCascades                     = 0;
        std::vector<float> m_CascadeSplitPoints              = {};
        bool               m_MoveLightTexelSize              = true;
        std::vector<Mat4>  m_ShadowViewProjectionMatrix      = {};
        std::vector<Mat4>  m_ShadowProjectionMatrix          = {};
        bool               m_CascadeDirty                    = true;
    };

} // namespace cauldron
