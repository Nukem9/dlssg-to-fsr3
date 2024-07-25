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

#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

IDXGIFactory* getDXGIFactoryFromSwapChain(IDXGISwapChain* swapChain)
{
    IDXGIFactory* factory = nullptr;
    if (FAILED(swapChain->GetParent(IID_PPV_ARGS(&factory)))) {

    }

    return factory;
}

void waitForPerformanceCount(const int64_t targetCount)
{
    int64_t currentCount = 0;
    do
    {
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentCount));
    } while (currentCount < targetCount);
}

bool waitForFenceValue(ID3D12Fence* fence, UINT64 value, DWORD dwMilliseconds)
{
    bool status = false;

    if (fence)
    {
        if (dwMilliseconds == INFINITE)
        {
            while (fence->GetCompletedValue() < value);
            status = true;
        }
        else
        {
            status = fence->GetCompletedValue() >= value;

            if (!status)
            {
                HANDLE handle = CreateEvent(0, false, false, 0);

                if (isValidHandle(handle))
                {
                    //Wait until command queue is done.
                    if (!status)
                    {
                        if (SUCCEEDED(fence->SetEventOnCompletion(value, handle)))
                        {
                            status = (WaitForSingleObject(handle, dwMilliseconds) == WAIT_OBJECT_0);
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

