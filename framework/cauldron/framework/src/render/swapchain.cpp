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

#include "render/swapchain.h"
#include "render/texture.h"
#include "core/framework.h"
#include "core/uimanager.h"

namespace cauldron
{
    const wchar_t* SwapChain::s_SwapChainRTName = L"SwapChainTarget";

    SwapChain::SwapChain()
    {
        // Setup fence tracking
        uint32_t backBufferCount = GetConfig()->BackBufferCount;
        m_BackBufferFences.resize(backBufferCount);
        for (uint8_t i = 0; i < backBufferCount; ++i)
            m_BackBufferFences[i] = 0;

        // Register UI or defer until after init is done.
        std::function<void(void*)> addUiTaskFunc = [this](void*) {
            UISection* uiSection = GetUIManager()->RegisterUIElements("SwapChain");
            uiSection->RegisterUIElement<UICheckBox>("Vsync", m_VSyncEnabled);
            };
        if (GetUIManager())
            addUiTaskFunc(nullptr);
        else
        {
            Task addUiTask(addUiTaskFunc);
            GetFramework()->AddContentCreationTask(addUiTask);
        }
    }

    SwapChain::~SwapChain()
    {
        DestroySwapChainRenderTargets();
        
        delete m_pRenderTarget;

        delete m_pSwapChainRTV;
    };

    size_t SwapChain::GetBackBufferCount() 
    { 
        return m_pRenderTarget->GetBackBufferCount(); 
    }

    ResourceFormat SwapChain::GetFormat(DisplayMode displayMode)
    {
        switch (displayMode)
        {
        case DisplayMode::DISPLAYMODE_LDR:
            return ResourceFormat::RGBA8_UNORM;
        case DisplayMode::DISPLAYMODE_FSHDR_2084:
        case DisplayMode::DISPLAYMODE_HDR10_2084:
            return ResourceFormat::RGB10A2_UNORM;
        case DisplayMode::DISPLAYMODE_FSHDR_SCRGB:
        case DisplayMode::DISPLAYMODE_HDR10_SCRGB:
            return ResourceFormat::RGBA16_FLOAT;
        default:
            return ResourceFormat::Unknown;
        }
    }

    DisplayMode SwapChain::CheckAndGetDisplayModeRequested(DisplayMode DispMode)
    {
        for (const auto& it : m_SupportedDisplayModes)
        {
            if (it == DispMode)
                return it;
        }

        if (DispMode == DisplayMode::DISPLAYMODE_FSHDR_2084) {

            CauldronWarning(L"FSHDR PQ not supported, trying HDR10 PQ");
            return CheckAndGetDisplayModeRequested(DisplayMode::DISPLAYMODE_HDR10_2084);
        } else if (DispMode == DisplayMode::DISPLAYMODE_FSHDR_SCRGB) {

            CauldronWarning(L"FSHDR SCRGB not supported, trying HDR10 SCRGB");
            return CheckAndGetDisplayModeRequested(DisplayMode::DISPLAYMODE_HDR10_SCRGB);
        }

        if (DispMode != DisplayMode::DISPLAYMODE_LDR)
        {
            CauldronWarning(L"HDR modes not supported, defaulting to LDR");
        }

        return DisplayMode::DISPLAYMODE_LDR;
    }

    void SwapChain::PopulateHDRMetadataBasedOnDisplayMode()
    {
        switch (m_CurrentDisplayMode)
        {
            case DisplayMode::DISPLAYMODE_LDR:
                // Values set here make no difference on HDR wide gamut monitors
                // Monitors will not undersell their capabilities, if they can go beyond rec709 gamut and 100 nits.

                // [0, 1] in respective RGB channel maps to display gamut.
                m_HDRMetadata.RedPrimary[0] = 0.64f;
                m_HDRMetadata.RedPrimary[1] = 0.33f;
                m_HDRMetadata.GreenPrimary[0] = 0.30f;
                m_HDRMetadata.GreenPrimary[1] = 0.60f;
                m_HDRMetadata.BluePrimary[0] = 0.15f;
                m_HDRMetadata.BluePrimary[1] = 0.06f;
                m_HDRMetadata.WhitePoint[0] = 0.3127f;
                m_HDRMetadata.WhitePoint[1] = 0.3290f;

                // [0, 1] actually maps to display brightness.
                // This gets ignored, writing it for completeness
                m_HDRMetadata.MinLuminance = 0.0f;
                m_HDRMetadata.MaxLuminance = 100.0f;

                // Scene dependent
                m_HDRMetadata.MaxContentLightLevel = 2000.0f;
                m_HDRMetadata.MaxFrameAverageLightLevel = 500.0f;
                break;

            case DisplayMode::DISPLAYMODE_HDR10_2084:
                // Values set here either get clipped at display capabilities or tone and gamut mapped on the display to fit its brightness and gamut range.

                // rec 2020 primaries
                m_HDRMetadata.RedPrimary[0] = 0.708f;
                m_HDRMetadata.RedPrimary[1] = 0.292f;
                m_HDRMetadata.GreenPrimary[0] = 0.170f;
                m_HDRMetadata.GreenPrimary[1] = 0.797f;
                m_HDRMetadata.BluePrimary[0] = 0.131f;
                m_HDRMetadata.BluePrimary[1] = 0.046f;
                m_HDRMetadata.WhitePoint[0] = 0.3127f;
                m_HDRMetadata.WhitePoint[1] = 0.3290f;

                // Max nits of 500 is actually low
                // Can set this value to 1000, 2000 and 4000 based on target display and content contrast range
                // However we are trying to make sure HDR10 mode doesn't look bad on HDR displays with only 300 nits brightness, hence the low max lum value.
                m_HDRMetadata.MinLuminance = 0.0f;
                m_HDRMetadata.MaxLuminance = 500.0f;

                // Scene dependent
                m_HDRMetadata.MaxContentLightLevel = 2000.0f;
                m_HDRMetadata.MaxFrameAverageLightLevel = 500.0f;
                break;

            case DisplayMode::DISPLAYMODE_HDR10_SCRGB:
                // Same as above

                // rec 709 primaries
                m_HDRMetadata.RedPrimary[0] = 0.64f;
                m_HDRMetadata.RedPrimary[1] = 0.33f;
                m_HDRMetadata.GreenPrimary[0] = 0.30f;
                m_HDRMetadata.GreenPrimary[1] = 0.60f;
                m_HDRMetadata.BluePrimary[0] = 0.15f;
                m_HDRMetadata.BluePrimary[1] = 0.06f;
                m_HDRMetadata.WhitePoint[0] = 0.3127f;
                m_HDRMetadata.WhitePoint[1] = 0.3290f;

                // Same comment as HDR10_2084.
                m_HDRMetadata.MinLuminance = 0.0f;
                m_HDRMetadata.MaxLuminance = 500.0f;

                // Scene dependent
                m_HDRMetadata.MaxContentLightLevel = 2000.0f;
                m_HDRMetadata.MaxFrameAverageLightLevel = 500.0f;
                break;

            case DisplayMode::DISPLAYMODE_FSHDR_2084:
            case DisplayMode::DISPLAYMODE_FSHDR_SCRGB:
                // FS HDR modes should already have the monitor's primaries queried through backend API's like DXGI or VK ext

                // Scene dependent
                m_HDRMetadata.MaxContentLightLevel      = 2000.0f;
                m_HDRMetadata.MaxFrameAverageLightLevel = 500.0f;
                break;
        }
    }

    void SwapChain::DestroySwapChainRenderTargets()
    {
        if (m_pRenderTarget != nullptr)
            m_pRenderTarget->ClearResources();
    }

} // namespace cauldron
