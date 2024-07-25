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
#include "render/dx12/copyresource_dx12.h"
#include "render/dx12/device_dx12.h"
#include "render/dx12/gpuresource_dx12.h"

#include "core/framework.h"
#include "misc/assert.h"

#include "dxheaders/include/directx/d3dx12.h"

namespace cauldron
{
    CopyResource* CopyResource::CreateCopyResource(const GPUResource* pDest, const SourceData* pSrc, ResourceState initialState)
    {
        return new CopyResourceInternal(pDest, pSrc, initialState);
    }

    CopyResourceInternal::CopyResourceInternal(const GPUResource* pDest, const SourceData* pSrc, ResourceState initialState):
        CopyResource(pSrc)
    {
        // The resource description of the resource we'll copy to
        D3D12_RESOURCE_DESC dx12ResourceDesc = pDest->GetImpl()->DX12Desc();

        // Get the resource footprint
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT dx12Footprint = {};
        UINT   rowCount;
        UINT64 rowSizeInBytes;
        UINT64 totalBytes;
        GetDevice()->GetImpl()->DX12Device()->GetCopyableFootprints(&dx12ResourceDesc, 0, 1, 0, &dx12Footprint, &rowCount, &rowSizeInBytes, &totalBytes);

        // strip all usage as it's just for copying
        dx12ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        // Create a description for the copy resource
        D3D12_RESOURCE_DESC dx12UploadBufferDesc = {};
        dx12UploadBufferDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
        dx12UploadBufferDesc.Width               = totalBytes;
        dx12UploadBufferDesc.Height              = 1;
        dx12UploadBufferDesc.DepthOrArraySize    = 1;
        dx12UploadBufferDesc.MipLevels           = 1;
        dx12UploadBufferDesc.Format              = DXGI_FORMAT_UNKNOWN;
        dx12UploadBufferDesc.SampleDesc.Count    = 1;
        dx12UploadBufferDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        // Allocate the resource using the memory allocator
        std::wstring name = pDest->GetName();
        name += L"_CopyResource";

        GPUResourceInitParams initParams = {};
        initParams.heapType = D3D12_HEAP_TYPE_UPLOAD;
        initParams.resourceDesc = dx12UploadBufferDesc;
        initParams.type = GPUResourceType::Buffer;

        m_pResource = GPUResource::CreateGPUResource(name.c_str(), this, initialState, &initParams);
        CauldronAssert(ASSERT_ERROR, m_pResource != nullptr, L"Could not create copy resource for resource %ls", pDest->GetName());

        // Copy the data in
        D3D12_RANGE emptyRange = {};
        void*       pDestData  = nullptr;
        CauldronThrowOnFail(m_pResource->GetImpl()->DX12Resource()->Map(0, &emptyRange, &pDestData));

        const uint8_t* src = static_cast<uint8_t*>(pSrc->buffer);
        uint8_t*       dst = static_cast<uint8_t*>(pDestData);
        for (uint32_t currentRowIndex = 0; currentRowIndex < dx12ResourceDesc.Height; ++currentRowIndex)
        {
            if (pSrc->type == SourceData::Type::BUFFER)
            {
                memcpy(dst, src, rowSizeInBytes);
                src += rowSizeInBytes;
            }
            else if (pSrc->type == SourceData::Type::VALUE)
                memset(dst, pSrc->value, rowSizeInBytes);
            else
            {
                CauldronCritical(L"Invalid type of source data");
            }
            dst += dx12Footprint.Footprint.RowPitch;
        }
        m_pResource->GetImpl()->DX12Resource()->Unmap(0, nullptr);
    }

} // namespace cauldron

#endif // #if defined(_DX12)
