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
#include "render/dx12/indirectworkload_dx12.h"
#include "render/dx12/resourceview_dx12.h"
#include "render/dx12/pipelineobject_dx12.h"
#include "render/dx12/resourceviewallocator_dx12.h"
#include "render/dx12/texture_dx12.h"
#include "render/dx12/uploadheap_dx12.h"
#include "render/rasterview.h"

#include "dxheaders/include/directx/d3dx12.h"

namespace cauldron
{
    CommandList* CommandList::CreateCommandList(const wchar_t* name, CommandQueue queueType, void* pInitParams)
    {
        CommandListInitParams* pParams = (CommandListInitParams*)pInitParams;
        return new CommandListInternal(name, pParams->pCmdList, pParams->pCmdAllocator, queueType);
    }

    CommandList* CommandList::GetWrappedCmdListFromSDK(const wchar_t* name, CommandQueue queueType, void* pSDKCmdList)
    {
        return new CommandListInternal(name, (ID3D12GraphicsCommandList2*)pSDKCmdList, nullptr, queueType);
    }

    void CommandList::ReleaseWrappedCmdList(CommandList* pCmdList)
    {
        delete pCmdList;
    }

    CommandListInternal::CommandListInternal(const wchar_t* name, MSComPtr<ID3D12GraphicsCommandList2> pCmdList, MSComPtr<ID3D12CommandAllocator> pCmdAllocator, CommandQueue queueType) :
        CommandList(queueType),
        m_pCommandList(pCmdList), m_pCmdAllocator(pCmdAllocator)
    {
        pCmdList->SetName(name);
    }

    CommandListInternal::~CommandListInternal()
    {
        // Release command allocator on delete
        if (m_pCmdAllocator)
            GetDevice()->GetImpl()->ReleaseCommandAllocator(this);
    }

    //////////////////////////////////////////////////////////////////////////
    // UploadContext

    UploadContext* UploadContext::CreateUploadContext()
    {
        return new UploadContextInternal();
    }

    UploadContextInternal::UploadContextInternal() :
        UploadContext()
    {
        m_pCopyCmdList = GetDevice()->CreateCommandList(L"ImmediateCopyCommandList", CommandQueue::Copy);
        m_pTransitionCmdList = GetDevice()->CreateCommandList(L"ImmediateGraphicsCommandList", CommandQueue::Graphics);
    }

    UploadContextInternal::~UploadContextInternal()
    {
        delete m_pCopyCmdList;
        delete m_pTransitionCmdList;
    }

    void UploadContextInternal::Execute()
    {
        // Close cmd lists
        CloseCmdList(m_pCopyCmdList);
        CloseCmdList(m_pTransitionCmdList);

        // Execute and sync
        std::vector<CommandList*> copyList = { m_pCopyCmdList };
        GetDevice()->ExecuteCommandListsImmediate(copyList, CommandQueue::Copy);

        // Do all resource transitions
        std::vector<CommandList*> transitionList = { m_pTransitionCmdList };
        GetDevice()->ExecuteCommandListsImmediate(transitionList, CommandQueue::Graphics);
    }

    //////////////////////////////////////////////////////////////////////////
    // CommandList calling functions

    // Binds both CBV_SRV_UAV heap and Sample heap
    void SetAllResourceViewHeaps(CommandList* pCmdList, ResourceViewAllocator* pAllocator /*=nullptr*/)
    {
        // If no allocator was provided, use the framework's
        std::vector<ID3D12DescriptorHeap*> heapList;
        if (pAllocator)
        {
            heapList.push_back(pAllocator->GetImpl()->DX12DescriptorHeap(ResourceViewHeapType::GPUResourceView));
            heapList.push_back(pAllocator->GetImpl()->DX12DescriptorHeap(ResourceViewHeapType::GPUSamplerView));
        }
        else
        {
            heapList.push_back(GetResourceViewAllocator()->GetImpl()->DX12DescriptorHeap(ResourceViewHeapType::GPUResourceView));
            heapList.push_back(GetResourceViewAllocator()->GetImpl()->DX12DescriptorHeap(ResourceViewHeapType::GPUSamplerView));
        }
        pCmdList->GetImpl()->DX12CmdList()->SetDescriptorHeaps((UINT)heapList.size(), heapList.data());
    }

    void CloseCmdList(CommandList* pCmdList)
    {
        pCmdList->GetImpl()->DX12CmdList()->Close();
    }

    void ResourceBarrier(CommandList* pCmdList, uint32_t barrierCount, const Barrier* pBarriers)
    {
        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        for (uint32_t i = 0; i < barrierCount; ++i)
        {
            const Barrier barrier = pBarriers[i];
            switch (barrier.Type)
            {
            case BarrierType::Transition:
            {
                CauldronAssert(ASSERT_CRITICAL, barrier.SourceState == barrier.pResource->GetCurrentResourceState(barrier.SubResource), L"ResourceBarrier::Error : ResourceState and Barrier.SourceState do not match.");
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(const_cast<ID3D12Resource*>(barrier.pResource->GetImpl()->DX12Resource()), GetDXResourceState(barrier.SourceState), GetDXResourceState(barrier.DestState),
                                                                        (barrier.SubResource == 0xffffffff) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : barrier.SubResource));
                // Set the new internal state (this is largely used for debugging)
                const_cast<GPUResource*>(barrier.pResource)->SetCurrentResourceState(barrier.DestState);
                break;
            }
            case BarrierType::UAV:
            {
                // Resource is expected to be in UAV state
                CauldronAssert(ASSERT_CRITICAL,
                               ResourceState::UnorderedAccess == barrier.pResource->GetCurrentResourceState(barrier.SubResource) ||
                                   ResourceState::RTAccelerationStruct == barrier.pResource->GetCurrentResourceState(barrier.SubResource),
                    L"ResourceBarrier::Error : ResourceState and Barrier.SourceState do not match.");
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(const_cast<ID3D12Resource*>(barrier.pResource->GetImpl()->DX12Resource())));
                break;
            }
            default:
                CauldronError(L"Aliasing barrier requested but not yet supported! Please implement!");
                break;
            }
        }

        if (barriers.size() > 0)
            pCmdList->GetImpl()->DX12CmdList()->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    void CopyTextureRegion(CommandList* pCmdList, const TextureCopyDesc* pCopyDesc)
    {
        pCmdList->GetImpl()->DX12CmdList()->CopyTextureRegion(&pCopyDesc->GetImpl()->Dst, 0, 0, 0, &pCopyDesc->GetImpl()->Src, pCopyDesc->GetImpl()->pCopyBox);
    }

    void CopyBufferRegion(CommandList* pCmdList, const BufferCopyDesc* pCopyDesc)
    {
        pCmdList->GetImpl()->DX12CmdList()->CopyBufferRegion(pCopyDesc->GetImpl()->pDst, pCopyDesc->GetImpl()->DstOffset, pCopyDesc->GetImpl()->pSrc, pCopyDesc->GetImpl()->SrcOffset, pCopyDesc->GetImpl()->Size);
    }

    void ClearRenderTarget(CommandList* pCmdList, const ResourceViewInfo* pRendertargetView, const float clearColor[4])
    {
        pCmdList->GetImpl()->DX12CmdList()->ClearRenderTargetView(pRendertargetView->GetImpl()->hCPUHandle, clearColor, 0, nullptr);
    }

    void ClearDepthStencil(CommandList* pCmdList, const ResourceViewInfo* pDepthStencilView, uint8_t stencilValue)
    {
        static bool s_InvertedDepth = GetConfig()->InvertedDepth;
        pCmdList->GetImpl()->DX12CmdList()->ClearDepthStencilView(
            pDepthStencilView->GetImpl()->hCPUHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            s_InvertedDepth ? 0.0f : 1.0f,
            stencilValue,
            0, nullptr);
    }

    void ClearUAVFloat(CommandList* pCmdList, const GPUResource* pResource, const ResourceViewInfo* pGPUView, const ResourceViewInfo* pCPUView, float clearColor[4])
    {
        pCmdList->GetImpl()->DX12CmdList()->ClearUnorderedAccessViewFloat(pGPUView->GetImpl()->hGPUHandle, pCPUView->GetImpl()->hCPUHandle, const_cast<GPUResource*>(pResource)->GetImpl()->DX12Resource(), clearColor, 0, nullptr);
    }

    void ClearUAVUInt(CommandList* pCmdList, const GPUResource* pResource, const ResourceViewInfo* pGPUView, const ResourceViewInfo* pCPUView, uint32_t clearColor[4])
    {
        pCmdList->GetImpl()->DX12CmdList()->ClearUnorderedAccessViewUint(pGPUView->GetImpl()->hGPUHandle, pCPUView->GetImpl()->hCPUHandle, const_cast<GPUResource*>(pResource)->GetImpl()->DX12Resource(), clearColor, 0, nullptr);
    }

    void SetPipelineState(CommandList* pCmdList, PipelineObject* pPipeline)
    {
        pCmdList->GetImpl()->DX12CmdList()->SetPipelineState(pPipeline->GetImpl()->DX12PipelineState());
    }

    void SetPrimitiveTopology(CommandList* pCmdList, PrimitiveTopology topology)
    {
        D3D_PRIMITIVE_TOPOLOGY d3dTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        switch (topology)
        {
        case PrimitiveTopology::PointList:
            d3dTopology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            break;
        case PrimitiveTopology::LineList:
            d3dTopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case PrimitiveTopology::TriangleStrip:
            d3dTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        default:
            d3dTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        }
        pCmdList->GetImpl()->DX12CmdList()->IASetPrimitiveTopology(d3dTopology);
    }

    void SetVertexBuffers(CommandList* pCmdList, uint32_t startSlot, uint32_t numBuffers, BufferAddressInfo* pVertexBufferView)
    {
        D3D12_VERTEX_BUFFER_VIEW views[8];
        for (uint32_t i = 0; i < numBuffers; ++i)
        {
            views[i].BufferLocation = pVertexBufferView[i].GetImpl()->GPUBufferView;
            views[i].SizeInBytes    = pVertexBufferView[i].GetImpl()->SizeInBytes;
            views[i].StrideInBytes  = pVertexBufferView[i].GetImpl()->StrideInBytes;
        }
        pCmdList->GetImpl()->DX12CmdList()->IASetVertexBuffers(startSlot, numBuffers, views);
    }

    void SetIndexBuffer(CommandList* pCmdList, BufferAddressInfo* pIndexBufferView)
    {
        D3D12_INDEX_BUFFER_VIEW view;
        view.BufferLocation = pIndexBufferView->GetImpl()->GPUBufferView;
        view.SizeInBytes    = pIndexBufferView->GetImpl()->SizeInBytes;
        view.Format         = pIndexBufferView->GetImpl()->Format;
        pCmdList->GetImpl()->DX12CmdList()->IASetIndexBuffer(&view);
    }

    void SetRenderTargets(CommandList* pCmdList, uint32_t numRasterViews, const ResourceViewInfo* pRasterViews, const ResourceViewInfo* pDepthView /*= nullptr*/)
    {
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtDescriptorHandles;
        const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthDescriptorHandle = pDepthView ? &pDepthView->GetImpl()->hCPUHandle : nullptr;
        for (uint32_t i = 0; i < numRasterViews; ++i)
            rtDescriptorHandles.push_back(pRasterViews[i].GetImpl()->hCPUHandle);

        pCmdList->GetImpl()->DX12CmdList()->OMSetRenderTargets(numRasterViews, rtDescriptorHandles.data(), false, pDepthDescriptorHandle);
    }

    void BeginRaster(CommandList*                   pCmdList,
                     uint32_t                       numRasterViews,
                     const RasterView**             pRasterViews,
                     const RasterView*              pDepthView,
                     const VariableShadingRateInfo* pVrsInfo)
    {
        CauldronAssert(ASSERT_WARNING, !pCmdList->GetRastering(), L"Calling BeginRaster before previous EndRaster. Strangeness or crashes may occur.");

        if (pVrsInfo != nullptr)
        {
            pCmdList->BeginVRSRendering(pVrsInfo);
        }

        std::vector<ResourceViewInfo> rasterViews;
        for (uint32_t i = 0; i < numRasterViews; ++i)
            rasterViews.push_back(pRasterViews[i]->GetResourceView());

        SetRenderTargets(pCmdList, numRasterViews, rasterViews.data(), pDepthView? &pDepthView->GetResourceView() : nullptr);

        // Flag that we are currently doing raster ops
        pCmdList->SetRastering(true);
    }

     void BeginRaster(CommandList*                  pCmdList,
                     uint32_t                       numColorViews,
                     const ResourceViewInfo*        pColorViews,
                     const ResourceViewInfo*        pDepthView,
                     const VariableShadingRateInfo* pVrsInfo)
    {
        CauldronAssert(ASSERT_WARNING, !pCmdList->GetRastering(), L"Calling BeginRaster before previous EndRaster. Strangeness or crashes may occur.");

        if (pVrsInfo != nullptr)
        {
            pCmdList->BeginVRSRendering(pVrsInfo);
        }

        std::vector<ResourceViewInfo> rasterViews;
        for (uint32_t i = 0; i < numColorViews; ++i)
            rasterViews.push_back(pColorViews[i]);

        SetRenderTargets(pCmdList, numColorViews, rasterViews.data(), pDepthView);

        // Flag that we are currently doing raster ops
        pCmdList->SetRastering(true);
    }

    void EndRaster(CommandList* pCmdList, const VariableShadingRateInfo* pVrsInfo)
    {
        // Done with raster ops
        pCmdList->SetRastering(false);

        if (pVrsInfo != nullptr)
        {
            pCmdList->EndVRSRendering(pVrsInfo);
        }

    }

    void SetViewport(CommandList* pCmdList, Viewport* pViewport)
    {
        D3D12_VIEWPORT vp = { pViewport->X, pViewport->Y, pViewport->Width, pViewport->Height, pViewport->MinDepth, pViewport->MaxDepth };
        pCmdList->GetImpl()->DX12CmdList()->RSSetViewports(1, &vp);
    }

    void SetScissorRects(CommandList* pCmdList, uint32_t numRects, Rect* pRectList)
    {
        CauldronAssert(ASSERT_ERROR, numRects <= 8, L"Cannot set more than 8 scissors sets");
        D3D12_RECT ScissorRects[8];
        for (uint32_t i = 0; i < numRects; ++i)
            ScissorRects[i] = { (LONG)pRectList[i].Left, (LONG)pRectList[i].Top, (LONG)pRectList[i].Right, (LONG)pRectList[i].Bottom };

        pCmdList->GetImpl()->DX12CmdList()->RSSetScissorRects(numRects, ScissorRects);
    }

    void SetViewportScissorRect(CommandList* pCmdList, uint32_t left, uint32_t top, uint32_t width, uint32_t height, float nearDist, float farDist)
    {
        Viewport vp = {static_cast<float>(left), static_cast<float>(top), static_cast<float>(width), static_cast<float>(height), nearDist, farDist};
        SetViewport(pCmdList, &vp);
        Rect scissorRect = {0, 0, width, height};
        SetScissorRects(pCmdList, 1, &scissorRect);
    }

    void DrawInstanced(CommandList* pCmdList, uint32_t vertexCountPerInstance, uint32_t instanceCount/*=1*/, uint32_t startVertex/*=0*/, uint32_t startInstance/*=0*/)
    {
        pCmdList->GetImpl()->DX12CmdList()->DrawInstanced(vertexCountPerInstance, instanceCount, startVertex, startInstance);
    }

    void DrawIndexedInstanced(CommandList* pCmdList, uint32_t indexCountPerInstance, uint32_t instanceCount/*=1*/, uint32_t startIndex/*=0*/, uint32_t baseVertex/*=0*/, uint32_t startInstance/*=0*/)
    {
        pCmdList->GetImpl()->DX12CmdList()->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndex, baseVertex, startInstance);
    }

    void ExecuteIndirect(CommandList* pCmdList, IndirectWorkload* pIndirectWorkload, const Buffer* pArgumentBuffer, uint32_t drawCount, uint32_t offset)
    {
        pCmdList->GetImpl()->DX12CmdList()->ExecuteIndirect(static_cast<IndirectWorkloadInternal*>(pIndirectWorkload)->m_pCommandSignature.Get(), drawCount, (ID3D12Resource*)(pArgumentBuffer->GetResource()->GetImpl()->DX12Resource()), offset, nullptr, 0);
    }

    void Dispatch(CommandList* pCmdList, uint32_t numGroupX, uint32_t numGroupY, uint32_t numGroupZ)
    {
        CauldronAssert(ASSERT_CRITICAL, numGroupX && numGroupY && numGroupZ, L"One of the dispatch group sizes is 0. Please ensure at least 1 group per dispatch dimension.");
        pCmdList->GetImpl()->DX12CmdList()->Dispatch(numGroupX, numGroupY, numGroupZ);
    }

    void WriteBufferImmediate(CommandList* pCmdList, const GPUResource* pResource, uint32_t numParams, const uint32_t* offsets, const uint32_t* values)
    {
        std::vector<D3D12_WRITEBUFFERIMMEDIATE_PARAMETER> params(numParams);
        for (uint32_t i = 0; i < numParams; ++i)
            params[i] = { const_cast<GPUResource*>(pResource)->GetImpl()->DX12Resource()->GetGPUVirtualAddress() + offsets[i], values[i] };
        pCmdList->GetImpl()->DX12CmdList()->WriteBufferImmediate(numParams, params.data(), nullptr);
    }

    void WriteBreadcrumbsMarker(Device* pDevice, CommandList* pCmdList, Buffer* pBuffer, uint64_t gpuAddress, uint32_t value, bool isBegin)
    {
        const D3D12_WRITEBUFFERIMMEDIATE_MODE mode = isBegin ? D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_IN : D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_OUT;
        const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER params = { gpuAddress, value };
        pCmdList->GetImpl()->DX12CmdList()->WriteBufferImmediate(1, &params, &mode);
    }

    static D3D12_SHADING_RATE_COMBINER GetDXShadingRateCombiner(ShadingRateCombiner combiner)
    {
        D3D12_SHADING_RATE_COMBINER d3d12Combiner;
        switch (combiner)
        {
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Passthrough:
            d3d12Combiner = D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
            break;
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Override:
            d3d12Combiner = D3D12_SHADING_RATE_COMBINER_OVERRIDE;
            break;
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Min:
            d3d12Combiner = D3D12_SHADING_RATE_COMBINER_MIN;
            break;
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Max:
            d3d12Combiner = D3D12_SHADING_RATE_COMBINER_MAX;
            break;
        case cauldron::ShadingRateCombiner::ShadingRateCombiner_Sum:
            d3d12Combiner = D3D12_SHADING_RATE_COMBINER_SUM;
            break;
        default:
            CauldronCritical(L"Unknown shading rate combiner!");
            break;
        }

        return d3d12Combiner;
    }

    void SetShadingRate(CommandList* pCmdList, const ShadingRate shadingRate, const ShadingRateCombiner* combiners, const GPUResource* pShadingRateImage)
    {
        ID3D12GraphicsCommandList5* pCommandList5;
        CauldronThrowOnFail(pCmdList->GetImpl()->DX12CmdList()->QueryInterface(__uuidof(ID3D12GraphicsCommandList5), (void**)&pCommandList5));

        D3D12_SHADING_RATE baseShadingRate = D3D12_SHADING_RATE_1X1;

        switch (shadingRate)
        {
        case cauldron::ShadingRate::ShadingRate_1X1:
            baseShadingRate = D3D12_SHADING_RATE_1X1;
            break;
        case cauldron::ShadingRate::ShadingRate_1X2:
            baseShadingRate = D3D12_SHADING_RATE_1X2;
            break;
        case cauldron::ShadingRate::ShadingRate_2X1:
            baseShadingRate = D3D12_SHADING_RATE_2X1;
            break;
        case cauldron::ShadingRate::ShadingRate_2X2:
            baseShadingRate = D3D12_SHADING_RATE_2X2;
            break;
        case cauldron::ShadingRate::ShadingRate_2X4:
            baseShadingRate = D3D12_SHADING_RATE_2X4;
            break;
        case cauldron::ShadingRate::ShadingRate_4X2:
            baseShadingRate = D3D12_SHADING_RATE_4X2;
            break;
        case cauldron::ShadingRate::ShadingRate_4X4:
            baseShadingRate = D3D12_SHADING_RATE_4X4;
            break;
        default:
            CauldronCritical(L"Unknown base shading rate!");
            break;
        }

        D3D12_SHADING_RATE_COMBINER d3d12Combiners[D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT];
        d3d12Combiners[0] = GetDXShadingRateCombiner(combiners[0]);
        d3d12Combiners[1] = GetDXShadingRateCombiner(combiners[1]);

        pCommandList5->RSSetShadingRate(baseShadingRate, d3d12Combiners);

        if (pShadingRateImage != nullptr)
            pCommandList5->RSSetShadingRateImage(const_cast<ID3D12Resource*>(pShadingRateImage->GetImpl()->DX12Resource()));
        else
            pCommandList5->RSSetShadingRateImage(nullptr);

        pCommandList5->Release();
    }
} // namespace cauldron

#endif // #if defined(_DX12)
