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

#if defined(_DX12)
#include "render/dx12/buffer_dx12.h"
#include "render/dx12/commandlist_dx12.h"
#include "render/dx12/device_dx12.h"
#include "render/dx12/rtresources_dx12.h"
#include "render/dx12/profiler_dx12.h"
#include "render/dx12/defines_dx12.h"

#include <core/framework.h>
#include "core/components/meshcomponent.h"

namespace cauldron
{
    Buffer* CreateScratchBuffer(uint64_t size, const std::wstring& name)
    {
        BufferDesc bufferDesc;
        bufferDesc.Type  = BufferType::Data;
        bufferDesc.Flags = ResourceFlags::AllowUnorderedAccess;
        bufferDesc.Size  = (uint32_t)AlignUp(size, (uint64_t)D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
        bufferDesc.Name  = name;

        return Buffer::CreateBufferResource(&bufferDesc, ResourceState::UnorderedAccess);
    }

    Buffer* CreateASBuffer(uint64_t size, const std::wstring& name)
    {
        BufferDesc bufferDesc;
        bufferDesc.Type  = BufferType::AccelerationStructure;
        bufferDesc.Flags = ResourceFlags::AllowUnorderedAccess;
        bufferDesc.Size  = (uint32_t)AlignUp(size, (uint64_t)D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
        bufferDesc.Name  = name;

        return Buffer::CreateBufferResource(&bufferDesc, ResourceState::RTAccelerationStruct);
    }

    //////////////////////////////////////////////////////////////////////////
    // BLAS

    BLAS* BLAS::CreateBLAS()
    {
        return new BLASInternal();
    }

    void BLASInternal::Build(cauldron::CommandList* pCmdList)
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc{};
        desc.Inputs                           = m_DXRAccelStructInputs ;

        BufferAddressInfo addressInfo = m_pScratchBuffer->GetAddressInfo();
        desc.ScratchAccelerationStructureData = addressInfo.GetImpl()->GPUBufferView;

        addressInfo = m_pBackingBuffer->GetAddressInfo();
        desc.DestAccelerationStructureData = addressInfo.GetImpl()->GPUBufferView;

        MSComPtr<ID3D12GraphicsCommandList4> cmdList4;
        CauldronThrowOnFail(pCmdList->GetImpl()->DX12CmdList()->QueryInterface(IID_PPV_ARGS(&cmdList4)));
        cmdList4->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
    }

    void BLASInternal::AddGeometry(const Mesh* pMesh, const std::vector<VertexBufferInformation>& vertexPositions)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC desc{};
        desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

        for (uint32_t i = 0; i < (uint32_t)pMesh->GetNumSurfaces(); ++i)
        {
            const Surface* pSurface           = pMesh->GetSurface(i);
            const VertexBufferInformation& vb       = vertexPositions.at(pSurface->GetSurfaceID());
            const IndexBufferInformation&  ib = pSurface->GetIndexBuffer();
            BufferAddressInfo addressInfo;

            //#pragma message(Reminder "Implement transparent geometry")
            //
            // Only process opaque geometry
            if (pSurface->HasTranslucency())
                continue;
            desc.Flags                                = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            addressInfo = vb.pBuffer->GetAddressInfo();
            desc.Triangles.VertexBuffer.StartAddress  = addressInfo.GetImpl()->GPUBufferView;
            desc.Triangles.VertexBuffer.StrideInBytes = addressInfo.GetImpl()->StrideInBytes;
            desc.Triangles.VertexCount                = vb.Count;

            if (vb.ResourceDataFormat == ResourceFormat::RGB32_FLOAT)
                desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            else
                CauldronError(L"Unsupported resource format for ray tracing vertices");

            addressInfo = ib.pBuffer->GetAddressInfo();
            desc.Triangles.IndexBuffer                = addressInfo.GetImpl()->GPUBufferView;

            switch (ib.IndexFormat)
            {
            case ResourceFormat::R16_UINT:
                desc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
                break;
            case ResourceFormat::R32_UINT:
                desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
                break;
            default:
                CauldronError(L"Unsupported resource format for ray tracing indices");
            }

            desc.Triangles.IndexCount                 = ib.Count;
            desc.Triangles.Transform3x4               = 0;

            m_DXRGeometries.push_back(desc);
        }
    }

    void BLASInternal::InitBufferResources()
    {
        // ------------------------- //
        // Get AS build Info
        // ------------------------- //
        m_DXRAccelStructInputs .Type  = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        m_DXRAccelStructInputs .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        m_DXRAccelStructInputs .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        m_DXRAccelStructInputs .pGeometryDescs = m_DXRGeometries.data();
        m_DXRAccelStructInputs .NumDescs       = (UINT)m_DXRGeometries.size();

        MSComPtr<ID3D12Device5>                               device5;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizeInfo = {};
        CauldronThrowOnFail(GetDevice()->GetImpl()->DX12Device()->QueryInterface(IID_PPV_ARGS(&device5)));
        device5->GetRaytracingAccelerationStructurePrebuildInfo(&m_DXRAccelStructInputs , &sizeInfo);

        // ------------------------- //
        // Allocate Memory for AS
        // ------------------------- //
        m_pScratchBuffer = CreateScratchBuffer(sizeInfo.ScratchDataSizeInBytes, L"AS::BLAS_ScratchBuffer");
        m_pBackingBuffer = CreateASBuffer(sizeInfo.ResultDataMaxSizeInBytes, L"AS::BLAS_BackingResource");
    }

    //////////////////////////////////////////////////////////////////////////
    // TLAS

    TLAS* TLAS::CreateTLAS()
    {
        return new TLASInternal();
    }

    TLASInternal::TLASInternal() : TLAS()
    {
        m_pScratchBuffer = CreateScratchBuffer(TOTAL_TLAS_SCRATCH_SIZE, L"AS::TLAS_ScratchBuffer");
        m_pBackingBuffer = CreateASBuffer(TOTAL_TLAS_SIZE, L"AS::TLAS_BackingResource");
    }

    void TLASInternal::Build(cauldron::CommandList* pCmdList)
    {
        GPUScopedProfileCapture tlasMarker(pCmdList, L"TLAS Build");

        BufferAddressInfo instancesBufferInfo = GetFramework()->GetDynamicBufferPool()->AllocConstantBuffer(
            (uint32_t)sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * (uint32_t)m_DXRInstanceDescriptors.size(), m_DXRInstanceDescriptors.data());
        CauldronAssert(ASSERT_ERROR, instancesBufferInfo.GetImpl()->GPUBufferView, L"Could not allocate Buffer for RayTracing instances");

        // ------------------------- //
        // Get AS build Info
        // ------------------------- //
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS asInputs = {};
        asInputs.Type        = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        asInputs.Flags       = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        asInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

        asInputs.NumDescs      = static_cast<UINT>(m_DXRInstanceDescriptors.size());
        asInputs.InstanceDescs = instancesBufferInfo.GetImpl()->GPUBufferView;

#if _DEBUG
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizeInfo = {};

        MSComPtr<ID3D12Device5> device5;
        CauldronThrowOnFail(GetDevice()->GetImpl()->DX12Device()->QueryInterface(IID_PPV_ARGS(&device5)));
        device5->GetRaytracingAccelerationStructurePrebuildInfo(&asInputs, &sizeInfo);

        CauldronAssert(ASSERT_ERROR,
                       sizeInfo.ResultDataMaxSizeInBytes < TOTAL_TLAS_SIZE && sizeInfo.ScratchDataSizeInBytes < TOTAL_TLAS_SCRATCH_SIZE,
                       L"TLAS not big enough to contain input geometry");
#endif

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc{};
        desc.Inputs                           = asInputs;

        BufferAddressInfo addressInfo = m_pScratchBuffer->GetAddressInfo();
        desc.ScratchAccelerationStructureData = addressInfo.GetImpl()->GPUBufferView;

        addressInfo = m_pBackingBuffer->GetAddressInfo();
        desc.DestAccelerationStructureData = addressInfo.GetImpl()->GPUBufferView;

        MSComPtr<ID3D12GraphicsCommandList4> cmdList4;
        CauldronThrowOnFail(pCmdList->GetImpl()->DX12CmdList()->QueryInterface(IID_PPV_ARGS(&cmdList4)));
        cmdList4->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

        Barrier asBarrier = Barrier::UAV(this->GetBuffer()->GetResource());
        ResourceBarrier(pCmdList, 1, &asBarrier);
    }

    void TLASInternal::AddInstance(const BLAS* pBlas, const Mat4& transform, const uint32_t instanceID)
    {
        D3D12_RAYTRACING_INSTANCE_DESC desc = {};
        memcpy(desc.Transform, math::toFloatPtr(math::transpose(transform)), sizeof(desc.Transform));
        desc.InstanceID                          = instanceID;
        desc.InstanceMask                        = 0xFF;
        desc.InstanceContributionToHitGroupIndex = 0;
        desc.Flags                               = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

        BufferAddressInfo addressInfo = pBlas->GetBuffer()->GetAddressInfo();
        desc.AccelerationStructure = addressInfo.GetImpl()->GPUBufferView;

        m_DXRInstanceDescriptors.emplace_back(std::move(desc));
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

}  // namespace cauldron

#endif // #if defined(_DX12)
