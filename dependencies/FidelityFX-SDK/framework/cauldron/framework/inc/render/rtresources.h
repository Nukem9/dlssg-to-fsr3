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

#include "render/buffer.h"

#include <queue>

#define TOTAL_BLAS_SIZE         (1024 * 1024 * 256)
#define TOTAL_BLAS_SCRATCH_SIZE (1024 * 1024 * 256)
#define TOTAL_TLAS_SIZE         (1024 * 1024 * 64)
#define TOTAL_TLAS_SCRATCH_SIZE (1024 * 1024 * 64)
#define MAX_INSTANCES           (1<<20)

namespace cauldron
{
    class Mesh;
    class CommandList;
    struct VertexBufferInformation;

    /**
     * @class BLAS
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of the Bottom Level Acceleration Structure.
     * Only created when "BuildRayTracingAccelerationStructure" config is set to true.
     *
     * @ingroup CauldronRender
     */
    class BLAS
    {
    public:

        /**
         * @brief   BLAS instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static BLAS* CreateBLAS();

        /**
         * @brief   Destruction.
         */
        virtual ~BLAS()
        {
            delete m_pBackingBuffer;
            delete m_pScratchBuffer;
        }

        /**
         * @brief   Returns the backing <c><i>Buffer</i></c> resource.
         */
        const Buffer* GetBuffer() const { return m_pBackingBuffer; }

        /**
         * @brief   Adds a mesh to the BLAS instance.
         */
        virtual void AddGeometry(const Mesh* pMesh, const std::vector<VertexBufferInformation>& vertexPositions) = 0;

        /**
         * @brief   Initializes BLAS buffer resources.
         */
        virtual void InitBufferResources()                  = 0;

        /**
         * @brief   Builds the bottom level acceleration structure.
         */
        virtual void Build(CommandList* pCmdList) = 0;

    private:
        NO_COPY(BLAS)
        NO_MOVE(BLAS)

    protected:
        BLAS() = default;
        Buffer* m_pBackingBuffer = nullptr;
        Buffer* m_pScratchBuffer = nullptr;
    };

    /**
     * @class TLAS
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of the Top Level Acceleration Structure.
     * Only created when "BuildRayTracingAccelerationStructure" config is set to true.
     *
     * @ingroup CauldronRender
     */
    class TLAS
    {
    public:

        /**
         * @brief   TLAS instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static TLAS* CreateTLAS();

        /**
         * @brief   Destruction.
         */
        virtual ~TLAS()
        {
            delete m_pBackingBuffer;
            delete m_pScratchBuffer;
        };

        /**
         * @brief   Returns the backing <c><i>Buffer</i></c> resource.
         */
        const Buffer* GetBuffer() const { return m_pBackingBuffer; }

        /**
         * @brief   Builds the top level acceleration structure.
         */
        virtual void Build(CommandList* pCmdList)                 = 0;

        /**
         * @brief   Adds a transformed BLAS instance to the TLAS instance.
         */
        virtual void AddInstance(const BLAS* pBlas, const Mat4& transform, const uint32_t instanceID)  = 0;

    private:
        NO_COPY(TLAS)
        NO_MOVE(TLAS)

    protected:
        TLAS() = default;
        Buffer* m_pBackingBuffer = nullptr;
        Buffer* m_pScratchBuffer   = nullptr;
    };

    /**
     * @class ASInstance
     *
     * The Acceleration Structure instance representation stored in the acceleration structure manager.
     *
     * @ingroup CauldronRender
     */
    struct ASInstance
    {
        const Mesh* Mesh = nullptr;         ///< A mesh instance.
        const Mat4& Transform;              ///< The mesh instance's transform.
        const BLAS* AnimatedBlas = nullptr; ///< A pointer to the animated Blas
    };

    /**
     * @class ASManager
     *
     * The Acceleration Structure manager used to update and build various rt acceleration structures.
     *
     * @ingroup CauldronRender
     */
    class ASManager
    {
    public:

        /**
         * @brief   ASManager instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static ASManager* CreateASManager();

        /**
         * @brief   Destruction with default behavior.
         */
        virtual ~ASManager() = default;

        /**
         * @brief   Updates all managed <c><i>ASInstance</i></c>s and build the top level acceleration structure.
         */
        virtual void Update(CommandList* pCmdList) = 0;

        /**
         * @brief   Returns the top level acceleration structure.
         */
        const TLAS* GetTLAS() const { return m_pTlas; }

        /**
         * @brief   Pushes a new <c><i>ASInstance</i></c> for a <c><i>Mesh</i></c> to the managed list of instances.
         */
        void PushInstance(const Mesh* pMesh, const Mat4& transform, const BLAS* animatedBlas = nullptr) { m_ManagedInstances.push({pMesh, transform, animatedBlas}); }

    private:
        NO_COPY(ASManager)
        NO_MOVE(ASManager)

    protected:
        ASManager() = default;
        std::queue<ASInstance>   m_ManagedInstances{};
        TLAS*                    m_pTlas = nullptr;
    };

} // namespace cauldron
