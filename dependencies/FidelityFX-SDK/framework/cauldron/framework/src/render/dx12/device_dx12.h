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

#include "misc/threadsafe_queue.h"
#include "render/device.h"
#include "render/dx12/defines_dx12.h"

#include <agilitysdk/include/d3d12.h>
#include <windows.h>
#include <dxgi1_6.h>

#include "AGS/amd_ags.h"
#include "memoryallocator/D3D12MemAlloc.h"
#include "antilag2/ffx_antilag2_dx12.h"

namespace cauldron
{
    struct SwapChainCreationParams
    {
        DXGI_SWAP_CHAIN_DESC1                   DX12Desc;
        HWND                                    WndHandle;
        MSComPtr<IDXGIFactory6>                 pFactory;
    };

    class DeviceInternal final : public Device
    {
    public:
        virtual ~DeviceInternal();

        const ID3D12Device* DX12Device() const { return m_pDevice; }
        ID3D12Device* DX12Device() { return m_pDevice; }

        const ID3D12CommandQueue* DX12CmdQueue(CommandQueue queueType) const { return m_QueueSyncPrims[static_cast<int32_t>(queueType)].m_pQueue.Get(); }
        ID3D12CommandQueue* DX12CmdQueue(CommandQueue queueType) { return m_QueueSyncPrims[static_cast<int32_t>(queueType)].m_pQueue.Get(); }

        const MSComPtr<IDXGIAdapter> GetAdapter() const
        {
            return m_pAdapter;
        }

        D3D12MA::Allocator* GetD3D12MemoryAllocator() { return m_pD3D12Allocator.Get(); }

        const AGSContext* GetAGSContext() const { return m_pAGSContext; }
        const AGSGPUInfo* GetAGSGpuInfo() const { return &m_AGSGPUInfo; }

        void ReleaseCommandAllocator(CommandList* pCmdList);

        virtual void GetFeatureInfo(DeviceFeature feature, void* pFeatureInfo) override;
        virtual void FlushQueue(CommandQueue queueType) override;
        virtual uint64_t QueryPerformanceFrequency(CommandQueue queueType) override;

        virtual CommandList* CreateCommandList(const wchar_t* name, CommandQueue queueType) override;

        virtual void CreateSwapChain(SwapChain*& pSwapChain, const SwapChainCreationParams& params, CommandQueue queueType) override;
        virtual uint64_t PresentSwapChain(SwapChain* pSwapChain) override;

        virtual void WaitOnQueue(uint64_t waitValue, CommandQueue queueType) const override;
        virtual uint64_t QueryLastCompletedValue(CommandQueue queueType) override;
        virtual uint64_t SignalQueue(CommandQueue queueType) override;

        virtual uint64_t ExecuteCommandLists(std::vector<CommandList*>& cmdLists, CommandQueue queueType, bool isFirstSubmissionOfFrame = false, bool isLastSubmissionOfFrame = false) override;
        virtual void ExecuteCommandListsImmediate(std::vector<CommandList*>& cmdLists, CommandQueue queueType) override;

        virtual void ExecuteResourceTransitionImmediate(uint32_t barrierCount, const Barrier* pBarriers) override;
        virtual void ExecuteTextureResourceCopyImmediate(uint32_t resourceCopyCount, const TextureCopyDesc* pCopyDescs) override;

        virtual const DeviceInternal* GetImpl() const override { return this; }
        virtual DeviceInternal* GetImpl() override { return this; }

    private:
        friend class Device;
        DeviceInternal();

        struct QueueSyncPrimitive
        {
            // Thread-safe access
            MSComPtr<ID3D12CommandQueue>                                    m_pQueue                   = nullptr;
            ThreadSafeQueue<MSComPtr<ID3D12CommandAllocator>> m_AvailableQueueAllocators = {};

            // Non-thread-safe access
            MSComPtr<ID3D12Fence>               m_pQueueFence      = nullptr;
            uint64_t                            m_QueueSignalValue = 1;
            std::mutex                          m_QueueAccessMutex;

            void Wait(uint64_t waitValue) const;
        };

        void InitDevice();
        void InitQueueSyncPrim(CommandQueue queueType, QueueSyncPrimitive& queueSyncPrim, const wchar_t* name);

        // internal implementations
        CommandList* CreateCommandList(const wchar_t* name, CommandQueue queueType, QueueSyncPrimitive& queueSyncPrim, const wchar_t* allocatorName);
        MSComPtr<ID3D12CommandAllocator> GetAllocator(CommandQueue queueType, QueueSyncPrimitive& queueSyncPrim, const wchar_t* allocatorName);
        static void ExecuteCommandListsImmediate(std::vector<CommandList*>& cmdLists, QueueSyncPrimitive& queueSyncPrim);
        static void WaitOnQueue(uint64_t waitValue, const QueueSyncPrimitive& queueSyncPrim);

        virtual void UpdateAntiLag2() override;

    private:
        QueueSyncPrimitive m_QueueSyncPrims[static_cast<int32_t>(CommandQueue::Count)] = {};

        ID3D12Device* m_pDevice = nullptr;
        AGSContext*   m_pAGSContext = nullptr;
        AGSGPUInfo    m_AGSGPUInfo = {};

        MSComPtr<D3D12MA::Allocator> m_pD3D12Allocator = nullptr;
        MSComPtr<IDXGIAdapter>       m_pAdapter        = nullptr;

        AMD::AntiLag2DX12::Context m_AntiLag2Context = {};

    };

} // namespace cauldron

#endif // #if defined(_DX12)
