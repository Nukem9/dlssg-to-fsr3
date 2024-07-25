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
#include "render/sampler.h"
#include "render/shaderbuilder.h"

#include <FidelityFX/host/ffx_breadcrumbs.h>


namespace cauldron
{
    class ParameterSet;
    class PipelineObject;
    class RootSignature;
    class RasterView;
    class Texture;
} // namespace cauldron

/// @defgroup FfxBreadcrumbsSample FidelityFX Breadcrumbs sample
/// Sample documentation for FidelityFX Breadcrumbs
/// 
/// @ingroup SDKEffects

/// @defgroup BreadcrumbsRM BreadcrumbsRenderModule
/// BreadcrumbsRenderModule Reference Documentation
/// 
/// @ingroup FfxBreadcrumbsSample
/// @{

/**
 * @class BreadcrumbsRenderModule
 * BreadcrumbsRenderModule handles a number of tasks related to the AMD FidelityFX Breadcrumbs Library.
 *
 * BreadcrumbsRenderModule takes care of:
 *      - creating UI section that enable users to switch between BLUR effect options: kernel size & floating point math type.
 *      - executes multiple different blur effects, including but not limited to FFX Blur.
 *      - implements a comparison mode for comparing quality and performance of FFX Blur
 *        to conventional blur implementations. Comparison mode displays the difference between
 *        two different blur effects (see blur_compare_filters_cs.hlsl). The magnitude of the
 *        difference can be amplified via UI configurable "Diff Factor".
 */
class BreadcrumbsRenderModule : public cauldron::RenderModule
{
public:
    /**
     * @brief   Constructor with default behavior.
     */
    BreadcrumbsRenderModule();
    /**
     * @brief   Tear down the FFX API Context and release all resources.
     */
    virtual ~BreadcrumbsRenderModule();

    /**
     * @brief   Initialize the FFX API Context.
     */
    void Init(const json& initData) override;

    /**
     * @brief   Render a simple triangle and crash in selected frame.
     */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

private:
    // Only single queue will be used (for DX12 just use D3D12_COMMAND_LIST_TYPE and for Vulkan queue family index).
    uint32_t                    m_GpuQueue = 0;
    // Number of crashing frame where faulty commands are submitted to GPU, causing shader hang and in result crash will be reported.
    uint64_t                    m_CrashFrame = 2800;

    bool                        m_BreadContextCreated = false;
    void*                       m_BackendScratchBuffer = nullptr;
    FfxBreadcrumbsContext       m_BreadContext;

    const cauldron::Texture*    m_pRenderTarget = nullptr;
    const cauldron::RasterView* m_pRasterView = nullptr;
    cauldron::RootSignature*    m_pRootSig = nullptr;
    cauldron::PipelineObject*   m_pPipeline = nullptr;
    cauldron::ParameterSet*     m_pParams = nullptr;

    static void ProcessDeviceRemovedEvent(void* data);
/// @}
};
