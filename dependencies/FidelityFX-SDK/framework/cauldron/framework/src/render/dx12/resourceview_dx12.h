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

#if defined (_DX12)

#include "render/resourceview.h"

#include <agilitysdk/include/d3d12.h>

namespace cauldron
{
    struct ResourceViewInfoInternal final
    {
        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE hGPUHandle;
    };
    static_assert(sizeof(ResourceViewInfo::resourceViewSize) >= sizeof(ResourceViewInfoInternal), "ResourceViewInfo is not large enough to hold all implementation details. Please grow.");

    struct ResourceViewInitParams
    {
        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE hGPUHandle;
        uint32_t                    descriptorSize;
    };

    class ResourceViewInternal final : public ResourceView
    {
    public:
        const ResourceViewInfo GetViewInfo(uint32_t index = 0) const override;

        void BindTextureResource(const GPUResource* pResource, const TextureDesc& textureDesc, ResourceViewType type, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstArraySlice, uint32_t index = 0) override;
        void BindBufferResource(const GPUResource* pResource, const BufferDesc& bufferDesc, ResourceViewType type, uint32_t firstElement, uint32_t numElements, uint32_t index = 0) override;
        void BindSamplerResource(const Sampler* pSampler, uint32_t index = 0) override;

    private:
        friend class ResourceView;
        ResourceViewInternal(D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE hGPUHandle, ResourceViewHeapType type, uint32_t count, uint32_t descriptorSize);
        ResourceViewInternal() = delete;
        virtual ~ResourceViewInternal() = default;

        void BindRTV(const GPUResource* pResource, const TextureDesc& textureDesc, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t index);
        void BindDSV(const GPUResource* pResource, const TextureDesc& textureDesc, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t index);
        void BindTextureSRV(const GPUResource* pResource, const TextureDesc& textureDesc, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t index);
        void BindTextureUAV(const GPUResource* pResource, const TextureDesc& textureDesc, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t index);

        void BindBufferCBV(const GPUResource* pResource, uint32_t index);
        void BindBufferSRV(const GPUResource* pResource, const BufferDesc& bufferDesc, uint32_t firstElement, uint32_t numElements, uint32_t index);
        void BindBufferUAV(const GPUResource* pResource, const BufferDesc& bufferDesc, uint32_t firstElement, uint32_t numElements, uint32_t index);
        void BindSampler(const Sampler* pSampler, uint32_t index);
        void BindAccelerationStructure(const GPUResource* pResource, uint32_t index);

    private:
        D3D12_GPU_DESCRIPTOR_HANDLE m_hGPUHandle = {};
        D3D12_CPU_DESCRIPTOR_HANDLE m_hCPUHandle = {};
        uint32_t                    m_DescriptorSize = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;
    };

} // namespace cauldron

#endif // #if defined (_DX12)
