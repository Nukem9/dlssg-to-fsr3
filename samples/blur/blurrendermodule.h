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

#include <FidelityFX/host/ffx_blur.h>

#include <tuple>
#include <vector>

namespace cauldron
{
    class ParameterSet;
    class PipelineObject;
    class RootSignature;
    class Texture;
} // namespace cauldron

/// @defgroup FfxBlurSample FidelityFX Blur sample
/// Sample documentation for FidelityFX Blur
/// 
/// @ingroup SDKEffects

/// @defgroup BlurRM BlurRenderModule
/// BlurRenderModule reference documentation
/// 
/// @ingroup FfxBlurSample
/// @{

/**
 * @class BlurRenderModule
 * BlurRenderModule handles a number of tasks related to Blur.
 *
 * BlurRenderModule takes care of:
 *      - creating UI section that enable users to switch between BLUR effect options: kernel size & floating point math type.
 *      - executes multiple different blur effects, including but not limited to FFX Blur.
 *      - implements a comparison mode for comparing quality and performance of FFX Blur
 *        to conventional blur implementations. Comparison mode displays the difference between
 *        two different blur effects (see blur_compare_filters_cs.hlsl). The magnitude of the
 *        difference can be amplified via UI configurable "Diff Factor".
 */
class BlurRenderModule : public cauldron::RenderModule
{
public:
    /**
     * @brief   Constructor with default behavior.
     */
    BlurRenderModule();
    /**
     * @brief   Tear down the FFX API Context and release all resources.
     */
    virtual ~BlurRenderModule();

    /**
     * @brief   Initialize UI, also the FFX API Context and the other "conventional" blur effects. 
     */
    void Init(const json& initData) override;

    /**
     * @brief   Execute the currently selected blur effect or execute the comparison mode shaders.
     */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * @brief Called by the framework when resolution changes.
     */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

private:

    enum Algorithm
    {
        NONE = 0,
        FIDELITYFX_BLUR_GAUSSIAN,
        SINGLE_PASS_BOX_FILTER,                 // naive NxN filter
        MULTI_PASS_SEPARABLE_FILTER,            // Industry-standard 1xN filter, 2 passes, baked kernels
        MULTI_PASS_SEPARABLE_FILTER_TRANSPOSE,  // 2x horizontal passes, both transposing the output NxM -> MxN
        NUM_GAUSSIAN_BLUR_ALGORITHMS,
    };

    /**
     * @brief   Create and initialize m_BackendInterface.
     */
    void InitFfxBackend();
    /**
     * @brief   Create and initialize textures required for blur and comparison mode effects.
     */
    void InitTextures();
    /**
     * @brief   Create and initialize all of the blur compute pipelines and the comparison mode compute pipeline.
     */
    void InitPipelines();

    void CreatePassThroughPipeline(const wchar_t* computeShaderFilename);
    void CreateSinglePassBoxFilterPipelines(
        const wchar_t** ppSigmas, size_t sigmasCount, const wchar_t** ppKernelSizes, size_t kernelSizesCount, const wchar_t* comparisonFiltersComputeShader);
    void CreateMultiPassSeparableFilterPipelines(
        const wchar_t** ppSigmas, size_t sigmasCount, const wchar_t** ppKernelSizes, size_t kernelSizesCount, const wchar_t* comparisonFiltersComputeShader);
    void CreateMultiPassSeparableTransposeFilterPipelines(
        const wchar_t** ppSigmas, size_t sigmasCount, const wchar_t** ppKernelSizes, size_t kernelSizesCount, const wchar_t* comparisonFiltersComputeShader);

    cauldron::PipelineObject* CreatePipeline(
        cauldron::RootSignature* pRootSignature,
        const std::wstring& pipelineName,
        const std::wstring& shaderFile,
        const std::wstring& entryFunc,
        cauldron::DefineList& defines);

    void CreateBlurContexts();
    void DestroyBlurContexts();

    size_t m_KernelSizesCount = 0u;

    int32_t m_CurrentAlgorithm1     = 1;
    int32_t m_CurrentGaussianSigma1 = 2;
    int32_t m_CurrentKernelSize1    = 6;  // defaults to 17x17
    int32_t m_CurrentFpMath1        = 1;

    FfxInterface m_BackendInterface = {0};

    FfxBlurContext m_BlurContext1        = {};
    bool           m_BlurContext1Created = false;
    FfxBlurContext m_BlurContext2        = {};
    bool           m_BlurContext2Created = false;

    // 2nd Blur context is used for the comparison mode.
    int32_t m_CurrentAlgorithm2     = 1;
    int32_t m_CurrentGaussianSigma2 = 1;
    int32_t m_CurrentKernelSize2    = 2;  // defaults to 9x9
    int32_t m_CurrentFpMath2        = 1;

    cauldron::RootSignature* m_pFilterPipelineRootSig = nullptr;

    cauldron::PipelineObject*              m_pPassThroughPipeline = nullptr;
    std::vector<cauldron::PipelineObject*> m_SinglePassBoxFilterPipelinesFp32;
    std::vector<cauldron::PipelineObject*> m_SinglePassBoxFilterPipelinesFp16;
    std::vector<cauldron::PipelineObject*> m_MultiPassSeparableFilterPipelinesFp32;
    std::vector<cauldron::PipelineObject*> m_MultiPassSeparableFilterPipelinesFp16;
    std::vector<cauldron::PipelineObject*> m_MultiPassSeparableTransposeFilterPipelinesFp32;
    std::vector<cauldron::PipelineObject*> m_MultiPassSeparableTransposeFilterPipelinesFp16;

    struct ParameterSets
    {
        ~ParameterSets();

        cauldron::ParameterSet* pNormalModeParams = nullptr;

        cauldron::ParameterSet* pComparisonModeParams1 = nullptr;
        cauldron::ParameterSet* pComparisonModeParams2 = nullptr;
    };

    ParameterSets m_SinglePassParams;

    struct MultiPassParameterSets
    {
        ~MultiPassParameterSets();

        cauldron::ParameterSet* pPass1NormalModeParams = nullptr;
        cauldron::ParameterSet* pPass2NormalModeParams = nullptr;

        cauldron::ParameterSet* pPass1ComparisonModeParams1 = nullptr;
        cauldron::ParameterSet* pPass2ComparisonModeParams1 = nullptr;

        cauldron::ParameterSet* pPass1ComparisonModeParams2 = nullptr;
        cauldron::ParameterSet* pPass2ComparisonModeParams2 = nullptr;
    };

    MultiPassParameterSets m_MultiPassParams;
    MultiPassParameterSets m_MultiPassTransposeParams;

    cauldron::RootSignature*  m_pComparisonPipelineRootSig = nullptr;
    cauldron::PipelineObject* m_pComparisonPipeline        = nullptr;
    cauldron::ParameterSet*   m_pComparisonPipelineParams  = nullptr;

    bool m_ComparisonModeEnabled = false;
    bool m_EnableFilterOptions1  = true;
    bool m_EnableFilterOptions2  = false;
    bool m_RebuildShaders = false;

    const cauldron::Texture* m_pInput                = nullptr;
    const cauldron::Texture* m_pPass1Output          = nullptr;
    const cauldron::Texture* m_pTransposePass1Output = nullptr;
    const cauldron::Texture* m_pOutput               = nullptr;

    const cauldron::Texture* m_pComparisonOutput1 = nullptr;
    const cauldron::Texture* m_pComparisonOutput2 = nullptr;

    struct Constants
    {
        uint32_t Width;
        uint32_t Height;
    };

    void ExecutePassThrough(cauldron::CommandList* pCmdList, const wchar_t* pProfile, cauldron::ParameterSet* pParamSet);

    void ExecuteSinglePassBoxFilter(cauldron::CommandList*  pCmdList,
                                    const wchar_t*          pProfile,
                                    cauldron::ParameterSet* pParameterSet,
                                    int32_t                 kernelPerm,
                                    int32_t                 kernelSize,
                                    FfxBlurFloatPrecision   floatPrecision);

    using ParameterSetPair = std::pair<cauldron::ParameterSet*, cauldron::ParameterSet*>;

    void ExecuteMultiPassFilter(cauldron::CommandList* pCmdList,
                                const wchar_t*         pProfile,
                                ParameterSetPair&      paramSets,
                                int32_t                kernelPerm,
                                int32_t                kernelSize,
                                FfxBlurFloatPrecision  floatPrecision);

    void ExecuteMultiPassTransposeFilter(cauldron::CommandList* pCmdList,
                                         const wchar_t*         pProfile,
                                         ParameterSetPair&      paramSets,
                                         int32_t                kernelPerm,
                                         int32_t                kernelSize,
                                         FfxBlurFloatPrecision  floatPrecision);

    void ExecuteTwoPassFilter(cauldron::CommandList*    pCmdList,
                              cauldron::PipelineObject* pPass1PipelineSet,
                              cauldron::PipelineObject* pPass2PipelineSet,
                              ParameterSetPair&         paramSets,
                              const cauldron::Texture*  pPass1Output);

    using TexturePair = std::pair<const cauldron::Texture*, const cauldron::Texture*>;

    void ExecuteBlurEffect(cauldron::CommandList* pCmdList,
                           const wchar_t*         pProfile,
                           FfxBlurContext&        blurContext,
                           const TexturePair&     inputOutputPair,
                           FfxBlurKernelPermutation   kernelPermutation,
                           FfxBlurKernelSize      kernelSize);

    void UpdateConstants(uint32_t width, uint32_t height, cauldron::ParameterSet* pParameterSet);

    struct ComparisonConstants
    {
        uint32_t Width;
        uint32_t Height;
        float    DiffFactor;
    };
    float m_DiffFactor = 1.0f;
    void ExecuteComparisonPass(cauldron::CommandList* pCmdList);

/// @}
};
