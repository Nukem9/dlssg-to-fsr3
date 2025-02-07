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

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <comdef.h>
#include <synchapi.h>

#include <FidelityFX/host/ffx_assert.h>

typedef int32_t FfxErrorCode;
typedef FfxErrorCode(*FfxWaitCallbackFunc)(wchar_t* fenceName, uint64_t fenceValueToWaitFor);

constexpr UINT UNKNOWN_TIMER_RESOlUTION = 0;  //Timer resolution is not known. 

IDXGIFactory*           getDXGIFactoryFromSwapChain(IDXGISwapChain* swapChain);
bool                    isExclusiveFullscreen(IDXGISwapChain* swapChain);
void                    waitForPerformanceCount(const int64_t targetCount, const int64_t frequency, const UINT timerResolution, const UINT spinTime);
bool                    waitForFenceValue(ID3D12Fence* fence, UINT64 value, DWORD dwMilliseconds = INFINITE, FfxWaitCallbackFunc waitCallback = nullptr, const bool waitForSingleObjectOnFence = false);
bool                    isTearingSupported(IDXGIFactory* dxgiFactory);
bool                    getMonitorLuminanceRange(IDXGISwapChain* swapChain, float* outMinLuminance, float* outMaxLuminance);
inline bool             isValidHandle(HANDLE handle);
IDXGIOutput6*           getMostRelevantOutputFromSwapChain(IDXGISwapChain* swapChain);
uint64_t                GetResourceGpuMemorySize(ID3D12Resource* resource);

    // Safe release for interfaces
template<class Interface>
inline UINT SafeRelease(Interface*& pInterfaceToRelease)
{
    UINT refCount = std::numeric_limits<UINT>::max();
    if (pInterfaceToRelease != nullptr)
    {
        refCount = pInterfaceToRelease->Release();

        pInterfaceToRelease = nullptr;
    }

    return refCount;
}

inline void SafeCloseHandle(HANDLE& handle)
{
    if (handle)
    {
        CloseHandle(handle);
        handle = 0;
    }
}

// fix up format in case resource passed for SRV cannot be mapped
static DXGI_FORMAT convertFormatSrv(DXGI_FORMAT format)
{
    switch (format) 
    {
        // Handle Depth
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        case DXGI_FORMAT_D32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case DXGI_FORMAT_D16_UNORM:
            return DXGI_FORMAT_R16_UNORM;

        // Handle TYPELESS format for color: assume FLOAT for 16 and 32 bit channels, else UNORM
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case DXGI_FORMAT_R32G32B32_TYPELESS:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_R32G32_TYPELESS:
            return DXGI_FORMAT_R32G32_FLOAT;
        case DXGI_FORMAT_R16G16_TYPELESS:
            return DXGI_FORMAT_R16G16_FLOAT;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
            return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        case DXGI_FORMAT_R32_TYPELESS:
            return DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_R8G8_TYPELESS:
            return DXGI_FORMAT_R8G8_UNORM;
        case DXGI_FORMAT_R16_TYPELESS:
            return DXGI_FORMAT_R16_FLOAT;
        case DXGI_FORMAT_R8_TYPELESS:
            return DXGI_FORMAT_R8_UNORM;
        default:
            return format;
    }
}

class Dx12Commands
{
    ID3D12CommandQueue*        queue                = nullptr;
    ID3D12CommandAllocator*    allocator            = nullptr;
    ID3D12GraphicsCommandList* list                 = nullptr;
    ID3D12Fence*               fence                = nullptr;
    UINT64                     availableFenceValue  = 0;

public:
    void release()
    {
        SafeRelease(allocator);
        SafeRelease(list);
        SafeRelease(fence);
    }

public:
    ~Dx12Commands()
    {
        release();
    }

    bool initiated()
    {
        return allocator != nullptr;
    }

    bool verify(ID3D12CommandQueue* pQueue)
    {
        HRESULT hr = initiated() ? S_OK : E_FAIL;

        if (FAILED(hr))
        {
            ID3D12Device* device = nullptr;
            if (SUCCEEDED(pQueue->GetDevice(IID_PPV_ARGS(&device))))
            {
                D3D12_COMMAND_QUEUE_DESC queueDesc = pQueue->GetDesc();
                if (SUCCEEDED(device->CreateCommandAllocator(queueDesc.Type, IID_PPV_ARGS(&allocator))))
                {
                    allocator->SetName(L"Dx12CommandPool::Allocator");
                    if (SUCCEEDED(device->CreateCommandList(queueDesc.NodeMask, queueDesc.Type, allocator, nullptr, IID_PPV_ARGS(&list))))
                    {
                        allocator->SetName(L"Dx12CommandPool::Commandlist");
                        list->Close();

                        if (SUCCEEDED(device->CreateFence(availableFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
                        {
                            hr = S_OK;
                        }
                    }
                }

                SafeRelease(device);
            }
        }

        if (FAILED(hr))
        {
            release();
        }

        return SUCCEEDED(hr);
    }

    void occupy(ID3D12CommandQueue* pQueue, const wchar_t* name)
    {
        availableFenceValue++;
        queue = pQueue;
        allocator->SetName(name);
        list->SetName(name);
        fence->SetName(name);
    }

    ID3D12GraphicsCommandList* reset()
    {
        if (SUCCEEDED(allocator->Reset()))
        {
            if (SUCCEEDED(list->Reset(allocator, nullptr)))
            {
            }
        }

        return list;
    }

    ID3D12GraphicsCommandList* getList()
    {
        return list;
    }

    void execute(bool listIsOpen = false)
    {
        if (listIsOpen)
        {
            list->Close();
        }

        ID3D12CommandList* pListsToExec[] = {list};
        queue->ExecuteCommandLists(_countof(pListsToExec), pListsToExec);
        queue->Signal(fence, availableFenceValue);
    }

    void drop(bool listIsOpen = false)
    {
        if (listIsOpen)
        {
            getList()->Close();
        }
        queue->Signal(fence, availableFenceValue);
    }

    bool available()
    {
        return fence->GetCompletedValue() >= availableFenceValue;
    }
};

template <size_t Capacity>
class Dx12CommandPool
{
public:

private:
    CRITICAL_SECTION criticalSection{};
    Dx12Commands     buffer[4 /* D3D12_COMMAND_LIST_TYPE_COPY == 3 */][Capacity] = {};

public:

    Dx12CommandPool()
    {
        InitializeCriticalSection(&criticalSection);
    }

    ~Dx12CommandPool()
    {
        EnterCriticalSection(&criticalSection);

        for (size_t type = 0; type < 4; type++)
        {
            for (size_t idx = 0; idx < Capacity; idx++)
            {
                auto& cmds = buffer[type][idx];
                while (cmds.initiated() && !cmds.available())
                {
                    // wait for list to be idling
                }
                cmds.release();
            }
        }

        LeaveCriticalSection(&criticalSection);

        DeleteCriticalSection(&criticalSection);
    }

    Dx12Commands* get(ID3D12CommandQueue* pQueue, const wchar_t* name)
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = pQueue->GetDesc();

        EnterCriticalSection(&criticalSection);

        Dx12Commands* pCommands = nullptr;
        for (size_t idx = 0; idx < Capacity && (pCommands == nullptr); idx++)
        {
            auto& cmds = buffer[queueDesc.Type][idx];
            if (cmds.verify(pQueue) && cmds.available())
            {
                pCommands = &cmds;
            }
        }

        FFX_ASSERT(pCommands);

        pCommands->occupy(pQueue, name);
        LeaveCriticalSection(&criticalSection);

        return pCommands;
    }
};

template <const int Size, typename Type = double>
struct SimpleMovingAverage
{
    Type                    history[Size] = {};
    unsigned int            idx           = 0;
    unsigned int            updateCount   = 0;

    Type getAverage()
    {
        if (updateCount < Size)
            return 0.0;

        Type          average    = 0.f;
        unsigned int  iterations = (updateCount >= Size) ? Size : updateCount;
        
        if (iterations > 0)
        {
            for (size_t i = 0; i < iterations; i++)
            {
                average += history[i];
            }
            average /= iterations;
        }

        return average;
    }

    Type getVariance()
    {
        if (updateCount < Size)
            return 0.0;

        Type average  = getAverage();
        Type variance = 0.f;
        unsigned int iterations = (updateCount >= Size) ? Size : updateCount;

        if (iterations > 0)
        {
            for (size_t i = 0; i < iterations; i++)
            {
                variance += (history[i] - average) * (history[i] - average);
            }
            variance /= iterations;
        }

        return sqrt(variance);
    }

    void reset()
    {
        updateCount = 0;
        idx         = 0;
    }

    void update(Type newValue)
    {
        history[idx] = newValue;
        idx          = (idx + 1) % Size;
        updateCount++;
    }
};

