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

#include "core/components/animationcomponent.h"
#include "core/entity.h"
#include "core/framework.h"
#include "render/rtresources.h"

#include "misc/assert.h"
#include "misc/math.h"

#include<algorithm>

namespace cauldron
{
    const wchar_t*         AnimationComponentMgr::s_ComponentName     = L"AnimationComponent";
    AnimationComponentMgr* AnimationComponentMgr::s_pComponentManager = nullptr;

    AnimationComponentMgr::AnimationComponentMgr()
        : ComponentMgr()
    {
    }

    AnimationComponentMgr::~AnimationComponentMgr()
    {
    }

    AnimationComponent* AnimationComponentMgr::SpawnAnimationComponent(Entity* pOwner, ComponentData* pData)
    {
        // Create the component
        AnimationComponent* pComponent = new AnimationComponent(pOwner, pData, this);

        // Add it to the owner
        pOwner->AddComponent(pComponent);

        return pComponent;
    }

    void AnimationComponentMgr::Initialize()
    {
        CauldronAssert(ASSERT_CRITICAL,
                       !s_pComponentManager,
                       L"AnimationComponentMgr instance is non-null. Component managers can ONLY be created through framework registration using "
                       L"RegisterComponentManager<>()");

        // Initialize the convenience accessor to avoid having to do a map::find each time we want the manager
        s_pComponentManager = this;
    }

    void AnimationComponentMgr::Shutdown()
    {
        // Clear out the convenience instance pointer
        CauldronAssert(
            ASSERT_ERROR, s_pComponentManager, L"AnimationComponentMgr instance is null. Component managers can ONLY be destroyed through framework shutdown");
        s_pComponentManager = nullptr;
    }

    AnimationComponent::AnimationComponent(Entity* pOwner, ComponentData* pData, AnimationComponentMgr* pManager)
        : Component(pOwner, pData, pManager)
        , m_pData(reinterpret_cast<AnimationComponentData*>(pData))
    {
        m_pData->m_animatedBlas = BLAS::CreateBLAS();
    }

    AnimationComponent::~AnimationComponent()
    {
        for (size_t i = 0; i < m_pData->m_skinnedPositions.size(); ++i)
        {
            delete m_pData->m_skinnedPositions[i].pBuffer;
            delete m_pData->m_skinnedNormals[i].pBuffer;
            delete m_pData->m_skinnedPreviousPosition[i].pBuffer;
        }

        delete m_pData->m_animatedBlas;
    }

    void AnimationComponent::UpdateLocalMatrix(uint32_t animationIndex, float time)
    {
        if (animationIndex >= m_pData->m_pAnimRef->size())
        {
            CauldronWarning(L"Animation selected not available");
            return;
        }

        Animation* animation = (*m_pData->m_pAnimRef)[animationIndex];

        // Loop animation
        time = fmod(time, animation->GetDuration());

        const AnimChannel* pAnimChannel = animation->GetAnimationChannel(m_pData->m_nodeId);
        if (pAnimChannel->HasComponentSampler(AnimChannel::ComponentSampler::Translation) ||
            pAnimChannel->HasComponentSampler(AnimChannel::ComponentSampler::Rotation) ||
            pAnimChannel->HasComponentSampler(AnimChannel::ComponentSampler::Scale))
        {
            float frac, *pCurr, *pNext;

            // Animate translation
            //
            Vec4 translation = Vec4(0, 0, 0, 0);
            if (pAnimChannel->HasComponentSampler(AnimChannel::ComponentSampler::Translation))
            {
                pAnimChannel->SampleAnimComponent(AnimChannel::ComponentSampler::Translation, time, &frac, &pCurr, &pNext);
                translation = ((1.0f - frac) * math::Vector4(pCurr[0], pCurr[1], pCurr[2], 0)) + ((frac)*math::Vector4(pNext[0], pNext[1], pNext[2], 0));
            }
            else
            {
                translation = GetLocalTransform().getCol3();
            }

            // Animate rotation
            //
            Mat4 rotation = Mat4::identity();
            if (pAnimChannel->HasComponentSampler(AnimChannel::ComponentSampler::Rotation))
            {
                pAnimChannel->SampleAnimComponent(AnimChannel::ComponentSampler::Rotation, time, &frac, &pCurr, &pNext);
                rotation =
                    math::Matrix4(math::slerp(frac, math::Quat(pCurr[0], pCurr[1], pCurr[2], pCurr[3]), math::Quat(pNext[0], pNext[1], pNext[2], pNext[3])),
                                  math::Vector3(0.0f, 0.0f, 0.0f));
            }

            // Animate scale
            //
            Vec4 scale = Vec4(1, 1, 1, 1);
            if (pAnimChannel->HasComponentSampler(AnimChannel::ComponentSampler::Scale))
            {
                pAnimChannel->SampleAnimComponent(AnimChannel::ComponentSampler::Scale, time, &frac, &pCurr, &pNext);
                scale = ((1.0f - frac) * math::Vector4(pCurr[0], pCurr[1], pCurr[2], 0)) + ((frac)*math::Vector4(pNext[0], pNext[1], pNext[2], 0));
            }

            Mat4 transform = math::Matrix4::translation(translation.getXYZ()) * rotation * math::Matrix4::scale(scale.getXYZ());

            m_localTransform = transform;
        }
    }

    void cauldron::AnimationComponentMgr::UpdateComponents(double deltaTime)
    {
        static double time = 0.0;
        time += deltaTime;

        // Update local transforms
        for (auto& component : m_ManagedComponents)
        {
            component->Update(time);
        }

        // Update global transforms (process the hierarchy)
        for (auto& component : m_ManagedComponents)
        {
            const auto& parent = component->GetOwner()->GetParent();
            const auto& data   = static_cast<const AnimationComponent*>(component)->GetData();

            Mat4 parentTransform = parent == nullptr ? Mat4::identity() : parent->GetTransform();
            Mat4 globalTransform = parentTransform * static_cast<AnimationComponent*>(component)->GetLocalTransform();

            /*
            * Currently supports only one Skin per Model. Most assets work this way but it's technically possible for a Model to have multiple Skins.
            * If supporting these models is desired in the future, the following code needs to change.
            */
            const auto& skins = m_skinningData[data->m_modelId].m_pSkins;
            if (skins != nullptr && skins->size() > 0 && skins->at(0)->m_skeletonId == data->m_nodeId)
            {
                globalTransform = static_cast<AnimationComponent*>(component)->GetLocalTransform();
            }

            component->GetOwner()->SetPrevTransform(component->GetOwner()->GetTransform());
            component->GetOwner()->SetTransform(globalTransform);
        }


        // Skinning
        for (auto& component : m_ManagedComponents)
        {
            const auto& data = static_cast<const AnimationComponent*>(component)->GetData();

            // Animated models with no skinning
            if (!m_skinningData[data->m_modelId].m_pSkins)
                continue;

            for (size_t skinIdx = 0; skinIdx < m_skinningData[data->m_modelId].m_pSkins->size(); ++skinIdx)
            {
                const AnimationSkin* skin = m_skinningData[data->m_modelId].m_pSkins->at(skinIdx);

                // if this node is in the target list of joints to be updated, update it
                for (size_t i = 0; i < skin->m_jointsNodeIdx.size(); ++i)
                {
                    if (data->m_nodeId == skin->m_jointsNodeIdx[i])
                    {
                        const Mat4* pM = (Mat4*)skin->m_InverseBindMatrices.Data.data();
                        m_skinningData[data->m_modelId].m_SkinningMatrices[skinIdx][i].Set(component->GetOwner()->GetTransform() * pM[i]);
                    }
                }
            }
        }
    }

    void AnimationComponent::Update(double time)
    {
        UpdateLocalMatrix(0, static_cast<float>(time));
    }

} // namespace cauldron
