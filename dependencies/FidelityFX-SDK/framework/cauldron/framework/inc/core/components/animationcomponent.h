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
#include "render/animation.h"

#include "core/contentloader.h"

#include <assert.h>

namespace cauldron
{
    class AnimationComponent;
    class GLTFLoader;

    /**
     * @class AnimationComponentMgr
     *
     * Component manager class for <c><i>AnimationComponent</i></c>s.
     *
     * @ingroup CauldronComponent
     */
    class AnimationComponentMgr : public ComponentMgr
    {
    public:
        static const wchar_t* s_ComponentName;      ///< Component name

    public:

        /**
         * @brief   Constructor with default behavior.
         */
        AnimationComponentMgr();

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~AnimationComponentMgr();

        /**
         * @brief   Component creator.
         */
        Component* SpawnComponent(Entity* pOwner, ComponentData* pData) override
        {
            return reinterpret_cast<Component*>(SpawnAnimationComponent(pOwner, pData));
        }

        /**
         * @brief   Allocates a new <c><i>AnimationComponent</i></c> for the given entity.
         */
        AnimationComponent* SpawnAnimationComponent(Entity* pOwner, ComponentData* pData);

        /**
         * @brief   Gets the component type string ID.
         */
        const wchar_t* ComponentType() const override { return s_ComponentName; }

        /**
         * @brief   Initializes the component manager.
         */
        void Initialize() override;

        /**
         * @brief   Shuts down the component manager.
         */
        void Shutdown() override;

        /**
         * @brief   Updates all managed components.
         */
        void UpdateComponents(double deltaTime) override;

        /**
         * @brief   Component manager instance accessor.
         */
        static AnimationComponentMgr* Get() { return s_pComponentManager; }

        /**
         * @brief   Returns the skinning matrices for the input modelId
         */
        const std::vector<MatrixPair>& GetSkinningMatrices(uint32_t modelId, int32_t skinId) const
        {
            return m_skinningData.at(modelId).m_SkinningMatrices[skinId];
        }

    private:
        // <ModelID, SkinningData>
        std::unordered_map<uint32_t, SkinningData>     m_skinningData = {};
        static AnimationComponentMgr* s_pComponentManager;

        friend class GLTFLoader;
    };

    /**
     * @struct AnimationComponentData
     *
     * Initialization data structure for the <c><i>AnimationComponent</i></c>.
     *
     * @ingroup CauldronComponent
     */
    struct AnimationComponentData : public ComponentData
    {
        uint32_t                        m_nodeId;       ///< Node index in the model representation
        const std::vector<Animation*>*  m_pAnimRef;     ///< Reference to the animation associated with this component
        int32_t                         m_skinId;       ///< Reference to the skin associated with this AnimationComponent, used at runtime to check which skin to apply to the surface being rendered
        uint32_t                        m_modelId;      ///< ID which identifies the model whose data we are interested in fetching

        // Skinning vertex buffers, one for each surface of the mesh
        std::vector<VertexBufferInformation> m_skinnedPositions;
        std::vector<VertexBufferInformation> m_skinnedNormals;
        std::vector<VertexBufferInformation> m_skinnedPreviousPosition;
        BLAS*                                m_animatedBlas = nullptr;
    };

    /**
     * @class AnimationComponent
     *
     * Animation component class. Implements animation functionality on an entity.
     *
     * @ingroup CauldronComponent
     */
    class AnimationComponent : public Component
    {
    public:

        /**
         * @brief   Constructor.
         */
        AnimationComponent(Entity* pOwner, ComponentData* pData, AnimationComponentMgr* pManager);

        /**
         * @brief   Destructor.
         */
        virtual ~AnimationComponent();

        /**
         * @brief   Sets the component's local transform (Animated transform for frame).
         */
        void SetLocalTransform(const Mat4& transform) { m_localTransform = transform; }

        /**
         * @brief   Gets the component's local transform (Animated transform for frame).
         */
        Mat4 GetLocalTransform() { return m_localTransform; }

        /**
         * @brief   Gets the component's animation data.
         */
        const AnimationComponentData* GetData() const { return m_pData; }

        /**
         * @brief   Component update. Process rigid body animation for frame.
         */
        void Update(double deltaTime) override;

    private:

        AnimationComponent() = delete;

        void UpdateLocalMatrix(uint32_t animationIndex, float time);

        Mat4 m_localTransform = Mat4::identity();

        // Keep a pointer on our initialization data for matrix reconstruction
        AnimationComponentData* m_pData;
    };

} // namespace cauldron
