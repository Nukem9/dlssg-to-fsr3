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

#include "core/components/cameracomponent.h"
#include "core/entity.h"
#include "core/framework.h"
#include "core/scene.h"
#include "core/inputmanager.h"

#include "misc/assert.h"
#include "misc/math.h"

#include<algorithm>

namespace cauldron
{
    const wchar_t* CameraComponentMgr::s_ComponentName = L"CameraComponent";
    CameraComponentMgr* CameraComponentMgr::s_pComponentManager = nullptr;

    CameraComponentMgr::CameraComponentMgr() :
        ComponentMgr()
    {
    }

    CameraComponentMgr::~CameraComponentMgr()
    {

    }

    CameraComponent* CameraComponentMgr::SpawnCameraComponent(Entity* pOwner, ComponentData* pData)
    {
        // Create the component
        CameraComponent* pComponent = new CameraComponent(pOwner, pData, this);

        // Add it to the owner
        pOwner->AddComponent(pComponent);

        return pComponent;
    }

    void CameraComponentMgr::Initialize()
    {
        CauldronAssert(ASSERT_CRITICAL, !s_pComponentManager, L"CameraComponentMgr instance is non-null. Component managers can ONLY be created through framework registration using RegisterComponentManager<>()");

        // Initialize the convenience accessor to avoid having to do a map::find each time we want the manager
        s_pComponentManager = this;
    }

    void CameraComponentMgr::Shutdown()
    {
        // Clear out the convenience instance pointer
        CauldronAssert(ASSERT_ERROR, s_pComponentManager, L"CameraComponentMgr instance is null. Component managers can ONLY be destroyed through framework shutdown");
        s_pComponentManager = nullptr;
    }

    CameraJitterCallback CameraComponent::s_pSetJitterCallback = nullptr;

    CameraComponent::CameraComponent(Entity* pOwner, ComponentData* pData, CameraComponentMgr* pManager) :
        Component(pOwner, pData, pManager),
        m_pData(reinterpret_cast<CameraComponentData*>(pData)),
        m_ResetMatrix(m_pOwner->GetTransform())
    {
        // Initialize all component data
        switch (m_pData->Type)
        {
        case CameraType::Perspective:
            m_ProjectionMatrix = CalculatePerspectiveMatrix();
            break;
        case CameraType::Orthographic:
            m_ProjectionMatrix = CalculateOrthogonalMatrix();
            break;
        }

        // Initialize arc-ball distance with distance to origin
        m_Distance = length(m_pOwner->GetTransform().getTranslation());

        // Setup core view matrices
        m_ViewMatrix = InverseMatrix(m_pOwner->GetTransform()); // Owner's transform is our camera's matrix
        m_InvViewMatrix = m_pOwner->GetTransform();

        // Update m_ProjJittered according to current jitterValues
        SetProjectionJitteredMatrix();

        // Calculate remaining matrices
        SetViewBasedMatrices();

        // Setup yaw and pitch
        UpdateYawPitch();

        // Update temporal information
        m_PrevViewMatrix = m_ViewMatrix;
        m_PrevViewProjectionMatrix = m_ViewProjectionMatrix;
        m_PrevProjJittered         = m_ProjJittered;
    }

    CameraComponent::~CameraComponent()
    {

    }

    void CameraComponent::ResetCamera()
    {
        // Reset owner's transform
        m_pOwner->SetTransform(m_ResetMatrix);

        // Initialize all component data
        switch (m_pData->Type)
        {
        case CameraType::Perspective:
            m_ProjectionMatrix = CalculatePerspectiveMatrix();
            break;
        case CameraType::Orthographic:
            m_ProjectionMatrix = CalculateOrthogonalMatrix();
            break;
        }

        // Initialize arc-ball distance with distance to origin
        m_Distance = length(m_pOwner->GetTransform().getTranslation());

        // Setup core view matrices
        m_ViewMatrix = InverseMatrix(m_pOwner->GetTransform()); // Owner's transform is our camera's matrix
        m_InvViewMatrix = m_pOwner->GetTransform();

        // Reset ProjJitteredMatrix
        m_jitterValues = Vec2(0, 0);
        SetProjectionJitteredMatrix();

        // Calculate remaining matrices
        SetViewBasedMatrices();

        // Setup yaw and pitch
        UpdateYawPitch();

        // Update temporal information
        m_PrevViewMatrix = m_ViewMatrix;
        m_PrevViewProjectionMatrix = m_ViewProjectionMatrix;
        m_PrevProjJittered         = m_ProjJittered;

        m_Dirty = true;
        m_CameraReset = true;
    }

    void CameraComponent::SetViewBasedMatrices()
    {
        // Calculate all view/invView dependent matrices
        m_ViewProjectionMatrix      = m_ProjJittered * m_ViewMatrix;
        m_InvProjectionMatrix       = InverseMatrix(m_ProjJittered);
        m_InvViewProjectionMatrix   = InverseMatrix(m_ViewProjectionMatrix);
    }

    void CameraComponent::UpdateYawPitch()
    {
        // Setup yaw and pitch
        Vec4 zBasis = m_ViewMatrix.getRow(2);
        m_Yaw = std::atan2f(zBasis.getX(), zBasis.getZ());
        float fLen = std::sqrtf(zBasis.getZ() * zBasis.getZ() + zBasis.getX() * zBasis.getX());
        m_Pitch = std::atan2f(zBasis.getY(), fLen);
    }

    void CameraComponent::LookAt(const Vec4& eyePos, const Vec4& lookAt)
    {
        m_ViewMatrix = LookAtMatrix(eyePos, lookAt, Vec4(0, 1, 0, 0));
        m_InvViewMatrix = inverse(m_ViewMatrix);
        m_pOwner->SetTransform(m_InvViewMatrix);

        // Update our distance
        m_Distance = length(eyePos - lookAt);

        // Update Yaw/Pitch
        UpdateYawPitch();
    }

    Mat4 CameraComponent::CalculatePerspectiveMatrix()
    {
        static bool s_InvertedDepth = GetConfig()->InvertedDepth;

        // Fix up aspect ratio and vertical field of view (which may have changed)
        m_pData->Perspective.AspectRatio = GetFramework()->GetAspectRatio();
        float Xfov = std::min<float>(m_pData->Perspective.Yfov * m_pData->Perspective.AspectRatio, CAULDRON_PI2);
        m_pData->Perspective.Yfov = Xfov / m_pData->Perspective.AspectRatio;

        return Perspective(m_pData->Perspective.Yfov, m_pData->Perspective.AspectRatio, m_pData->Znear, m_pData->Zfar, s_InvertedDepth);
    }

    Mat4 CameraComponent::CalculateOrthogonalMatrix()
    {
        static bool s_InvertedDepth = GetConfig()->InvertedDepth;
        return Orthographic(-0.5f * m_pData->Orthographic.Xmag, 0.5f * m_pData->Orthographic.Xmag, -0.5f * m_pData->Orthographic.Ymag, 0.5f * m_pData->Orthographic.Ymag, m_pData->Znear, m_pData->Zfar, s_InvertedDepth);
    }

    void CameraComponent::SetProjectionJitteredMatrix()
    {
        Mat4 jitterMat(Mat3::identity(), Vec3(m_jitterValues.getX(), m_jitterValues.getY(), 0));
        m_ProjJittered = jitterMat * m_ProjectionMatrix;
    }

    void CameraComponent::OnFocusGained()
    {
        // Right after focus is regained the mouse delta is often very large, so we should skip updating
        // the camera until at least one Update has happened.
        m_SkipUpdate = true;
    }

    void CameraComponent::Update(double deltaTime)
    {
        if (m_SkipUpdate)
        {
            m_SkipUpdate = false;
            return;
        }
        // Always update temporal information
        m_PrevViewMatrix = m_ViewMatrix;
        m_PrevViewProjectionMatrix = m_ViewProjectionMatrix;
        m_PrevProjJittered         = m_ProjJittered;
        
        // Reset camera reset status (in case it was set)
        m_CameraReset              = false;

        // If this camera is the currently active camera for the scene, check for input
        if (GetScene()->GetCurrentCamera() == this)
        {
            // if (animated)
            // {
            //      #pragma message(Reminder "Support camera animations")
            //      Update positional info with animation
            //      Mark camera dirty
            // }
            // else

            // Do camera update (Updates will be made to View matrix - similar to Cauldron 1 - and then pushed up to owner via InvViewMatrix)
            {
                const InputState& inputState = GetInputManager()->GetInputState();

                // Camera mode toggle
                if (inputState.GetMouseButtonUpState(Mouse_RButton) || inputState.GetGamePadButtonUpState(Pad_L3))
                    m_ArcBallMode = !m_ArcBallMode;

                // We will scale camera displacement according to the size of the scene
                const BoundingBox& boundingBox = GetScene()->GetBoundingBox();
                float sceneSize = length(boundingBox.GetMax().getXYZ() - boundingBox.GetMin().getXYZ());
                float displacementIncr = 0.05f * sceneSize;  // Displacements are 5% of scene size by default

                // If we are holding down ctrl, magnify the displacement by 10
                if (inputState.GetKeyState(Key_Ctrl))
                {
                    displacementIncr *= 10.f;
                }

                // If we are holding down shift, magnify the displacement by 0.1
                else if (inputState.GetKeyState(Key_Shift))
                {
                    displacementIncr *= 0.1f;
                }

                // Read in inputs

                // Use right game pad stick to pitch and yaw the camera
                bool hasRotation = false;
                if (inputState.GetGamePadAxisState(Pad_RightThumbX) || inputState.GetGamePadAxisState(Pad_RightThumbY))
                {
                    // All rotations (per frame) are of 0.01 radians
                    m_Yaw -= inputState.GetGamePadAxisState(Pad_RightThumbX) / 100.f;
                    m_Pitch += inputState.GetGamePadAxisState(Pad_RightThumbY) / 100.f;
                    hasRotation = true;
                }

                // Left click + mouse move == free cam look & WASDEQ movement (+ mouse wheel in/out)
                else if (inputState.GetMouseButtonState(Mouse_LButton))
                {
                    // Only rotate a 10th of a degree per frame
                    m_Yaw -= inputState.GetMouseAxisDelta(Mouse_XAxis) / 100.f;
                    m_Pitch += inputState.GetMouseAxisDelta(Mouse_YAxis) / 100.f;
                    hasRotation = true;
                }
                
                // If hitting the 'r' key or back button on game pad, reset camera to original transform
                if (inputState.GetKeyState(Key_R) || inputState.GetGamePadButtonState(Pad_Back))
                {
                    ResetCamera();
                    UpdateMatrices();
                    return;
                }

                Vec4 eyePos = Vec4(m_InvViewMatrix.getTranslation(), 0.f);
                Vec4 polarVector = PolarToVector(m_Yaw, m_Pitch);
                Vec4 lookAt = eyePos - polarVector;
                // If we are in arc-ball mode, do arc-ball based camera updates
                if (m_ArcBallMode && (hasRotation || inputState.GetMouseAxisDelta(Mouse_Wheel)))
                {
                    // Modify Pitch/Yaw according to mouse input (prevent from hitting the max/min in pitch by 1 degree to prevent stuttering)
                    m_Pitch = std::max(-CAULDRON_PI2 + DEG_TO_RAD(1), std::min(m_Pitch, CAULDRON_PI2 - DEG_TO_RAD(1)));

                    // Mouse wheel
                    float wheel = inputState.GetMouseAxisDelta(Mouse_Wheel) * displacementIncr / 3.f;
                    float distanceMod = m_Distance - wheel;
                    distanceMod = std::max(distanceMod, 0.01f);

                    // Update everything
                    Vec4 dir = m_InvViewMatrix.getCol2();
                    Vec4 polarVector = PolarToVector(m_Yaw, m_Pitch);
                    lookAt = eyePos - (dir * m_Distance);

                    eyePos = lookAt + (polarVector * distanceMod);
                    m_Dirty = true;
                }

                // Otherwise, we are either translating or free rotating (or both)
                else
                {
                    // WASDQE == camera translation
                    float x(0.f), y(0.f), z(0.f);
                    x -= (inputState.GetKeyState(Key_A)) ? displacementIncr : 0.f;
                    x += (inputState.GetKeyState(Key_D)) ? displacementIncr : 0.f;
                    y -= (inputState.GetKeyState(Key_Q)) ? displacementIncr : 0.f;
                    y += (inputState.GetKeyState(Key_E)) ? displacementIncr : 0.f;
                    z -= (inputState.GetKeyState(Key_W)) ? displacementIncr : 0.f;
                    z += (inputState.GetKeyState(Key_S)) ? displacementIncr : 0.f;

                    // Controller input can also translate
                    x += inputState.GetGamePadAxisState(Pad_LeftThumbX) * displacementIncr;
                    z -= inputState.GetGamePadAxisState(Pad_LeftThumbY) * displacementIncr;
                    y -= inputState.GetGamePadAxisState(Pad_LTrigger) * displacementIncr;
                    y += inputState.GetGamePadAxisState(Pad_RTrigger) * displacementIncr;
                    Vec4 movement = Vec4(x, y, z, 0.f);

                    Mat4& transform = m_pOwner->GetTransform();

                    // Update from inputs
                    if (hasRotation || dot(movement.getXYZ(), movement.getXYZ()))
                    {
                        // Setup new eye position
                        eyePos = m_InvViewMatrix.getCol3() + (m_InvViewMatrix * movement * static_cast<float>(deltaTime));  // InvViewMatrix is the owner's transform

                        // Update everything
                        lookAt = eyePos - polarVector;
                        m_Dirty = true;
                    }
                }

                // Update camera jitter if we need it
                if (CameraComponent::s_pSetJitterCallback)
                {
                    s_pSetJitterCallback(m_jitterValues);
                    m_Dirty = true;
                }
                else
                {
                    // Reset jitter if disabled
                    if (m_jitterValues.getX() != 0.f || m_jitterValues.getY() != 0.f)
                    {
                        m_jitterValues = Vec2(0.f, 0.f);
                        m_Dirty        = true;
                    }
                }

                if (m_Dirty)
                {
                    LookAt(eyePos, lookAt);
                    UpdateMatrices();         
                }
            }
        }
    }

    void CameraComponent::UpdateMatrices()
    {
        // Check if we need to update our projection
        if (m_pData->Type == CameraType::Perspective && GetFramework()->GetAspectRatio() != m_pData->Perspective.AspectRatio)
            m_ProjectionMatrix = CalculatePerspectiveMatrix();

        // Initialize arc-ball distance with distance to origin
        m_Distance = length(m_pOwner->GetTransform().getTranslation());

        // Update m_ProjJittered according to current jitterValues
        SetProjectionJitteredMatrix();

        // View and InvView are setup during input handling, so just calculate remaining matrices
        SetViewBasedMatrices();

        // No longer dirty
        m_Dirty = false;
     }

} // namespace cauldron
