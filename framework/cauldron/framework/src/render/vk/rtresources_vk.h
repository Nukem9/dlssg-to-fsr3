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

#if defined(_VK)

#include "render/rtresources.h"

#include <vulkan/vulkan.h>

namespace cauldron
{

    class Mesh;
    class Buffer;

    class BLASInternal final : public BLAS
    {
    public:
        void Build(cauldron::CommandList* pCmdList) override;
        void InitBufferResources() override;
        void AddGeometry(const Mesh* pMesh, const std::vector<VertexBufferInformation>& vertexPositions) override;

    private:
        friend class BLAS;
        BLASInternal() : BLAS() {};
        virtual ~BLASInternal();

    private:

        std::vector<VkAccelerationStructureGeometryKHR>       m_VKRTGeometries      = {};
        std::vector<uint32_t>                                 m_VKRTMaxPrimitives   = {};
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> m_VKRTBuildRangeInfos = {};

        VkAccelerationStructureBuildGeometryInfoKHR           m_VKRTAccelStructInputs = {};
        VkAccelerationStructureKHR                            m_VKBLAS                = VK_NULL_HANDLE;
    };

    class TLASInternal final : public TLAS
    {
    public:
        void Reset() { m_VKInstances.clear(); };
        void Build(cauldron::CommandList* pCmdList)            override;
        void AddInstance(const BLAS* pBlas, const Mat4& transform, const uint32_t instanceID) override;
        const VkAccelerationStructureKHR& GetHandle() const { return m_VKTLAS; };

    private:
        friend class TLAS;
        TLASInternal();
        virtual ~TLASInternal();

    private:
        std::vector<VkAccelerationStructureInstanceKHR> m_VKInstances     = {};
        VkAccelerationStructureKHR                      m_VKTLAS          = VK_NULL_HANDLE;
        Buffer*                                         m_pInstanceBuffer = nullptr;
    };

    class ASManagerInternal final : public ASManager
    {
    public:
        void Update(cauldron::CommandList* pCmdList) override;

    private:
        friend class ASManager;
        ASManagerInternal();
        virtual ~ASManagerInternal();

    };

} // namespace cauldron

#endif // #if defined(_VK)
