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
#include <FidelityFX/host/ffx_parallelsort.h>

namespace cauldron
{
    class Buffer;
    class CopyResource;
    class ParameterSet;
    class PipelineObject;
    class RasterView;
    class RootSignature;
    class Texture;
}

/// @defgroup FfxParallelSortSample FidelityFX Parallel Sort sample
/// Sample documentation for FidelityFX Parallel Sort
///
/// @ingroup SDKEffects

/// @defgroup ParallelSortRM ParallelSortRenderModule
/// ParallelSortRenderModule Reference Documentation
///
/// @ingroup FfxParallelSortSample
/// @{

/**
 * @class ParallelSortRenderModule
 * ParallelSortRenderModule handles a number of tasks related to Parallel Sort.
 *
 * ParallelSortRenderModule takes care of:
 *      - creating UI section that enable users to switch between options of Parallel Sort
 *      - performs sorting of key and (optional) payload using FidelityFX ParallelSort Effect Component
 *      - displays sort result validation (use sorted keys to re-construct a texture image properly)
 */
class ParallelSortRenderModule : public cauldron::RenderModule
{
public:
    /**
     * @brief   Constructor with default behavior.
     */
    ParallelSortRenderModule() : RenderModule(L"ParallelSortRenderModule") {}

    /**
     * @brief   Tear down the FFX API Context and release resources.
     */
    virtual ~ParallelSortRenderModule();

    /**
     * @brief   Initialize FFX API Context, creates randomized key/payload data to sort, 
     *          loads required textures, and setup UI section for Parallel Sort.
     */
    void Init(const json& initData) override;

    /**
     * @brief   Setup input resources and parameters FFX API needs this frame and then call the FFX Dispatch to sort the keys (and payload).
     */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * @brief Called by the framework when resolution changes.
     */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

private:

    enum ResolutionSize
    {
        ResSize_1080 = 0,
        ResSize_1440,
        ResSize_4k,

        ResSize_Count
    };

    static const uint32_t s_NumKeys[ResSize_Count];
    static_assert(ResSize_Count == 3, "Number of resolutions supported expected to be 3. Modify s_NumKeys if changed.");

    const cauldron::Buffer* m_pKeysToSort = nullptr;
    const cauldron::Buffer* m_pPayloadToSort = nullptr;

    const cauldron::Buffer* m_pUnsortedBuffers[ResSize_Count] = { nullptr };
    const cauldron::CopyResource* m_pCopyResources[ResSize_Count] = { nullptr };
    const cauldron::Texture* m_pValidationTextures[ResSize_Count] = { nullptr };

    void InitFfxContext();

    void DestroyFfxContext();

    /**
     * @brief   Callback for texture loading so we can complete parameter binding and mark the module "ready"
     */
    void TextureLoadComplete(const std::vector<const cauldron::Texture*>& textureList, void*);

    /**
     * @brief   Destroys and recreates FfxParallelSortContext to reflect changes request from the UI
     */
    void ResetParallelSortContext();

    // FidelityFX Parallel Sort context
    FfxParallelSortContextDescription m_InitializationParameters = {};
    FfxParallelSortContext            m_ParallelSortContext;

    int32_t     m_ParallelSortResolutions           = 1;
    int32_t     m_ParallelSortPresentationMode      = 0;
    bool        m_ParallelSortRenderSortedKeys      = false;
    bool        m_ParallelSortPayload               = false;
    bool        m_ParallelSortIndirectExecution     = false;

    cauldron::RootSignature*    m_pRootSignature    = nullptr;
    cauldron::PipelineObject*   m_pPipelineObj      = nullptr;
    cauldron::ParameterSet*     m_pParameters       = nullptr;
    const cauldron::Texture*    m_pRenderTarget     = nullptr;
    const cauldron::RasterView* m_pRasterView       = nullptr;

};
/// @}
