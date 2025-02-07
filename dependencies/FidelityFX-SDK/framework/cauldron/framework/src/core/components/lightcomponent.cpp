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

#include "core/components/lightcomponent.h"
#include "core/components/cameracomponent.h"
#include "core/entity.h"
#include "core/framework.h"
#include "core/scene.h"
#include "misc/assert.h"
#include "misc/math.h"

#include <algorithm>

namespace cauldron
{
#if !defined(_M_ARM64)
    // __m128 Matrix helper
    bool fneq128_b(__m128 const& a, __m128 const& b, float epsilon = 1.e-8f)
    {
        // epsilon vector
        auto eps = _mm_set1_ps(epsilon);
        // absolute of difference of a and b
        auto abd = _mm_andnot_ps(_mm_set1_ps(-0.0f), _mm_sub_ps(a, b));
        // compare abd to eps
        // returns true if one of the elements in abd is not less than
        // epsilon
        return _mm_movemask_ps(_mm_cmplt_ps(abd, eps)) != 0xF;
    }
#endif

    bool IsEqual(const Mat4& mat0, const Mat4& mat1)
    {
        for (int i = 0; i < 4; ++i)
        {
#if defined(_M_ARM64)
            if (mat0[i] != mat1[i])
#else
            if (fneq128_b(mat0.getCol(i).get128(), mat1.getCol(i).get128()))
#endif
                return false;
        }
        return true;
    }

    // CSM helper
    void CreateFrustumPointsFromCascadeInterval(float camNear, float cascadeIntervalBegin, float cascadeIntervalEnd, const Mat4& projectionMatrix, Vec4* cornerPointsWorld);

    const wchar_t* LightComponentMgr::s_ComponentName = L"LightComponent";
    LightComponentMgr* LightComponentMgr::s_pComponentManager = nullptr;

    LightComponentMgr::LightComponentMgr() :
        ComponentMgr()
    {
    }

    LightComponentMgr::~LightComponentMgr()
    {
    }

    LightComponent* LightComponentMgr::SpawnLightComponent(Entity* pOwner, ComponentData* pData)
    {
        // Validate shadow resolution in light data
        static std::vector<int32_t> s_ValidShadowResolutions = { 256, 512, 1024, 2048 };
        int32_t shadowRes = static_cast<LightComponentData*>(pData)->ShadowResolution;
        auto valueIt = std::find(s_ValidShadowResolutions.begin(), s_ValidShadowResolutions.end(), shadowRes);
        if (valueIt == s_ValidShadowResolutions.end())
        {
            CauldronWarning(L"Unsupported shadow map resolution of %d requested on light \"%ls\". Resolution will be resized to largest valid size without exceeding original value or 2048.", shadowRes, pOwner->GetName());
            valueIt = std::find_if(s_ValidShadowResolutions.begin(), s_ValidShadowResolutions.end(), [&shadowRes](int32_t& iterValue){ return (shadowRes - iterValue) < 0; }); // Get the first index that is bigger than the requested resolution
            static_cast<LightComponentData*>(pData)->ShadowResolution = (valueIt == s_ValidShadowResolutions.end()) ? s_ValidShadowResolutions.back() :  *valueIt;
        }

        // Create the component
        LightComponent* pComponent = new LightComponent(pOwner, pData, this);

        // Add it to the owner
        pOwner->AddComponent(pComponent);

        return pComponent;
    }

    void LightComponentMgr::Initialize()
    {
        CauldronAssert(ASSERT_CRITICAL, !s_pComponentManager, L"LightComponentMgr instance is non-null. Component managers can ONLY be created through framework registration using RegisterComponentManager<>()");

        // Initialize the convenience accessor to avoid having to do a map::find each time we want the manager
        s_pComponentManager = this;
    }

    void LightComponentMgr::Shutdown()
    {
        // Clear out the convenience instance pointer
        CauldronAssert(ASSERT_ERROR, s_pComponentManager, L"LightComponentMgr instance is null. Component managers can ONLY be destroyed through framework shutdown");
        s_pComponentManager = nullptr;
    }

    LightComponent::LightComponent(Entity* pOwner, ComponentData* pData, LightComponentMgr* pManager) :
        Component(pOwner, pData, pManager),
        m_pData(reinterpret_cast<LightComponentData*>(pData))
    {
        static bool s_InvertedDepth = GetConfig()->InvertedDepth;
        switch (m_pData->Type)
        {
        case LightType::Directional:
            // Set a default ortho matrix for now, first update will test the scene bounding volume to set appropriately
            m_ProjectionMatrix = Orthographic(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 1000.0f, s_InvertedDepth);
            break;
        case LightType::Spot:
            m_ProjectionMatrix = Perspective(m_pData->SpotOuterConeAngle * 2.0f, 1, .1f, m_pData->Range, s_InvertedDepth);
            break;
        case LightType::Point:
        default:
            // Points don't require a projection matrix (unless we start supporting point-shadows)
            m_ProjectionMatrix = Mat4::identity();
            break;
        }

        // Owner's transform is our camera's matrix
        m_ViewMatrix = InverseMatrix(m_pOwner->GetTransform());
        m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;

        // Inverses
        m_InvViewMatrix = m_pOwner->GetTransform();
        m_InvProjectionMatrix = InverseMatrix(m_ProjectionMatrix);
        m_InvViewProjectionMatrix = InverseMatrix(m_ViewProjectionMatrix);
    }

    LightComponent::~LightComponent()
    {

    }

    void LightComponent::Update(double deltaTime)
    {
        if (m_pOwner->IsActive())
        {
            // Do light updates (todo - animate lights if needed)
        }

        if (m_Dirty)
        {
            static bool s_InvertedDepth = GetConfig()->InvertedDepth;

            // upadte view matrices
            m_ViewMatrix = InverseMatrix(m_pOwner->GetTransform());

            // Check if we need to update our projection
            if (m_pData->Type == LightType::Directional)
            {
                // Scene stores the min/max bounding information for everything in the scene so we can pull
                // from it to update directionals with a proper matrix. Here we'll want to query the bounds and check if
                // they are different from the ones we previously used

                // Update ortho projection if needed
                BoundingBox sceneBB = GetScene()->GetBoundingBox();

                if (sceneBB.IsEmpty())
                {
                    m_ProjectionMatrix = Orthographic(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 1000.0f, s_InvertedDepth);
                }
                else
                {
                    Vec4 center = sceneBB.GetCenter();
                    Vec4 radius = sceneBB.GetRadius();

                    BoundingBox bb;
                    bb.Grow(m_ViewMatrix * (center + Vec4(-radius.getX(), -radius.getY(), -radius.getZ(), 0.0f)));
                    bb.Grow(m_ViewMatrix * (center + Vec4(-radius.getX(), -radius.getY(),  radius.getZ(), 0.0f)));
                    bb.Grow(m_ViewMatrix * (center + Vec4(-radius.getX(),  radius.getY(), -radius.getZ(), 0.0f)));
                    bb.Grow(m_ViewMatrix * (center + Vec4(-radius.getX(),  radius.getY(),  radius.getZ(), 0.0f)));
                    bb.Grow(m_ViewMatrix * (center + Vec4( radius.getX(), -radius.getY(), -radius.getZ(), 0.0f)));
                    bb.Grow(m_ViewMatrix * (center + Vec4( radius.getX(), -radius.getY(),  radius.getZ(), 0.0f)));
                    bb.Grow(m_ViewMatrix * (center + Vec4( radius.getX(),  radius.getY(), -radius.getZ(), 0.0f)));
                    bb.Grow(m_ViewMatrix * (center + Vec4( radius.getX(),  radius.getY(),  radius.getZ(), 0.0f)));

                    // we want a square
                    Vec4 minBB = bb.GetMin();
                    Vec4 maxBB = bb.GetMax();
                    minBB /= minBB.getW();
                    maxBB /= maxBB.getW();

                    float rangeX = maxBB.getX() - minBB.getX();
                    float rangeY = maxBB.getY() - minBB.getY();

                    if (rangeX > rangeY)
                    {
                        float centerY = 0.5f * (maxBB.getY() + minBB.getY());
                        minBB.setY(centerY - 0.5f * rangeX);
                        maxBB.setY(centerY + 0.5f * rangeX);
                    }
                    else
                    {
                        float centerX = 0.5f * (maxBB.getX() + minBB.getX());
                        minBB.setX(centerX - 0.5f * rangeY);
                        maxBB.setX(centerX + 0.5f * rangeY);
                    }

                    m_ProjectionMatrix = Orthographic(minBB.getX(), maxBB.getX(), minBB.getY(), maxBB.getY(), -maxBB.getZ(), -minBB.getZ(), s_InvertedDepth);
                }
            }

            // Regular transforms
            m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;

            // Inverses
            m_InvViewMatrix = m_pOwner->GetTransform();
            m_InvProjectionMatrix = InverseMatrix(m_ProjectionMatrix);
            m_InvViewProjectionMatrix = InverseMatrix(m_ViewProjectionMatrix); // m_InvViewMatrix* m_InvProjectionMatrix;

            m_CascadeDirty = true;

            // No longer dirty
            m_Dirty = false;
        }

        const CameraComponent* pCamera = GetScene()->GetCurrentCamera();
        m_CascadeDirty = m_CascadeDirty || !IsEqual(pCamera->GetViewProjection(), pCamera->GetPreviousViewProjection()) || GetScene()->IsBoundingBoxUpdated();
        if (m_CascadeDirty)
        {
            if (m_pData->Type == LightType::Directional)
            {
                CalculateCascadeShadowProjection(pCamera->GetProjection(),
                                           pCamera->GetView(),
                                           m_ViewMatrix,
                                           pCamera->GetNearPlane(),
                                           GetScene()->GetBoundingBox(),
                                           m_NumCascades,
                                           m_CascadeSplitPoints.data(),
                                           (float)GetShadowMapRect().Right - GetShadowMapRect().Left,
                                           m_MoveLightTexelSize);

            }
            m_CascadeDirty = false;
        }
    }

    void LightComponent::SetupCascades(int numCascades, const std::vector<float>& cascadeSplitPoints, bool moveLightTexelSize)
    {
        if (m_NumCascades != numCascades)
        {
            m_pData->ShadowMapIndex     = std::vector<int>(numCascades > 0 ? numCascades : 1, -1);
            m_pData->ShadowMapCellIndex = std::vector<int32_t>(numCascades > 0 ? numCascades : 1, -1);
            m_pData->ShadowMapRect      = std::vector<Rect>(numCascades > 0 ? numCascades : 1, Rect());

            m_ShadowProjectionMatrix.clear();
            m_ShadowProjectionMatrix.resize(numCascades);

            m_ShadowViewProjectionMatrix.clear();
            m_ShadowViewProjectionMatrix.resize(numCascades);
        }

        m_NumCascades        = numCascades;
        m_CascadeSplitPoints = cascadeSplitPoints;
        m_MoveLightTexelSize = moveLightTexelSize;

        m_CascadeDirty = true;
    }

    void CreateFrustumPointsFromCascadeInterval(float camNear, float cascadeIntervalBegin, float cascadeIntervalEnd, const Mat4& projectionMatrix, Vec4* cornerPointsWorld)
    {
        Mat4 inverseProjectionMatrix = InverseMatrix(projectionMatrix);

        Vec4 topLeft     = {-1.0f, 1.0f, 0.0f, 1.0f};
        Vec4 topRight    = {1.0f, 1.0f, 0.0f, 1.0f};
        Vec4 bottomLeft  = {-1.0f, -1.0f, 0.0f, 1.0f};
        Vec4 bottomRight = {1.0f, -1.0f, 0.0f, 1.0f};

        static bool s_InvertedDepth = GetConfig()->InvertedDepth;
        if (s_InvertedDepth)
        {
            topLeft     = {-1.0f, 1.0f, 1.0f, 1.0f};
            topRight    = {1.0f, 1.0f, 1.0f, 1.0f};
            bottomLeft  = {-1.0f, -1.0f, 1.0f, 1.0f};
            bottomRight = {1.0f, -1.0f, 1.0f, 1.0f};
        }

        Vec4 camNearTopLeftView = inverseProjectionMatrix * topLeft;
        camNearTopLeftView /= camNearTopLeftView.getW();

        Vec4 camNearTopRightView = inverseProjectionMatrix * topRight;
        camNearTopRightView /= camNearTopRightView.getW();

        Vec4 camNearBottomLeftView = inverseProjectionMatrix * bottomLeft;
        camNearBottomLeftView /= camNearBottomLeftView.getW();

        Vec4 camNearBottomRightView = inverseProjectionMatrix * bottomRight;
        camNearBottomRightView /= camNearBottomRightView.getW();

        float beginIntervalScale = cascadeIntervalBegin / camNear;
        float endIntervalScale   = cascadeIntervalEnd / camNear;

        cornerPointsWorld[0] = camNearTopLeftView * beginIntervalScale;
        cornerPointsWorld[0].setW(1.0f);

        cornerPointsWorld[1] = camNearTopRightView * beginIntervalScale;
        cornerPointsWorld[1].setW(1.0f);

        cornerPointsWorld[2] = camNearBottomLeftView * beginIntervalScale;
        cornerPointsWorld[2].setW(1.0f);

        cornerPointsWorld[3] = camNearBottomRightView * beginIntervalScale;
        cornerPointsWorld[3].setW(1.0f);

        cornerPointsWorld[4] = camNearTopLeftView * endIntervalScale;
        cornerPointsWorld[4].setW(1.0f);

        cornerPointsWorld[5] = camNearTopRightView * endIntervalScale;
        cornerPointsWorld[5].setW(1.0f);

        cornerPointsWorld[6] = camNearBottomLeftView * endIntervalScale;
        cornerPointsWorld[6].setW(1.0f);

        cornerPointsWorld[7] = camNearBottomRightView * endIntervalScale;
        cornerPointsWorld[7].setW(1.0f);
    }

    void LightComponent::CalculateCascadeShadowProjection(const Mat4& cameraProjectionMatrix, const Mat4& cameraViewMatrix, const Mat4& lightViewMatrix, float camNear, const BoundingBox& sceneBoundingBox,
        int numCascades, const float* cascadeSplitPoints, float width, bool moveLightTexelSize)
    {
        m_ShadowViewProjectionMatrix.resize(numCascades);
        m_ShadowProjectionMatrix.resize(numCascades);

        Mat4 cameraInverseViewMatrix = math::affineInverse(cameraViewMatrix);

        Vec4 boxbounds[8];
        boxbounds[0] = Vec4(-1, -1, 1, 0);
        boxbounds[1] = Vec4(1, -1, 1, 0);
        boxbounds[2] = Vec4(1, 1, 1, 0);
        boxbounds[3] = Vec4(-1, 1, 1, 0);
        boxbounds[4] = Vec4(-1, -1, -1, 0);
        boxbounds[5] = Vec4(1, -1, -1, 0);
        boxbounds[6] = Vec4(1, 1, -1, 0);
        boxbounds[7] = Vec4(-1, 1, -1, 0);

        // Find scene min/max
        Vec4 sceneAABBMin    = sceneBoundingBox.GetMin();
        Vec4 sceneAABBMax    = sceneBoundingBox.GetMax();
        Vec4 sceneAABBCenter = sceneBoundingBox.GetCenter();
        Vec4 sceneAABBRadius = sceneBoundingBox.GetRadius();

        // Lets get scene bounding box in light space
        BoundingBox sceneBoundingBoxLightSpace;
        for (int i = 0; i < 8; ++i)
        {
            sceneBoundingBoxLightSpace.Grow(lightViewMatrix * (sceneAABBCenter + math::mulPerElem(boxbounds[i], sceneAABBRadius)));
        }

        //These are the unconfigured near and far plane values.  They are purposly awful to show
        // how important calculating accurate near and far planes is.
        float nearPlane = 0.0f;
        float farPlane  = 10000.0f;

        // We calculate the min and max vectors of the scene in light space. The min and max "Z" values of the
        // light space AABB can be used for the near and far plane. This is easier than intersecting the scene with the AABB
        // and in some cases provides similar results.
        Vec4 lightSpaceSceneAABBminValue = sceneBoundingBoxLightSpace.GetMin();  // world space scene aabb
        Vec4 lightSpaceSceneAABBmaxValue = sceneBoundingBoxLightSpace.GetMax();

        // The min and max z values are the near and far planes.
        nearPlane = lightSpaceSceneAABBminValue.getZ();
        farPlane  = lightSpaceSceneAABBmaxValue.getZ();

        float frustumIntervalBegin, frustumIntervalEnd;
        Vec4  lightCameraOrthographicMin;  // light space frustrum aabb
        Vec4  lightCameraOrthographicMax;

        Point3 sceneMin          = Point3(sceneAABBMin.getXYZ());
        Point3 sceneMax          = Point3(sceneAABBMax.getXYZ());
#if VECTORMATH_MODE_SSE
        float  sceneNearFarRange = math::SSE::dist(sceneMin, sceneMax);
#else
        float sceneNearFarRange  = math::Scalar::dist(sceneMin, sceneMax);
#endif

        Vec4 worldUnitsPerTexel = {0.0f, 0.0f, 0.0f, 0.0f};

        // We loop over the cascades to calculate the orthographic projection for each cascade.
        for (int cascadeIndex = 0; cascadeIndex < numCascades; ++cascadeIndex)
        {
            // Calculate the interval of the View Frustum that this cascade covers. We measure the interval
            // the cascade covers as a Min and Max distance along the Z Axis.
            {
                // Because we want to fit the orthogrpahic projection tightly around the Cascade, we set the Mimiumum cascade
                // value to the previous Frustum end Interval
                if (cascadeIndex == 0)
                    frustumIntervalBegin = 0.0f;
                else
                    frustumIntervalBegin = cascadeSplitPoints[cascadeIndex - 1];
            }

            // Scale the intervals between 0 and 1. They are now percentages that we can scale with.
            frustumIntervalEnd = cascadeSplitPoints[cascadeIndex];
            const float maxCascadeSplitPoint = 100;
            frustumIntervalBegin /= maxCascadeSplitPoint;
            frustumIntervalEnd /= maxCascadeSplitPoint;

            if (cascadeIndex == numCascades - 1)
            {
                frustumIntervalEnd = 1;  // last cascade goes to the edge of the scene
            }

            frustumIntervalBegin = frustumIntervalBegin * sceneNearFarRange;
            frustumIntervalEnd   = frustumIntervalEnd * sceneNearFarRange;

            // Lets get bounding box of current frustum
            Vec4 frustumPoints[8];
            CreateFrustumPointsFromCascadeInterval(camNear, frustumIntervalBegin, frustumIntervalEnd, cameraProjectionMatrix, frustumPoints);

            lightCameraOrthographicMin = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
            lightCameraOrthographicMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX};

            // Lets get bounding box of frustum after translating it into light view space
            Vec4 tempTranslatedCornerPoint;
            for (int icpIndex = 0; icpIndex < 8; ++icpIndex)
            {
                // Transform the frustum from camera view space to world space.
                frustumPoints[icpIndex] = cameraInverseViewMatrix * frustumPoints[icpIndex];
                // Transform the point from world space to Light Camera Space.
                tempTranslatedCornerPoint = lightViewMatrix * frustumPoints[icpIndex];

                // Find the closest point.
                lightCameraOrthographicMin = MinPerElement(tempTranslatedCornerPoint, lightCameraOrthographicMin);
                lightCameraOrthographicMax = MaxPerElement(tempTranslatedCornerPoint, lightCameraOrthographicMax);
            }

            const Vec4 g_halfVector          = {0.5f, 0.5f, 0.5f, 0.5f};
            const Vec4 g_multiplySetzwToZero = {1.0f, 1.0f, 0.0f, 0.0f};

            // This code removes the shimmering effect along the edges of shadows due to
            // the light changing to fit the camera.
            {
                Vec4 normalizeByBufferSize = Vec4((1.0f / width), (1.0f / width), 0.0f, 0.0f);

                // We calculate the offsets as a percentage of the bound.
                Vec4 boarderOffset = lightCameraOrthographicMax - lightCameraOrthographicMin;
                boarderOffset      = math::mulPerElem(boarderOffset, g_halfVector);
                lightCameraOrthographicMax += boarderOffset;
                lightCameraOrthographicMin -= boarderOffset;

                // The world units per texel are used to snap  the orthographic projection
                // to texel sized increments.
                // Because we're fitting tighly to the cascades, the shimmering shadow edges will still be present when the
                // camera rotates.  However, when zooming in or strafing the shadow edge will not shimmer.
                worldUnitsPerTexel = lightCameraOrthographicMax - lightCameraOrthographicMin;
                worldUnitsPerTexel = math::mulPerElem(worldUnitsPerTexel, normalizeByBufferSize);
            }

            if (moveLightTexelSize)
            {
                // We snap the camera to 1 pixel increments so that moving the camera does not cause the shadows to jitter.
                // This is a matter of integer dividing by the world space size of a texel
                lightCameraOrthographicMin = math::divPerElem(lightCameraOrthographicMin, worldUnitsPerTexel);
                lightCameraOrthographicMin = Vec4(floorf(lightCameraOrthographicMin.getX()),
                                                  floorf(lightCameraOrthographicMin.getY()),
                                                  floorf(lightCameraOrthographicMin.getZ()),
                                                  floorf(lightCameraOrthographicMin.getW()));
                lightCameraOrthographicMin = math::mulPerElem(lightCameraOrthographicMin, worldUnitsPerTexel);

                lightCameraOrthographicMax = math::divPerElem(lightCameraOrthographicMax, worldUnitsPerTexel);
                lightCameraOrthographicMax = Vec4(floorf(lightCameraOrthographicMax.getX()),
                                                  floorf(lightCameraOrthographicMax.getY()),
                                                  floorf(lightCameraOrthographicMax.getZ()),
                                                  floorf(lightCameraOrthographicMax.getW()));
                lightCameraOrthographicMax = math::mulPerElem(lightCameraOrthographicMax, worldUnitsPerTexel);
            }

            static bool s_InvertedDepth = GetConfig()->InvertedDepth;

            // Create the orthographic projection for this cascade.
            Mat4 shadowProjection = Orthographic(lightCameraOrthographicMin.getX(),
                                                                lightCameraOrthographicMax.getX(),
                                                                lightCameraOrthographicMin.getY(),
                                                                lightCameraOrthographicMax.getY(),
                                                                -farPlane,
                                                                -nearPlane,
                                                                s_InvertedDepth);


            m_ShadowProjectionMatrix[cascadeIndex]     = shadowProjection;
            m_ShadowViewProjectionMatrix[cascadeIndex] = shadowProjection * lightViewMatrix;
        }
    }

} // namespace cauldron
