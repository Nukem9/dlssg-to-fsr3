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

#include "misc/helpers.h"
#include "render/renderdefines.h"
#include <vector>

// Define cauldron type
namespace cauldron
{
    struct Barrier;
    struct TextureCopyDesc;
    struct BufferCopyDesc;
    struct BufferAddressInfo;
    class CommandListInternal;
    struct ResourceViewInfo;
    class PipelineObject;
    class RasterView;
    class GPUResource;
    class ResourceViewAllocator;
    struct VariableShadingRateInfo;
    class Buffer;
    class IndirectWorkload;
    struct TransferInfo;
    class UploadContextInternal;
    class Device;

    /**
     * @class CommandList
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of a command list.
     *
     * @ingroup CauldronRender
     */
    class CommandList
    {
    public:

        /**
         * @brief   Destruction with default behavior.
         */
        virtual ~CommandList() = default;

        /**
         * @brief   CommandList instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static CommandList* CreateCommandList(const wchar_t* Name, CommandQueue queueType, void* pInitParams);

        /**
         * @brief   Creates a framework command list from a FidelityFX SDK command list. This resource must be
         *          manually destroyed after use by the caller.
         */
        static CommandList* GetWrappedCmdListFromSDK(const wchar_t* name, CommandQueue queueType, void* pSDKCmdList);

        /**
         * @brief   Releases sdk-backed command list.
         */
        static void ReleaseWrappedCmdList(CommandList* pCmdList);

        /**
         * @brief   Query whether we are currently between Begin/End Raster.
         */
        const bool GetRastering() const { return m_Rastering; }

        /**
         * @brief   Tells the CommandList we are doing rasterization work.
         */
        void SetRastering(bool state) { m_Rastering = state; }

        /**
         * @brief   Get the CommandQueue type for this CommandList.
         */
        CommandQueue GetQueueType() const { return m_QueueType; }

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        virtual const CommandListInternal* GetImpl() const = 0;
        virtual CommandListInternal* GetImpl() = 0;

    private:
        // No copy, No move
        NO_COPY(CommandList)
        NO_MOVE(CommandList)

        friend void BeginRaster(CommandList* pCmdList, uint32_t numRasterViews, const RasterView** pRasterViews,
                                const RasterView* pDepthView, const VariableShadingRateInfo* pVrsInfo);
        friend void BeginRaster(CommandList*                   pCmdList,
                                uint32_t                       numColorViews,
                                const ResourceViewInfo*        pColorViews,
                                const ResourceViewInfo*        pDepthView,
                                const VariableShadingRateInfo* pVrsInfo);
        void BeginVRSRendering(const VariableShadingRateInfo* pVrsInfo);

        friend void EndRaster(CommandList* pCmdList, const VariableShadingRateInfo* pVrsInfo);
        void EndVRSRendering(const VariableShadingRateInfo* pVrsInfo);
    
    protected:
        CommandList(CommandQueue queueType);
                    
        CommandQueue                                        m_QueueType = CommandQueue::Graphics;
        bool                                                m_Rastering = false;
    };


   /**
   * @class UploadContext
   *
   * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of an upload context. 
   * Used to transfer asset data from CPU memory to GPU memory via the copy queue.
   *
   * @ingroup CauldronRender
   */
    class UploadContext
    {
    public:

        /**
         * @brief   UploadContext instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static UploadContext* CreateUploadContext();
        
        /**
         * @brief   Destruction.
         */
        virtual ~UploadContext();

        /**
         * @brief   Executes batched GPU resource copies.
         */
        virtual void Execute() = 0;

        /**
         * @brief   Adds TransferInfo data to the queue of resource copies to execute.
         */
        void AppendTransferInfo(TransferInfo* pTransferInfo) { m_TransferInfos.push_back(pTransferInfo); }

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        virtual const UploadContextInternal* GetImpl() const = 0;
        virtual UploadContextInternal* GetImpl() = 0;

    private:

        // No copy, No move
        NO_COPY(UploadContext)
        NO_MOVE(UploadContext)

    protected:

        UploadContext() = default;

        std::vector<TransferInfo*> m_TransferInfos        = {};
    };

    /// Set all resource view heaps to the CommandList.
    ///
    /// @ingroup CauldronRender
    void SetAllResourceViewHeaps(CommandList* pCmdList, ResourceViewAllocator* pAllocator = nullptr);

    /// Closes the CommandList.
    ///
    /// @ingroup CauldronRender
    void CloseCmdList(CommandList* pCmdList);

    /// Submits 1 (or more) resource barriers.
    ///
    /// @ingroup CauldronRender
    void ResourceBarrier(CommandList* pCmdList, uint32_t barrierCount, const Barrier* pBarriers);

    /// Copy texture data from one resource to another.
    ///
    /// @ingroup CauldronRender
    void CopyTextureRegion(CommandList* pCmdList, const TextureCopyDesc* pCopyDesc);

    /// Copy buffer data from one resource to another.
    ///
    /// @ingroup CauldronRender
    void CopyBufferRegion(CommandList* pCmdList, const BufferCopyDesc* pCopyDesc);

    /// Clears a render target with the specified value.
    ///
    /// @ingroup CauldronRender
    void ClearRenderTarget(CommandList* pCmdList, const ResourceViewInfo* pRendertargetView, const float clearColor[4]);

    /// Clears a depth(/stencil) target with the specified value.
    ///
    /// @ingroup CauldronRender
    void ClearDepthStencil(CommandList* pCmdList, const ResourceViewInfo* pDepthStencilView, uint8_t stencilValue);

    /// Clears a resource with the specified value.
    ///
    /// @ingroup CauldronRender
    void ClearUAVFloat(CommandList* pCmdList, const GPUResource* pResource, const ResourceViewInfo* pGPUView, const ResourceViewInfo* pCPUView, float clearColor[4]);

    /// Clears a resource with the specified value.
    ///
    /// @ingroup CauldronRender
    void ClearUAVUInt(CommandList* pCmdList, const GPUResource* pResource, const ResourceViewInfo* pGPUView, const ResourceViewInfo* pCPUView, uint32_t clearColor[4]);

    /// Begins rasterization workload submission to the CommandList
    ///
    /// @ingroup CauldronRender
    void BeginRaster(CommandList*                   pCmdList,
                     uint32_t                       numRasterViews,
                     const RasterView**             pRasterViews,
                     const RasterView*              pDepthView = nullptr,
                     const VariableShadingRateInfo* pVrsInfo   = nullptr);
    
    /// Begins rasterization workload submission to the CommandList
    ///
    /// @ingroup CauldronRender
    void BeginRaster(CommandList*                   pCmdList,
                     uint32_t                       numColorViews,
                     const ResourceViewInfo*        pColorViews,
                     const ResourceViewInfo*        pDepthView = nullptr,
                     const VariableShadingRateInfo* pVrsInfo   = nullptr);

    /// Binds the rendertarget/depth views to the GPU for rendering
    ///
    /// @ingroup CauldronRender
    void SetRenderTargets(CommandList* pCmdList, uint32_t numRasterViews, const ResourceViewInfo* pRasterViews, const ResourceViewInfo* pDepthView = nullptr);

    /// Ends rasterization workload submissions to the CommandList
    ///
    /// @ingroup CauldronRender
    void EndRaster(CommandList* pCmdList, const VariableShadingRateInfo* pVrsInfo = nullptr);

    /// Sets a viewport for rasterization work
    ///
    /// @ingroup CauldronRender
    void SetViewport(CommandList* pCmdList, Viewport* pViewport);

    /// Sets the scissor rect for rasterization work
    ///
    /// @ingroup CauldronRender
    void SetScissorRects(CommandList* pCmdList, uint32_t numRects, Rect* pRectList);
   
    /// Convenience function to set both viewport and scissor rect through a single call
    ///
    /// @ingroup CauldronRender
    void SetViewportScissorRect(CommandList* pCmdList, uint32_t left, uint32_t top, uint32_t width, uint32_t height, float nearDist, float farDist);

    /// Set the pipeline object to use for Draw/Dispatch
    ///
    /// @ingroup CauldronRender
    void SetPipelineState(CommandList* pCmdList, PipelineObject* pPipeline);

    /// Sets the primitive topology to use on rasterization workloads
    ///
    /// @ingroup CauldronRender
    void SetPrimitiveTopology(CommandList* pCmdList, PrimitiveTopology topology);

    /// Sets a vertex buffer for rasterization work
    ///
    /// @ingroup CauldronRender
    void SetVertexBuffers(CommandList* pCmdList, uint32_t startSlot, uint32_t numBuffers, BufferAddressInfo* pVertexBufferView);

    /// Sets an index buffer for rasterization work
    ///
    /// @ingroup CauldronRender
    void SetIndexBuffer(CommandList* pCmdList, BufferAddressInfo* pIndexBufferView);

    /// Instanced draw function
    ///
    /// @ingroup CauldronRender
    void DrawInstanced(CommandList* pCmdList, uint32_t vertexCountPerInstance, uint32_t instanceCount = 1, uint32_t startVertex = 0, uint32_t startInstance = 0);

    /// Instanced drawindexed function
    ///
    /// @ingroup CauldronRender
    void DrawIndexedInstanced(CommandList* pCmdList, uint32_t indexCountPerInstance, uint32_t instanceCount = 1, uint32_t startIndex = 0, uint32_t baseVertex = 0, uint32_t startInstance = 0);

    /// Executes indirect workloads
    ///
    /// @ingroup CauldronRender
    void ExecuteIndirect(CommandList* pCmdList, IndirectWorkload* pIndirectWorkload, const Buffer* pArgumentBuffer, uint32_t drawCount, uint32_t offset);

    /// Dispatches GPU workloads
    ///
    /// @ingroup CauldronRender
    void Dispatch(CommandList* pCmdList, uint32_t numGroupX, uint32_t numGroupY, uint32_t numGroupZ);

    /// Does immediate writes to buffer resources
    ///
    /// @ingroup CauldronRender
    void WriteBufferImmediate(CommandList* pCmdList, const GPUResource* pResource, uint32_t numParams, const uint32_t* offsets, const uint32_t* values);

    /// Writes AMD FidelityFX Breadcrumbs Library marker to specified GPU locations
    ///
    /// @ingroup CauldronRender
    void WriteBreadcrumbsMarker(Device* pDevice, CommandList* pCmdList, Buffer* pBuffer, uint64_t gpuAddress, uint32_t value, bool isBegin);

    /// Sets the shading rate to use for rasterization workloads
    ///
    /// @ingroup CauldronRender
    void SetShadingRate(CommandList* pCmdList, const ShadingRate shadingRate, const ShadingRateCombiner* combiners, const GPUResource* pShadingRateImage = nullptr);

} // namespace cauldron
