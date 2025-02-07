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

#include "misc/assert.h"
#include "misc/math.h"
#include "shaders/surfacerendercommon.h"

#include <vector>


namespace cauldron
{
    /**
     * @struct AnimInterpolants
     *
     * Represents animation interpolation data for a specific frame of animation
     *
     * @ingroup CauldronRender
     */
    struct AnimInterpolants
    {
        std::vector<char> Data;
        int32_t     Count = 0;
        int32_t     Stride;
        int32_t     Dimension;
        Vec4        Min = Vec4(0.f, 0.f, 0.f, 0.f);
        Vec4        Max = Vec4(0.f, 0.f, 0.f, 0.f);
    };

    /// Gets the <c><i>AnimInterpolants</i></c> for the given index (frame)
    ///
    /// @param [in] pInterpolant    The animation interpolants to read from.
    /// @param [in] index           The index of frame of the data to fetch.
    ///
    /// @returns                    A pointer to the offsetted interpolant data.
    ///
    /// @ingroup CauldronRender
    const void* GetInterpolant(const AnimInterpolants* pInterpolant, int32_t index);

    /// Gets the closest <c><i>AnimInterpolants</i></c> for the given animation value
    ///
    /// @param [in] pInterpolant    The animation interpolants to read from.
    /// @param [in] value           The value (time) at which to get nearest interpolated data from.
    ///
    /// @returns                    The index of the closest interpolant data to the desired value.
    ///
    /// @ingroup CauldronRender
    int32_t FindClosestInterpolant(const AnimInterpolants* pInterpolant, float value);

    /**
     * @struct AnimationSkin
     *
     * Represents an Animation Skin data for a specific mesh
     *
     * @ingroup CauldronRender
     */
    struct AnimationSkin
    {
        AnimInterpolants m_InverseBindMatrices;
        uint32_t         m_skeletonId;
        std::vector<int> m_jointsNodeIdx;
    };

    /**
     * @struct SkinningData
     *
     * Stores all the skins and skinning matrices for animated meshes
     *
     * @ingroup CauldronRender
     */
    struct SkinningData
    {
        // <skindIdx, Matrices>
        std::vector<std::vector<MatrixPair>> m_SkinningMatrices = {};
        const std::vector<AnimationSkin*>*   m_pSkins           = nullptr;
    };

    /**
     * @class AnimChannel
     *
     * An animation channel represents a single channel of an <c><i>Animation</i></c>. Each channel can
     * have multiple components to it, such as Translation, Rotation, and Scale components.
     *
     * @ingroup CauldronRender
     */
    class AnimChannel
    {
    public:

        /**
         * @brief   AnimChannel construction with default behavior.
         */
        AnimChannel() = default;

        /**
         * @brief   AnimChannel copy construction with default behavior.
         */
        AnimChannel(const AnimChannel&) = default;

        /**
         * @brief   AnimChannel assignment operator with default behavior.
         */
        AnimChannel& operator=(const AnimChannel&) = default;

        /**
         * @brief   AnimChannel destruction.
         */
        ~AnimChannel()
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(ComponentSampler::Count); ++i)
                delete m_pComponentSamplers[i];
        };

        /**
         * @enum ComponentSampler
         *
         * The types of components that can be found in an <c><i>AnimChannel</i></c>
         *
         * @ingroup CauldronRender
         */
        enum class ComponentSampler : uint32_t
        {
            Translation = 0,        ///< <c><i>AnimChannel</i></c> represents a Translation animation component.
            Rotation,               ///< <c><i>AnimChannel</i></c> represents a Rotation animation component.
            Scale,                  ///< <c><i>AnimChannel</i></c> represents a Scale animation component.

            Count
        };

        /**
         * @brief   Query if the animation channels contains a <c><i>ComponentSampler</i></c> of the requested type.
         */
        bool HasComponentSampler(ComponentSampler samplerID) const
        {
            return nullptr != m_pComponentSamplers[static_cast<uint32_t>(samplerID)];
        }

        /**
         * @brief   Samples the requested <c><i>ComponentSampler</i></c> at a specific time to get the animation data.
         */
        void SampleAnimComponent(ComponentSampler samplerID, float time, float* frac, float** pCurr, float** pNext) const
        {
            if (HasComponentSampler(samplerID))
            {
                SampleLinear(*m_pComponentSamplers[static_cast<uint32_t>(samplerID)], time, frac, pCurr, pNext);
            }
        }

        /**
         * @brief   Creates a <c><i>ComponentSampler</i></c> and assigns time and value fields to be populated.
         */
        void CreateComponentSampler(ComponentSampler samplerID, AnimInterpolants** timeInterpolants, AnimInterpolants** valueInterpolants)
        {
            CauldronAssert(ASSERT_CRITICAL, nullptr == m_pComponentSamplers[static_cast<uint32_t>(samplerID)], L"Overriding and existing animation component sampler. Memory leak!");
            m_pComponentSamplers[static_cast<uint32_t>(samplerID)] = new AnimSampler();
            *timeInterpolants = &m_pComponentSamplers[static_cast<uint32_t>(samplerID)]->m_Time;
            *valueInterpolants = &m_pComponentSamplers[static_cast<uint32_t>(samplerID)]->m_Value;
        }

        /**
         * @brief   Queries the <c><i>ComponentSampler</i></c> animation duration.
         */
        float GetComponentSamplerDuration(ComponentSampler samplerID) const
        {
            if (m_pComponentSamplers[static_cast<uint32_t>(samplerID)])
            {
                // Duration is based on max value in the "Time" interpolant
                return m_pComponentSamplers[static_cast<uint32_t>(samplerID)]->m_Time.Max[0];
            }

            return 0.f;
        }

    private:

        typedef struct AnimSampler
        {
            AnimInterpolants m_Time;
            AnimInterpolants m_Value;
        } AnimSampler;

        void SampleLinear(const AnimSampler& sampler, float time, float* frac, float** pCurr, float** pNext) const;

        AnimSampler* m_pComponentSamplers[static_cast<uint32_t>(ComponentSampler::Count)] = { nullptr };
    };

    /**
     * @class Animation
     *
     * Holds all high level information related to an animation instance. An animation is made up of various
     * <c><i>AnimChannel</i></c>.
     *
     * @ingroup CauldronRender
     */
    class Animation
    {
    public:

        /**
         * @brief   Construction with default behavior.
         */
        Animation() = default;

        /**
         * @brief   Destruction with default behavior.
         */
        ~Animation() = default;

        /**
         * @brief   Fetches the duration of the <c><i>Animation</i></c>.
         */
        const float GetDuration() const { return m_Duration; }

        /**
         * @brief   Sets the duration of the <c><i>Animation</i></c>.
         */
        void SetDuration(float duration) { m_Duration = duration; }

        /**
         * @brief   Sets the number of <c><i>AnimChannel</i></c>s in the <c><i>Animation</i></c>.
         */
        void SetNumAnimationChannels(uint32_t numChannels) { m_AnimationChannels.resize(numChannels); }

        /**
         * @brief   Gets a specific <c><i>AnimChannel</i></c> in order to query it's animation data.
         */
        const AnimChannel* GetAnimationChannel(uint32_t animationIndex) const { return &m_AnimationChannels[animationIndex]; }
        AnimChannel* GetAnimationChannel(uint32_t animationIndex) { return &m_AnimationChannels[animationIndex]; }

    private:
        float                       m_Duration;
        std::vector<AnimChannel>    m_AnimationChannels;
    };
}
