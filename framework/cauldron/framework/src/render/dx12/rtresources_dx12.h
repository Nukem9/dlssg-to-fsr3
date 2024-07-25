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

#if defined(_DX12)

#include "render/rtresources.h"

#include <agilitysdk/include/d3d12.h>
#include <vector>

namespace cauldron
{
    class Mesh;
    class Buffer;

    class BLASInternal final : public BLAS
    {
    public:
        void AddGeometry(const Mesh* pMesh, const std::vector<VertexBufferInformation>& vertexPositions) override;
        void InitBufferResources() override;
        void Build(CommandList* pCmdList) override;

    private:
        friend class BLAS;
        BLASInternal() : BLAS() {};
        virtual ~BLASInternal() = default;

    private:
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>          m_DXRGeometries = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS m_DXRAccelStructInputs  = {};
    };

    class TLASInternal final : public TLAS
    {
    public:
        void Reset() { m_DXRInstanceDescriptors.clear(); };
        void Build(CommandList* pCmdList) override;
        void AddInstance(const BLAS* pBlas, const Mat4& transform, const uint32_t instanceID) override;

    private:
        friend class TLAS;
        TLASInternal();
        virtual ~TLASInternal() = default;

    private:
        std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_DXRInstanceDescriptors = {};
    };

    class ASManagerInternal final : public ASManager
    {
    public:
        void Update(CommandList* pCmdList) override;

    private:
        friend class ASManager;
        ASManagerInternal();
        virtual ~ASManagerInternal();
    };

} // namespace cauldron

#endif // #if defined(_DX12)
