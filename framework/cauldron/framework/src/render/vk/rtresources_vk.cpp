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

#if defined(_VK)
#include "render/vk/buffer_vk.h"
#include "render/vk/commandlist_vk.h"
#include "render/vk/device_vk.h"
#include "render/vk/gpuresource_vk.h"
#include "render/vk/profiler_vk.h"
#include "render/vk/rtresources_vk.h"

#include "core/components/meshcomponent.h"
#include <core/framework.h>

namespace cauldron
{
    Buffer* CreateScratchBuffer(uint64_t size, const std::wstring& name)
    {
        BufferDesc bufferDesc;
        bufferDesc.Type  = BufferType::Data;
        bufferDesc.Flags = ResourceFlags::AllowUnorderedAccess;
        bufferDesc.Size  = static_cast<uint32_t>(size);
        bufferDesc.Alignment = GetDevice()->GetImpl()->GetMinAccelerationStructureScratchOffsetAlignment();
        bufferDesc.Name  = name;

        return Buffer::CreateBufferResource(&bufferDesc, ResourceState::UnorderedAccess);
    }

    Buffer* CreateASBuffer(uint64_t size, const std::wstring& name)
    {
        BufferDesc bufferDesc;
        bufferDesc.Type  = BufferType::AccelerationStructure;
        bufferDesc.Flags = ResourceFlags::AllowUnorderedAccess;
        bufferDesc.Size  = static_cast<uint32_t>(size);
        bufferDesc.Name  = name;

        return Buffer::CreateBufferResource(&bufferDesc, ResourceState::RTAccelerationStruct);
    }

    //////////////////////////////////////////////////////////////////////////
    // BLAS

    BLAS* BLAS::CreateBLAS()
    {
        return new BLASInternal();
    }

    BLASInternal::~BLASInternal()
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        pDevice->GetDestroyAccelerationStructureKHR()(pDevice->VKDevice(), m_VKBLAS, nullptr);
    }

    void BLASInternal::Build(cauldron::CommandList* pCmdList)
    {
        VkAccelerationStructureBuildRangeInfoKHR* blasRanges[] = {m_VKRTBuildRangeInfos.data()};

        GetDevice()->GetImpl()->GetCmdBuildAccelerationStructuresKHR()(pCmdList->GetImpl()->VKCmdBuffer(), 1, &m_VKRTAccelStructInputs, blasRanges);
    }

    void BLASInternal::AddGeometry(const Mesh* pMesh, const std::vector<VertexBufferInformation>& vertexPositions)
    {
        VkAccelerationStructureGeometryKHR desc;
        desc.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        desc.pNext        = nullptr;
        desc.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;


        for (uint32_t i = 0; i < (uint32_t)pMesh->GetNumSurfaces(); ++i)
        {
            const Surface*                 pSurface = pMesh->GetSurface(i);
            const VertexBufferInformation& vb       = vertexPositions.at(pSurface->GetSurfaceID());
            const IndexBufferInformation&  ib       = pSurface->GetIndexBuffer();

            //#pragma message(Reminder "Implement transparent geometry")

            // Only process opaque geometry
            if (pSurface->HasTranslucency())
                continue;
            desc.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            desc.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            desc.geometry.triangles.vertexData.deviceAddress = vb.pBuffer->GetResource()->GetImpl()->GetDeviceAddress();

            BufferAddressInfo addressInfo = vb.pBuffer->GetAddressInfo();
            desc.geometry.triangles.vertexStride              = addressInfo.GetImpl()->StrideInBytes;
            desc.geometry.triangles.maxVertex                 = vb.Count;
            desc.geometry.triangles.pNext                     = nullptr;
            desc.geometry.triangles.transformData.hostAddress = nullptr;


            if (vb.ResourceDataFormat == ResourceFormat::RGB32_FLOAT)
                desc.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            else
                CauldronError(L"Unsupported resource format for ray tracing vertices");

            desc.geometry.triangles.indexData.deviceAddress    = ib.pBuffer->GetResource()->GetImpl()->GetDeviceAddress();

            switch (ib.IndexFormat)
            {
            case ResourceFormat::R16_UINT:
                desc.geometry.triangles.indexType = VK_INDEX_TYPE_UINT16;
                break;
            case ResourceFormat::R32_UINT:
                desc.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
                break;
            default:
                CauldronError(L"Unsupported resource format for ray tracing indices");
            }

            VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
            buildRangeInfo.primitiveCount  = ib.Count / 3;
            buildRangeInfo.primitiveOffset = 0;
            buildRangeInfo.firstVertex     = 0;
            buildRangeInfo.transformOffset = 0;

            m_VKRTBuildRangeInfos.push_back(buildRangeInfo);
            m_VKRTMaxPrimitives.push_back(ib.Count / 3);
            m_VKRTGeometries.push_back(desc);
        }
    }

    void BLASInternal::InitBufferResources()
    {
        // ------------------------- //
        // Get AS build Info
        // ------------------------- //
        m_VKRTAccelStructInputs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        m_VKRTAccelStructInputs.type  = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        m_VKRTAccelStructInputs.mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        m_VKRTAccelStructInputs.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        m_VKRTAccelStructInputs.pNext = nullptr;
        m_VKRTAccelStructInputs.geometryCount = static_cast<uint32_t>(m_VKRTGeometries.size());
        m_VKRTAccelStructInputs.pGeometries   = m_VKRTGeometries.data();

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        pDevice->GetAccelerationStructureBuildSizesKHR()(
            pDevice->VKDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &m_VKRTAccelStructInputs, m_VKRTMaxPrimitives.data(), &sizeInfo);

        // ------------------------- //
        // Allocate Memory for AS
        // ------------------------- //
        m_pScratchBuffer = CreateScratchBuffer(sizeInfo.buildScratchSize, L"AS::BLAS_ScratchBuffer");
        m_pBackingBuffer = CreateASBuffer(sizeInfo.accelerationStructureSize, L"AS::BLAS_BackingResource");

        // Create AS Handle
        VkAccelerationStructureCreateInfoKHR desc{};
        desc.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        desc.pNext  = nullptr;

        BufferAddressInfo addressInfo = m_pBackingBuffer->GetAddressInfo();
        desc.buffer = addressInfo.GetImpl()->Buffer;
        desc.size   = sizeInfo.accelerationStructureSize;
        desc.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        pDevice->GetCreateAccelerationStructureKHR()(pDevice->VKDevice(), &desc, nullptr, &m_VKBLAS);

        m_VKRTAccelStructInputs.dstAccelerationStructure  = m_VKBLAS;
        m_VKRTAccelStructInputs.scratchData.deviceAddress = m_pScratchBuffer->GetResource()->GetImpl()->GetDeviceAddress();
    }

    //////////////////////////////////////////////////////////////////////////
    // TLAS

    TLAS* TLAS::CreateTLAS()
    {
        return new TLASInternal();
    }

    TLASInternal::TLASInternal() : 
        TLAS()
    {
        m_pScratchBuffer = CreateScratchBuffer(TOTAL_TLAS_SCRATCH_SIZE, L"AS::TLAS_ScratchBuffer");
        m_pBackingBuffer = CreateASBuffer(TOTAL_TLAS_SIZE, L"AS::TLAS_BackingResource");

        // Create AS Handle
        VkAccelerationStructureCreateInfoKHR desc{};
        desc.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        desc.pNext  = nullptr;

        BufferAddressInfo addressInfo = m_pBackingBuffer->GetAddressInfo();
        desc.buffer = addressInfo.GetImpl()->Buffer;
        desc.size   = TOTAL_TLAS_SIZE;
        desc.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        pDevice->GetCreateAccelerationStructureKHR()(pDevice->VKDevice(), &desc, nullptr, &m_VKTLAS);

        // Create Instance Buffer
        BufferDesc instanceBufferDesc = {};
        instanceBufferDesc.Type       = BufferType::Data;
        instanceBufferDesc.Flags      = ResourceFlags::AllowUnorderedAccess;
        instanceBufferDesc.Size       = sizeof(VkAccelerationStructureInstanceKHR) * MAX_INSTANCES;
        instanceBufferDesc.Name       = L"InstanceBuffer";
        m_pInstanceBuffer             = Buffer::CreateBufferResource(&instanceBufferDesc, ResourceState::CommonResource);
    }

    TLASInternal::~TLASInternal()
    {
        delete m_pInstanceBuffer;

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        pDevice->GetDestroyAccelerationStructureKHR()(pDevice->VKDevice(), m_VKTLAS, nullptr);
    }

    void TLASInternal::Build(cauldron::CommandList* pCmdList)
    {
        if (m_VKInstances.empty())
            return;

        GPUScopedProfileCapture tlasMarker(pCmdList, L"TLAS::Build");

        // Update instances
        m_pInstanceBuffer->CopyData(m_VKInstances.data(), sizeof(VkAccelerationStructureInstanceKHR) * m_VKInstances.size());

        // ------------------------- //
        // Get AS build Info
        // ------------------------- //
        VkAccelerationStructureGeometryKHR tlasGeometry{};
        tlasGeometry.sType                                   = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        tlasGeometry.geometryType                            = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        tlasGeometry.geometry.instances.sType                = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        tlasGeometry.geometry.instances.arrayOfPointers      = VK_FALSE;
        tlasGeometry.geometry.instances.data.deviceAddress   = m_pInstanceBuffer->GetResource()->GetImpl()->GetDeviceAddress();

        VkAccelerationStructureBuildGeometryInfoKHR asInputs = {};
        asInputs.sType                                       = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        asInputs.type                      = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        asInputs.geometryCount                               = 1;
        asInputs.pGeometries                                 = &tlasGeometry;
        asInputs.mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        asInputs.pNext                     = nullptr;
        asInputs.dstAccelerationStructure  = m_VKTLAS;
        asInputs.scratchData.deviceAddress = m_pScratchBuffer->GetResource()->GetImpl()->GetDeviceAddress();

        VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo{};
        tlasRangeInfo.primitiveCount = static_cast<uint32_t>(m_VKInstances.size());

        VkAccelerationStructureBuildRangeInfoKHR* tlasRanges[] = {&tlasRangeInfo};

        GetDevice()->GetImpl()->GetCmdBuildAccelerationStructuresKHR()(pCmdList->GetImpl()->VKCmdBuffer(), 1, &asInputs, tlasRanges);

        Barrier asBarrier = Barrier::UAV(this->GetBuffer()->GetResource());
        ResourceBarrier(pCmdList, 1, &asBarrier);
    }

    void TLASInternal::AddInstance(const BLAS* pBlas, const Mat4& transform, uint32_t instanceID)
    {
        VkAccelerationStructureInstanceKHR tlasStructure{};
        memcpy((void*)&tlasStructure.transform, math::toFloatPtr(math::transpose(transform)), sizeof(tlasStructure.transform));
        tlasStructure.mask = 0xff;
        tlasStructure.instanceCustomIndex = instanceID;
        tlasStructure.instanceShaderBindingTableRecordOffset = 0;

        tlasStructure.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        tlasStructure.accelerationStructureReference = pBlas->GetBuffer()->GetResource()->GetImpl()->GetDeviceAddress();

        m_VKInstances.push_back(tlasStructure);
    }

    //////////////////////////////////////////////////////////////////////////
    // ASManager

    ASManager* ASManager::CreateASManager()
    {
        return new ASManagerInternal();
    }

    ASManagerInternal::ASManagerInternal() :
        ASManager()
    {
        m_pTlas = TLAS::CreateTLAS();
    }

    ASManagerInternal::~ASManagerInternal()
    {
        delete m_pTlas;
    }

    void ASManagerInternal::Update(cauldron::CommandList* pCmdList)
    {
        static_cast<TLASInternal*>(m_pTlas)->Reset();

        while (!m_ManagedInstances.empty())
        {
            ASInstance& asInstance = m_ManagedInstances.front();

            const BLAS* activeBlas = asInstance.Mesh->HasAnimatedBlas() ? asInstance.AnimatedBlas : asInstance.Mesh->GetStaticBlas();
            m_pTlas->AddInstance(activeBlas, asInstance.Transform, asInstance.Mesh->GetMeshIndex());

            m_ManagedInstances.pop();
        }

        m_pTlas->Build(pCmdList);
    }

} // namespace cauldron

#endif // #if defined(_VK)
