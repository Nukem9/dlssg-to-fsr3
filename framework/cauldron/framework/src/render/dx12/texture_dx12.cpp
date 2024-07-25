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
#include "core/loaders/textureloader.h"
#include "misc/assert.h"

#include "render/dx12/device_dx12.h"
#include "render/dx12/gpuresource_dx12.h"
#include "render/dx12/texture_dx12.h"
#include "render/dx12/uploadheap_dx12.h"

#include "memoryallocator/D3D12MemAlloc.h"
#include "dxheaders/include/directx/d3dx12.h"

namespace cauldron
{
    //////////////////////////////////////////////////////////////////////////
    // Helpers

    CD3DX12_RESOURCE_DESC CreateResourceDesc(const TextureDesc& desc)
    {
        // Create a resource backed by the memory allocator
        CD3DX12_RESOURCE_DESC resourceDesc;

        switch (desc.Dimension)
        {
        case TextureDimension::Texture1D:
            resourceDesc = CD3DX12_RESOURCE_DESC::Tex1D(GetDXGIFormat(desc.Format), desc.Width, desc.DepthOrArraySize, desc.MipLevels, GetDXResourceFlags(desc.Flags));
            break;
        case TextureDimension::Texture3D:
            resourceDesc = CD3DX12_RESOURCE_DESC::Tex3D(GetDXGIFormat(desc.Format), desc.Width, desc.Height, desc.DepthOrArraySize, desc.MipLevels, GetDXResourceFlags(desc.Flags));
            break;
        case TextureDimension::CubeMap:
        case TextureDimension::Texture2D:
        default:
            resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(GetDXGIFormat(desc.Format), desc.Width, desc.Height, desc.DepthOrArraySize, desc.MipLevels, 1, 0, GetDXResourceFlags(desc.Flags));
            break;
        }

        return resourceDesc;
    }

    //////////////////////////////////////////////////////////////////////////
    // Texture

    Texture::Texture(const TextureDesc* pDesc, ResourceState initialState, ResizeFunction fn) :
        m_TextureDesc(*pDesc),
        m_ResizeFn(fn)
    {
        // Create a resource backed by the memory allocator
        GPUResourceInitParams initParams = {};
        initParams.resourceDesc = CreateResourceDesc(*pDesc);
        initParams.heapType = D3D12_HEAP_TYPE_DEFAULT;
        initParams.type = GPUResourceType::Texture;

        // Allocate the resource using the memory allocator
        m_pResource = GPUResource::CreateGPUResource(pDesc->Name.c_str(), this, initialState, &initParams, m_ResizeFn != nullptr);
        CauldronAssert(ASSERT_ERROR, m_pResource != nullptr, L"Could not create GPU resource for texture %ls", pDesc->Name.c_str());

        // Update the texture desc after creation (as some parameters can auto-generate info (i.e. mip levels)
        m_TextureDesc.MipLevels             = m_pResource->GetImpl()->DX12Desc().MipLevels;
    }

    Texture::Texture(const TextureDesc* pDesc, GPUResource* pResource) :
        m_TextureDesc(*pDesc),
        m_pResource(pResource),
        m_ResizeFn(nullptr)
    {
    }

    void Texture::CopyData(TextureDataBlock* pTextureDataBlock)
    {
        // Get mip footprints (if it is an array we reuse the mip footprints for all the elements of the array)
        UINT64 uplHeapSize;
        uint32_t numRows[D3D12_REQ_MIP_LEVELS] = { 0 };
        UINT64 rowSizesInBytes[D3D12_REQ_MIP_LEVELS] = { 0 };
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTex2D[D3D12_REQ_MIP_LEVELS];
        DXGI_FORMAT dxFormat = GetDXGIFormat(m_TextureDesc.Format);
        CD3DX12_RESOURCE_DESC rDescs = CD3DX12_RESOURCE_DESC::Tex2D(dxFormat, m_TextureDesc.Width, m_TextureDesc.Height, 1, m_TextureDesc.MipLevels);
        GetDevice()->GetImpl()->DX12Device()->GetCopyableFootprints(&rDescs, 0, m_TextureDesc.MipLevels, 0, placedTex2D, numRows, rowSizesInBytes, &uplHeapSize);

        // Compute the pixel size
        UINT32 formatStride = GetDXGIFormatStride(m_TextureDesc.Format);
        UINT32 pixelsPerBlock = 1;
        if ((dxFormat >= DXGI_FORMAT_BC1_TYPELESS) && (dxFormat <= DXGI_FORMAT_BC7_UNORM_SRGB))
        {
            pixelsPerBlock = (4 * 4);    // BC formats have 4*4 pixels per block
            pixelsPerBlock /= 4;        // We need to divide by 4 because GetCopyableFootprints introduces a *2 stride divides the rows /4 
        }

        UploadHeap* pUploadHeap = GetUploadHeap();

        // Get what we need to transfer data
        TransferInfo* pTransferInfo = pUploadHeap->BeginResourceTransfer(uplHeapSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, m_TextureDesc.DepthOrArraySize);

        std::vector<TextureCopyDesc>    copyInfoList;
        uint32_t readOffset = 0;
        for (uint32_t a = 0; a < m_TextureDesc.DepthOrArraySize; ++a)
        {
            // Get the pointer for the entries in this slice (depth slice or array entr6y)
            UINT8* pPixels = pTransferInfo->DataPtr(a);

            // Copy all the mip slices into the offsets specified by the footprint structure
            for (uint32_t mip = 0; mip < m_TextureDesc.MipLevels; ++mip)
            {
                pTextureDataBlock->CopyTextureData(pPixels + placedTex2D[mip].Offset, placedTex2D[mip].Footprint.RowPitch, (placedTex2D[mip].Footprint.Width * formatStride) / pixelsPerBlock, numRows[mip], readOffset);
                readOffset += ((numRows[mip] * placedTex2D[mip].Footprint.Width * formatStride) / pixelsPerBlock);
                
                D3D12_PLACED_SUBRESOURCE_FOOTPRINT slice = placedTex2D[mip];
                slice.Offset += (pPixels - pUploadHeap->BasePtr());
                
                TextureCopyDesc copyDesc = {};
                copyDesc.GetImpl()->Dst = CD3DX12_TEXTURE_COPY_LOCATION(m_pResource->GetImpl()->DX12Resource(), a * m_TextureDesc.MipLevels + mip);
                copyDesc.GetImpl()->Src = CD3DX12_TEXTURE_COPY_LOCATION(pUploadHeap->GetImpl()->DX12Resource(), slice);
                copyDesc.GetImpl()->pCopyBox = nullptr;

                copyInfoList.push_back(copyDesc);
            }
        }

        // Copy all immediate
        GetDevice()->ExecuteTextureResourceCopyImmediate(static_cast<int32_t>(copyInfoList.size()), copyInfoList.data());

        // Kick off the resource transfer. When we get back from here the resource is ready to be used.
        pUploadHeap->EndResourceTransfer(pTransferInfo);
    }

    void Texture::Recreate()
    {
        // Create a resource backed by the memory allocator
        CD3DX12_RESOURCE_DESC resourceDesc = CreateResourceDesc(m_TextureDesc);

        // recreate the resource
        m_pResource->GetImpl()->RecreateResource(resourceDesc, D3D12_HEAP_TYPE_DEFAULT, m_pResource->GetCurrentResourceState());
    }

    //////////////////////////////////////////////////////////////////////////
    // TextureCopyDesc
    TextureCopyDesc::TextureCopyDesc(const GPUResource* pSrc, const GPUResource* pDst, unsigned int arrayIndex, unsigned int mipLevel)
    {
        GetImpl()->pCopyBox = nullptr;

        D3D12_RESOURCE_DESC dx12Desc = pDst->GetImpl()->DX12Desc();

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT dx12Footprint = {};
        UINT                               rowCount;
        UINT64                             rowSizeInBytes;
        UINT64                             totalBytes;

        GetDevice()->GetImpl()->DX12Device()->GetCopyableFootprints(&dx12Desc, arrayIndex * dx12Desc.MipLevels + mipLevel, 1, 0, &dx12Footprint, &rowCount, &rowSizeInBytes, &totalBytes);
        D3D12_TEXTURE_COPY_LOCATION dx12DestinationLocation = {};
        dx12DestinationLocation.pResource = const_cast<ID3D12Resource*>(pDst->GetImpl()->DX12Resource());
        dx12DestinationLocation.SubresourceIndex = arrayIndex * dx12Desc.MipLevels + mipLevel;
        dx12DestinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

        D3D12_TEXTURE_COPY_LOCATION dx12SourceLocation = {};

        // If both src & dst are textures, use the right copy source
        if (pSrc->IsTexture() == pDst->IsTexture())
        {
            GetDevice()->GetImpl()->DX12Device()->GetCopyableFootprints(&pSrc->GetImpl()->DX12Desc(), arrayIndex * pSrc->GetImpl()->DX12Desc().MipLevels + mipLevel, 1, 0, &dx12Footprint, &rowCount, &rowSizeInBytes, &totalBytes);
            dx12SourceLocation.pResource = const_cast<ID3D12Resource*>(pSrc->GetImpl()->DX12Resource());
            dx12SourceLocation.SubresourceIndex = arrayIndex * pSrc->GetImpl()->DX12Desc().MipLevels + mipLevel;
            dx12SourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

            // If sizes don't match, make a copy box
            if (dx12Desc.Width != pSrc->GetImpl()->DX12Desc().Width ||
                dx12Desc.Height != pSrc->GetImpl()->DX12Desc().Height)
            {
                GetImpl()->CopyBox.top = GetImpl()->CopyBox.left = 0;
                GetImpl()->CopyBox.right = (UINT)std::min(dx12Desc.Width, pSrc->GetImpl()->DX12Desc().Width);
                GetImpl()->CopyBox.bottom = (UINT)std::min(dx12Desc.Height, pSrc->GetImpl()->DX12Desc().Height);
                GetImpl()->CopyBox.front = 0;
                GetImpl()->CopyBox.back = 1;
                GetImpl()->pCopyBox = &GetImpl()->CopyBox;
            }
        }
        else
        {
            dx12SourceLocation.pResource = const_cast<ID3D12Resource*>(pSrc->GetImpl()->DX12Resource());
            dx12SourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dx12SourceLocation.PlacedFootprint = dx12Footprint;
        }

        GetImpl()->Src = CD3DX12_TEXTURE_COPY_LOCATION(dx12SourceLocation);
        GetImpl()->Dst = CD3DX12_TEXTURE_COPY_LOCATION(dx12DestinationLocation);
    }

} // namespace cauldron

#endif // #if defined(_DX12)
