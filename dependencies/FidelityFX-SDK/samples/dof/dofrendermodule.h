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

#include "render/rendermodule.h"
#include "core/uimanager.h"

#include <FidelityFX/host/ffx_dof.h>

#include <vector>

namespace cauldron
{
    class RootSignature;
    class RasterView;
    class ParameterSet;
    class PipelineObject;
    class Texture;
    class Buffer;
}  // namespace cauldron

/// @defgroup FfxDofSample FidelityFX Depth of Field sample
/// Sample documentation for FidelityFX Depth of Field
///
/// @ingroup SDKEffects

/// @defgroup DofRM DoFRenderModule
/// DoFRenderModule Reference Documentation
///
/// @ingroup FfxDofSample
/// @{

/**
 * @class DoFRenderModule
 * DOFRenderModule handles a number of tasks related to DOF.
 *
 * DoFRenderModule takes care of:
 *      - creating UI section that enable users to select DoF quality and lens model options
 *      - applying depth of field to the color target
 */
class DoFRenderModule : public cauldron::RenderModule
{
public:
    /// Constructor with default behavior. Initializes parameters to sensible defaults for metric unit systems.
    DoFRenderModule()
        : RenderModule(L"DoFRenderModule")
    {
    }

    /// Destructs the FFX API context and releases resources
    virtual ~DoFRenderModule();

    /// Initialize FFX API context and set up UI.
    /// @param initData Not used.
    void Init(const json& initData) override;

    /// Update the DOF UI.
    /// @param deltaTime Not used.
    virtual void UpdateUI(double deltaTime) override;

    /// Dispatch the DOF effect using FFX API and update the Focus Distance slider range according to the scene bounds.
    /// @param deltaTime Not used.
    /// @param pCmdList Command list on which to dispatch.
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /// Recreate the FFX API context to resize internal resources. Called by the framework when the resolution changes.
    /// @param resInfo New resolution info.
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

private:
    void SetupFidelityFxInterface();
    /// Destroy or create the FFX API context using the currently set parameters.
    void UpdateDofContext(bool enabled);

    void DestroyDofContext();

    // Lens and quality parameters
    float m_Aperture        = 0.01f;
    float m_FocusDist       = 2.0f;
    float m_SensorSize      = 0.02f;
    float m_CocLimit        = 0.01f;  // ~10px radius at 1080p
    int   m_Quality         = 10;
    bool  m_EnableRingMerge = false;

    cauldron::UISection* m_UISection = nullptr; // weak ptr
    cauldron::UISlider<float>* m_UIFocusDist = nullptr; // weak ptr

    // Effect resources
    const cauldron::Texture* m_pColorTarget = nullptr;
    const cauldron::Texture* m_pDepthTarget = nullptr;

    // FidelityFX DoF information
    FfxDofContextDescription m_InitializationParameters = {0};
    FfxDofContext            m_DofContext;
};
