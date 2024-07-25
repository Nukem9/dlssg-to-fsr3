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

#include "core/framework.h"
#include "misc/assert.h"

#include "render/dx12/device_dx12.h"
#include "render/dx12/resourceview_dx12.h"
#include "render/dx12/resourceviewallocator_dx12.h"

namespace cauldron
{
    ResourceViewAllocator* ResourceViewAllocator::CreateResourceViewAllocator()
    {
        return new ResourceViewAllocatorInternal();
    }

    ResourceViewAllocatorInternal::ResourceViewAllocatorInternal() :
        ResourceViewAllocator()
    {
        ID3D12Device* pDevice = GetDevice()->GetImpl()->DX12Device();

        // Create descriptor heaps for all resource types
        for (uint32_t i = 0; i < static_cast<uint32_t>(ResourceViewHeapType::Count); ++i)
        {
            std::wstring heapName;
            ResourceViewHeapType       heapViewType = static_cast<ResourceViewHeapType>(i);
            D3D12_DESCRIPTOR_HEAP_TYPE d3dHeapType;
            switch (heapViewType)
            {
            case ResourceViewHeapType::GPUResourceView:
                heapName = L"GPUResourceView_DescriptorHeap";
                m_NumDescriptors[i] = m_NumViews[i];
                d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                break;
            case ResourceViewHeapType::CPUResourceView:
                heapName = L"CPUResourceView_DescriptorHeap";
                m_NumDescriptors[i] = m_NumViews[i];
                d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                break;
            case ResourceViewHeapType::CPURenderView:
                heapName = L"CPURenderView_DescriptorHeap";
                m_NumDescriptors[i] = m_NumViews[i];
                d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                break;
            case ResourceViewHeapType::CPUDepthView:
                heapName = L"CPUDepthView_DescriptorHeap";
                m_NumDescriptors[i] = m_NumViews[i];
                d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
                break;
            case ResourceViewHeapType::GPUSamplerView:
                heapName = L"GPUSamplerView_DescriptorHeap";
                m_NumDescriptors[i] = m_NumViews[i];
                d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
                break;
            }

            // Grab the descriptor size for offset calculations
            m_DescriptorSizes[i] = pDevice->GetDescriptorHandleIncrementSize(d3dHeapType);

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
            heapDesc.NumDescriptors = m_NumDescriptors[i];
            heapDesc.Type = d3dHeapType;
            heapDesc.NodeMask = 0;
            heapDesc.Flags = (heapViewType == ResourceViewHeapType::GPUResourceView || heapViewType == ResourceViewHeapType::GPUSamplerView)
                ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            CauldronThrowOnFail(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_pDescriptorHeaps[i])));
            m_pDescriptorHeaps[i]->SetName(heapName.c_str());
        }
    }

    ResourceView* ResourceViewAllocatorInternal::AllocateViews(ResourceViewHeapType type, uint32_t count)
    {
        uint32_t heapID = static_cast<uint32_t>(type);
        CauldronAssert(ASSERT_CRITICAL,
                       m_DescriptorIndex[heapID] + count <= m_NumDescriptors[heapID],
                       L"Resource view allocator has run out of memory, please increase its size.");

        ResourceView* pView = nullptr;

        // Do we need GPU view mappings?
        bool needsGPU(false);
        if (type == ResourceViewHeapType::GPUResourceView || type == ResourceViewHeapType::GPUSamplerView)
            needsGPU = true;

        // Allocate the handles
        {
            // This can happen on background threads
            std::unique_lock<std::mutex> lock(m_CriticalSection);

            uint32_t                    offset  = m_DescriptorIndex[heapID] * m_DescriptorSizes[heapID];
            D3D12_CPU_DESCRIPTOR_HANDLE cpuView = m_pDescriptorHeaps[heapID]->GetCPUDescriptorHandleForHeapStart();
            cpuView.ptr += offset;

            D3D12_GPU_DESCRIPTOR_HANDLE gpuView = {};
            if (needsGPU)
            {
                gpuView = m_pDescriptorHeaps[heapID]->GetGPUDescriptorHandleForHeapStart();
                gpuView.ptr += offset;
            }

            // Create the view(s)
            ResourceViewInitParams initParams = {};
            initParams.hCPUHandle = cpuView;
            initParams.hGPUHandle = gpuView;
            initParams.descriptorSize = m_DescriptorSizes[heapID];
            pView = ResourceView::CreateResourceView(type, count, &initParams);
            CauldronAssert(ASSERT_ERROR, pView != nullptr, L"Could not allocate ResourceView");

            // Increment internals
            m_DescriptorIndex[heapID] += count;
        }

        return pView;
    }


    void ResourceViewAllocatorInternal::AllocateCPUResourceViews(ResourceView** ppResourceView, uint32_t count /*=1*/)
    {
        *ppResourceView = AllocateViews(ResourceViewHeapType::CPUResourceView, count);
    }

    void ResourceViewAllocatorInternal::AllocateGPUResourceViews(ResourceView** ppResourceView, uint32_t count /*=1*/)
    {
        *ppResourceView = AllocateViews(ResourceViewHeapType::GPUResourceView, count);
    }

    void ResourceViewAllocatorInternal::AllocateGPUSamplerViews(ResourceView** ppResourceView, uint32_t count /*=1*/)
    {
        *ppResourceView = AllocateViews(ResourceViewHeapType::GPUSamplerView, count);
    }

    void ResourceViewAllocatorInternal::AllocateCPURenderViews(ResourceView** ppResourceView, uint32_t count /*=1*/)
    {
        *ppResourceView = AllocateViews(ResourceViewHeapType::CPURenderView, count);
    }

    void ResourceViewAllocatorInternal::AllocateCPUDepthViews(ResourceView** ppResourceView, uint32_t count /*=1*/)
    {
        *ppResourceView = AllocateViews(ResourceViewHeapType::CPUDepthView, count);
    }

    ID3D12DescriptorHeap* ResourceViewAllocatorInternal::DX12DescriptorHeap(ResourceViewHeapType type) const
    {
        CauldronAssert(ASSERT_CRITICAL, static_cast<size_t>(type) < static_cast<size_t>(ResourceViewHeapType::Count), L"Requesting invalid descriptor heap. Access violation.");
        return m_pDescriptorHeaps[static_cast<size_t>(type)].Get();
    }

} // namespace cauldron

#endif // #if defined(_DX12)
