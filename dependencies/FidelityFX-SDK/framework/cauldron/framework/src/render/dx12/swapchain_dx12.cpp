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

// _DX12 define implies windows
#include "core/win/framework_win.h"
#include "misc/assert.h"

#include "render/dx12/commandlist_dx12.h"
#include "render/dx12/device_dx12.h"
#include "render/dx12/gpuresource_dx12.h"
#include "render/dx12/resourceviewallocator_dx12.h"
#include "render/dx12/swapchain_dx12.h"
#include "render/dx12/texture_dx12.h"

#include "dxheaders/include/directx/d3dx12.h"

// To dump screenshot to file
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

using namespace std::experimental;

namespace cauldron
{
    //////////////////////////////////////////////////////////////////////////
    // SwapChain
    SwapChain* SwapChain::CreateSwapchain()
    {
        return new SwapChainInternal();
    }

    SwapChainInternal::SwapChainInternal() :
        SwapChain()
    {
        // Will need config settings to initialize the swapchain
        const CauldronConfig* pConfig = GetConfig();

        // Query all connected outputs
        EnumerateOutputs();

        // Find current output window is being displayed to
        FindCurrentOutput();

        // Find all HDR modes supported by current display
        EnumerateHDRModes();

        // See if display mode requested in config is supported and return it, or default to LDR
        m_CurrentDisplayMode = CheckAndGetDisplayModeRequested(pConfig->CurrentDisplayMode);

        // Set format based on display mode
        m_SwapChainFormat = GetFormat(m_CurrentDisplayMode);

        // If config file provides a swapchainformat override, try to use it
        if (pConfig->SwapChainFormat != ResourceFormat::Unknown && pConfig->SwapChainFormat != m_SwapChainFormat)
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS FeatureData;
            ZeroMemory(&FeatureData, sizeof(FeatureData));
            DXGI_FORMAT requestedSwapchainFormat = GetDXGIFormat(pConfig->SwapChainFormat);
            D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport = {requestedSwapchainFormat, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE};
            HRESULT hr = GetDevice()->GetImpl()->DX12Device()->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));
            if (SUCCEEDED(hr) && (FormatSupport.Support1 & D3D12_FORMAT_SUPPORT1_DISPLAY) != 0)
            {
                m_SwapChainFormat = pConfig->SwapChainFormat;
            }
            else
            {
                CauldronWarning(L"The requested swapchain format from the config file cannot be used for present/display. Override is ignored.");
            }
        }

        // Set primaries based on display mode
        PopulateHDRMetadataBasedOnDisplayMode();

        MSComPtr<IDXGIFactory6> pFactory;
        CauldronThrowOnFail(CreateDXGIFactory1(IID_PPV_ARGS(&pFactory)));

        // Setup swap chain description
        m_SwapChainDesc1 = {};
        m_SwapChainDesc1.BufferCount = pConfig->BackBufferCount;
        m_SwapChainDesc1.Width = pConfig->Width;
        m_SwapChainDesc1.Height = pConfig->Height;
        m_SwapChainDesc1.Format = GetDXGIFormat(m_SwapChainFormat);
        m_SwapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        m_SwapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // use most optimized mode
        m_SwapChainDesc1.SampleDesc.Count = 1;

        // Query tearing support
        CauldronWarnOnFail(pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &m_TearingSupported, sizeof(BOOL))); // Must use sizeof(BOOL) or will not query properly
        m_SwapChainDesc1.Flags = (m_TearingSupported) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        // Set VSync status
        m_VSyncEnabled = pConfig->Vsync;

        // Create the swap chain with the given parameters
        SwapChainCreationParams creationParams;
        creationParams.DX12Desc = m_SwapChainDesc1;
        creationParams.WndHandle = GetFramework()->GetImpl()->GetHWND();
        creationParams.pFactory = pFactory;

        SwapChain* pSwapChain = this;
        GetDevice()->CreateSwapChain(pSwapChain, creationParams, CommandQueue::Graphics);

        // Retrieve various desc structs now that swapchain has been created
        m_pSwapChain->GetDesc(&m_SwapChainDesc);
        m_pSwapChain->GetDesc1(&m_SwapChainDesc1);
        m_pSwapChain->GetFullscreenDesc(&m_FullscreenDesc);

        SetHDRMetadataAndColorspace();

        // Create render targets backed by the swap chain buffers
        CreateSwapChainRenderTargets();
    }

    SwapChainInternal::~SwapChainInternal()
    {
    }

    void SwapChainInternal::OnResize(uint32_t width, uint32_t height)
    {
        // Delete our current render targets
        DestroySwapChainRenderTargets();

        // Resize
        m_SwapChainDesc1.Width = width;
        m_SwapChainDesc1.Height = height;
        CauldronThrowOnFail(m_pSwapChain->ResizeBuffers(m_SwapChainDesc1.BufferCount, width, height, GetDXGIFormat(m_SwapChainFormat), m_SwapChainDesc1.Flags));

        // Always remember to re set metadata when swapchain is recreated
        SetHDRMetadataAndColorspace();

        // Recreate render targets
        CreateSwapChainRenderTargets();
    }

    void SwapChainInternal::WaitForSwapChain()
    {
        m_CurrentBackBuffer = m_pSwapChain->GetCurrentBackBufferIndex();
        m_pRenderTarget->SetCurrentBackBufferIndex(m_CurrentBackBuffer);

        // Make sure the buffer is ready to render into
        const uint64_t fenceValue = m_BackBufferFences[m_CurrentBackBuffer];
        GetDevice()->WaitOnQueue(fenceValue, m_CreationQueue);
    }

    void SwapChainInternal::Present()
    {
        uint64_t signalValue = GetDevice()->PresentSwapChain(this);
        m_BackBufferFences[m_CurrentBackBuffer] = signalValue;
    }

    void SwapChainInternal::EnumerateOutputs()
    {
        const MSComPtr<IDXGIAdapter> adapter = GetDevice()->GetImpl()->GetAdapter();

        MSComPtr<IDXGIOutput> pOutput = nullptr;

        for (UINT outputIndex = 0; adapter->EnumOutputs(outputIndex, &pOutput) != DXGI_ERROR_NOT_FOUND; outputIndex++)
        {
            MSComPtr<IDXGIOutput6> pOutput6;
            CauldronThrowOnFail(pOutput->QueryInterface(__uuidof(IDXGIOutput6), (void**)&pOutput6));
            m_pAttachedOutputs.push_back(pOutput6);
        }
    }

    void SwapChainInternal::DumpSwapChainToFile(filesystem::path filePath)
    {
        D3D12_RESOURCE_DESC fromDesc = m_pRenderTarget->GetCurrentResource()->GetImpl()->DX12Desc();

        CD3DX12_HEAP_PROPERTIES readBackHeapProperties(D3D12_HEAP_TYPE_READBACK);

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Alignment = 0;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.Height = 1;
        bufferDesc.Width = fromDesc.Width * fromDesc.Height * GetResourceFormatStride(m_pRenderTarget->GetFormat());
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.SampleDesc.Quality = 0;

        ID3D12Resource* pResourceReadBack = nullptr;
        GetDevice()->GetImpl()->DX12Device()->CreateCommittedResource(&readBackHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pResourceReadBack));

        CommandList* pCmdList = GetDevice()->CreateCommandList(L"SwapchainToFileCL", CommandQueue::Graphics);
        Barrier barrier = Barrier::Transition(m_pRenderTarget->GetCurrentResource(), ResourceState::Present, ResourceState::CopySource);
        ResourceBarrier(pCmdList, 1, &barrier);

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout[1] = { 0 };
        uint32_t num_rows[1] = { 0 };
        UINT64 row_sizes_in_bytes[1] = { 0 };
        UINT64 uploadHeapSize = 0;
        GetDevice()->GetImpl()->DX12Device()->GetCopyableFootprints(&fromDesc, 0, 1, 0, layout, num_rows, row_sizes_in_bytes, &uploadHeapSize);

        CD3DX12_TEXTURE_COPY_LOCATION copyDest(pResourceReadBack, layout[0]);
        CD3DX12_TEXTURE_COPY_LOCATION copySrc(m_pRenderTarget->GetCurrentResource()->GetImpl()->DX12Resource(), 0);
        pCmdList->GetImpl()->DX12CmdList()->CopyTextureRegion(&copyDest, 0, 0, 0, &copySrc, nullptr);

        ID3D12Fence* pFence;
        CauldronThrowOnFail(GetDevice()->GetImpl()->DX12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));
        CauldronThrowOnFail(GetDevice()->GetImpl()->DX12CmdQueue(CommandQueue::Graphics)->Signal(pFence, 1));
        CauldronThrowOnFail(pCmdList->GetImpl()->DX12CmdList()->Close());

        ID3D12CommandList* CmdListList[] = { pCmdList->GetImpl()->DX12CmdList() };
        GetDevice()->GetImpl()->DX12CmdQueue(CommandQueue::Graphics)->ExecuteCommandLists(1, CmdListList);

        // Wait for fence
        HANDLE mHandleFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        pFence->SetEventOnCompletion(1, mHandleFenceEvent);
        WaitForSingleObject(mHandleFenceEvent, INFINITE);
        CloseHandle(mHandleFenceEvent);
        pFence->Release();

        UINT64* pTimingsBuffer = NULL;
        D3D12_RANGE range;
        range.Begin = 0;
        range.End = uploadHeapSize;
        pResourceReadBack->Map(0, &range, reinterpret_cast<void**>(&pTimingsBuffer));
        stbi_write_jpg(WStringToString(filePath.c_str()).c_str(), (int)fromDesc.Width, (int)fromDesc.Height, 4, pTimingsBuffer, 100);
        pResourceReadBack->Unmap(0, NULL);

        GetDevice()->FlushAllCommandQueues();

        // Release
        pResourceReadBack->Release();
        pResourceReadBack = nullptr;
        delete pCmdList;
    }

    void SwapChainInternal::FindCurrentOutput()
    {
        const HWND hWnd = GetFramework()->GetImpl()->GetHWND();
        RECT windowRect;
        GetWindowRect(hWnd, &windowRect);

        float bestIntersectArea = -1.0f;
        for (auto output : m_pAttachedOutputs)
        {
            DXGI_OUTPUT_DESC outputDesc;
            CauldronThrowOnFail(output->GetDesc(&outputDesc));
            RECT outputRect = outputDesc.DesktopCoordinates;

            if (IntersectWindowAndOutput(windowRect, outputRect, bestIntersectArea))
            {
                m_pCurrentOutput = output;
            }
        }
    }

    void SwapChainInternal::EnumerateHDRModes()
    {
        // If we are attached via remote desktop, there will be no current output, so just return (will assume LDR display)
        if (!m_pCurrentOutput)
            return;

        m_SupportedDisplayModes.clear();

        DXGI_OUTPUT_DESC1 outputDesc1;
        CauldronThrowOnFail(m_pCurrentOutput->GetDesc1(&outputDesc1));

        if (outputDesc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
        {
            m_SupportedDisplayModes.push_back(DisplayMode::DISPLAYMODE_HDR10_2084);
            m_SupportedDisplayModes.push_back(DisplayMode::DISPLAYMODE_HDR10_SCRGB);

            // Init HDR metadata with queried values in DXGI Output Desc
            // Will be used as is for FS HDR mdoe
            m_HDRMetadata.RedPrimary[0]   = outputDesc1.RedPrimary[0];
            m_HDRMetadata.RedPrimary[1]   = outputDesc1.RedPrimary[1];
            m_HDRMetadata.GreenPrimary[0] = outputDesc1.GreenPrimary[0];
            m_HDRMetadata.GreenPrimary[1] = outputDesc1.GreenPrimary[1];
            m_HDRMetadata.BluePrimary[0]  = outputDesc1.BluePrimary[0];
            m_HDRMetadata.BluePrimary[0]  = outputDesc1.BluePrimary[1];
            m_HDRMetadata.WhitePoint[0]   = outputDesc1.WhitePoint[0];
            m_HDRMetadata.WhitePoint[1]   = outputDesc1.WhitePoint[1];
            m_HDRMetadata.MinLuminance    = outputDesc1.MinLuminance;
            m_HDRMetadata.MaxLuminance    = outputDesc1.MaxLuminance;

            CheckFSHDRSupport();
        }
    }

    void SwapChainInternal::CheckFSHDRSupport()
    {
        // Check FS2 HDR feature if AGS is enabled
        if (GetDevice()->GetImpl()->GetAGSContext())
        {
            CauldronAssert(ASSERT_WARNING,
                           GetDevice()->GetImpl()->GetAGSGpuInfo()->numDevices == 1,
                           L"Following AGS Freesync Premium Pro HDR feature enablement assumes single GPU setup");

            for (int32_t i = 0; i < GetDevice()->GetImpl()->GetAGSGpuInfo()->numDevices; i++)
            {
                const AGSDeviceInfo& device = GetDevice()->GetImpl()->GetAGSGpuInfo()->devices[i];

                if (StringToWString(std::string(device.adapterString)) == std::wstring(GetDevice()->GetDeviceName()))
                {
                    // Find display, app window is rendering to in ags display list
                    // Unfortunately we cannot use m_pCurrentOutput
                    const HWND hWnd = GetFramework()->GetImpl()->GetHWND();
                    int        displayIndexAGS   = -1;
                    float      bestIntersectArea = -1.0f;
                    for (int j = 0; j < device.numDisplays; j++)
                    {
                        RECT windowRect;
                        GetWindowRect(hWnd, &windowRect);
                        AGSRect AGSmonitorRect = device.displays[j].currentResolution;
                        RECT    monitorRect    = {AGSmonitorRect.offsetX,
                                            AGSmonitorRect.offsetY,
                                            AGSmonitorRect.offsetX + AGSmonitorRect.width,
                                            AGSmonitorRect.offsetY + AGSmonitorRect.height};
                        if (IntersectWindowAndOutput(windowRect, monitorRect, bestIntersectArea))
                        {
                            displayIndexAGS = j;
                        }
                    }

                    CauldronAssert(ASSERT_ERROR, displayIndexAGS != -1, L"AGS could not find monitor GPU is rendering to.");   

                    // Check for FS2 HDR support
                    if (displayIndexAGS != -1 && device.displays[displayIndexAGS].freesyncHDR)
                    {
                        m_SupportedDisplayModes.push_back(DisplayMode::DISPLAYMODE_FSHDR_2084);
                        m_SupportedDisplayModes.push_back(DisplayMode::DISPLAYMODE_FSHDR_SCRGB);
                        break;
                    }
                }
            }
        }
    }

    void SwapChainInternal::CreateSwapChainRenderTargets()
    {
        // Create render targets backed by the swap chain buffers
        std::vector<GPUResource*> resourceArray;
        resourceArray.reserve(m_SwapChainDesc1.BufferCount);
        for (uint32_t i = 0; i < m_SwapChainDesc1.BufferCount; i++)
        {
            ID3D12Resource* pBackBuffer;
            CauldronThrowOnFail(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));

            std::wstring name = L"BackBuffer ";
            name += std::to_wstring(i);
            pBackBuffer->SetName(name.c_str());

            GPUResourceInitParams initParams = {};
            initParams.pResource = pBackBuffer;
            initParams.type = GPUResourceType::Swapchain;

            // Create the GPU resource
            resourceArray.push_back(GPUResource::CreateGPUResource(name.c_str(), nullptr, ResourceState::Present, &initParams, true));
        }

        TextureDesc rtDesc = TextureDesc::Tex2D(SwapChain::s_SwapChainRTName, m_SwapChainFormat, m_SwapChainDesc1.Width, m_SwapChainDesc1.Height, 1, 0, ResourceFlags::AllowRenderTarget);
        if (m_pRenderTarget == nullptr)
            m_pRenderTarget = new SwapChainRenderTarget(&rtDesc, resourceArray);
        else
            m_pRenderTarget->Update(&rtDesc, resourceArray);

        // Map the RTVs
        CauldronAssert(ASSERT_CRITICAL, m_pSwapChainRTV == nullptr || m_pSwapChainRTV->GetCount() == m_SwapChainDesc1.BufferCount, L"SwapChain RTV has a wrong size");
        if (m_pSwapChainRTV == nullptr)
            GetResourceViewAllocator()->AllocateCPURenderViews(&m_pSwapChainRTV, m_SwapChainDesc1.BufferCount);
        for (uint32_t i = 0; i < m_SwapChainDesc1.BufferCount; ++i)
            m_pSwapChainRTV->BindTextureResource(m_pRenderTarget->GetResource(i), m_pRenderTarget->GetDesc(), ResourceViewType::RTV, ViewDimension::Texture2D, 0, 1, 0, i);
    }

    // Compute the overlay area of two rectangles, A and B.
    // (ax1, ay1) = left-top coordinates of A; (ax2, ay2) = right-bottom coordinates of A
    // (bx1, by1) = left-top coordinates of B; (bx2, by2) = right-bottom coordinates of B
    // based on https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HDR/src/D3D12HDR.cpp
    //--------------------------------------------------------------------------------------
    inline LONG ComputeIntersectionArea(int ax1, int ay1, int ax2, int ay2, int bx1, int by1, int bx2, int by2)
    {
        return (std::max)(0, (std::min)(ax2, bx2) - (std::max)(ax1, bx1)) * (std::max)(0, (std::min)(ay2, by2) - (std::max)(ay1, by1));
    }

    bool SwapChainInternal::IntersectWindowAndOutput(const RECT& windowRect, const RECT& outputRect, float& bestIntersectArea)
    {
        LONG ax1 = windowRect.left;
        LONG ay1 = windowRect.top;
        LONG ax2 = windowRect.right;
        LONG ay2 = windowRect.bottom;

        LONG bx1 = outputRect.left;
        LONG by1 = outputRect.top;
        LONG bx2 = outputRect.right;
        LONG by2 = outputRect.bottom;

        // Compute the intersection
        LONG intersectArea = ComputeIntersectionArea(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
        if (intersectArea > bestIntersectArea)
        {
            bestIntersectArea = static_cast<float>(intersectArea);
            return true;
        }

        return false;
    }

    void SwapChainInternal::SetHDRMetadataAndColorspace()
    {
        DXGI_HDR_METADATA_HDR10 HDR10MetaData = {};

        // Chroma values are normalized to 50,000.
        HDR10MetaData.RedPrimary[0] = static_cast<UINT16>(m_HDRMetadata.RedPrimary[0] * 50000.0f);
        HDR10MetaData.RedPrimary[1] = static_cast<UINT16>(m_HDRMetadata.RedPrimary[1] * 50000.0f);
        HDR10MetaData.GreenPrimary[0] = static_cast<UINT16>(m_HDRMetadata.GreenPrimary[0] * 50000.0f);
        HDR10MetaData.GreenPrimary[1] = static_cast<UINT16>(m_HDRMetadata.GreenPrimary[1] * 50000.0f);
        HDR10MetaData.BluePrimary[0] = static_cast<UINT16>(m_HDRMetadata.BluePrimary[0] * 50000.0f);
        HDR10MetaData.BluePrimary[1] = static_cast<UINT16>(m_HDRMetadata.BluePrimary[1] * 50000.0f);
        HDR10MetaData.WhitePoint[0] = static_cast<UINT16>(m_HDRMetadata.WhitePoint[0] * 50000.0f);
        HDR10MetaData.WhitePoint[1] = static_cast<UINT16>(m_HDRMetadata.WhitePoint[1] * 50000.0f);

        // Max luminance value is absolute.
        HDR10MetaData.MaxMasteringLuminance = static_cast<UINT>(m_HDRMetadata.MaxLuminance);

        // Min luminance value is normalized to 10,000.
        HDR10MetaData.MinMasteringLuminance = static_cast<UINT>(m_HDRMetadata.MinLuminance * 10000.0f);

        // Max content and frame average light level values are absolute.
        HDR10MetaData.MaxContentLightLevel = static_cast<UINT16>(m_HDRMetadata.MaxContentLightLevel);
        HDR10MetaData.MaxFrameAverageLightLevel = static_cast<UINT16>(m_HDRMetadata.MaxFrameAverageLightLevel);

        // Set HDR meta data.
        CauldronThrowOnFail(m_pSwapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &HDR10MetaData));

        // Set color space.
        switch (m_CurrentDisplayMode)
        {
            case DisplayMode::DISPLAYMODE_LDR:
                CauldronThrowOnFail(m_pSwapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709));
                break;
            case DisplayMode::DISPLAYMODE_FSHDR_2084:
            case DisplayMode::DISPLAYMODE_HDR10_2084:
                // FS HDR mode will only use PQ rec2020 as a final swapchain back buffer "transport container" to the driver
                // It is not to be confused as its required colour space and transfer function
                // We tone and gamut map FS HDR to display's native capabilities
                CauldronThrowOnFail(m_pSwapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020));
                break;
            case DisplayMode::DISPLAYMODE_FSHDR_SCRGB:
            case DisplayMode::DISPLAYMODE_HDR10_SCRGB:
                CauldronThrowOnFail(m_pSwapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709));
                break;
        }
    }

    #include <dwmapi.h>
    #pragma comment(lib, "Dwmapi.lib")

    RECT clientRectToScreenSpace(HWND const hWnd)
    {
        RECT rc{0};
        if (GetClientRect(hWnd, &rc) == TRUE)
        {
                if (MapWindowPoints(hWnd, nullptr, reinterpret_cast<POINT*>(&rc), 2) == 0)
                {
                }
        }

        return rc;
    }

    bool isDirectFlip(IDXGISwapChain* swapChain, RECT monitorExtents)
    {
        bool bResult = false;

        IDXGIOutput* dxgiOutput = nullptr;
        if (SUCCEEDED(swapChain->GetContainingOutput(&dxgiOutput)))
        {
            IDXGIOutput6* dxgiOutput6 = nullptr;
            if (SUCCEEDED(dxgiOutput->QueryInterface(IID_PPV_ARGS(&dxgiOutput6))))
            {
                UINT hwSupportFlags = 0;
                if (SUCCEEDED(dxgiOutput6->CheckHardwareCompositionSupport(&hwSupportFlags)))
                {
                    // check support in fullscreen mode
                    if (hwSupportFlags & DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN)
                    {
                        DXGI_SWAP_CHAIN_DESC desc{};
                        if (SUCCEEDED(swapChain->GetDesc(&desc)))
                        {
                            RECT windowExtents = clientRectToScreenSpace(desc.OutputWindow);
                            bResult |= memcmp(&windowExtents, &monitorExtents, sizeof(windowExtents)) == 0;
                        }
                    }

                    // check support in windowed mode
                    if (hwSupportFlags & DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_WINDOWED)
                    {
                        bResult |= true;
                    }
                }

                dxgiOutput6->Release();
            }

            dxgiOutput->Release();
        }
        return bResult;
    }
    void SwapChainInternal::GetRefreshRate(double* outRefreshRate)
    {
        double dwsRate  = 1000.0;
        *outRefreshRate = 1000.0;

        bool         bIsPotentialDirectFlip = false;
        IDXGIOutput* dxgiOutput             = nullptr;
        BOOL         isFullscreen           = false;
        m_pSwapChain->GetFullscreenState(&isFullscreen, &dxgiOutput);

        if (!isFullscreen)
        {
            DWM_TIMING_INFO compositionTimingInfo{};
            compositionTimingInfo.cbSize = sizeof(DWM_TIMING_INFO);
            double  monitorRefreshRate   = 0.0f;
            HRESULT hr                   = DwmGetCompositionTimingInfo(nullptr, &compositionTimingInfo);
            if (SUCCEEDED(hr))
            {
                dwsRate = double(compositionTimingInfo.rateRefresh.uiNumerator) / compositionTimingInfo.rateRefresh.uiDenominator;
            }
            m_pSwapChain->GetContainingOutput(&dxgiOutput);
        }

        // if FS this should be the monitor used for FS, in windowed the window containing the main portion of the output
        if (dxgiOutput)
        {
            IDXGIOutput1* dxgiOutput1 = nullptr;
            if (SUCCEEDED(dxgiOutput->QueryInterface(IID_PPV_ARGS(&dxgiOutput1))))
            {
                DXGI_OUTPUT_DESC outputDes{};
                if (SUCCEEDED(dxgiOutput->GetDesc(&outputDes)))
                {
                    MONITORINFOEXW info{};
                    info.cbSize = sizeof(info);
                    if (GetMonitorInfoW(outputDes.Monitor, &info) != 0)
                    {
                        bIsPotentialDirectFlip = isDirectFlip(m_pSwapChain.Get(), info.rcMonitor);

                        UINT32 numPathArrayElements, numModeInfoArrayElements;
                        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, &numModeInfoArrayElements) == ERROR_SUCCESS)
                        {
                            std::vector<DISPLAYCONFIG_PATH_INFO> pathArray(numPathArrayElements);
                            std::vector<DISPLAYCONFIG_MODE_INFO> modeInfoArray(numModeInfoArrayElements);
                            if (QueryDisplayConfig(
                                    QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, pathArray.data(), &numModeInfoArrayElements, modeInfoArray.data(), nullptr) ==
                                ERROR_SUCCESS)
                            {
                                bool rateFound = false;
                                // iterate through all the paths until find the exact source to match
                                for (size_t i = 0; i < pathArray.size() && !rateFound; i++)
                                {
                                    const DISPLAYCONFIG_PATH_INFO&   path = pathArray[i];
                                    DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
                                    sourceName.header = {
                                        DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(sourceName), path.sourceInfo.adapterId, path.sourceInfo.id};

                                    if (DisplayConfigGetDeviceInfo(&sourceName.header) == ERROR_SUCCESS)
                                    {
                                        if (wcscmp(info.szDevice, sourceName.viewGdiDeviceName) == 0)
                                        {
                                            const DISPLAYCONFIG_RATIONAL& rate = path.targetInfo.refreshRate;
                                            if (rate.Denominator > 0)
                                            {
                                                double refrate  = (double)rate.Numerator / (double)rate.Denominator;
                                                *outRefreshRate = refrate;

                                                // we found a valid rate?
                                                rateFound = (refrate > 0.0);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                dxgiOutput1->Release();
            }
            dxgiOutput->Release();

            if (!bIsPotentialDirectFlip)
            {
                *outRefreshRate = std::min(*outRefreshRate, dwsRate);
            }
        }
    }

} // namespace cauldron

#endif // #if defined(_DX12)
