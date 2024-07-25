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
#include <functional>

namespace cauldron
{
    class CameraComponent;

    /**
     * @class CameraComponentMgr
     *
     * Component manager class for <c><i>CameraComponent</i></c>s.
     *
     * @ingroup CauldronComponent
     */
    class CameraComponentMgr : public ComponentMgr
    {
    public:
        static const wchar_t* s_ComponentName;      ///< Component name

    public:
        /**
         * @brief   Constructor with default behavior.
         */
        CameraComponentMgr();

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~CameraComponentMgr();

        /**
         * @brief   Component creator.
         */
        virtual Component* SpawnComponent(Entity* pOwner, ComponentData* pData) override { return reinterpret_cast<Component*>(SpawnCameraComponent(pOwner, pData)); }

        /**
         * @brief   Allocates a new <c><i>CameraComponent</i></c> for the given entity.
         */
        CameraComponent* SpawnCameraComponent(Entity* pOwner, ComponentData* pData);

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
        static CameraComponentMgr* Get() { return s_pComponentManager; }

    private:
        static CameraComponentMgr* s_pComponentManager;
    };

    /**
     * @enum CameraType
     *
     * Describes the type of camera.
     *
     * @ingroup CauldronComponent
     */
    enum class CameraType
    {
        Perspective,        ///< A perspective projection camera
        Orthographic        ///< An orthographic projection camera
    };

    /**
     * @struct CameraComponentData
     *
     * Initialization data structure for the <c><i>CameraComponent</i></c>.
     *
     * @ingroup CauldronComponent
     */
    struct CameraComponentData : public ComponentData
    {
        CameraType    Type       = CameraType::Perspective; ///< <c><i>CameraType</i></c>. Either perspective or orthographic.
        float         Znear      = 0.1f;                    ///< Camera near Z
        float         Zfar       = 100.0f;                  ///< Camera far Z

        union
        {
            struct {
                float Yfov;                                 ///< Vertical field-of-view
                float AspectRatio;                          ///< Aspect ratio
            } Perspective;
            struct {
                float Xmag;                                 ///< Magnitude in X axis
                float Ymag;                                 ///< Magnitude in Y axis
            } Orthographic;
        };
        std::wstring  Name       = L"";                     ///< Component name
    };

    typedef std::function<void(Vec2& values)> CameraJitterCallback;

    /**
     * @class CameraComponent
     *
     * Camera component class. Implements camera functionality on an entity.
     *
     * @ingroup CauldronComponent
     */
    class CameraComponent : public Component
    {
    public:

        /**
         * @brief   Constructor.
         */
        CameraComponent(Entity* pOwner, ComponentData* pData, CameraComponentMgr* pManager);

        /**
         * @brief   Destructor.
         */
        virtual ~CameraComponent();

        /**
         * @brief   Component update. Update the camera if dirty. Processes input, updates all matrices.
         */
        virtual void Update(double deltaTime) override;

        /**
         * @brief   Component data accessor.
         */
        CameraComponentData& GetData() { return *m_pData; }
        const CameraComponentData& GetData() const { return *m_pData; }

        /**
         * @brief   Marks the camera dirty.
         */
        void SetDirty() { m_Dirty = true; }

        /**
         * @brief   Gets the camera's translation matrix.
         */
        const Vec4& GetCameraTranslation() const { return m_pOwner->GetTransform().getCol3(); }

        /**
         * @brief   Gets the camera's position.
         */
        const Vec3  GetCameraPos() const { return m_pOwner->GetTransform().getTranslation(); }

        /**
         * @brief   Gets the camera's direction.
         */
        const Vec4 GetDirection() const { return m_InvViewMatrix.getCol2(); }

        /**
         * @brief   Gets the camera's view matrix.
         */
        const Mat4& GetView() const { return m_ViewMatrix; }

        /**
         * @brief   Gets the camera's projection matrix.
         */
        const Mat4& GetProjection() const { return m_ProjectionMatrix; }

        /**
         * @brief   Gets the camera's view projection matrix.
         */
        const Mat4& GetViewProjection() const { return m_ViewProjectionMatrix; }

        /**
         * @brief   Gets the camera's inverse view matrix.
         */
        const Mat4& GetInverseView() const { return m_InvViewMatrix; }

        /**
         * @brief   Gets the camera's inverse projection matrix.
         */
        const Mat4& GetInverseProjection() const { return m_InvProjectionMatrix; }

        /**
         * @brief   Gets the camera's inverse view projection matrix.
         */
        const Mat4& GetInverseViewProjection() const { return m_InvViewProjectionMatrix; }

        /**
         * @brief   Gets the camera's previous view matrix.
         */
        const Mat4& GetPreviousView() const { return m_PrevViewMatrix; }

        /**
         * @brief   Gets the camera's previous view projection matrix.
         */
        const Mat4& GetPreviousViewProjection() const { return m_PrevViewProjectionMatrix; }

        /**
         * @brief   Gets the camera's jittered projection matrix.
         */
        const Mat4& GetProjectionJittered() const { return m_ProjJittered; }

        /**
         * @brief   Gets the camera's previous jittered projection matrix.
         */
        const Mat4& GetPrevProjectionJittered() const { return m_PrevProjJittered; }

        /**
         * @brief   Gets the camera's near plane value.
         */
        const float GetNearPlane() const { return m_pData->Znear; }

        /**
         * @brief   Gets the camera's far plane value.
         */
        const float GetFarPlane() const { return m_pData->Zfar; }

        /**
         * @brief   Gets the camera's horizontal field of view.
         */
        const float GetFovX() const { return std::min<float>(m_pData->Perspective.Yfov * m_pData->Perspective.AspectRatio, CAULDRON_PI2); }

        /**
         * @brief   Gets the camera's vertical field of view.
         */
        const float GetFovY() const { return m_pData->Perspective.Yfov; }

        /**
         * @brief   Sets the camera's jitter update callback to use.
         */
        static void SetJitterCallbackFunc(CameraJitterCallback callbackFunc) { s_pSetJitterCallback = callbackFunc; }

        /**
         * @brief   Let's the caller know if this camera was reset this frame
         */
        bool WasCameraReset() const { return m_CameraReset; }

    private:
        CameraComponent() = delete;

        void ResetCamera();
        void SetViewBasedMatrices();
        void UpdateMatrices();

        void UpdateYawPitch();
        void LookAt(const Vec4& eyePos, const Vec4& lookAt);

        Mat4 CalculatePerspectiveMatrix();
        Mat4 CalculateOrthogonalMatrix();

        void SetProjectionJitteredMatrix();

        void OnFocusGained() override;

    protected:
        // After regaining focus, skip next update because the mouse delta will be too large.
        bool m_SkipUpdate = false;

        // Keep a pointer on our initialization data for matrix reconstruction
        CameraComponentData*    m_pData;

        const Mat4  m_ResetMatrix = Mat4::identity(); // Used to reset camera to initial state
        float       m_Distance = 1.f;                 // Distance to look at
        float       m_Yaw = 0.f;                      // Current camera Yaw (in Radians)
        float       m_Pitch = 0.f;                    // Current camera Pitch (in Radians)

        // Core matrix information
        Mat4   m_ViewMatrix = Mat4::identity();
        Mat4   m_ProjectionMatrix = Mat4::identity();
        Mat4   m_ViewProjectionMatrix = Mat4::identity();

        // Inverses
        Mat4   m_InvViewMatrix = Mat4::identity();
        Mat4   m_InvProjectionMatrix = Mat4::identity();
        Mat4   m_InvViewProjectionMatrix = Mat4::identity();

        // Temporal matrices
        Mat4   m_PrevViewMatrix = Mat4::identity();
        Mat4   m_PrevViewProjectionMatrix = Mat4::identity();

        bool            m_Dirty = true;         // Whether or not we need to recalculate everything
        bool            m_ArcBallMode = true;   // Use arc-ball rotation or WASD free cam
        bool            m_CameraReset = false;  // Used to track if the camera was reset throughout the frame

        // Jitter
        Vec2 m_jitterValues     = Vec2(0, 0);
        Mat4 m_ProjJittered     = Mat4::identity();
        Mat4 m_PrevProjJittered = Mat4::identity();
        static CameraJitterCallback s_pSetJitterCallback;
    };

} // namespace cauldron
