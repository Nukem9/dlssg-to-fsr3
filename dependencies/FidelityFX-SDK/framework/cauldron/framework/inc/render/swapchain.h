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
#include "render/resourceview.h"
#include "shaders/shadercommon.h"

namespace cauldron
{
    class SwapChainRenderTarget;

    /// Per platform/API implementation of <c><i>SwapChain</i></c>
    ///
    /// @ingroup CauldronRender
    class SwapChainInternal;

    /**
     * @class SwapChain
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of the swapchain object. Interface for all
     * presentation-related interfaces.
     *
     * @ingroup CauldronRender
     */
    class SwapChain
    {
    public:
        /// The name associated with swapchain resources
        ///
        static const wchar_t* s_SwapChainRTName;

        /**
         * @brief   SwapChain instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static SwapChain* CreateSwapchain();

        /**
         * @brief   Destruction.
         */
        virtual ~SwapChain();

        /**
         * @brief   Returns the swap chain resource's format.
         */
        ResourceFormat GetSwapChainFormat() const { return m_SwapChainFormat; }

        /**
         * @brief   Returns the configured backbuffer count for this run.
         */
        size_t GetBackBufferCount();

        /**
         * @brief   Returns the current <c><i>SwapChainRenderTarget</i></c>.
         */
        SwapChainRenderTarget* GetBackBufferRT() { return m_pRenderTarget; }

        /**
         * @brief   Returns the current back buffer's render target view <c><i>ResourceViewInfo</i></c>.
         */
        const ResourceViewInfo GetBackBufferRTV() const { return GetBackBufferRTV(m_CurrentBackBuffer); }

        /**
         * @brief   Returns the specified back buffer's render target view <c><i>ResourceViewInfo</i></c>.
         */
        const ResourceViewInfo GetBackBufferRTV(uint8_t idx) const { return m_pSwapChainRTV->GetViewInfo(static_cast<uint32_t>(idx)); }

        /**
         * @brief   Returns the current back buffer's index.
         */
        uint8_t GetBackBufferIndex() const { return m_CurrentBackBuffer; }

        /**
         * @brief   Returns the swap chain's configured <c><i>DisplayMode</i></c>.
         */
        const DisplayMode GetSwapChainDisplayMode() const { return m_CurrentDisplayMode; }

        /**
         * @brief   Returns the swap chain's configured <c><i>HDRMetadata</i></c>.
         */
        const HDRMetadata& GetHDRMetaData() const { return m_HDRMetadata; }

        /**
         * @brief   Callback invoked while processing OnResize events.
         */
        virtual void OnResize(uint32_t width, uint32_t height) = 0;

        /**
         * @brief   Waits until the last submitted swap chain has finished presenting. Only waits when we run too far ahead.
         */
        virtual void WaitForSwapChain() = 0;

        /**
         * @brief   Executes device presentation of the swapchain.
         */
        virtual void Present() = 0;

        /**
         * @brief   Indicates if this is a replacement frame interpolation swapchain
         */
        virtual bool IsFrameInterpolation() const { return false; }

        /**
         * @brief   Gets the last present count for the swapchain.
         */
        virtual void GetLastPresentCount(UINT* pLastPresentCount) { *pLastPresentCount = 0; }

        /**
         * @brief   Gets the current refresh rate for the swapchain.
         */
        virtual void GetRefreshRate(double* outRefreshRate) { *outRefreshRate = 0.0; }

        /**
         * @brief   Creates a screenshot of the current swap chain.
         */
        virtual void DumpSwapChainToFile(std::experimental::filesystem::path filePath) = 0;

        /**
         * @brief   Verifies if requested display mode can be supported.
         */
        DisplayMode CheckAndGetDisplayModeRequested(DisplayMode DispMode);

        /**
         * @brief   Prepares the HDRMetadata based on the selected display mode for the run.
         */
        void PopulateHDRMetadataBasedOnDisplayMode();

        /**
         * @brief   Calculates and sets HDRMetadata and color space information.
         */
        virtual void SetHDRMetadataAndColorspace() = 0;

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        virtual SwapChainInternal* GetImpl() = 0;
        virtual const SwapChainInternal* GetImpl() const = 0;

    private:
        // No copy, No move
        NO_COPY(SwapChain)
        NO_MOVE(SwapChain)

    protected:

        SwapChain();

        // Helper to get formats
        ResourceFormat GetFormat(DisplayMode displayMode);

        virtual void CreateSwapChainRenderTargets() = 0;
        void DestroySwapChainRenderTargets();

        std::vector<uint64_t>  m_BackBufferFences   = {};
        uint8_t                m_CurrentBackBuffer  = 0;
        std::vector<DisplayMode> m_SupportedDisplayModes = {};
        DisplayMode            m_CurrentDisplayMode = DisplayMode::DISPLAYMODE_LDR;
        HDRMetadata            m_HDRMetadata = {};
        CommandQueue           m_CreationQueue = CommandQueue::Graphics;
        bool                   m_TearingSupported   = false;
        bool                   m_VSyncEnabled       = false;

        ResourceFormat         m_SwapChainFormat    = ResourceFormat::Unknown;
        ResourceView*          m_pSwapChainRTV      = nullptr;
        SwapChainRenderTarget* m_pRenderTarget   = nullptr;
    };

} // namespace cauldron
