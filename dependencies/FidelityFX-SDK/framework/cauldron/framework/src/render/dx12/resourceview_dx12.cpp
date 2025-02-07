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
#include "render/dx12/resourceview_dx12.h"
#include "render/dx12/sampler_dx12.h"
#include "render/dx12/texture_dx12.h"

#include "core/framework.h"
#include "misc/assert.h"


namespace cauldron
{
    ResourceView* ResourceView::CreateResourceView(ResourceViewHeapType type, uint32_t count, void* pInitParams)
    {
        ResourceViewInitParams* pParams = (ResourceViewInitParams*)pInitParams;
        return new ResourceViewInternal(pParams->hCPUHandle, pParams->hGPUHandle, type, count, pParams->descriptorSize);
    }

    ResourceViewInternal::ResourceViewInternal(D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE hGPUHandle, ResourceViewHeapType type, uint32_t count, uint32_t descriptorSize) :
        ResourceView(type, count),
        m_hGPUHandle(hGPUHandle),
        m_hCPUHandle(hCPUHandle),
        m_DescriptorSize(descriptorSize)
    {
    }

    const ResourceViewInfo ResourceViewInternal::GetViewInfo(uint32_t index) const
    {
        CauldronAssert(ASSERT_CRITICAL, index < m_Count, L"Accessing view out of the bounds");
        ResourceViewInfo viewInfo = {};
        ResourceViewInfoInternal* pViewInfo = (ResourceViewInfoInternal*)&viewInfo;

        // Set the GPU hand (if we have one)
        if (m_hGPUHandle.ptr)
        {
            pViewInfo->hGPUHandle = GetGPUHandle(index);
        }

        // Always have a CPU handle
        pViewInfo->hCPUHandle = GetCPUHandle(index);

        return viewInfo;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ResourceViewInternal::GetCPUHandle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle = m_hCPUHandle;
        hCPUHandle.ptr += index * m_DescriptorSize;
        return hCPUHandle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE ResourceViewInternal::GetGPUHandle(uint32_t index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE hGPUHandle = m_hGPUHandle;
        hGPUHandle.ptr += index * m_DescriptorSize;
        return hGPUHandle;
    }

    void ResourceViewInternal::BindRTV(const GPUResource* pResource, const TextureDesc& textureDesc, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t index)
    {
        D3D12_RESOURCE_DESC renderTargetDesc = pResource->GetImpl()->DX12Desc();

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = ConvertTypelessDXGIFormat(GetDXGIFormat(textureDesc.Format));

        if (renderTargetDesc.SampleDesc.Count == 1)
        {
            switch (dimension)
            {
            case ViewDimension::Texture1D:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                rtvDesc.Texture1D.MipSlice = (mip == -1) ? 0 : mip;
                break;
            case ViewDimension::Texture1DArray:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                rtvDesc.Texture1DArray.MipSlice = (mip == -1) ? 0 : mip;
                rtvDesc.Texture1DArray.FirstArraySlice = (firstSlice == -1) ? 0 : firstSlice;
                rtvDesc.Texture1DArray.ArraySize = (arraySize == -1) ? renderTargetDesc.DepthOrArraySize : arraySize;
                CauldronAssert(ASSERT_ERROR, rtvDesc.Texture1DArray.FirstArraySlice + rtvDesc.Texture1DArray.ArraySize <= renderTargetDesc.DepthOrArraySize, L"The number of requested slices exceeds the number of available slices.");
                break;
            case ViewDimension::Texture2D:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = (mip == -1) ? 0 : mip;
                rtvDesc.Texture2D.PlaneSlice = 0;
                break;
            case ViewDimension::TextureCube:
            case ViewDimension::Texture2DArray:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = (mip == -1) ? 0 : mip;
                rtvDesc.Texture2DArray.FirstArraySlice = (firstSlice == -1) ? 0 : firstSlice;
                rtvDesc.Texture2DArray.ArraySize = (arraySize == -1) ? renderTargetDesc.DepthOrArraySize : arraySize;
                rtvDesc.Texture2DArray.PlaneSlice = 0;
                CauldronAssert(ASSERT_ERROR, rtvDesc.Texture2DArray.FirstArraySlice + rtvDesc.Texture2DArray.ArraySize <= renderTargetDesc.DepthOrArraySize, L"The number of requested slices exceeds the number of available slices.");
                break;
            case ViewDimension::Texture2DMS:
            case ViewDimension::Texture2DMSArray:
                CauldronCritical(L"Texture2DMS & Texture2DMSArray not yet supported. Please file an issue in git.");
                break;
            case ViewDimension::Texture3D:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
                rtvDesc.Texture3D.MipSlice = (mip == -1) ? 0 : mip;
                rtvDesc.Texture3D.FirstWSlice = (firstSlice == -1) ? 0 : firstSlice;
                rtvDesc.Texture3D.WSize = (arraySize == -1) ? renderTargetDesc.DepthOrArraySize : arraySize;
                CauldronAssert(ASSERT_ERROR, rtvDesc.Texture3D.FirstWSlice + rtvDesc.Texture3D.WSize <= renderTargetDesc.DepthOrArraySize, L"The number of requested slices exceeds the number of available slices.");
                break;
            case ViewDimension::Unknown:
            default:
                CauldronCritical(L"Invalid TextureDimension used for RTV binding");
                break;
            }
        }
        else
        {
            CauldronCritical(L"Unsupported functionality. Please implement.");
        }

        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle = GetCPUHandle(index);
        GetDevice()->GetImpl()->DX12Device()->CreateRenderTargetView(const_cast<ID3D12Resource*>(pResource->GetImpl()->DX12Resource()), &rtvDesc, hCPUHandle);
    }

    void ResourceViewInternal::BindDSV(const GPUResource* pResource, const TextureDesc& textureDesc, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t index)
    {
        D3D12_RESOURCE_DESC renderTargetDesc = pResource->GetImpl()->DX12Desc();

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Format = renderTargetDesc.Format;
        if (renderTargetDesc.SampleDesc.Count == 1)
        {
            switch (dimension)
            {
            case ViewDimension::Texture1D:
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
                dsvDesc.Texture1D.MipSlice = (mip == -1) ? 0 : mip;
                break;
            case ViewDimension::Texture1DArray:
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                dsvDesc.Texture1DArray.MipSlice = (mip == -1) ? 0 : mip;
                dsvDesc.Texture1DArray.FirstArraySlice = (firstSlice == -1) ? 0 : firstSlice;
                dsvDesc.Texture1DArray.ArraySize = (arraySize == -1) ? renderTargetDesc.DepthOrArraySize : arraySize;
                CauldronAssert(ASSERT_ERROR, dsvDesc.Texture1DArray.FirstArraySlice + dsvDesc.Texture1DArray.ArraySize <= renderTargetDesc.DepthOrArraySize, L"The number of requested slices exceeds the number of available slices.");
                break;
            case ViewDimension::Texture2D:
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsvDesc.Texture2D.MipSlice = (mip == -1) ? 0 : mip;
                break;
            case ViewDimension::Texture2DArray:
            case ViewDimension::TextureCube:
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                dsvDesc.Texture2DArray.MipSlice = (mip == -1) ? 0 : mip;
                dsvDesc.Texture2DArray.FirstArraySlice = (firstSlice == -1) ? 0 : firstSlice;
                dsvDesc.Texture2DArray.ArraySize = (arraySize == -1) ? renderTargetDesc.DepthOrArraySize : arraySize;
                CauldronAssert(ASSERT_ERROR, dsvDesc.Texture2DArray.FirstArraySlice + dsvDesc.Texture2DArray.ArraySize <= renderTargetDesc.DepthOrArraySize, L"The number of requested slices exceeds the number of available slices.");
                break;
            case ViewDimension::Texture2DMS:
            case ViewDimension::Texture2DMSArray:
                CauldronCritical(L"Texture2DMS & Texture2DMSArray not yet supported. Please file an issue in git.");
                break;
            case ViewDimension::Texture3D:
            case ViewDimension::TextureCubeArray:
            case ViewDimension::Unknown:
            default:
                CauldronCritical(L"Invalid TextureDimension used for DSV binding");
                break;
            }
        }
        else
        {
            CauldronCritical(L"Unsupported functionality. Please implement.");
        }

        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle = GetCPUHandle(index);
        GetDevice()->GetImpl()->DX12Device()->CreateDepthStencilView(const_cast<ID3D12Resource*>(pResource->GetImpl()->DX12Resource()), &dsvDesc, hCPUHandle);
    }

    void ResourceViewInternal::BindTextureSRV(const GPUResource* pResource, const TextureDesc& textureDesc, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t index)
    {
        D3D12_RESOURCE_DESC resourceDesc = pResource->GetImpl()->DX12Desc();

        // use the format from the TextureDesc to allow overriding it, e.g. for reading SRGB surfaces
        resourceDesc.Format              = ConvertTypelessDXGIFormat(GetDXGIFormat(textureDesc.Format));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

        switch (resourceDesc.Format)
        {
            case DXGI_FORMAT_D32_FLOAT:
                srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
                break;
            case DXGI_FORMAT_D16_UNORM:
                srvDesc.Format = DXGI_FORMAT_R16_UNORM;
                break;
            default:
                srvDesc.Format = resourceDesc.Format;
                break;
        }

        switch (dimension)
        {
        case ViewDimension::Texture1D:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            srvDesc.Texture1D.MostDetailedMip = (mip == -1) ? 0 : mip;
            srvDesc.Texture1D.MipLevels = (mip == -1) ? textureDesc.MipLevels : 1;
            break;
        case ViewDimension::Texture1DArray:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            srvDesc.Texture1DArray.MostDetailedMip = (mip == -1) ? 0 : mip;
            srvDesc.Texture1DArray.MipLevels = (mip == -1) ? textureDesc.MipLevels : 1;
            srvDesc.Texture1DArray.FirstArraySlice = (firstSlice == -1) ? 0 : firstSlice;
            srvDesc.Texture1DArray.ArraySize = (arraySize == -1) ? resourceDesc.DepthOrArraySize : arraySize;
            CauldronAssert(ASSERT_ERROR, srvDesc.Texture1DArray.FirstArraySlice + srvDesc.Texture1DArray.ArraySize <= resourceDesc.DepthOrArraySize, L"The number of requested slices exceeds the number of available slices.");
            break;
        case ViewDimension::Texture2D:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = (mip == -1) ? 0 : mip;
            srvDesc.Texture2D.MipLevels = (mip == -1) ? textureDesc.MipLevels : 1;
            srvDesc.Texture2D.PlaneSlice = 0;
            break;
        case ViewDimension::Texture2DArray:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MostDetailedMip = (mip == -1) ? 0 : mip;
            srvDesc.Texture2DArray.MipLevels = (mip == -1) ? textureDesc.MipLevels : 1;
            srvDesc.Texture2DArray.FirstArraySlice = (firstSlice == -1) ? 0 : firstSlice;
            srvDesc.Texture2DArray.ArraySize = (arraySize == -1) ? resourceDesc.DepthOrArraySize : arraySize;
            srvDesc.Texture2DArray.PlaneSlice = 0;
            CauldronAssert(ASSERT_ERROR, srvDesc.Texture2DArray.FirstArraySlice + srvDesc.Texture2DArray.ArraySize <= resourceDesc.DepthOrArraySize, L"The number of requested slices exceeds the number of available slices.");
            break;
        case ViewDimension::TextureCube:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MostDetailedMip = (mip == -1) ? 0 : mip;
            srvDesc.TextureCube.MipLevels = (mip == -1) ? textureDesc.MipLevels : 1;
            break;
        case ViewDimension::Texture3D:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MostDetailedMip = (mip == -1) ? 0 : mip;
            srvDesc.Texture3D.MipLevels = (mip == -1) ? textureDesc.MipLevels : 1;
            break;
        case ViewDimension::Texture2DMS:
        case ViewDimension::Texture2DMSArray:
            CauldronCritical(L"Texture2DMS & Texture2DMSArray not yet supported. Please file an issue in git.");
            break;
        case ViewDimension::TextureCubeArray:
        case ViewDimension::Unknown:
        default:
            CauldronCritical(L"Invalid TextureDimension used for Texture SRV binding");
            break;
        }

        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;


        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle = GetCPUHandle(index);
        GetDevice()->GetImpl()->DX12Device()->CreateShaderResourceView(const_cast<ID3D12Resource*>(pResource->GetImpl()->DX12Resource()), &srvDesc, hCPUHandle);
    }

    void ResourceViewInternal::BindTextureUAV(const GPUResource* pResource, const TextureDesc& textureDesc, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t index)
    {
        D3D12_RESOURCE_DESC resourceDesc = pResource->GetImpl()->DX12Desc();

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

        // Override TYPELESS resources to prevent device removal
        resourceDesc.Format = ConvertTypelessDXGIFormat(resourceDesc.Format);

        switch (resourceDesc.Format)
        {
        case DXGI_FORMAT_D32_FLOAT:
            uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
            break;
        case DXGI_FORMAT_D16_UNORM: 
            uavDesc.Format = DXGI_FORMAT_R16_UNORM;
            break;
        default:
            // sRGB format aren't allowed for UAV
            uavDesc.Format = DXGIFromGamma(resourceDesc.Format);
            break;
        }

        switch (dimension)
        {
        case ViewDimension::Texture1D:
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            uavDesc.Texture1D.MipSlice = (mip == -1) ? 0 : mip;
            break;
        case ViewDimension::Texture1DArray:
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            uavDesc.Texture1DArray.MipSlice = (mip == -1) ? 0 : mip;
            uavDesc.Texture1DArray.FirstArraySlice = (firstSlice == -1) ? 0 : firstSlice;
            uavDesc.Texture1DArray.ArraySize = (arraySize == -1) ? resourceDesc.DepthOrArraySize : arraySize;
            CauldronAssert(ASSERT_ERROR, uavDesc.Texture1DArray.FirstArraySlice + uavDesc.Texture1DArray.ArraySize <= resourceDesc.DepthOrArraySize, L"The number of requested slices exceeds the number of available slices.");
            break;
        case ViewDimension::Texture2D:
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = (mip == -1) ? 0 : mip;
            uavDesc.Texture2D.PlaneSlice = 0;
            break;
        case ViewDimension::Texture2DArray:
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.MipSlice = (mip == -1) ? 0 : mip;
            uavDesc.Texture2DArray.FirstArraySlice = (firstSlice == -1) ? 0 : firstSlice;
            uavDesc.Texture2DArray.ArraySize = (arraySize == -1) ? resourceDesc.DepthOrArraySize : arraySize;
            uavDesc.Texture2DArray.PlaneSlice = 0;
            CauldronAssert(ASSERT_ERROR, uavDesc.Texture2DArray.FirstArraySlice + uavDesc.Texture2DArray.ArraySize <= resourceDesc.DepthOrArraySize, L"The number of requested slices exceeds the number of available slices.");
            break;
        case ViewDimension::Texture3D:
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            uavDesc.Texture3D.MipSlice = (mip == -1) ? 0 : mip;
            uavDesc.Texture3D.FirstWSlice = (firstSlice == -1) ? 0 : firstSlice;
            uavDesc.Texture3D.WSize = (arraySize == -1) ? resourceDesc.DepthOrArraySize : arraySize;
            CauldronAssert(ASSERT_ERROR, uavDesc.Texture3D.FirstWSlice + uavDesc.Texture3D.WSize <= resourceDesc.DepthOrArraySize, L"The number of requested slices exceeds the number of available slices.");
            break;
        case ViewDimension::Texture2DMS:
        case ViewDimension::Texture2DMSArray:
            CauldronCritical(L"Texture2DMS & Texture2DMSArray not yet supported. Please file an issue in git.");
            break;
        case ViewDimension::TextureCube:
        case ViewDimension::TextureCubeArray:
        case ViewDimension::Unknown:
        default:
            CauldronCritical(L"Invalid TextureDimension used for Texture UAV binding");
            break;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle = GetCPUHandle(index);
        GetDevice()->GetImpl()->DX12Device()->CreateUnorderedAccessView(const_cast<ID3D12Resource*>(pResource->GetImpl()->DX12Resource()), nullptr, &uavDesc, hCPUHandle);
    }

    void ResourceViewInternal::BindBufferCBV(const GPUResource* pResource, uint32_t index)
    {
        CauldronCritical(L"Not yet implemented.");
    }

    void ResourceViewInternal::BindBufferSRV(const GPUResource* pResource, const BufferDesc& bufferDesc, uint32_t firstElement, uint32_t numElements, uint32_t index)
    {
        D3D12_RESOURCE_DESC resourceDesc = pResource->GetImpl()->DX12Desc();
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

        srvDesc.Format = resourceDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = (firstElement == -1) ? 0 : firstElement;;

        uint32_t stride = bufferDesc.Stride;
        if (bufferDesc.Type == BufferType::Index || bufferDesc.Type == BufferType::Vertex) {
            stride = 4;
        }

        srvDesc.Buffer.NumElements = (numElements == -1) ? bufferDesc.Size / stride : numElements;
        srvDesc.Buffer.StructureByteStride = stride;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle = GetCPUHandle(index);
        GetDevice()->GetImpl()->DX12Device()->CreateShaderResourceView(const_cast<ID3D12Resource*>(pResource->GetImpl()->DX12Resource()), &srvDesc, hCPUHandle);
    }

    void ResourceViewInternal::BindBufferUAV(const GPUResource* pResource, const BufferDesc& bufferDesc, uint32_t firstElement, uint32_t numElements, uint32_t index)
    {
        D3D12_RESOURCE_DESC resourceDesc = pResource->GetImpl()->DX12Desc();
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

        uavDesc.Format = resourceDesc.Format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = (firstElement == -1) ? 0 : firstElement;
        uavDesc.Buffer.NumElements = static_cast<UINT>((numElements == -1) ? bufferDesc.Size / bufferDesc.Stride : numElements);
        uavDesc.Buffer.StructureByteStride = bufferDesc.Stride;
        uavDesc.Buffer.CounterOffsetInBytes = 0;            // TODO when needed?
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;  // Support more when needed

        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle = GetCPUHandle(index);
        GetDevice()->GetImpl()->DX12Device()->CreateUnorderedAccessView(const_cast<ID3D12Resource*>(pResource->GetImpl()->DX12Resource()), nullptr, &uavDesc, hCPUHandle);
    }

    void ResourceViewInternal::BindAccelerationStructure(const GPUResource* pResource, uint32_t index)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

        srvDesc.ViewDimension                            = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping                  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = const_cast<ID3D12Resource*>(pResource->GetImpl()->DX12Resource())->GetGPUVirtualAddress();

        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle = GetCPUHandle(index);
        GetDevice()->GetImpl()->DX12Device()->CreateShaderResourceView(nullptr, &srvDesc, hCPUHandle);
    }


    void ResourceViewInternal::BindSampler(const Sampler* pSampler, uint32_t index)
    {
        D3D12_SAMPLER_DESC samplerDesc = pSampler->GetImpl()->DX12Desc();
        D3D12_CPU_DESCRIPTOR_HANDLE hCPUHandle = GetCPUHandle(index);

        GetDevice()->GetImpl()->DX12Device()->CreateSampler(&samplerDesc, hCPUHandle);
    }

    void ResourceViewInternal::BindTextureResource(const GPUResource* pResource, const TextureDesc& textureDesc, ResourceViewType type, ViewDimension dimension, int32_t mip, int32_t arraySize, int32_t firstArraySlice, uint32_t index)
    {
        // Create the view
        CauldronAssert(ASSERT_ERROR, index < m_Count, L"Texture index out of ResourceView bounds.");
        switch (type)
        {
        case ResourceViewType::TextureSRV:
            BindTextureSRV(pResource, textureDesc, dimension, mip, arraySize, firstArraySlice, index);
            break;
        case ResourceViewType::TextureUAV:
            BindTextureUAV(pResource, textureDesc, dimension, mip, arraySize, firstArraySlice, index);
            break;
        case ResourceViewType::RTV:
            CauldronAssert(ASSERT_ERROR, m_Type == ResourceViewHeapType::CPURenderView, L"Invalid view type for the heap type.");
            BindRTV(pResource, textureDesc, dimension, mip, arraySize, firstArraySlice, index);
            break;
        case ResourceViewType::DSV:
            CauldronAssert(ASSERT_ERROR, m_Type == ResourceViewHeapType::CPUDepthView, L"Invalid view type for the heap type.");
            BindDSV(pResource, textureDesc, dimension, mip, arraySize, firstArraySlice, index);
            break;
        default:
            CauldronCritical(L"Unsupported texture resource binding requested");
            break;
        }
    }

    void ResourceViewInternal::BindBufferResource(const GPUResource* pResource, const BufferDesc& bufferDesc, ResourceViewType type, uint32_t firstElement, uint32_t numElements, uint32_t index/*= 0*/)
    {
        // Create the view
        CauldronAssert(ASSERT_ERROR,
                       m_Type == ResourceViewHeapType::GPUResourceView || m_Type == ResourceViewHeapType::CPUResourceView,
                       L"Invalid view type for the heap type.");
        CauldronAssert(ASSERT_ERROR, index < m_Count, L"Buffer index out of ResourceView bounds.");

        switch (type)
        {
        case ResourceViewType::CBV:
            BindBufferCBV(pResource, index);
            break;
        case ResourceViewType::BufferSRV:
            if (bufferDesc.Type == BufferType::AccelerationStructure)
                BindAccelerationStructure(pResource, index);
            else
                BindBufferSRV(pResource, bufferDesc, firstElement, numElements, index);
            break;
        case ResourceViewType::BufferUAV:
            BindBufferUAV(pResource, bufferDesc, firstElement, numElements, index);
            break;
        default:
            CauldronCritical(L"Unsupported buffer resource binding requested");
            break;
        }
    }

    void ResourceViewInternal::BindSamplerResource(const Sampler* pSampler, uint32_t index/*=0*/)
    {
        // Create the view
        CauldronAssert(ASSERT_ERROR, m_Type == ResourceViewHeapType::GPUSamplerView, L"Invalid view type for the heap type.");
        CauldronAssert(ASSERT_ERROR, index < m_Count, L"Buffer index out of ResourceView bounds.");
        BindSampler(pSampler, index);
    }

} // namespace cauldron

#endif // #if defined(_DX12)
