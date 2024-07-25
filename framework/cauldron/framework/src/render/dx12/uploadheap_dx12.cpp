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

#include "render/dx12/device_dx12.h"
#include "render/dx12/gpuresource_dx12.h"
#include "render/dx12/uploadheap_dx12.h"

#include "core/framework.h"
#include "misc/assert.h"

namespace cauldron
{
    UploadHeap* UploadHeap::CreateUploadHeap()
    {
        return new UploadHeapInternal();
    }

    UploadHeapInternal::UploadHeapInternal() :
        UploadHeap()
    {
        m_Size = GetConfig()->UploadHeapSize;

        GPUResourceInitParams initParams = {};
        initParams.heapType = D3D12_HEAP_TYPE_UPLOAD;
        initParams.resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_Size);
        initParams.type = GPUResourceType::Buffer;

        m_pResource = GPUResource::CreateGPUResource(L"Cauldron Upload Heap", nullptr, ResourceState::GenericRead, &initParams);

        // Map the memory
        CauldronThrowOnFail(m_pResource->GetImpl()->DX12Resource()->Map(0, nullptr, (void**)&m_pDataBegin));
        m_pDataEnd = m_pDataBegin + m_pResource->GetImpl()->DX12Desc().Width;

        // Now that memory is mapped, initialize the allocation block scheme
        InitAllocationBlocks();
    }

    ID3D12Resource* UploadHeapInternal::DX12Resource()
    { 
        return m_pResource->GetImpl()->DX12Resource();
    }

    const ID3D12Resource* UploadHeapInternal::DX12Resource() const
    { 
        return m_pResource->GetImpl()->DX12Resource();
    }

} // namespace cauldron

#endif // #if defined(_DX12)
