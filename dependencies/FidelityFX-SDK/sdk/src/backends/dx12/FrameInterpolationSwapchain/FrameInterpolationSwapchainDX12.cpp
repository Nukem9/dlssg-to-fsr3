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

#include <tuple>
#include <initguid.h>
#include "FrameInterpolationSwapchainDX12.h"

#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#include "FrameInterpolationSwapchainDX12_UiComposition.h"
#include "FrameInterpolationSwapchainDX12_DebugPacing.h"
#include "antilag2/ffx_antilag2_dx12.h"

#pragma comment(lib, "winmm.lib")
#include <timeapi.h>

FfxErrorCode ffxRegisterFrameinterpolationUiResourceDX12(FfxSwapchain gameSwapChain, FfxResource uiResource, uint32_t flags)
        {
    IDXGISwapChain4* swapChain = ffxGetDX12SwapchainPtr(gameSwapChain);

    FrameInterpolationSwapChainDX12* framinterpolationSwapchain = nullptr;
    if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&framinterpolationSwapchain))))
    {
        framinterpolationSwapchain->registerUiResource(uiResource, flags);

        SafeRelease(framinterpolationSwapchain);

        return FFX_OK;
    }

    return FFX_ERROR_INVALID_ARGUMENT;
}

FFX_API FfxErrorCode ffxSetFrameGenerationConfigToSwapchainDX12(FfxFrameGenerationConfig const* config)
{
    FfxErrorCode result = FFX_ERROR_INVALID_ARGUMENT;

    if (config->swapChain)
    {
        IDXGISwapChain4*                 swapChain = ffxGetDX12SwapchainPtr(config->swapChain);
        FrameInterpolationSwapChainDX12* framinterpolationSwapchain = nullptr;
        if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&framinterpolationSwapchain))))
        {
            framinterpolationSwapchain->setFrameGenerationConfig(config);

            SafeRelease(framinterpolationSwapchain);
            
            result = FFX_OK;
        }
    }

    return result;
}

FfxErrorCode ffxConfigureFrameInterpolationSwapchainDX12(FfxSwapchain gameSwapChain, FfxFrameInterpolationSwapchainConfigureKey key, void* valuePtr)
{
    IDXGISwapChain4* swapChain = ffxGetDX12SwapchainPtr(gameSwapChain);
    
    FrameInterpolationSwapChainDX12* framinterpolationSwapchain = nullptr;
    if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&framinterpolationSwapchain))))
    {
        switch (key)
        {
            case FFX_FI_SWAPCHAIN_CONFIGURE_KEY_WAITCALLBACK:
                framinterpolationSwapchain->setWaitCallback(static_cast<FfxWaitCallbackFunc>(valuePtr));
            break;
            case FFX_FI_SWAPCHAIN_CONFIGURE_KEY_FRAMEPACINGTUNING:
                if (valuePtr != nullptr)
                {
                    framinterpolationSwapchain->setFramePacingTuning(static_cast<FfxSwapchainFramePacingTuning*>(valuePtr));
                }
            break;
        }
        SafeRelease(framinterpolationSwapchain);

        return FFX_OK;
    }

    return FFX_ERROR_INVALID_ARGUMENT;
}

FfxResource ffxGetFrameinterpolationTextureDX12(FfxSwapchain gameSwapChain)
{
    FfxResource                      res = { nullptr };
    IDXGISwapChain4* swapChain = ffxGetDX12SwapchainPtr(gameSwapChain);
    FrameInterpolationSwapChainDX12* framinterpolationSwapchain = nullptr;
    if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&framinterpolationSwapchain))))
    {
        res = framinterpolationSwapchain->interpolationOutput(0);

        SafeRelease(framinterpolationSwapchain);
    }
    return res;
}

FfxErrorCode ffxGetFrameinterpolationCommandlistDX12(FfxSwapchain gameSwapChain, FfxCommandList& gameCommandlist)
{
    // 1) query FrameInterpolationSwapChainDX12 from gameSwapChain
    // 2) call  FrameInterpolationSwapChainDX12::getInterpolationCommandList()
    IDXGISwapChain4* swapChain = ffxGetDX12SwapchainPtr(gameSwapChain);

    FrameInterpolationSwapChainDX12* framinterpolationSwapchain = nullptr;
    if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&framinterpolationSwapchain))))
    {
        gameCommandlist = framinterpolationSwapchain->getInterpolationCommandList();

        SafeRelease(framinterpolationSwapchain);

        return FFX_OK;
    }

    return FFX_ERROR_INVALID_ARGUMENT;
}

FfxErrorCode ffxFrameInterpolationSwapchainGetGpuMemoryUsageDX12(FfxSwapchain gameSwapChain, FfxEffectMemoryUsage* vramUsage)
{
    FFX_RETURN_ON_ERROR(vramUsage, FFX_ERROR_INVALID_POINTER);
    FfxErrorCode result = FFX_ERROR_INVALID_ARGUMENT;
    IDXGISwapChain4* swapChain = ffxGetDX12SwapchainPtr(gameSwapChain);
    
    FrameInterpolationSwapChainDX12* framinterpolationSwapchain = nullptr;
    if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&framinterpolationSwapchain))))
    {
        framinterpolationSwapchain->GetGpuMemoryUsage(vramUsage);
        SafeRelease(framinterpolationSwapchain);
        result = FFX_OK;
    }
    return FFX_ERROR_INVALID_ARGUMENT;
}

FfxErrorCode ffxReplaceSwapchainForFrameinterpolationDX12(FfxCommandQueue gameQueue, FfxSwapchain& gameSwapChain)
{
    FfxErrorCode     status            = FFX_ERROR_INVALID_ARGUMENT;
    IDXGISwapChain4* dxgiGameSwapChain = reinterpret_cast<IDXGISwapChain4*>(gameSwapChain);
    FFX_ASSERT(dxgiGameSwapChain);

    ID3D12CommandQueue* queue = reinterpret_cast<ID3D12CommandQueue*>(gameQueue);
    FFX_ASSERT(queue);

    // we just need the desc, release the real swapchain as we'll replace that with one doing frameinterpolation
    HWND                            hWnd;
    DXGI_SWAP_CHAIN_DESC1           desc1;
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc;
    if (SUCCEEDED(dxgiGameSwapChain->GetDesc1(&desc1)) &&
        SUCCEEDED(dxgiGameSwapChain->GetFullscreenDesc(&fullscreenDesc)) &&
        SUCCEEDED(dxgiGameSwapChain->GetHwnd(&hWnd))
        )
    {
        FFX_ASSERT_MESSAGE(fullscreenDesc.Windowed == TRUE, "Illegal to release a fullscreen swap chain.");

        IDXGIFactory* dxgiFactory = getDXGIFactoryFromSwapChain(dxgiGameSwapChain);
        SafeRelease(dxgiGameSwapChain);

        FfxSwapchain proxySwapChain;
        status = ffxCreateFrameinterpolationSwapchainForHwndDX12(hWnd, &desc1, &fullscreenDesc, queue, dxgiFactory, proxySwapChain);
        if (status == FFX_OK)
        {
            gameSwapChain = proxySwapChain;
        }

        SafeRelease(dxgiFactory);
    }

    return status;
}

FfxErrorCode ffxCreateFrameinterpolationSwapchainDX12(const DXGI_SWAP_CHAIN_DESC*   desc,
                                                      ID3D12CommandQueue*           queue,
                                                      IDXGIFactory*                 dxgiFactory,
                                                      FfxSwapchain&                 outGameSwapChain)
{
    FFX_ASSERT(desc);
    FFX_ASSERT(queue);
    FFX_ASSERT(dxgiFactory);

    DXGI_SWAP_CHAIN_DESC1 desc1{};
    desc1.Width         = desc->BufferDesc.Width;
    desc1.Height        = desc->BufferDesc.Height;
    desc1.Format        = desc->BufferDesc.Format;
    desc1.SampleDesc    = desc->SampleDesc;
    desc1.BufferUsage   = desc->BufferUsage;
    desc1.BufferCount   = desc->BufferCount;
    desc1.SwapEffect    = desc->SwapEffect;
    desc1.Flags         = desc->Flags;

    // for clarity, params not part of DXGI_SWAP_CHAIN_DESC
    // implicit behavior of DXGI when you call the IDXGIFactory::CreateSwapChain
    desc1.Scaling       = DXGI_SCALING_STRETCH;
    desc1.AlphaMode     = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc1.Stereo        = FALSE;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc{};
    fullscreenDesc.Scaling          = desc->BufferDesc.Scaling;
    fullscreenDesc.RefreshRate      = desc->BufferDesc.RefreshRate;
    fullscreenDesc.ScanlineOrdering = desc->BufferDesc.ScanlineOrdering;
    fullscreenDesc.Windowed         = desc->Windowed;

    return ffxCreateFrameinterpolationSwapchainForHwndDX12(desc->OutputWindow, &desc1, &fullscreenDesc, queue, dxgiFactory, outGameSwapChain);
}

FfxErrorCode ffxCreateFrameinterpolationSwapchainForHwndDX12(HWND                                   hWnd,
                                                             const DXGI_SWAP_CHAIN_DESC1*           desc1,
                                                             const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreenDesc,
                                                             ID3D12CommandQueue*                    queue,
                                                             IDXGIFactory*                          dxgiFactory,
                                                             FfxSwapchain&                          outGameSwapChain)
{
    // don't assert fullscreenDesc, nullptr valid
    FFX_ASSERT(hWnd != 0);
    FFX_ASSERT(desc1);
    FFX_ASSERT(queue);
    FFX_ASSERT(dxgiFactory);

    FfxErrorCode err = FFX_ERROR_INVALID_ARGUMENT;

    IDXGIFactory2* dxgiFactory2 = nullptr;
    if (SUCCEEDED(dxgiFactory->QueryInterface(IID_PPV_ARGS(&dxgiFactory2))))
    {
        // Create proxy swapchain
        FrameInterpolationSwapChainDX12* fiSwapchain = new FrameInterpolationSwapChainDX12();
        if (fiSwapchain)
        {
            if (SUCCEEDED(fiSwapchain->init(hWnd, desc1, fullscreenDesc, queue, dxgiFactory2)))
            {
                outGameSwapChain = ffxGetSwapchainDX12(fiSwapchain);

                err = FFX_OK;
            }
            else
            {
                delete fiSwapchain;
                err = FFX_ERROR_INVALID_ARGUMENT;
            }
        }
        else
        {
            err = FFX_ERROR_OUT_OF_MEMORY;
        }

        SafeRelease(dxgiFactory2);
    }

    return err;
}

FfxErrorCode ffxWaitForPresents(FfxSwapchain gameSwapChain)
{
    IDXGISwapChain4* swapChain = ffxGetDX12SwapchainPtr(gameSwapChain);

    FrameInterpolationSwapChainDX12* framinterpolationSwapchain;
    if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&framinterpolationSwapchain))))
    {
        framinterpolationSwapchain->waitForPresents();
        SafeRelease(framinterpolationSwapchain);

        return FFX_OK;
    }

    return FFX_ERROR_INVALID_ARGUMENT;
}

void setSwapChainBufferResourceInfo(IDXGISwapChain4* swapChain, bool isInterpolated)
{
    uint32_t        currBackbufferIndex = swapChain->GetCurrentBackBufferIndex();
    ID3D12Resource* swapchainBackbuffer = nullptr;

    if (SUCCEEDED(swapChain->GetBuffer(currBackbufferIndex, IID_PPV_ARGS(&swapchainBackbuffer))))
    {
        FfxFrameInterpolationSwapChainResourceInfo info{};
        info.version = FFX_SDK_MAKE_VERSION(FFX_FRAME_INTERPOLATION_SWAP_CHAIN_VERSION_MAJOR,
                                            FFX_FRAME_INTERPOLATION_SWAP_CHAIN_VERSION_MINOR,
                                            FFX_FRAME_INTERPOLATION_SWAP_CHAIN_VERSION_PATCH);
        info.isInterpolated = isInterpolated;
        HRESULT hr = swapchainBackbuffer->SetPrivateData(IID_IFfxFrameInterpolationSwapChainResourceInfo, sizeof(info), &info);
        FFX_ASSERT(SUCCEEDED(hr));

        /*
        usage example:
        
        FfxFrameInterpolationSwapChainResourceInfo info{};
        UINT size = sizeof(info);
        if (SUCCEEDED(swapchainBackbuffer->GetPrivateData(IID_IFfxFrameInterpolationSwapChainResourceInfo, &size, &info))) {
            
        } else {
            // buffer was not presented using proxy swapchain
        }
        */

        SafeRelease(swapchainBackbuffer);
    }
}

HRESULT compositeSwapChainFrame(FrameinterpolationPresentInfo* presenter, PacingData* pacingEntry, uint32_t frameID)
{

    const PacingData::FrameInfo& frameInfo = pacingEntry->frames[frameID];

    presenter->presentQueue->Wait(presenter->interpolationFence, frameInfo.interpolationCompletedFenceValue);

    if (pacingEntry->drawDebugPacingLines)
    {
        auto gpuCommands = presenter->commandPool.get(presenter->presentQueue, L"compositeSwapChainFrame");

        uint32_t        currBackbufferIndex = presenter->swapChain->GetCurrentBackBufferIndex();
        ID3D12Resource* swapchainBackbuffer = nullptr;
        presenter->swapChain->GetBuffer(currBackbufferIndex, IID_PPV_ARGS(&swapchainBackbuffer));

        FfxPresentCallbackDescription desc{};
        desc.commandList            = ffxGetCommandListDX12(gpuCommands->reset());
        desc.device                 = presenter->device;
        desc.isInterpolatedFrame    = frameID != PacingData::FrameType::Real;
        desc.outputSwapChainBuffer  = ffxGetResourceDX12(swapchainBackbuffer, ffxGetResourceDescriptionDX12(swapchainBackbuffer), nullptr, FFX_RESOURCE_STATE_PRESENT);
        desc.currentBackBuffer      = frameInfo.resource;
        desc.currentUI              = pacingEntry->uiSurface;
        desc.usePremulAlpha         = pacingEntry->usePremulAlphaComposite;
        desc.frameID                = pacingEntry->currentFrameID;

        ffxFrameInterpolationDebugPacing(&desc);

        gpuCommands->execute(true);

        SafeRelease(swapchainBackbuffer);
    }


    if (pacingEntry->presentCallback)
    {
        auto gpuCommands = presenter->commandPool.get(presenter->presentQueue, L"compositeSwapChainFrame");

        uint32_t        currBackbufferIndex = presenter->swapChain->GetCurrentBackBufferIndex();
        ID3D12Resource* swapchainBackbuffer = nullptr;
        presenter->swapChain->GetBuffer(currBackbufferIndex, IID_PPV_ARGS(&swapchainBackbuffer));

        FfxPresentCallbackDescription desc{};
        desc.commandList            = ffxGetCommandListDX12(gpuCommands->reset());
        desc.device                 = presenter->device;
        desc.isInterpolatedFrame    = frameID != PacingData::FrameType::Real;
        desc.outputSwapChainBuffer  = ffxGetResourceDX12(swapchainBackbuffer, ffxGetResourceDescriptionDX12(swapchainBackbuffer), nullptr, FFX_RESOURCE_STATE_PRESENT);
        desc.currentBackBuffer      = frameInfo.resource;
        desc.currentUI              = pacingEntry->uiSurface;
        desc.usePremulAlpha         = pacingEntry->usePremulAlphaComposite;
        desc.frameID                = pacingEntry->currentFrameID;

        pacingEntry->presentCallback(&desc, pacingEntry->presentCallbackContext);

        gpuCommands->execute(true);

        SafeRelease(swapchainBackbuffer);
    }

    presenter->presentQueue->Signal(presenter->compositionFenceGPU, frameInfo.presentIndex);
    presenter->compositionFenceCPU->Signal(frameInfo.presentIndex);

    return S_OK;
}

void presentToSwapChain(FrameinterpolationPresentInfo* presenter, PacingData* pacingEntry, PacingData::FrameType frameType)
{
    const PacingData::FrameInfo& frameInfo = pacingEntry->frames[frameType];

    const UINT uSyncInterval            = pacingEntry->vsync ? 1 : 0;
    const bool bExclusiveFullscreen     = isExclusiveFullscreen(presenter->swapChain);
    const bool bSetAllowTearingFlag     = pacingEntry->tearingSupported && !bExclusiveFullscreen && (0 == uSyncInterval);
    const UINT uFlags                   = bSetAllowTearingFlag * DXGI_PRESENT_ALLOW_TEARING;

    struct AntiLag2Data
    {
        AMD::AntiLag2DX12::Context* context;
        bool                        enabled;
    } data;

    // {5083ae5b-8070-4fca-8ee5-3582dd367d13}
    static const GUID IID_IFfxAntiLag2Data = {0x5083ae5b, 0x8070, 0x4fca, {0x8e, 0xe5, 0x35, 0x82, 0xdd, 0x36, 0x7d, 0x13}};

    bool isInterpolated = frameType != PacingData::Real;

    UINT size = sizeof(data);
    if (SUCCEEDED(presenter->swapChain->GetPrivateData(IID_IFfxAntiLag2Data, &size, &data)))
    {
        if (data.enabled)
        {
            AMD::AntiLag2DX12::SetFrameGenFrameType(data.context, isInterpolated);
        }
    }

    presenter->swapChain->Present(uSyncInterval, uFlags);

    // tick frames sent for presentation
    presenter->presentQueue->Signal(presenter->presentFence, frameInfo.presentIndex);

}

DWORD WINAPI presenterThread(LPVOID param)
{
    FrameinterpolationPresentInfo* presenter = static_cast<FrameinterpolationPresentInfo*>(param);

    if (presenter)
    {
        UINT64 numFramesSentForPresentation = 0;
        int64_t qpcFrequency                 = 0;

        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        qpcFrequency = freq.QuadPart;

        TIMECAPS timerCaps;
        timerCaps.wPeriodMin = UNKNOWN_TIMER_RESOlUTION; //Default to unknown to prevent sleep without guarantees.

        presenter->previousPresentQpc = 0;

        while (!presenter->shutdown)
        {

            WaitForSingleObject(presenter->pacerEvent, INFINITE);

            if (!presenter->shutdown)
            {
                EnterCriticalSection(&presenter->criticalSectionScheduledFrame);

                PacingData entry = presenter->scheduledPresents;
                presenter->scheduledPresents.invalidate();

                LeaveCriticalSection(&presenter->criticalSectionScheduledFrame);

                if (entry.numFramesToPresent > 0)
                {
                    // we might have dropped entries so have to update here, oterwise we might deadlock
                    presenter->presentQueue->Signal(presenter->presentFence, entry.numFramesSentForPresentationBase);
                    presenter->presentQueue->Wait(presenter->interpolationFence, entry.interpolationCompletedFenceValue);

                    for (uint32_t frameType = 0; frameType < PacingData::FrameType::Count; frameType++)
                    {
                        const PacingData::FrameInfo& frameInfo = entry.frames[frameType];
                        if (frameInfo.doPresent)
                        {
                            compositeSwapChainFrame(presenter, &entry, frameType);

                            // signal replacement buffer availability
                            if (frameInfo.presentIndex == entry.replacementBufferFenceSignal)
                            {
                                presenter->presentQueue->Signal(presenter->replacementBufferFence, entry.replacementBufferFenceSignal);
                            }

                            
                            MMRESULT result = timeGetDevCaps(&timerCaps, sizeof(timerCaps));
                            if (result != MMSYSERR_NOERROR || !presenter->allowHybridSpin)
                            {
                                timerCaps.wPeriodMin = UNKNOWN_TIMER_RESOlUTION;
                            }
                            else
                            {
                                timerCaps.wPeriodMin = FFX_MAXIMUM(1, timerCaps.wPeriodMin);
                            }

                            // pacing without composition
                            waitForFenceValue(presenter->compositionFenceGPU, frameInfo.presentIndex);
                            uint64_t targetQpc = presenter->previousPresentQpc + frameInfo.presentQpcDelta;
                            waitForPerformanceCount(targetQpc, qpcFrequency, timerCaps.wPeriodMin, presenter->hybridSpinTime);

                            int64_t currentPresentQPC;
                            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentPresentQPC));
                            presenter->previousPresentQpc = currentPresentQPC;

                            presentToSwapChain(presenter, &entry, (PacingData::FrameType)frameType);
                        }
                    }

                    numFramesSentForPresentation = entry.numFramesSentForPresentationBase + entry.numFramesToPresent;

                }
            }
        }

        waitForFenceValue(presenter->presentFence, numFramesSentForPresentation);
    }

    return 0;
}

DWORD WINAPI interpolationThread(LPVOID param)
{
    FrameinterpolationPresentInfo* presenter = static_cast<FrameinterpolationPresentInfo*>(param);

    if (presenter)
    {
        HANDLE presenterThreadHandle = CreateThread(nullptr, 0, presenterThread, param, 0, nullptr);
        FFX_ASSERT(presenterThreadHandle != NULL);

        if (presenterThreadHandle != 0)
        {
            SetThreadPriority(presenterThreadHandle, THREAD_PRIORITY_HIGHEST);
            SetThreadDescription(presenterThreadHandle, L"AMD FSR Presenter Thread");

            SimpleMovingAverage<10, double> frameTime{};

            int64_t previousQpc = 0;
            int64_t previousDelta = 0;
            int64_t qpcFrequency;
            QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&qpcFrequency)); 

            while (!presenter->shutdown)
            {
                WaitForSingleObject(presenter->presentEvent, INFINITE);

                if (!presenter->shutdown)
                {
                    EnterCriticalSection(&presenter->criticalSectionScheduledFrame);

                    PacingData entry = presenter->scheduledInterpolations;
                    presenter->scheduledInterpolations.invalidate();

                    LeaveCriticalSection(&presenter->criticalSectionScheduledFrame);
                    
                    int64_t preWaitQPC = 0;
                    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&preWaitQPC));
                    int64_t previousPresentQPC = presenter->previousPresentQpc;
                    int64_t targetDelta = (previousPresentQPC + previousDelta) - preWaitQPC;
                    
                    //Risk of late wake if overthreading. If allowed, use WaitForSingleObject to wait for interpolationFence if the target is more than 2ms later.
                    if (previousPresentQPC && (targetDelta * 1000000) / qpcFrequency > 2000)  
                    {
                        waitForFenceValue(
                            presenter->interpolationFence, 
                            entry.frames[PacingData::FrameType::Interpolated_1].interpolationCompletedFenceValue, 
                            INFINITE,
                            nullptr,
                            presenter->allowWaitForSingleObjectOnFence
                        );
                        
                    }
                    else
                    {
                        // spin to wait for interpolationFence if the target is less than 2ms.
                        waitForFenceValue(
                            presenter->interpolationFence, 
                            entry.frames[PacingData::FrameType::Interpolated_1].interpolationCompletedFenceValue, 
                            INFINITE,
                            nullptr,
                            false
                        );
                    }
                    
                    SetEvent(presenter->interpolationEvent);

                    int64_t currentQpc = 0;
                    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentQpc));

                    const double deltaQpc = double(currentQpc - previousQpc) * (previousQpc > 0);
                    previousQpc           = currentQpc;

                    // reset pacing averaging if delta > 10 fps,
                    const float fTimeoutInSeconds       = 0.1f;
                    double      deltaQpcResetThreashold = double(qpcFrequency * fTimeoutInSeconds);
                    if ((deltaQpc > deltaQpcResetThreashold) || presenter->resetTimer)
                    {
                        frameTime.reset();
                    }
                    else
                    {
                        frameTime.update(deltaQpc);
                    }

                    // set presentation time: reduce based on variance and subract safety margin so we don't lock on a framerate lower than necessary
                    int64_t qpcSafetyMargin         = int64_t(qpcFrequency * presenter->safetyMarginInSec);
                    const int64_t conservativeAvg   = int64_t(frameTime.getAverage() * 0.5 - frameTime.getVariance() * presenter->varianceFactor);
                    const int64_t deltaToUse        = conservativeAvg > qpcSafetyMargin ? (conservativeAvg - qpcSafetyMargin) : 0;
                    entry.frames[PacingData::FrameType::Interpolated_1].presentQpcDelta = deltaToUse;
                    entry.frames[PacingData::FrameType::Real].presentQpcDelta           = deltaToUse;
                    previousDelta                                                       = deltaToUse;
                    
                    // schedule presents
                    EnterCriticalSection(&presenter->criticalSectionScheduledFrame);
                    presenter->scheduledPresents = entry;
                    LeaveCriticalSection(&presenter->criticalSectionScheduledFrame);
                    SetEvent(presenter->pacerEvent);
                }
            }

            // signal event to allow thread to finish
            SetEvent(presenter->pacerEvent);
            WaitForSingleObject(presenterThreadHandle, INFINITE);
            SafeCloseHandle(presenterThreadHandle);
        }
    }

    return 0;
}

bool FrameInterpolationSwapChainDX12::verifyBackbufferDuplicateResources()
{
    HRESULT hr = S_OK;

    ID3D12Device8*  device = nullptr;
    ID3D12Resource* buffer = nullptr;
    if (SUCCEEDED(real()->GetBuffer(0, IID_PPV_ARGS(&buffer))))
    {
        if (SUCCEEDED(buffer->GetDevice(IID_PPV_ARGS(&device))))
        {
            auto bufferDesc = buffer->GetDesc();
            D3D12_CLEAR_VALUE clearValue{ bufferDesc.Format, 0.f, 0.f, 0.f, 1.f };

            D3D12_HEAP_PROPERTIES heapProperties{};
            D3D12_HEAP_FLAGS      heapFlags;
            buffer->GetHeapProperties(&heapProperties, &heapFlags);

            heapFlags &= ~D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
            heapFlags &= ~D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
            heapFlags &= ~D3D12_HEAP_FLAG_DENY_BUFFERS;
            heapFlags &= ~D3D12_HEAP_FLAG_ALLOW_DISPLAY;

            for (size_t i = 0; i < gameBufferCount; i++)
            {
                if (replacementSwapBuffers[i].resource == nullptr)
                {
                    
                    // create game render output resource
                    if (FAILED(device->CreateCommittedResource(&heapProperties,
                                                                heapFlags,
                                                                &bufferDesc,
                                                                D3D12_RESOURCE_STATE_PRESENT,
                                                                &clearValue,
                                                                IID_PPV_ARGS(&replacementSwapBuffers[i].resource))))
                    {
                        hr |= E_FAIL;
                    }
                    else
                    {
                        uint64_t resourceSize = GetResourceGpuMemorySize(replacementSwapBuffers[i].resource);
                        totalUsageInBytes += resourceSize;
                        replacementSwapBuffers[i].resource->SetName(L"AMD FSR Replacement BackBuffer");
                    }
                }
            }

            for (size_t i = 0; i < _countof(interpolationOutputs); i++)
            {
                // create interpolation output resource
                bufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
                if (interpolationOutputs[i].resource == nullptr)
                {
                    if (FAILED(device->CreateCommittedResource(
                            &heapProperties, heapFlags, &bufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &clearValue, IID_PPV_ARGS(&interpolationOutputs[i].resource))))
                    {
                        hr |= E_FAIL;
                    }
                    else
                    {
                        uint64_t resourceSize = GetResourceGpuMemorySize(interpolationOutputs[i].resource);
                        totalUsageInBytes += resourceSize;
                        interpolationOutputs[i].resource->SetName(L"AMD FSR Interpolation Output");
                    }
                }
            }

            SafeRelease(device);
        }

        SafeRelease(realBackBuffer0);
        realBackBuffer0 = buffer;
    }

    return SUCCEEDED(hr);
}

HRESULT FrameInterpolationSwapChainDX12::init(HWND                                  hWnd,
                                              const DXGI_SWAP_CHAIN_DESC1*          desc,
                                              const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreenDesc,
                                              ID3D12CommandQueue* queue,
                                              IDXGIFactory2* dxgiFactory)
{
    FFX_ASSERT(desc);
    FFX_ASSERT(queue);
    FFX_ASSERT(dxgiFactory);

    // store values we modify, to return when application asks for info
    gameBufferCount    = desc->BufferCount;
    gameFlags          = desc->Flags;
    gameSwapEffect     = desc->SwapEffect;

    // set default ui composition / frame interpolation present function
    presentCallback    = ffxFrameInterpolationUiComposition;

    HRESULT hr = E_FAIL;

    if (SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&presentInfo.device))))
    {
        presentInfo.gameQueue       = queue;

        InitializeCriticalSection(&criticalSection);
        InitializeCriticalSection(&criticalSectionUpdateConfig);
        InitializeCriticalSection(&presentInfo.criticalSectionScheduledFrame);
        presentInfo.presentEvent       = CreateEvent(NULL, FALSE, FALSE, nullptr);
        presentInfo.interpolationEvent = CreateEvent(NULL, FALSE, TRUE, nullptr);
        presentInfo.pacerEvent         = CreateEvent(NULL, FALSE, FALSE, nullptr);
        tearingSupported               = isTearingSupported(dxgiFactory);

        // Create presentation queue
        D3D12_COMMAND_QUEUE_DESC presentQueueDesc = queue->GetDesc();
        presentQueueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        presentQueueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        presentQueueDesc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
        presentQueueDesc.NodeMask                 = 0;
        presentInfo.device->CreateCommandQueue(&presentQueueDesc, IID_PPV_ARGS(&presentInfo.presentQueue));
        presentInfo.presentQueue->SetName(L"AMD FSR PresentQueue");

        // Setup pass-through swapchain default state is disabled/passthrough
        IDXGISwapChain1* pSwapChain1 = nullptr;

        DXGI_SWAP_CHAIN_DESC1 realDesc = getInterpolationEnabledSwapChainDescription(desc);
        hr = dxgiFactory->CreateSwapChainForHwnd(presentInfo.presentQueue, hWnd, &realDesc, fullscreenDesc, nullptr, &pSwapChain1);
        if (SUCCEEDED(hr) && queue)
        {
            if (SUCCEEDED(hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(&presentInfo.swapChain))))
            {
                // Register proxy swapchain to the real swap chain object
                presentInfo.swapChain->SetPrivateData(IID_IFfxFrameInterpolationSwapChain, sizeof(FrameInterpolationSwapChainDX12*), this);

                SafeRelease(pSwapChain1);
            }
            else
            {
                FFX_ASSERT_MESSAGE(hr == S_OK, "Could not query swapchain interface. Application will crash.");
                return hr;
            }
        }
        else
        {
            FFX_ASSERT_MESSAGE(hr == S_OK, "Could not create replacement swapchain. Application will crash.");
            return hr;
        }

        // init min and lax luminance according to monitor metadata
        // in case app doesn't set it through SetHDRMetadata
        getMonitorLuminanceRange(presentInfo.swapChain, &minLuminance, &maxLuminance);

        presentInfo.device->CreateFence(gameFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&presentInfo.gameFence));
        presentInfo.gameFence->SetName(L"AMD FSR GameFence");

        presentInfo.device->CreateFence(interpolationFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&presentInfo.interpolationFence));
        presentInfo.interpolationFence->SetName(L"AMD FSR InterpolationFence");

        presentInfo.device->CreateFence(framesSentForPresentation, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&presentInfo.presentFence));
        presentInfo.presentFence->SetName(L"AMD FSR PresentFence");

        presentInfo.device->CreateFence(framesSentForPresentation, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&presentInfo.replacementBufferFence));
        presentInfo.replacementBufferFence->SetName(L"AMD FSR ReplacementBufferFence");

        presentInfo.device->CreateFence(framesSentForPresentation, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&presentInfo.compositionFenceGPU));
        presentInfo.compositionFenceGPU->SetName(L"AMD FSR CompositionFence GPU");

        presentInfo.device->CreateFence(framesSentForPresentation, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&presentInfo.compositionFenceCPU));
        presentInfo.compositionFenceCPU->SetName(L"AMD FSR CompositionFence CPU");

        replacementFrameLatencyWaitableObjectHandle = CreateEvent(0, FALSE, TRUE, nullptr);

        // Create interpolation queue
        D3D12_COMMAND_QUEUE_DESC queueDesc = queue->GetDesc();
        queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
        queueDesc.NodeMask                 = 0;
        presentInfo.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&presentInfo.asyncComputeQueue));
        presentInfo.asyncComputeQueue->SetName(L"AMD FSR AsyncComputeQueue");

        // Default to dispatch interpolation workloads on the game queue
        presentInfo.interpolationQueue = presentInfo.gameQueue;
    }

    return hr;
}

FrameInterpolationSwapChainDX12::FrameInterpolationSwapChainDX12()
{

}

FrameInterpolationSwapChainDX12::~FrameInterpolationSwapChainDX12()
{
    shutdown();
}

UINT FrameInterpolationSwapChainDX12::getInterpolationEnabledSwapChainFlags(UINT nonAdjustedFlags)
{
    UINT flags = nonAdjustedFlags;

    // The DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT flag changes the D3D runtime behavior for fences
    // We will make our own waitable object for the app to wait on, but we need to keep the flag 
    flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    if (tearingSupported)
    {
        flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    return flags;
}

DXGI_SWAP_CHAIN_DESC1 FrameInterpolationSwapChainDX12::getInterpolationEnabledSwapChainDescription(const DXGI_SWAP_CHAIN_DESC1* nonAdjustedDesc)
{
    DXGI_SWAP_CHAIN_DESC1 fiDesc = *nonAdjustedDesc;

    // adjust swap chain descriptor to fit FI requirements
    fiDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    fiDesc.BufferCount = 3;
    fiDesc.Flags = getInterpolationEnabledSwapChainFlags(fiDesc.Flags);

    return fiDesc;
}

IDXGISwapChain4* FrameInterpolationSwapChainDX12::real()
{
    return presentInfo.swapChain;
}

HRESULT FrameInterpolationSwapChainDX12::shutdown()
{
    //m_pDevice will be nullptr already shutdown
    if (presentInfo.device)
    {
        destroyReplacementResources();
        
        EnterCriticalSection(&criticalSection);
        killPresenterThread();
        releaseUiBlitGpuResources();
        LeaveCriticalSection(&criticalSection);

        SafeCloseHandle(presentInfo.presentEvent);
        SafeCloseHandle(presentInfo.interpolationEvent);
        SafeCloseHandle(presentInfo.pacerEvent);

        // if we failed initialization, we may not have an interpolation queue or fence
        if (presentInfo.interpolationQueue)
        {
            if (presentInfo.interpolationFence)
            {
                presentInfo.interpolationQueue->Signal(presentInfo.interpolationFence, ++interpolationFenceValue);
                waitForFenceValue(presentInfo.interpolationFence, interpolationFenceValue, INFINITE, presentInfo.waitCallback, presentInfo.interpolationFence);
            }
        }
        
        SafeRelease(presentInfo.asyncComputeQueue);
        SafeRelease(presentInfo.presentQueue);

        SafeRelease(presentInfo.interpolationFence);
        SafeRelease(presentInfo.presentFence);
        SafeRelease(presentInfo.replacementBufferFence);
        SafeRelease(presentInfo.compositionFenceGPU);
        SafeRelease(presentInfo.compositionFenceCPU);

        std::ignore = SafeRelease(presentInfo.swapChain);

        if (presentInfo.gameFence)
        {
            waitForFenceValue(presentInfo.gameFence, gameFenceValue, INFINITE, presentInfo.waitCallback);
        }
        SafeRelease(presentInfo.gameFence);

        DeleteCriticalSection(&criticalSection);
        DeleteCriticalSection(&criticalSectionUpdateConfig);
        DeleteCriticalSection(&presentInfo.criticalSectionScheduledFrame);

        std::ignore = SafeRelease(presentInfo.device);
    }

    return S_OK;
}

bool FrameInterpolationSwapChainDX12::killPresenterThread()
{
    if (interpolationThreadHandle != NULL)
    {
        // prepare present CPU thread for shutdown
        presentInfo.shutdown = true;

        // signal event to allow thread to finish
        SetEvent(presentInfo.presentEvent);
        WaitForSingleObject(interpolationThreadHandle, INFINITE);
        SafeCloseHandle(interpolationThreadHandle);
    }

    return interpolationThreadHandle == nullptr;
}

bool FrameInterpolationSwapChainDX12::spawnPresenterThread()
{
    if (interpolationThreadHandle == NULL)
    {
        presentInfo.shutdown      = false;
        interpolationThreadHandle = CreateThread(nullptr, 0, interpolationThread, reinterpret_cast<void*>(&presentInfo), 0, nullptr);
        
        FFX_ASSERT(interpolationThreadHandle != NULL);

        if (interpolationThreadHandle != 0)
        {
            SetThreadPriority(interpolationThreadHandle, THREAD_PRIORITY_HIGHEST);
            SetThreadDescription(interpolationThreadHandle, L"AMD FSR Interpolation Thread");
        }

        SetEvent(presentInfo.interpolationEvent);
    }

    return interpolationThreadHandle != NULL;
}

void FrameInterpolationSwapChainDX12::discardOutstandingInterpolationCommandLists()
{
    // drop any outstanding interpolaton command lists
    for (size_t i = 0; i < _countof(registeredInterpolationCommandLists); i++)
    {
        if (registeredInterpolationCommandLists[i] != nullptr)
        {
            registeredInterpolationCommandLists[i]->drop(true);
            registeredInterpolationCommandLists[i] = nullptr;
        }
    }
}

void FrameInterpolationSwapChainDX12::setFrameGenerationConfig(FfxFrameGenerationConfig const* config)
{
    EnterCriticalSection(&criticalSectionUpdateConfig);

    FFX_ASSERT(config);

    // if config is a pointer to the internal config ::present called this function to apply the changes
    bool applyChangesNow = (config == &nextFrameGenerationConfig);
        
    FfxPresentCallbackFunc inputPresentCallback    = (nullptr != config->presentCallback) ? config->presentCallback : ffxFrameInterpolationUiComposition;
    void*                  inputPresentCallbackCtx = (nullptr != config->presentCallback) ? config->presentCallbackContext : nullptr;
    ID3D12CommandQueue*    inputInterpolationQueue = config->allowAsyncWorkloads ? presentInfo.asyncComputeQueue : presentInfo.gameQueue;

    // if this is called externally just copy the new config to the internal copy to avoid potentially stalling on criticalSection
    if (!applyChangesNow)
    {  
        nextFrameGenerationConfig = *config;

        // in case of actual reconfiguration: apply the changes immediately
        if ( presentInfo.interpolationQueue != inputInterpolationQueue 
           || interpolationEnabled != config->frameGenerationEnabled 
           || presentCallback != inputPresentCallback
           || presentCallbackContext != inputPresentCallbackCtx 
           || frameGenerationCallback != config->frameGenerationCallback
           || frameGenerationCallbackContext != config->frameGenerationCallbackContext
           || drawDebugPacingLines != config->drawDebugPacingLines)
        {
            applyChangesNow = true;
        }
    }

    if (applyChangesNow)
    {
        EnterCriticalSection(&criticalSection);

        currentFrameID          = config->frameID;
        presentInterpolatedOnly = config->onlyPresentInterpolated;
        interpolationRect       = config->interpolationRect;
        drawDebugPacingLines    = config->drawDebugPacingLines;

        if (presentInfo.interpolationQueue != inputInterpolationQueue)
        {
            waitForPresents();
            discardOutstandingInterpolationCommandLists();

            // change interpolation queue and reset fence value
            presentInfo.interpolationQueue = inputInterpolationQueue;
            interpolationFenceValue        = 0;
            presentInfo.interpolationQueue->Signal(presentInfo.interpolationFence, interpolationFenceValue);
        }

        if (interpolationEnabled != config->frameGenerationEnabled || presentCallback != inputPresentCallback ||
            frameGenerationCallback != config->frameGenerationCallback || configFlags != (FfxFsr3FrameGenerationFlags)config->flags ||
            presentCallbackContext != inputPresentCallbackCtx || frameGenerationCallbackContext != config->frameGenerationCallbackContext)
        {
            waitForPresents();
            presentCallback                = inputPresentCallback;
            presentCallbackContext         = inputPresentCallbackCtx;
            frameGenerationCallback        = config->frameGenerationCallback;
            configFlags                    = FfxFsr3FrameGenerationFlags(config->flags);
            frameGenerationCallbackContext = config->frameGenerationCallbackContext;

            // handle interpolation mode change
            if (interpolationEnabled != config->frameGenerationEnabled)
            {
                interpolationEnabled = config->frameGenerationEnabled;
                if (interpolationEnabled)
                {
                    frameInterpolationResetCondition = true;
                    nextPresentWaitValue             = framesSentForPresentation;

                    spawnPresenterThread();
                }
                else
                {
                    killPresenterThread();
                }
            }
        }
        LeaveCriticalSection(&criticalSection);
    }

    LeaveCriticalSection(&criticalSectionUpdateConfig);
}

bool FrameInterpolationSwapChainDX12::destroyReplacementResources()
{
    HRESULT hr = S_OK;

    EnterCriticalSection(&criticalSection);

    waitForPresents();

    const bool recreatePresenterThread = interpolationThreadHandle != nullptr;
    if (recreatePresenterThread)
    {
        killPresenterThread();
    }

    discardOutstandingInterpolationCommandLists();

    {
        for (size_t i = 0; i < _countof(replacementSwapBuffers); i++)
        {
            uint64_t resourceSize = GetResourceGpuMemorySize(replacementSwapBuffers[i].resource);
            totalUsageInBytes -= resourceSize;
            replacementSwapBuffers[i].destroy();
        }

        SafeRelease(realBackBuffer0);
        
        for (size_t i = 0; i < _countof(interpolationOutputs); i++)
        {
            uint64_t resourceSize = GetResourceGpuMemorySize(interpolationOutputs[i].resource);
            totalUsageInBytes -= resourceSize;
            interpolationOutputs[i].destroy();
        }

        if (uiReplacementBuffer.resource !=nullptr)
        {
            uint64_t resourceSize = GetResourceGpuMemorySize(uiReplacementBuffer.resource);
            totalUsageInBytes -= resourceSize;
        }
        
        uiReplacementBuffer.destroy();
    }

    // reset counters used in buffer management
    framesSentForPresentation        = 0;
    nextPresentWaitValue             = 0;
    replacementSwapBufferIndex       = 0;
    presentCount                     = 0;
    interpolationFenceValue          = 0;
    gameFenceValue                   = 0;

    // if we didn't init correctly, some parameters may not exist
    if (presentInfo.gameFence)
    {
        presentInfo.gameFence->Signal(gameFenceValue);
    }
    
    if (presentInfo.interpolationFence)
    {
        presentInfo.interpolationFence->Signal(interpolationFenceValue);
    }
    
    if (presentInfo.presentFence)
    {
        presentInfo.presentFence->Signal(framesSentForPresentation);
    }

    if (presentInfo.replacementBufferFence)
    {
        presentInfo.replacementBufferFence->Signal(framesSentForPresentation);
    }

    if (presentInfo.compositionFenceGPU)
    {
        presentInfo.compositionFenceGPU->Signal(framesSentForPresentation);
    }

    if (presentInfo.compositionFenceCPU)
    {
        presentInfo.compositionFenceCPU->Signal(framesSentForPresentation);
    }

    frameInterpolationResetCondition = true;

    if (recreatePresenterThread)
    {
        spawnPresenterThread();
    }

    discardOutstandingInterpolationCommandLists();

    LeaveCriticalSection(&criticalSection);

    return SUCCEEDED(hr);
}

bool FrameInterpolationSwapChainDX12::waitForPresents()
{
    // wait for interpolation to finish
    waitForFenceValue(presentInfo.gameFence, gameFenceValue, INFINITE, presentInfo.waitCallback);
    waitForFenceValue(presentInfo.interpolationFence, interpolationFenceValue, INFINITE, presentInfo.waitCallback);
    waitForFenceValue(presentInfo.presentFence, framesSentForPresentation, INFINITE, presentInfo.waitCallback);

    return true;
}

FfxResource FrameInterpolationSwapChainDX12::interpolationOutput(int index)
{
    index = interpolationBufferIndex;

    FfxResourceDescription interpolateDesc = ffxGetResourceDescriptionDX12(interpolationOutputs[index].resource);
    return ffxGetResourceDX12(interpolationOutputs[index].resource, interpolateDesc, nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
}

//IUnknown
HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::QueryInterface(REFIID riid, void** ppvObject)
{

    const GUID guidReplacements[] = {
        __uuidof(this),
        IID_IUnknown,
        IID_IDXGIObject,
        IID_IDXGIDeviceSubObject,
        IID_IDXGISwapChain,
        IID_IDXGISwapChain1,
        IID_IDXGISwapChain2,
        IID_IDXGISwapChain3,
        IID_IDXGISwapChain4,
        IID_IFfxFrameInterpolationSwapChain
    };

    for (auto guid : guidReplacements)
    {
        if (IsEqualGUID(riid, guid) == TRUE)
        {
            AddRef();
            *ppvObject = this;
            return S_OK;
        }
    }

    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::AddRef()
{
    InterlockedIncrement(&refCount);

    return refCount;
}

ULONG STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::Release()
{
    ULONG ref = InterlockedDecrement(&refCount);
    if (ref != 0)
    {
        return refCount;
    }

    delete this;

    return 0;
}

// IDXGIObject
HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::SetPrivateData(REFGUID Name, UINT DataSize, const void* pData)
{
    return real()->SetPrivateData(Name, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown)
{
    return real()->SetPrivateDataInterface(Name, pUnknown);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData)
{
    return real()->GetPrivateData(Name, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetParent(REFIID riid, void** ppParent)
{
    return real()->GetParent(riid, ppParent);
}

// IDXGIDeviceSubObject
HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetDevice(REFIID riid, void** ppDevice)
{
    return real()->GetDevice(riid, ppDevice);
}

void FrameInterpolationSwapChainDX12::registerUiResource(FfxResource uiResource, uint32_t flags)
{
    EnterCriticalSection(&criticalSection);

    presentInfo.currentUiSurface = uiResource;
    presentInfo.uiCompositionFlags = flags;
    if (nullptr == uiResource.resource)
        presentInfo.uiCompositionFlags &= ~FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING;

    LeaveCriticalSection(&criticalSection);
}

void FrameInterpolationSwapChainDX12::setWaitCallback(FfxWaitCallbackFunc waitCallbackFunc)
{
    presentInfo.waitCallback = waitCallbackFunc;
}

void FrameInterpolationSwapChainDX12::setFramePacingTuning(const FfxSwapchainFramePacingTuning* framePacingTuning)
{
    presentInfo.safetyMarginInSec = static_cast<double> (framePacingTuning->safetyMarginInMs) / 1000.0;
    presentInfo.varianceFactor = static_cast<double> (framePacingTuning->varianceFactor);
    presentInfo.allowHybridSpin = framePacingTuning->allowHybridSpin;
    presentInfo.hybridSpinTime = framePacingTuning->hybridSpinTime;
    presentInfo.allowWaitForSingleObjectOnFence = framePacingTuning->allowWaitForSingleObjectOnFence;
}

void FrameInterpolationSwapChainDX12::GetGpuMemoryUsage(FfxEffectMemoryUsage* vramUsage)
{
    vramUsage->totalUsageInBytes = totalUsageInBytes;
    vramUsage->aliasableUsageInBytes = aliasableUsageInBytes;
}

void FrameInterpolationSwapChainDX12::presentPassthrough(UINT SyncInterval, UINT Flags)
{
    ID3D12Resource* dx12SwapchainBuffer    = nullptr;
    UINT            currentBackBufferIndex = presentInfo.swapChain->GetCurrentBackBufferIndex();
    presentInfo.swapChain->GetBuffer(currentBackBufferIndex, IID_PPV_ARGS(&dx12SwapchainBuffer));

    auto passthroughList = presentInfo.commandPool.get(presentInfo.presentQueue, L"passthroughList()");
    auto list            = passthroughList->reset();

    ID3D12Resource* dx12ResourceSrc = replacementSwapBuffers[replacementSwapBufferIndex].resource;
    ID3D12Resource* dx12ResourceDst = dx12SwapchainBuffer;

    D3D12_TEXTURE_COPY_LOCATION dx12SourceLocation = {};
    dx12SourceLocation.pResource                   = dx12ResourceSrc;
    dx12SourceLocation.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dx12SourceLocation.SubresourceIndex            = 0;

    D3D12_TEXTURE_COPY_LOCATION dx12DestinationLocation = {};
    dx12DestinationLocation.pResource                   = dx12ResourceDst;
    dx12DestinationLocation.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dx12DestinationLocation.SubresourceIndex            = 0;

    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource   = dx12ResourceSrc;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;

    barriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource   = dx12ResourceDst;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    list->ResourceBarrier(_countof(barriers), barriers);

    list->CopyResource(dx12ResourceDst, dx12ResourceSrc);

    for (size_t i = 0; i < _countof(barriers); ++i)
    {
        D3D12_RESOURCE_STATES tmpStateBefore = barriers[i].Transition.StateBefore;
        barriers[i].Transition.StateBefore   = barriers[i].Transition.StateAfter;
        barriers[i].Transition.StateAfter    = tmpStateBefore;
    }

    list->ResourceBarrier(_countof(barriers), barriers);

    passthroughList->execute(true);

    presentInfo.presentQueue->Signal(presentInfo.replacementBufferFence, ++framesSentForPresentation);
    presentInfo.presentQueue->Signal(presentInfo.compositionFenceGPU, framesSentForPresentation);
    presentInfo.compositionFenceCPU->Signal(framesSentForPresentation);

    setSwapChainBufferResourceInfo(presentInfo.swapChain, false);
    presentInfo.swapChain->Present(SyncInterval, Flags);

    presentInfo.presentQueue->Signal(presentInfo.presentFence, framesSentForPresentation);
    presentInfo.gameQueue->Wait(presentInfo.presentFence, framesSentForPresentation);

    SafeRelease(dx12SwapchainBuffer);
}

void FrameInterpolationSwapChainDX12::presentWithUiComposition(UINT SyncInterval, UINT Flags)
{
    auto uiCompositionList = presentInfo.commandPool.get(presentInfo.presentQueue, L"uiCompositionList()");
    auto list              = uiCompositionList->reset();

    ID3D12Resource* dx12SwapchainBuffer    = nullptr;
    UINT            currentBackBufferIndex = presentInfo.swapChain->GetCurrentBackBufferIndex();
    presentInfo.swapChain->GetBuffer(currentBackBufferIndex, IID_PPV_ARGS(&dx12SwapchainBuffer));

    FfxResourceDescription outBufferDesc = ffxGetResourceDescriptionDX12(dx12SwapchainBuffer);
    FfxResourceDescription inBufferDesc  = ffxGetResourceDescriptionDX12(replacementSwapBuffers[replacementSwapBufferIndex].resource);

    FfxPresentCallbackDescription desc{};
    desc.commandList                = ffxGetCommandListDX12(list);
    desc.device                     = presentInfo.device;
    desc.isInterpolatedFrame        = false;
    desc.outputSwapChainBuffer      = ffxGetResourceDX12(dx12SwapchainBuffer, outBufferDesc, nullptr, FFX_RESOURCE_STATE_PRESENT);
    desc.currentBackBuffer          = ffxGetResourceDX12(replacementSwapBuffers[replacementSwapBufferIndex].resource, inBufferDesc, nullptr, FFX_RESOURCE_STATE_PRESENT);
    if (presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING)
    {
        FfxResourceDescription uiBufferDesc = ffxGetResourceDescriptionDX12(uiReplacementBuffer.resource);
        desc.currentUI                      = ffxGetResourceDX12(uiReplacementBuffer.resource, uiBufferDesc, nullptr, presentInfo.currentUiSurface.state);
    }
    else
    {
        desc.currentUI = presentInfo.currentUiSurface;
    }
    desc.usePremulAlpha             = (presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_USE_PREMUL_ALPHA) != 0;
    desc.frameID                    = currentFrameID;

    presentCallback(&desc, presentCallbackContext);

    uiCompositionList->execute(true);

    presentInfo.presentQueue->Signal(presentInfo.replacementBufferFence, ++framesSentForPresentation);
    presentInfo.presentQueue->Signal(presentInfo.compositionFenceGPU, framesSentForPresentation);
    presentInfo.compositionFenceCPU->Signal(framesSentForPresentation);

    setSwapChainBufferResourceInfo(presentInfo.swapChain, false);
    presentInfo.swapChain->Present(SyncInterval, Flags);

    presentInfo.presentQueue->Signal(presentInfo.presentFence, framesSentForPresentation);
    presentInfo.gameQueue->Wait(presentInfo.presentFence, framesSentForPresentation);

    SafeRelease(dx12SwapchainBuffer);
}

void FrameInterpolationSwapChainDX12::dispatchInterpolationCommands(FfxResource* pInterpolatedFrame, FfxResource* pRealFrame)
{
    FFX_ASSERT(pInterpolatedFrame);
    FFX_ASSERT(pRealFrame);
    
    const UINT             currentBackBufferIndex   = GetCurrentBackBufferIndex();
    ID3D12Resource*        pCurrentBackBuffer       = replacementSwapBuffers[currentBackBufferIndex].resource;
    FfxResourceDescription gameFrameDesc            = ffxGetResourceDescriptionDX12(pCurrentBackBuffer);
    FfxResource backbuffer                          = ffxGetResourceDX12(replacementSwapBuffers[currentBackBufferIndex].resource, gameFrameDesc, nullptr, FFX_RESOURCE_STATE_PRESENT);

    *pRealFrame = backbuffer;

    // interpolation queue must wait for output resource to become available
    presentInfo.interpolationQueue->Wait(presentInfo.compositionFenceGPU, interpolationOutputs[interpolationBufferIndex].availabilityFenceValue);

    auto pRegisteredCommandList = registeredInterpolationCommandLists[currentBackBufferIndex];
    if (pRegisteredCommandList != nullptr)
    {
        pRegisteredCommandList->execute(true);

        presentInfo.interpolationQueue->Signal(presentInfo.interpolationFence, ++interpolationFenceValue);

        *pInterpolatedFrame = interpolationOutput();
        frameInterpolationResetCondition = false;
    }
    else {
        Dx12Commands* interpolationCommandList = presentInfo.commandPool.get(presentInfo.interpolationQueue, L"getInterpolationCommandList()");
        auto dx12CommandList = interpolationCommandList->reset();

        FfxFrameGenerationDispatchDescription desc{};
        desc.commandList = dx12CommandList;
        desc.outputs[0] = interpolationOutput();
        desc.presentColor = backbuffer;
        desc.reset = frameInterpolationResetCondition;
        desc.numInterpolatedFrames = 1;
        desc.backBufferTransferFunction = static_cast<FfxBackbufferTransferFunction>(backBufferTransferFunction);
        desc.minMaxLuminance[0] = minLuminance;
        desc.minMaxLuminance[1] = maxLuminance;
        desc.interpolationRect  = interpolationRect;
        desc.frameID            = currentFrameID;

        if (frameGenerationCallback(&desc, frameGenerationCallbackContext) == FFX_OK)
        {
            interpolationCommandList->execute(true);

            presentInfo.interpolationQueue->Signal(presentInfo.interpolationFence, ++interpolationFenceValue);
        }

        // reset condition if at least one frame was interpolated
        if (desc.numInterpolatedFrames > 0)
        {
            frameInterpolationResetCondition = false;
            *pInterpolatedFrame = interpolationOutput();
        }
    }
}

void FrameInterpolationSwapChainDX12::presentInterpolated(UINT SyncInterval, UINT)
{
    const bool bVsync = SyncInterval > 0;

    // interpolation needs to wait for the game queue
    presentInfo.gameQueue->Signal(presentInfo.gameFence, ++gameFenceValue);
    presentInfo.interpolationQueue->Wait(presentInfo.gameFence, gameFenceValue);

    FfxResource interpolatedFrame{}, realFrame{};
    dispatchInterpolationCommands(&interpolatedFrame, &realFrame);

    EnterCriticalSection(&presentInfo.criticalSectionScheduledFrame);

    PacingData entry{};
    entry.presentCallback                   = presentCallback;
    entry.presentCallbackContext            = presentCallbackContext;
    entry.drawDebugPacingLines              = drawDebugPacingLines;

    if (presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING)
    {
        FfxResourceDescription uiBufferDesc = ffxGetResourceDescriptionDX12(uiReplacementBuffer.resource);
        entry.uiSurface                     = ffxGetResourceDX12(uiReplacementBuffer.resource, uiBufferDesc, nullptr, presentInfo.currentUiSurface.state);
    }
    else
    {
        entry.uiSurface = presentInfo.currentUiSurface;
    }
    entry.vsync                             = bVsync;
    entry.tearingSupported                  = tearingSupported;
    entry.numFramesSentForPresentationBase  = framesSentForPresentation;
    entry.interpolationCompletedFenceValue  = interpolationFenceValue;
    entry.usePremulAlphaComposite           = (presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_USE_PREMUL_ALPHA) != 0;
    entry.currentFrameID                    = currentFrameID;

    // interpolated
    PacingData::FrameInfo& fiInterpolated = entry.frames[PacingData::FrameType::Interpolated_1];
    if (interpolatedFrame.resource != nullptr)
    {
        fiInterpolated.doPresent                        = true;
        fiInterpolated.resource                         = interpolatedFrame;
        fiInterpolated.interpolationCompletedFenceValue = interpolationFenceValue;
        fiInterpolated.presentIndex                     = ++framesSentForPresentation;
    }

    // real
    if (!presentInterpolatedOnly)
    {
        PacingData::FrameInfo& fiReal = entry.frames[PacingData::FrameType::Real];
        if (realFrame.resource != nullptr)
        {
            fiReal.doPresent    = true;
            fiReal.resource     = realFrame;
            fiReal.presentIndex = ++framesSentForPresentation;
        }
    }

    entry.replacementBufferFenceSignal  = framesSentForPresentation;
    entry.numFramesToPresent            = UINT32(framesSentForPresentation - entry.numFramesSentForPresentationBase);

    interpolationOutputs[interpolationBufferIndex].availabilityFenceValue = entry.numFramesSentForPresentationBase + fiInterpolated.doPresent;

    presentInfo.resetTimer              = frameInterpolationResetCondition;
    presentInfo.scheduledInterpolations = entry;
    LeaveCriticalSection(&presentInfo.criticalSectionScheduledFrame);

    // Set event to kick off async CPU present thread
    SetEvent(presentInfo.presentEvent);

    // hold the replacement object back until previous frame or interpolated is presented
    nextPresentWaitValue = entry.numFramesSentForPresentationBase;
    
    UINT64 frameLatencyObjectWaitValue = (entry.numFramesSentForPresentationBase - 1) * (entry.numFramesSentForPresentationBase > 0);
    FFX_ASSERT(SUCCEEDED(presentInfo.presentFence->SetEventOnCompletion(frameLatencyObjectWaitValue, replacementFrameLatencyWaitableObjectHandle)));

}

bool FrameInterpolationSwapChainDX12::verifyUiDuplicateResource()
{
    HRESULT hr = S_OK;

    ID3D12Device8*  device     = nullptr;
    ID3D12Resource* uiResource = reinterpret_cast<ID3D12Resource*>(presentInfo.currentUiSurface.resource);

    if ((0 == (presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING)) || (uiResource == nullptr))
    {
        if (nullptr != uiReplacementBuffer.resource)
        {
            uint64_t resourceSize = GetResourceGpuMemorySize(uiReplacementBuffer.resource);
            totalUsageInBytes -= resourceSize;
            waitForFenceValue(presentInfo.compositionFenceGPU, framesSentForPresentation, INFINITE, presentInfo.waitCallback);
            SafeRelease(uiReplacementBuffer.resource);
            uiReplacementBuffer = {};
        }
    }
    else
    {
        auto uiResourceDesc = uiResource->GetDesc();

        if (uiReplacementBuffer.resource != nullptr)
        {
            auto internalDesc = uiReplacementBuffer.resource->GetDesc();

            if (uiResourceDesc.Format != internalDesc.Format || uiResourceDesc.Width != internalDesc.Width || uiResourceDesc.Height != internalDesc.Height)
            {
                waitForFenceValue(presentInfo.compositionFenceGPU, framesSentForPresentation, INFINITE, presentInfo.waitCallback);
                SafeRelease(uiReplacementBuffer.resource);
            }
        }

        if (uiReplacementBuffer.resource == nullptr)
        {
            if (SUCCEEDED(uiResource->GetDevice(IID_PPV_ARGS(&device))))
            {

                D3D12_HEAP_PROPERTIES heapProperties{};
                D3D12_HEAP_FLAGS      heapFlags;
                uiResource->GetHeapProperties(&heapProperties, &heapFlags);

                heapFlags &= ~D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
                heapFlags &= ~D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
                heapFlags &= ~D3D12_HEAP_FLAG_DENY_BUFFERS;
                heapFlags &= ~D3D12_HEAP_FLAG_ALLOW_DISPLAY;

                // create game render output resource
                if (FAILED(device->CreateCommittedResource(&heapProperties,
                                                           heapFlags,
                                                           &uiResourceDesc,
                                                           ffxGetDX12StateFromResourceState(presentInfo.currentUiSurface.state),
                                                           nullptr,
                                                           IID_PPV_ARGS(&uiReplacementBuffer.resource))))
                {
                    hr |= E_FAIL;
                }
                else
                {
                    uint64_t resourceSize = GetResourceGpuMemorySize(uiReplacementBuffer.resource);
                    totalUsageInBytes += resourceSize;
                    uiReplacementBuffer.resource->SetName(L"AMD FSR Internal Ui Resource");
                }

                SafeRelease(device);
            }
        }
    }

    return SUCCEEDED(hr);
}

void FrameInterpolationSwapChainDX12::copyUiResource()
{
    auto copyList = presentInfo.commandPool.get(presentInfo.gameQueue, L"uiResourceCopyList");
    auto dx12List = copyList->reset();

    ID3D12Resource* dx12ResourceSrc = reinterpret_cast<ID3D12Resource*>(presentInfo.currentUiSurface.resource);
    ID3D12Resource* dx12ResourceDst = uiReplacementBuffer.resource;

    D3D12_TEXTURE_COPY_LOCATION dx12SourceLocation = {};
    dx12SourceLocation.pResource                   = dx12ResourceSrc;
    dx12SourceLocation.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dx12SourceLocation.SubresourceIndex            = 0;

    D3D12_TEXTURE_COPY_LOCATION dx12DestinationLocation = {};
    dx12DestinationLocation.pResource                   = dx12ResourceDst;
    dx12DestinationLocation.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dx12DestinationLocation.SubresourceIndex            = 0;

    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource   = dx12ResourceSrc;
    barriers[0].Transition.StateBefore = ffxGetDX12StateFromResourceState(presentInfo.currentUiSurface.state);
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;

    barriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource   = dx12ResourceDst;
    barriers[1].Transition.StateBefore = ffxGetDX12StateFromResourceState(presentInfo.currentUiSurface.state);
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    dx12List->ResourceBarrier(_countof(barriers), barriers);

    dx12List->CopyResource(dx12ResourceDst, dx12ResourceSrc);

    for (size_t i = 0; i < _countof(barriers); ++i)
    {
        D3D12_RESOURCE_STATES tmpStateBefore = barriers[i].Transition.StateBefore;
        barriers[i].Transition.StateBefore   = barriers[i].Transition.StateAfter;
        barriers[i].Transition.StateAfter    = tmpStateBefore;
    }

    dx12List->ResourceBarrier(_countof(barriers), barriers);

    copyList->execute(true);

    presentInfo.currentUiSurface.resource = nullptr;
}

    // IDXGISwapChain1
HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::Present(UINT SyncInterval, UINT Flags)
{
    const UINT64 previousFramesSentForPresentation = framesSentForPresentation;

    if (Flags & DXGI_PRESENT_TEST)
    {
        return presentInfo.swapChain->Present(SyncInterval, Flags);
    }

    setFrameGenerationConfig(&nextFrameGenerationConfig);

    EnterCriticalSection(&criticalSection);

    const UINT currentBackBufferIndex = GetCurrentBackBufferIndex();

    // determine what present path to execute
    const bool fgCallbackConfigured    = frameGenerationCallback != nullptr;
    const bool fgCommandListConfigured = registeredInterpolationCommandLists[currentBackBufferIndex] != nullptr;
    const bool runInterpolation        = interpolationEnabled && (fgCallbackConfigured || fgCommandListConfigured);

    // Ensure presenter thread has signaled before applying any wait to the game queue
    waitForFenceValue(presentInfo.compositionFenceCPU, previousFramesSentForPresentation);
    presentInfo.gameQueue->Wait(presentInfo.compositionFenceGPU, previousFramesSentForPresentation);

    // Verify integrity of internal Ui resource
    if (verifyUiDuplicateResource())
    {
        if ((presentInfo.uiCompositionFlags & FFX_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING) && (presentInfo.currentUiSurface.resource != nullptr))
        {
            copyUiResource();
        }
    }

    previousFrameWasInterpolated = runInterpolation;
    if (runInterpolation)
    {
        WaitForSingleObject(presentInfo.interpolationEvent, INFINITE);

        presentInterpolated(SyncInterval, Flags);
    }
    else
    {
        // if no interpolation, then we copied directly to the swapchain. Render UI, present and be done
        presentInfo.gameQueue->Signal(presentInfo.gameFence, ++gameFenceValue);
        presentInfo.presentQueue->Wait(presentInfo.gameFence, gameFenceValue);

        if (presentCallback != nullptr)
        {
            presentWithUiComposition(SyncInterval, Flags);
        }
        else
        {
            presentPassthrough(SyncInterval, Flags);
        }

        // respect game provided latency settings
        UINT64 frameLatencyObjectWaitValue = (framesSentForPresentation - gameMaximumFrameLatency) * (framesSentForPresentation >= gameMaximumFrameLatency);
        FFX_ASSERT(SUCCEEDED(presentInfo.presentFence->SetEventOnCompletion(frameLatencyObjectWaitValue, replacementFrameLatencyWaitableObjectHandle)));
    }

    replacementSwapBuffers[currentBackBufferIndex].availabilityFenceValue = framesSentForPresentation;

    // Unregister any potential command list
    registeredInterpolationCommandLists[currentBackBufferIndex] = nullptr;
    presentCount++;
    interpolationBufferIndex                                                   = presentCount % _countof(interpolationOutputs);

    //update active backbuffer and block when no buffer is available
    replacementSwapBufferIndex = presentCount % gameBufferCount;

    LeaveCriticalSection(&criticalSection);

    waitForFenceValue(presentInfo.replacementBufferFence, replacementSwapBuffers[replacementSwapBufferIndex].availabilityFenceValue, INFINITE, presentInfo.waitCallback);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface)
{
    EnterCriticalSection(&criticalSection);

    HRESULT hr = E_FAIL;

    if (riid == IID_ID3D12Resource || riid == IID_ID3D12Resource1 || riid == IID_ID3D12Resource2)
    {
        if (verifyBackbufferDuplicateResources())
        {
            ID3D12Resource* pBuffer = replacementSwapBuffers[Buffer].resource;

            pBuffer->AddRef();
            *ppSurface = pBuffer;

            hr = S_OK;
        }
    }

    LeaveCriticalSection(&criticalSection);

    return hr;
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget)
{
    return real()->SetFullscreenState(Fullscreen, pTarget);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget)
{
    return real()->GetFullscreenState(pFullscreen, ppTarget);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc)
{
    HRESULT hr = real()->GetDesc(pDesc);

    // hide interpolation swapchaindesc to keep FI transparent for ISV
    if (SUCCEEDED(hr))
    {
        //update values we changed
        pDesc->BufferCount  = gameBufferCount;
        pDesc->Flags        = gameFlags;
        pDesc->SwapEffect   = gameSwapEffect;
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    destroyReplacementResources();

    EnterCriticalSection(&criticalSection);

    const UINT fiAdjustedFlags = getInterpolationEnabledSwapChainFlags(SwapChainFlags);

    // update params expected by the application
    if (BufferCount > 0)
    {
        FFX_ASSERT(BufferCount <= FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_BUFFER_COUNT);
        gameBufferCount = BufferCount;
    }
    gameFlags       = SwapChainFlags;

    HRESULT hr = real()->ResizeBuffers(0 /* preserve count */, Width, Height, NewFormat, fiAdjustedFlags);

    LeaveCriticalSection(&criticalSection);

    return hr;
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters)
{
    return real()->ResizeTarget(pNewTargetParameters);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetContainingOutput(IDXGIOutput** ppOutput)
{
    HRESULT hr = DXGI_ERROR_INVALID_CALL;

    if (ppOutput)
    {
        *ppOutput = getMostRelevantOutputFromSwapChain(real());
        hr        = S_OK;
    }
    
    return hr;
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats)
{
    return real()->GetFrameStatistics(pStats);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetLastPresentCount(UINT* pLastPresentCount)
{
    return real()->GetLastPresentCount(pLastPresentCount);
}

// IDXGISwapChain1
HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc)
{
    HRESULT hr = real()->GetDesc1(pDesc);

    // hide interpolation swapchaindesc to keep FI transparent for ISV
    if (SUCCEEDED(hr))
    {
        //update values we changed
        pDesc->BufferCount  = gameBufferCount;
        pDesc->Flags        = gameFlags;
        pDesc->SwapEffect   = gameSwapEffect;
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc)
{
    return real()->GetFullscreenDesc(pDesc);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetHwnd(HWND* pHwnd)
{
    return real()->GetHwnd(pHwnd);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetCoreWindow(REFIID refiid, void** ppUnk)
{
    return real()->GetCoreWindow(refiid, ppUnk);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS*)
{
    return Present(SyncInterval, PresentFlags);
}

BOOL STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::IsTemporaryMonoSupported(void)
{
    return real()->IsTemporaryMonoSupported();
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput)
{
    return real()->GetRestrictToOutput(ppRestrictToOutput);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::SetBackgroundColor(const DXGI_RGBA* pColor)
{
    return real()->SetBackgroundColor(pColor);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetBackgroundColor(DXGI_RGBA* pColor)
{
    return real()->GetBackgroundColor(pColor);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::SetRotation(DXGI_MODE_ROTATION Rotation)
{
    return real()->SetRotation(Rotation);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetRotation(DXGI_MODE_ROTATION* pRotation)
{
    return real()->GetRotation(pRotation);
}

// IDXGISwapChain2
HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::SetSourceSize(UINT Width, UINT Height)
{
    return real()->SetSourceSize(Width, Height);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetSourceSize(UINT* pWidth, UINT* pHeight)
{
    return real()->GetSourceSize(pWidth, pHeight);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::SetMaximumFrameLatency(UINT MaxLatency)
{
    // store value, so correct value is returned if game asks for it
    gameMaximumFrameLatency = MaxLatency;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetMaximumFrameLatency(UINT* pMaxLatency)
{
    if (pMaxLatency)
    {
        *pMaxLatency = gameMaximumFrameLatency;
    }

    return pMaxLatency ? S_OK : DXGI_ERROR_INVALID_CALL;
}

HANDLE STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetFrameLatencyWaitableObject(void)
{
    return replacementFrameLatencyWaitableObjectHandle;
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::SetMatrixTransform(const DXGI_MATRIX_3X2_F* pMatrix)
{
    return real()->SetMatrixTransform(pMatrix);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetMatrixTransform(DXGI_MATRIX_3X2_F* pMatrix)
{
    return real()->GetMatrixTransform(pMatrix);
}

// IDXGISwapChain3
UINT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::GetCurrentBackBufferIndex(void)
{
    EnterCriticalSection(&criticalSection);

    UINT result = (UINT)replacementSwapBufferIndex;

    LeaveCriticalSection(&criticalSection);

    return result;
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT* pColorSpaceSupport)
{
    return real()->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace)
{
    switch (ColorSpace)
    {
    case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
        backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
        break;
    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ;
        break;
    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
        backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_SCRGB;
        break;
    default:
        break;
    }

    return real()->SetColorSpace1(ColorSpace);
}

HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::ResizeBuffers1(
    UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*)
{
    FFX_ASSERT_MESSAGE(false, "AMD FSR Frame interpolaton proxy swapchain: ResizeBuffers1 currently not supported.");

    return S_OK;
}

// IDXGISwapChain4
HRESULT STDMETHODCALLTYPE FrameInterpolationSwapChainDX12::SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void* pMetaData)
{
    if (Size > 0 && pMetaData != nullptr)
    {
        DXGI_HDR_METADATA_HDR10* HDR10MetaData = NULL;

        switch (Type)
        {
        case DXGI_HDR_METADATA_TYPE_NONE:
            break;
        case DXGI_HDR_METADATA_TYPE_HDR10:
            HDR10MetaData = static_cast<DXGI_HDR_METADATA_HDR10*>(pMetaData);
            break;
        case DXGI_HDR_METADATA_TYPE_HDR10PLUS:
            break;
        }

        FFX_ASSERT_MESSAGE(HDR10MetaData != NULL, "FSR3 Frame interpolaton pxory swapchain: could not initialize HDR metadata");

        if (HDR10MetaData)
        {
            minLuminance = HDR10MetaData->MinMasteringLuminance / 10000.0f;
            maxLuminance = float(HDR10MetaData->MaxMasteringLuminance);
        }
    }

    return real()->SetHDRMetaData(Type, Size, pMetaData);
}

ID3D12GraphicsCommandList* FrameInterpolationSwapChainDX12::getInterpolationCommandList()
{
    EnterCriticalSection(&criticalSection);

    ID3D12GraphicsCommandList* dx12CommandList = nullptr;

    // store active backbuffer index to the command list, used to verify list usage later
    if (interpolationEnabled) {
        ID3D12Resource* currentBackBuffer = nullptr;
        const UINT      currentBackBufferIndex = GetCurrentBackBufferIndex();
        if (SUCCEEDED(GetBuffer(currentBackBufferIndex, IID_PPV_ARGS(&currentBackBuffer))))
        {
            Dx12Commands* registeredCommands = registeredInterpolationCommandLists[currentBackBufferIndex];

            // drop if already existing
            if (registeredCommands != nullptr)
            {
                registeredCommands->drop(true);
                registeredCommands = nullptr;
            }

            registeredCommands = presentInfo.commandPool.get(presentInfo.interpolationQueue, L"getInterpolationCommandList()");
            FFX_ASSERT(registeredCommands);

            dx12CommandList = registeredCommands->reset();

            registeredInterpolationCommandLists[currentBackBufferIndex] = registeredCommands;

            SafeRelease(currentBackBuffer);
        }
    }
    
    LeaveCriticalSection(&criticalSection);

    return dx12CommandList;
}
