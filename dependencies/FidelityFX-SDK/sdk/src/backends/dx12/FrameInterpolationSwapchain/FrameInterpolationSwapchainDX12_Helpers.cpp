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

#include <FidelityFX/host/ffx_FrameInterpolation.h>
#include <FidelityFX/host/backends/dx12/ffx_dx12.h>


#include "FrameInterpolationSwapchainDX12_Helpers.h"
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

#include <string> //needed for std::to_wstring

IDXGIFactory* getDXGIFactoryFromSwapChain(IDXGISwapChain* swapChain)
{
    IDXGIFactory* factory = nullptr;
    if (FAILED(swapChain->GetParent(IID_PPV_ARGS(&factory)))) {

    }

    return factory;
}

void waitForPerformanceCount(const int64_t targetCount, const int64_t frequency, const UINT timerResolution, const UINT spinTime)
{
    int64_t currentCount;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentCount));
    if (currentCount >= targetCount)
        return;
        
    double millis = static_cast<double>(((targetCount - currentCount) * 1000000) / frequency) / 1000.;

    //Sleep if safe, to free up cores.
    while (timerResolution != UNKNOWN_TIMER_RESOlUTION && millis > spinTime * timerResolution)
    {
        MMRESULT result = timeBeginPeriod(timerResolution);           //Request 1ms timer resolution from OS. Necessary to prevent overshooting sleep.
        if (result != TIMERR_NOERROR)
            break; //Can't guarantee sleep precision.              
        Sleep(static_cast<DWORD>((millis - timerResolution*spinTime)));  //End sleep a few timer resolution units early to prevent overshooting.
        timeEndPeriod(timerResolution);

        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentCount));

        millis = static_cast<double>(((targetCount - currentCount) * 1000000) / frequency) / 1000.;
    }

    do
    {
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentCount));
    } while (currentCount < targetCount);
}

bool waitForFenceValue(ID3D12Fence* fence, UINT64 value, DWORD dwMilliseconds, FfxWaitCallbackFunc waitCallback, const bool waitForSingleObjectOnFence)
{
    bool status = false;

    if (fence)
    {
        int64_t originalQpc = 0;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&originalQpc));
        int64_t currentQpc = originalQpc;
        int64_t qpcFrequency;
        QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&qpcFrequency));
        const DWORD waitCallbackIntervalInMs = 1;
        int64_t deltaQpcWaitCallback = qpcFrequency * waitCallbackIntervalInMs / 1000;
        int64_t deltaQpcTimeout = qpcFrequency * dwMilliseconds /1000;
        wchar_t fenceName[64];
        uint32_t fenceNameLen = sizeof(fenceName);
        fence->GetPrivateData(WKPDID_D3DDebugObjectNameW, &fenceNameLen, &fenceName);

        if (waitForSingleObjectOnFence == false)
        {
            int64_t previousQpc = originalQpc;
            while (status != true)
            {
                status = fence->GetCompletedValue() >= value;
                QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentQpc));
                if (waitCallback)
                {
                    int64_t deltaQpc = currentQpc - previousQpc;
                    if ((deltaQpc > deltaQpcWaitCallback))
                    {
                        waitCallback(fenceName, value);
                        previousQpc = currentQpc;
                    }
                }
                if (dwMilliseconds != INFINITE)
                {
                    int64_t deltaQpc = currentQpc - originalQpc;
                    if (deltaQpc > deltaQpcTimeout)
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            status = fence->GetCompletedValue() >= value;

            if (!status)
            {
                HANDLE handle = CreateEvent(0, false, false, 0);

                if (isValidHandle(handle))
                {
                    if (SUCCEEDED(fence->SetEventOnCompletion(value, handle)))
                    {
                        while (status != true)
                        {
                            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentQpc));
                            int64_t deltaQpc = currentQpc - originalQpc;
                            if (deltaQpc > deltaQpcTimeout && dwMilliseconds != INFINITE)
                            {
                                break;
                            }
                            status = (WaitForSingleObject(handle, waitCallbackIntervalInMs) == WAIT_OBJECT_0);
                            if (waitCallback)
                            {
                                waitCallback(fenceName, value);
                            }
                        }
                    }
                    CloseHandle(handle);
                }
            }
        }
    }

    return status;
}

bool isTearingSupported(IDXGIFactory* dxgiFactory)
{
    BOOL bTearingSupported = FALSE;

    IDXGIFactory5* pFactory5 = nullptr;
    if (dxgiFactory && SUCCEEDED(dxgiFactory->QueryInterface(IID_PPV_ARGS(&pFactory5))))
    {
        
        if (SUCCEEDED(pFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &bTearingSupported, sizeof(bTearingSupported)))) {

        }

        SafeRelease(pFactory5);
    }

    return bTearingSupported == TRUE;
}

inline bool isValidHandle(HANDLE handle)
{
    return handle != NULL;
}

bool isExclusiveFullscreen(IDXGISwapChain* swapChain)
{
    bool bIsExclusiveFullscreen = false;

    BOOL isFullscreen = FALSE;
    if (SUCCEEDED(swapChain->GetFullscreenState(&isFullscreen, nullptr)))
    {
        bIsExclusiveFullscreen = (isFullscreen == TRUE);
    }

    return bIsExclusiveFullscreen;
}

IDXGIOutput6* getMostRelevantOutputFromSwapChain(IDXGISwapChain* swapChain)
{
    IDXGIOutput6* pOutput6 = nullptr;

    IDXGIFactory* pFactory = getDXGIFactoryFromSwapChain(swapChain);
    if (pFactory)
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        if (SUCCEEDED(swapChain->GetDesc(&desc)))
        {
            UINT largestArea = 0;
            RECT windowRect{};
            if (GetWindowRect(desc.OutputWindow, &windowRect) == TRUE)
            {
                UINT          adapterIdx = 0;
                IDXGIAdapter* pAdapter   = nullptr;
                while (pFactory->EnumAdapters(adapterIdx++, &pAdapter) != DXGI_ERROR_NOT_FOUND)
                {
                    UINT         outputIdx = 0;
                    IDXGIOutput* pOutput   = nullptr;
                    while (pAdapter->EnumOutputs(outputIdx++, &pOutput) != DXGI_ERROR_NOT_FOUND)
                    {
                        DXGI_OUTPUT_DESC outputDesc{};
                        if (SUCCEEDED(pOutput->GetDesc(&outputDesc)))
                        {
                            RECT intersection{};
                            if (IntersectRect(&intersection, &windowRect, &outputDesc.DesktopCoordinates) == TRUE)
                            {
                                UINT area = (intersection.right - intersection.left) * (intersection.bottom - intersection.top);

                                if (area > largestArea)
                                {
                                    SafeRelease(pOutput6);
                                    if (SUCCEEDED(pOutput->QueryInterface(IID_PPV_ARGS(&pOutput6))))
                                    {
                                        largestArea = area;
                                    }
                                }
                            }
                        }

                        SafeRelease(pOutput);
                    }

                    SafeRelease(pAdapter);
                }
            }
        }
        SafeRelease(pFactory);
    }

    return pOutput6;
}

bool getMonitorLuminanceRange(IDXGISwapChain* swapChain, float *outMinLuminance, float *outMaxLuminance)
{
    bool bResult = false;

    IDXGIOutput6* dxgiOutput = getMostRelevantOutputFromSwapChain(swapChain);

    if (dxgiOutput)
    {
        DXGI_OUTPUT_DESC1 outputDesc1;
        if (SUCCEEDED(dxgiOutput->GetDesc1(&outputDesc1)))
        {
            *outMinLuminance = outputDesc1.MinLuminance;
            *outMaxLuminance = outputDesc1.MaxLuminance;
            bResult = true;
        }
        SafeRelease(dxgiOutput);
    }

    return bResult;
}

uint64_t GetResourceGpuMemorySize(ID3D12Resource* resource)
{
    uint64_t      size = 0;
    D3D12_RESOURCE_ALLOCATION_INFO allocInfo = {};
    if (resource)
    {
        D3D12_RESOURCE_DESC desc = resource->GetDesc();
        ID3D12Device4* pDevice4 = nullptr;
        if (SUCCEEDED(resource->GetDevice(IID_PPV_ARGS(&pDevice4))))
        {
            allocInfo = pDevice4->GetResourceAllocationInfo(0, 1, &desc);
            size = allocInfo.SizeInBytes;
            SafeRelease(pDevice4);
        }
    }
    return size;
}

