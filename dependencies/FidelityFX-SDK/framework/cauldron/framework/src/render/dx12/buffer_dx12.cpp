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

#include "render/dx12/buffer_dx12.h"
#include "render/dx12/commandlist_dx12.h"
#include "render/dx12/device_dx12.h"
#include "render/dx12/gpuresource_dx12.h"
#include "render/dx12/uploadheap_dx12.h"

#include "dxheaders/include/directx/d3dx12.h"

namespace cauldron
{
    BufferCopyDesc::BufferCopyDesc(const GPUResource* pSource, const GPUResource* pDest)
    {
        GetImpl()->pSrc = const_cast<ID3D12Resource*>(pSource->GetImpl()->DX12Resource());
        GetImpl()->SrcOffset = 0;
        GetImpl()->pDst = const_cast<ID3D12Resource*>(pDest->GetImpl()->DX12Resource());
        GetImpl()->DstOffset = 0;
        GetImpl()->Size = pSource->GetImpl()->DX12Desc().Width;
    }

    Buffer* Buffer::CreateBufferResource(const BufferDesc* pDesc, ResourceState initialState, ResizeFunction fn/*= nullptr*/, void* customOwner/*= nullptr*/)
    {
        return new BufferInternal(pDesc, initialState, fn, customOwner);
    }

    BufferInternal::BufferInternal(const BufferDesc* pDesc, ResourceState initialState, ResizeFunction fn, void* customOwner) :
        Buffer(pDesc, fn)
    {
        // Create a resource backed by the memory allocator
        D3D12_RESOURCE_ALLOCATION_INFO info;
        info.SizeInBytes = pDesc->Size;
        info.Alignment = 0; // Buffers don't have alignment

        GPUResourceInitParams initParams = {};
        if (static_cast<bool>(pDesc->Flags & ResourceFlags::BreadcrumbsBuffer))
            initParams.type = GPUResourceType::BufferBreadcrumbs;
        else
        {
            initParams.heapType = D3D12_HEAP_TYPE_DEFAULT;
            initParams.type = GPUResourceType::Buffer;
            customOwner = this;
        }
        initParams.resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(info, GetDXResourceFlags(pDesc->Flags));

        // Allocate the resource using the memory allocator
        m_pResource = GPUResource::CreateGPUResource(pDesc->Name.c_str(), customOwner, initialState, &initParams, m_ResizeFn != nullptr);
        CauldronAssert(ASSERT_ERROR, m_pResource != nullptr, L"Could not create GPU resource for buffer %ls", pDesc->Name.c_str());

        InitAddressInfo();
    }

    void BufferInternal::CopyData(const void* pData, size_t size)
    {
        UploadHeap* pUploadHeap = GetUploadHeap();

        TransferInfo* pTransferInfo = pUploadHeap->BeginResourceTransfer(size, 256, 1);

        uint8_t* pMapped = pTransferInfo->DataPtr(0);
        memcpy(pMapped, pData, size);

        BufferCopyDesc desc;
        desc.GetImpl()->pSrc = pUploadHeap->GetResource()->GetImpl()->DX12Resource();
        desc.GetImpl()->pDst = m_pResource->GetImpl()->DX12Resource();
        desc.GetImpl()->SrcOffset = pMapped - pUploadHeap->BasePtr();
        desc.GetImpl()->DstOffset = 0;
        desc.GetImpl()->Size = static_cast<UINT64>(size);

        CommandList* pImmediateCmdList = GetDevice()->CreateCommandList(L"BufferCopyCmdList", CommandQueue::Copy);
        CopyBufferRegion(pImmediateCmdList, &desc);
        CloseCmdList(pImmediateCmdList);

        // Execute and sync
        std::vector<CommandList*> cmdLists;
        cmdLists.push_back(pImmediateCmdList);
        GetDevice()->ExecuteCommandListsImmediate(cmdLists, CommandQueue::Copy);

        // No longer needed, will release allocator on destruction
        delete pImmediateCmdList;

        pUploadHeap->EndResourceTransfer(pTransferInfo);
    }

    // TODO: Make signature take an actual upload context
    void BufferInternal::CopyData(const void* pData, size_t size, UploadContext* pUploadContext, ResourceState postCopyState)
    {
        CauldronAssert(ASSERT_CRITICAL, pUploadContext != nullptr, L"null upload context");

        UploadHeap* pUploadHeap = GetUploadHeap();
        TransferInfo* pTransferInfo = pUploadHeap->BeginResourceTransfer(size, 256, 1);

        // Copy the data
        uint8_t* pMapped = pTransferInfo->DataPtr(0);
        memcpy(pMapped, pData, size);

        BufferCopyDesc desc;
        desc.GetImpl()->pSrc = pUploadHeap->GetResource()->GetImpl()->DX12Resource();
        desc.GetImpl()->pDst = m_pResource->GetImpl()->DX12Resource();
        desc.GetImpl()->SrcOffset = pMapped - pUploadHeap->BasePtr();
        desc.GetImpl()->DstOffset = 0;
        desc.GetImpl()->Size = static_cast<UINT64>(size);

        // Copy
        CopyBufferRegion(pUploadContext->GetImpl()->GetCopyCmdList(), &desc);

        // Transition
        Barrier barrier = Barrier::Transition(GetResource(), ResourceState::CopyDest, postCopyState);
        ResourceBarrier(pUploadContext->GetImpl()->GetTransitionCmdList(), 1, &barrier);
    }

    BufferAddressInfo BufferInternal::GetAddressInfo() const
    {
        return addressInfo;
    }

    static CD3DX12_RESOURCE_DESC CreateResourceDesc(const BufferDesc& desc)
    {
        D3D12_RESOURCE_ALLOCATION_INFO info{};
        info.SizeInBytes = desc.Size;
        info.Alignment   = desc.Alignment;

        return CD3DX12_RESOURCE_DESC::Buffer(info, GetDXResourceFlags(desc.Flags));
    }

    void BufferInternal::Recreate()
    {
        CD3DX12_RESOURCE_DESC ResourceDesc = CreateResourceDesc(m_BufferDesc);

        // Recreate the resource
        m_pResource->GetImpl()->RecreateResource(ResourceDesc, D3D12_HEAP_TYPE_DEFAULT, m_pResource->GetCurrentResourceState());
        InitAddressInfo();
    }

    void BufferInternal::InitAddressInfo()
    {
        addressInfo                      = {};
        BufferAddressInfoInternal* pInfo = (BufferAddressInfoInternal*)(&addressInfo);
        pInfo->GPUBufferView             = m_pResource->GetImpl()->DX12Resource()->GetGPUVirtualAddress();
        pInfo->SizeInBytes               = static_cast<UINT>(m_BufferDesc.Size);
        switch (m_BufferDesc.Type)
        {
        case BufferType::Vertex:
            pInfo->StrideInBytes = static_cast<UINT>(m_BufferDesc.Stride);
            break;
        case BufferType::Index:
            pInfo->Format = GetDXGIFormat(m_BufferDesc.Format);
            break;
        case BufferType::AccelerationStructure:
            pInfo->StrideInBytes = 1;
            break;
        case BufferType::Data:
            break;

        default:
            CauldronCritical(L"Unknown buffer type");
            break;
        }
    }


} // namespace cauldron

#endif // #if defined(_DX12)
