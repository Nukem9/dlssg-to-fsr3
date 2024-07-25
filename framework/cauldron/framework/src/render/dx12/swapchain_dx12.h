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

#include "render/swapchain.h"
#include "render/dx12/defines_dx12.h"

#include <dxgi1_6.h>

namespace cauldron
{
    class SwapChainInternal final : public SwapChain
    {
    public:
        const IDXGISwapChain4* DX12SwapChain() const { return m_pSwapChain.Get(); }
        IDXGISwapChain4* DX12SwapChain() { return m_pSwapChain.Get(); }

        void SetDXGISwapChain(IDXGISwapChain4* pDxgiSwapChain)
        {
            if (pDxgiSwapChain)
            {
                m_pSwapChain = pDxgiSwapChain;
                CreateSwapChainRenderTargets();
            }
            else
            {
                DestroySwapChainRenderTargets();
                m_pSwapChain = pDxgiSwapChain;
            }
        }

        virtual void GetLastPresentCount(UINT* pLastPresentCount)
        {
            m_pSwapChain->GetLastPresentCount(pLastPresentCount);
        }
        virtual void GetRefreshRate(double* outRefreshRate);

        void OnResize(uint32_t width, uint32_t height) override;
        void WaitForSwapChain() override;
        void Present() override;

        void DumpSwapChainToFile(std::experimental::filesystem::path filePath) override;

        void EnumerateOutputs();
        void FindCurrentOutput();
        void EnumerateHDRModes();
        void CheckFSHDRSupport();
        void SetHDRMetadataAndColorspace() override;

        SwapChainInternal* GetImpl() override { return this; }
        const SwapChainInternal* GetImpl() const override { return this; }

        const DXGI_SWAP_CHAIN_DESC& DX12SwapChainDesc() const { return m_SwapChainDesc; }
        const DXGI_SWAP_CHAIN_DESC1& DX12SwapChainDesc1() const { return m_SwapChainDesc1; }
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC& DX12SwapChainFullScreenDesc() const { return m_FullscreenDesc; }

    private:
        friend class SwapChain;
        SwapChainInternal();
        virtual ~SwapChainInternal();

        virtual void CreateSwapChainRenderTargets() override;

        bool IntersectWindowAndOutput(const RECT& windowRect, const RECT& outputRect, float& bestIntersectArea);

    private:
        // Internal members
        friend class DeviceInternal;
        MSComPtr<IDXGISwapChain4>           m_pSwapChain    = nullptr;


        DXGI_SWAP_CHAIN_DESC                m_SwapChainDesc = {};
        DXGI_SWAP_CHAIN_DESC1               m_SwapChainDesc1 = {};
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC     m_FullscreenDesc = {};

        std::vector<MSComPtr<IDXGIOutput6>> m_pAttachedOutputs;
        MSComPtr<IDXGIOutput6>              m_pCurrentOutput;
    };

} // namespace cauldron

#endif // #if defined(_DX12)
