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

#include "blurrendermodule.h"

#include "core/backend_interface.h"
#include "core/framework.h"
#include "core/loaders/textureloader.h"
#include "core/uimanager.h"
#include "render/device.h"
#include "render/dynamicresourcepool.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/renderdefines.h"
#include "render/uploadheap.h"

#include <array>
#include <limits>

using namespace cauldron;

static const char* s_FloatingPointMathOptions[] = {"Use FP16", "Use FP32"};
FfxBlurFloatPrecision GetFloatPrecision(int32_t fpMathIndex)
{
    std::string floatingPointMath = s_FloatingPointMathOptions[fpMathIndex];
    if (floatingPointMath == "Use FP16")
        return FFX_BLUR_FLOAT_PRECISION_16BIT;
    else if (floatingPointMath == "Use FP32")
        return FFX_BLUR_FLOAT_PRECISION_32BIT;
    else
    {
        // Unhandled float precision value.
        CAULDRON_ASSERT(false);
    }
        
    return FFX_BLUR_FLOAT_PRECISION_COUNT;
}

static const char* s_GaussianSigmaOptions[] = {"1.6", "2.8", "4.0"};
FfxBlurKernelPermutation GetGaussianSigmaPermutation(int32_t sigmaIndex)
{
    if (sigmaIndex == 0)
        return FFX_BLUR_KERNEL_PERMUTATION_0;
    else if (sigmaIndex == 1)
        return FFX_BLUR_KERNEL_PERMUTATION_1;
    else if (sigmaIndex == 2)
        return FFX_BLUR_KERNEL_PERMUTATION_2;
    else
    {
        // Unhandled Gaussian Sigma Index.
        CAULDRON_ASSERT(false);
    }

    return (FfxBlurKernelPermutation)FFX_BLUR_KERNEL_PERMUTATION_COUNT;
}

static const char* s_KernelSizeOptions[] = {"3x3", "5x5", "7x7", "9x9", "11x11", "13x13", "15x15", "17x17", "19x19", "21x21"};
FfxBlurKernelSize GetKernelSize(int32_t kernelSizeIndex)
{
    std::string kernelSize = s_KernelSizeOptions[kernelSizeIndex];
    if (kernelSize == "3x3")
        return FFX_BLUR_KERNEL_SIZE_3x3;
    else if (kernelSize == "5x5")
        return FFX_BLUR_KERNEL_SIZE_5x5;
    else if (kernelSize == "7x7")
        return FFX_BLUR_KERNEL_SIZE_7x7;
    else if (kernelSize == "9x9")
        return FFX_BLUR_KERNEL_SIZE_9x9;
    else if (kernelSize == "11x11")
        return FFX_BLUR_KERNEL_SIZE_11x11;
    else if (kernelSize == "13x13")
        return FFX_BLUR_KERNEL_SIZE_13x13;
    else if (kernelSize == "15x15")
        return FFX_BLUR_KERNEL_SIZE_15x15;
    else if (kernelSize == "17x17")
        return FFX_BLUR_KERNEL_SIZE_17x17;
    else if (kernelSize == "19x19")
        return FFX_BLUR_KERNEL_SIZE_19x19;
    else if (kernelSize == "21x21")
        return FFX_BLUR_KERNEL_SIZE_21x21;
    else
    {
        // Unhandled kernel size.
        CAULDRON_ASSERT(false);
    }

    return (FfxBlurKernelSize)FFX_BLUR_KERNEL_SIZE_COUNT;
}


BlurRenderModule::BlurRenderModule()
    : RenderModule(L"BlurRenderModule")
{
}

void BlurRenderModule::Init(const json& initData)
{
    UISection* uiSection = GetUIManager()->RegisterUIElements("Blur", UISectionType::Sample);

    if (uiSection)
    {
        std::vector<const char*> algoOptions = {
            "None", "FidelityFX Blur", "Single Pass Box Filter",
            "Multi-pass Separable Filter", "Multi-pass Separable Filter Transpose"};
        uiSection->RegisterUIElement<UICombo>(
            "Algorithm",
            m_CurrentAlgorithm1,
            algoOptions,
            [this](int32_t cur, int32_t old) {
                if (cur != old)
                {
                    m_EnableFilterOptions1 = m_CurrentAlgorithm1 != static_cast<int32_t>(Algorithm::NONE);

                    if (m_CurrentAlgorithm1 == static_cast<int32_t>(Algorithm::FIDELITYFX_BLUR_GAUSSIAN))
                    {
                        DestroyBlurContexts();
                        CreateBlurContexts();
                    }
                }
            });

        std::vector<const char*> gaussianSigmaOptions(s_GaussianSigmaOptions, s_GaussianSigmaOptions + _countof(s_GaussianSigmaOptions));
        uiSection->RegisterUIElement<UICombo>("Gaussian Kernel Sigma", m_CurrentGaussianSigma1, gaussianSigmaOptions, m_EnableFilterOptions1);

        std::vector<const char*> kernOptions(s_KernelSizeOptions, s_KernelSizeOptions + _countof(s_KernelSizeOptions));
        uiSection->RegisterUIElement<UICombo>("Kernel Size", m_CurrentKernelSize1, kernOptions, m_EnableFilterOptions1);

        std::vector<const char*> mathOptions(s_FloatingPointMathOptions, s_FloatingPointMathOptions + _countof(s_FloatingPointMathOptions));
        uiSection->RegisterUIElement<UICombo>(
            "Floating Point Math",
            m_CurrentFpMath1,
            mathOptions,
            m_EnableFilterOptions1,
            [this](int32_t cur, int32_t old) {
                if (cur != old)
                {
                    if (m_CurrentAlgorithm1 == static_cast<int32_t>(Algorithm::FIDELITYFX_BLUR_GAUSSIAN))
                    {
                        DestroyBlurContexts();
                        CreateBlurContexts();
                    }
                }
            });

        uiSection->RegisterUIElement<UICheckBox>(
            "Display the difference between two algorithms.",
            m_ComparisonModeEnabled,
            [this](bool cur, bool old) {
                m_EnableFilterOptions2 = cur ? m_CurrentAlgorithm2 != static_cast<int32_t>(Algorithm::NONE) : false;
            });
        uiSection->RegisterUIElement<UISeparator>();

        // Add controls for the comparison mode that are enabled/disabled by m_ComparisonModeEnabled.
        uiSection->RegisterUIElement<UICombo>(
            "Compare Algorithm",
            m_CurrentAlgorithm2,
            std::move(algoOptions),
            m_ComparisonModeEnabled,
            [this](int32_t cur, int32_t old) {
                if (cur != old)
                {
                    m_EnableFilterOptions2 = m_CurrentAlgorithm2 != static_cast<int32_t>(Algorithm::NONE);

                    if (m_CurrentAlgorithm2 == static_cast<int32_t>(Algorithm::FIDELITYFX_BLUR_GAUSSIAN))
                    {
                        DestroyBlurContexts();
                        CreateBlurContexts();
                    }
                }
            });

        uiSection->RegisterUIElement<UICombo>("Compare Gaussian Sigma", m_CurrentGaussianSigma2, std::move(gaussianSigmaOptions), m_EnableFilterOptions2);

        uiSection->RegisterUIElement<UICombo>("Compare Kernel Size", m_CurrentKernelSize2, std::move(kernOptions), m_EnableFilterOptions2);

        uiSection->RegisterUIElement<UICombo>(
            "Compare FP Math",
            m_CurrentFpMath2,
            std::move(mathOptions),
            m_EnableFilterOptions2,
            [this](int32_t cur, int32_t old) {
                if (cur != old)
                {
                    if (m_CurrentAlgorithm2 == static_cast<int32_t>(Algorithm::FIDELITYFX_BLUR_GAUSSIAN))
                    {
                        DestroyBlurContexts();
                        CreateBlurContexts();
                    }
                }
            });

        uiSection->RegisterUIElement<UISlider<float>>("Diff Factor", m_DiffFactor, 1.0f, 10.0f, m_ComparisonModeEnabled);
    }

    GetFramework()->ConfigureRuntimeShaderRecompiler(
        // PreReload Callback
        [this]() { DestroyBlurContexts(); },
        // PostReload Callback
        [this]() {
            InitFfxBackend();
            CreateBlurContexts();
        });

    InitTextures();

    InitPipelines();

    SetModuleReady(true);
}

void BlurRenderModule::InitTextures()
{
    m_pOutput = GetFramework()->GetColorTargetForCallback(GetName());

    TextureDesc texDesc = m_pOutput->GetDesc();
    texDesc.MipLevels = 1;

    auto& resizeFunc = [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t, uint32_t) {
        desc.Width  = displayWidth;
        desc.Height = displayHeight;
    };

    // m_pInput has to use the same format as the m_pOutput because of the CopyTextureRegion command.
    texDesc.Name = L"BLUR_Input";
    m_pInput     = GetDynamicResourcePool()->CreateRenderTexture(&texDesc, resizeFunc);

    texDesc.Name         = L"BLUR_ComparisonOutput1";
    m_pComparisonOutput1 = GetDynamicResourcePool()->CreateRenderTexture(&texDesc,  resizeFunc);
    texDesc.Name         = L"BLUR_ComparisonOutput2";
    m_pComparisonOutput2 = GetDynamicResourcePool()->CreateRenderTexture(&texDesc, resizeFunc);

    // Two pass algorithms require an intermediate output.
    texDesc.Name   = L"BLUR_Pass1Output";
    m_pPass1Output = GetDynamicResourcePool()->CreateRenderTexture(&texDesc, resizeFunc);

    // Transpose pass requires height by width intermediate output.
    texDesc.Name   = L"BLUR_TransposePass1Output";
    texDesc.Width  = m_pOutput->GetDesc().Height;
    texDesc.Height = m_pOutput->GetDesc().Width;

    auto& transposeResizeFunc = [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t, uint32_t) {
        desc.Width  = displayHeight;
        desc.Height = displayWidth;
    };
    m_pTransposePass1Output = GetDynamicResourcePool()->CreateRenderTexture(&texDesc, transposeResizeFunc);
}

static ParameterSet* CreateComparisonParameterSet(
    RootSignature* pRootSignature, size_t constantsSizeBytes, const Texture& input1, const Texture& input2, const Texture& output)
{
    ParameterSet* pParameterSet = ParameterSet::CreateParameterSet(pRootSignature);
    pParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), constantsSizeBytes, 0);

    pParameterSet->SetTextureSRV(&input1, ViewDimension::Texture2D, 0);
    pParameterSet->SetTextureSRV(&input2, ViewDimension::Texture2D, 1);

    pParameterSet->SetTextureUAV(&output, ViewDimension::Texture2D, 0);

    return pParameterSet;
}

void BlurRenderModule::InitFfxBackend()
{
    // Release the scratch buffer memory
    if (m_BackendInterface.scratchBuffer)
        free(m_BackendInterface.scratchBuffer);

    size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(2 * FFX_BLUR_CONTEXT_COUNT);
    void* scratchBuffer = calloc(scratchBufferSize, 1u);
    FfxErrorCode errorCode = SDKWrapper::ffxGetInterface(&m_BackendInterface, GetDevice(), scratchBuffer, scratchBufferSize, 2 * FFX_BLUR_CONTEXT_COUNT);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // valid effect library and backend versions
    CauldronAssert(ASSERT_CRITICAL, m_BackendInterface.fpGetSDKVersion(&m_BackendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
                        L"FidelityFX Blur 1.1 sample requires linking with a 1.1.2 version SDK backend");


    CauldronAssert(ASSERT_CRITICAL, ffxBlurGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 1, 0),
                       L"FidelityFX Blur 1.1 sample requires linking with a 1.1 version FidelityFX Blur library");
                       
    m_BackendInterface.fpRegisterConstantBufferAllocator(&m_BackendInterface, SDKWrapper::ffxAllocateConstantBuffer);
}

void BlurRenderModule::InitPipelines()
{
    // Initialize the FFX backend
    InitFfxBackend();

    CreateBlurContexts();

    // Create root signature
    RootSignatureDesc filterRootSigDesc;
    filterRootSigDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    filterRootSigDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);
    filterRootSigDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);

    std::wstring rootName = L"BlurEffect_RootSignature";

    m_pFilterPipelineRootSig = RootSignature::CreateRootSignature(rootName.c_str(), filterRootSigDesc);

    const wchar_t* gaussianSigmaPermutations[] = {L"0", L"1", L"2"};
    const wchar_t* kernelSizes[] = {L"3", L"5", L"7", L"9", L"11", L"13", L"15", L"17", L"19", L"21"};
    m_KernelSizesCount = _countof(kernelSizes);
    const wchar_t* baselineFiltersComputeShader = L"blur_baseline_filters_cs.hlsl";

    CreateSinglePassBoxFilterPipelines(
        gaussianSigmaPermutations, _countof(gaussianSigmaPermutations),
        kernelSizes, m_KernelSizesCount,
        baselineFiltersComputeShader);

    CreateMultiPassSeparableFilterPipelines(
        gaussianSigmaPermutations, _countof(gaussianSigmaPermutations),
        kernelSizes, m_KernelSizesCount,
        baselineFiltersComputeShader);

    CreateMultiPassSeparableTransposeFilterPipelines(
        gaussianSigmaPermutations, _countof(gaussianSigmaPermutations),
        kernelSizes, m_KernelSizesCount,
        baselineFiltersComputeShader);

    CreatePassThroughPipeline(baselineFiltersComputeShader);

    RootSignatureDesc compareRootSigDesc;
    compareRootSigDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    compareRootSigDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);
    compareRootSigDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1);
    compareRootSigDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);

    rootName = L"BlurEffect_CompareRootSignature";
    m_pComparisonPipelineRootSig = RootSignature::CreateRootSignature(rootName.c_str(), compareRootSigDesc);

    DefineList defines;
    m_pComparisonPipeline = CreatePipeline(m_pComparisonPipelineRootSig, L"BlurEffect_ComparisonPipeline", L"blur_compare_filters_cs.hlsl", L"MainCS", defines);

    m_pComparisonPipelineParams =
        CreateComparisonParameterSet(m_pComparisonPipelineRootSig,
                                     sizeof(ComparisonConstants),
                                     *m_pComparisonOutput1,
                                     *m_pComparisonOutput2,
                                     *m_pOutput);
}

static ParameterSet* CreateParameterSet(RootSignature* pRootSignature, size_t constantsSizeBytes, const Texture& input, const Texture& output)
{
    ParameterSet* pParameterSet = ParameterSet::CreateParameterSet(pRootSignature);
    pParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), constantsSizeBytes, 0);

    pParameterSet->SetTextureSRV(&input, ViewDimension::Texture2D, 0);
    pParameterSet->SetTextureUAV(&output, ViewDimension::Texture2D, 0);

    return pParameterSet;
}

void BlurRenderModule::CreatePassThroughPipeline(const wchar_t* computeShaderFilename)
{
    DefineList defines;
    defines[L"PASSTHROUGH"] = L"1";
    defines[L"KERNEL_DIMENSION"] = L"3"; // The PassThrough shader doesn't use this, but the shader code requires it to be defined.
    m_pPassThroughPipeline = CreatePipeline(m_pFilterPipelineRootSig, L"PassThrough", computeShaderFilename, L"CSMain_PassThrough", defines);
}

void BlurRenderModule::CreateSinglePassBoxFilterPipelines(
    const wchar_t** ppSigmas, size_t sigmasCount,
    const wchar_t** ppKernelSizes, size_t kernelSizesCount,
    const wchar_t* computeShaderFilename)
{
    DefineList defines;
    // #define macro that enables the single pass entry func.
    defines[L"SINGLE_PASS_BOX_FILTER"] = L"1";
    for (int32_t j = 0; j < sigmasCount; ++j)
    {
        defines[L"GAUSSIAN_SIGMA_PERMUTATION"] = ppSigmas[j];
        for (int32_t i = 0; i < kernelSizesCount; ++i)
        {
            defines[L"KERNEL_DIMENSION"] = ppKernelSizes[i];
            defines[L"HALF_PRECISION"]   = L"1";
            m_SinglePassBoxFilterPipelinesFp16.push_back(
                CreatePipeline(m_pFilterPipelineRootSig, L"SinglePassBoxFilterFP16", computeShaderFilename, L"CSMain_SinglePass_BoxFilter", defines));

            defines[L"HALF_PRECISION"] = L"0";
            m_SinglePassBoxFilterPipelinesFp32.push_back(
                CreatePipeline(m_pFilterPipelineRootSig, L"SinglePassBoxFilterFP32", computeShaderFilename, L"CSMain_SinglePass_BoxFilter", defines));
        }
    }

    m_SinglePassParams.pNormalModeParams = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pInput, *m_pOutput);

    m_SinglePassParams.pComparisonModeParams1 = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pInput, *m_pComparisonOutput1);
    m_SinglePassParams.pComparisonModeParams2 = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pInput, *m_pComparisonOutput2);
}

void BlurRenderModule::CreateMultiPassSeparableFilterPipelines(
    const wchar_t** ppSigmas, size_t sigmasCount,
    const wchar_t** ppKernelSizes, size_t kernelSizesCount,
    const wchar_t* computeShaderFilename)
{
    DefineList defines;
    // #define macro that enables the multi pass entry func.
    defines[L"MULTI_PASS_SEPARABLE_FILTER"] = L"1";
    for (int32_t j = 0; j < sigmasCount; ++j)
    {
        defines[L"GAUSSIAN_SIGMA_PERMUTATION"] = ppSigmas[j];
        for (int32_t i = 0; i < kernelSizesCount; ++i)
        {
            defines[L"KERNEL_DIMENSION"] = ppKernelSizes[i];
            defines[L"HALF_PRECISION"]   = L"1";
            m_MultiPassSeparableFilterPipelinesFp16.push_back(CreatePipeline(
                m_pFilterPipelineRootSig, L"MultiPassSeparableFilterFP16Pass1", computeShaderFilename, L"CSMain_SeparableFilter_X", defines));
            m_MultiPassSeparableFilterPipelinesFp16.push_back(CreatePipeline(
                m_pFilterPipelineRootSig, L"MultiPassSeparableFilterFP16Pass2", computeShaderFilename, L"CSMain_SeparableFilter_Y", defines));

            defines[L"HALF_PRECISION"] = L"0";
            m_MultiPassSeparableFilterPipelinesFp32.push_back(CreatePipeline(
                m_pFilterPipelineRootSig, L"MultiPassSeparableFilterFP32Pass1", computeShaderFilename, L"CSMain_SeparableFilter_X", defines));
            m_MultiPassSeparableFilterPipelinesFp32.push_back(CreatePipeline(
                m_pFilterPipelineRootSig, L"MultiPassSeparableFilterFP32Pass2", computeShaderFilename, L"CSMain_SeparableFilter_Y", defines));
        }
    }

    m_MultiPassParams.pPass1NormalModeParams  = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pInput, *m_pPass1Output);
    m_MultiPassParams.pPass2NormalModeParams = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pPass1Output, *m_pOutput);

    m_MultiPassParams.pPass1ComparisonModeParams1  = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pInput, *m_pPass1Output);
    m_MultiPassParams.pPass2ComparisonModeParams1 = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pPass1Output, *m_pComparisonOutput1);

    m_MultiPassParams.pPass1ComparisonModeParams2  = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pInput, *m_pPass1Output);
    m_MultiPassParams.pPass2ComparisonModeParams2 = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pPass1Output, *m_pComparisonOutput2);
}

void BlurRenderModule::CreateMultiPassSeparableTransposeFilterPipelines(
    const wchar_t** ppSigmas, size_t sigmasCount,
    const wchar_t** ppKernelSizes, size_t kernelSizesCount,
    const wchar_t* computeShaderFilename)
{
    DefineList defines;
    // #define macro that enables the multi pass entry func.
    defines[L"MULTI_PASS_SEPARABLE_FILTER"] = L"1";
    defines[L"TRANSPOSE_OUT"]               = L"1";
    for (int32_t j = 0; j < sigmasCount; ++j)
    {
        defines[L"GAUSSIAN_SIGMA_PERMUTATION"] = ppSigmas[j];
        for (int32_t i = 0; i < kernelSizesCount; ++i)
        {
            defines[L"KERNEL_DIMENSION"] = ppKernelSizes[i];
            defines[L"HALF_PRECISION"]   = L"1";
            m_MultiPassSeparableTransposeFilterPipelinesFp16.push_back(CreatePipeline(
                m_pFilterPipelineRootSig, L"MultiPassSeparableTransposeFilterFP16Pass1", computeShaderFilename, L"CSMain_SeparableFilter_X", defines));

            defines[L"HALF_PRECISION"] = L"0";
            m_MultiPassSeparableTransposeFilterPipelinesFp32.push_back(CreatePipeline(
                m_pFilterPipelineRootSig, L"MultiPassSeparableTransposeFilterFP32Pass1", computeShaderFilename, L"CSMain_SeparableFilter_X", defines));
        }
    }

    m_MultiPassTransposeParams.pPass1NormalModeParams  = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pInput, *m_pTransposePass1Output);
    m_MultiPassTransposeParams.pPass2NormalModeParams = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pTransposePass1Output, *m_pOutput);

    m_MultiPassTransposeParams.pPass1ComparisonModeParams1  = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pInput, *m_pTransposePass1Output);
    m_MultiPassTransposeParams.pPass2ComparisonModeParams1 = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pTransposePass1Output, *m_pComparisonOutput1);

    m_MultiPassTransposeParams.pPass1ComparisonModeParams2  = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pInput, *m_pTransposePass1Output);
    m_MultiPassTransposeParams.pPass2ComparisonModeParams2 = CreateParameterSet(m_pFilterPipelineRootSig, sizeof(Constants), *m_pTransposePass1Output, *m_pComparisonOutput2);
}

PipelineObject* BlurRenderModule::CreatePipeline(
    RootSignature* pRootSignature,
    const std::wstring& pipelineName,
    const std::wstring& shaderFile,
    const std::wstring& entryFunc,
    DefineList& defines)
{
    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(pRootSignature);

    psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(shaderFile.c_str(), entryFunc.c_str(), ShaderModel::SM6_0, &defines));

    PipelineObject* pPipelineObj = PipelineObject::CreatePipelineObject(pipelineName.c_str(), psoDesc);

    return pPipelineObj;
}

void BlurRenderModule::CreateBlurContexts()
{
    if (!m_BlurContext1Created)
    {
        FfxBlurContextDescription desc = {};

        desc.backendInterface = m_BackendInterface;
        desc.floatPrecision   = GetFloatPrecision(m_CurrentFpMath1);
        desc.kernelPermutations = FFX_BLUR_KERNEL_PERMUTATIONS_ALL;
        desc.kernelSizes      = FFX_BLUR_KERNEL_SIZE_ALL;

        ffxBlurContextCreate(&m_BlurContext1, &desc);

        m_BlurContext1Created = true;
    }

    if (!m_BlurContext2Created)
    {
        FfxBlurContextDescription desc = {};

        desc.backendInterface = m_BackendInterface;
        desc.floatPrecision   = GetFloatPrecision(m_CurrentFpMath2);
        desc.kernelPermutations = FFX_BLUR_KERNEL_PERMUTATIONS_ALL;
        desc.kernelSizes      = FFX_BLUR_KERNEL_SIZE_ALL;

        ffxBlurContextCreate(&m_BlurContext2, &desc);

        m_BlurContext2Created = true;
    }
}

BlurRenderModule::~BlurRenderModule()
{
    DestroyBlurContexts();

    if (m_pFilterPipelineRootSig != nullptr)
        delete m_pFilterPipelineRootSig;
    if (m_pComparisonPipelineRootSig != nullptr)
        delete m_pComparisonPipelineRootSig;

    if (m_pComparisonPipelineParams != nullptr)
        delete m_pComparisonPipelineParams;

    if (m_pComparisonOutput1 != nullptr)
        GetDynamicResourcePool()->DestroyResource(m_pComparisonOutput1->GetResource());
    if (m_pComparisonOutput2 != nullptr)
        GetDynamicResourcePool()->DestroyResource(m_pComparisonOutput2->GetResource());

    if (m_pInput != nullptr)
        GetDynamicResourcePool()->DestroyResource(m_pInput->GetResource());
    if (m_pPass1Output != nullptr)
        GetDynamicResourcePool()->DestroyResource(m_pPass1Output->GetResource());
    if (m_pTransposePass1Output != nullptr)
        GetDynamicResourcePool()->DestroyResource(m_pTransposePass1Output->GetResource());

    // Release the scratch buffer memory
    free(m_BackendInterface.scratchBuffer);
    
    if (m_pComparisonPipeline)
        delete m_pComparisonPipeline;

    if (m_pPassThroughPipeline)
        delete m_pPassThroughPipeline;

    for (auto& pPSO: m_SinglePassBoxFilterPipelinesFp16)
        delete pPSO;

    for (auto& pPSO: m_SinglePassBoxFilterPipelinesFp32)
        delete pPSO;

    for (auto& pPSO: m_MultiPassSeparableFilterPipelinesFp16)
        delete pPSO;

    for (auto& pPSO: m_MultiPassSeparableFilterPipelinesFp32)
        delete pPSO;

    for (auto& pPSO: m_MultiPassSeparableTransposeFilterPipelinesFp16)
        delete pPSO;

    for (auto& pPSO: m_MultiPassSeparableTransposeFilterPipelinesFp32)
        delete pPSO;
}

void BlurRenderModule::DestroyBlurContexts()
{
    if (m_BlurContext1Created || m_BlurContext2Created)
    {
        // Flush anything out of the pipes before destroying the context
        GetDevice()->FlushAllCommandQueues();

        if (m_BlurContext1Created)
        {
            ffxBlurContextDestroy(&m_BlurContext1);

            m_BlurContext1Created = false;
        }

        if (m_BlurContext2Created)
        {
            ffxBlurContextDestroy(&m_BlurContext2);

            m_BlurContext2Created = false;
        }
    }
}

void BlurRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    // We need to copy the current color buffer to our input buffer because we need to write our output
    // to the current color buffer so that it is used as input by the RenderModule that follows us.
    std::array<Barrier, 4u> barriers;
    barriers[0] = Barrier::Transition(m_pInput->GetResource(),
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
        ResourceState::CopyDest);
    barriers[1] = Barrier::Transition(m_pOutput->GetResource(),
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
        ResourceState::CopySource);
    ResourceBarrier(pCmdList, 2u, barriers.data());

    TextureCopyDesc desc(m_pOutput->GetResource(), m_pInput->GetResource());
    CopyTextureRegion(pCmdList, &desc);

    uint32_t barrierCount = 2;
    barriers[0] = Barrier::Transition(m_pInput->GetResource(),
        ResourceState::CopyDest,
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
    barriers[1] = Barrier::Transition(m_pOutput->GetResource(),
        ResourceState::CopySource,
        ResourceState::UnorderedAccess);

    if (m_ComparisonModeEnabled)
    {
        barriers[2] = Barrier::Transition(m_pComparisonOutput1->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::UnorderedAccess);

        barriers[3] = Barrier::Transition(m_pComparisonOutput2->GetResource(),
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
            ResourceState::UnorderedAccess);

        barrierCount += 2;
    }

    ResourceBarrier(pCmdList, barrierCount, barriers.data());

    switch (static_cast<Algorithm>(m_CurrentAlgorithm1))
    {
    case Algorithm::NONE:
        ExecutePassThrough(pCmdList,
                           !m_ComparisonModeEnabled ? L"None" : L"None Compare1",
                           !m_ComparisonModeEnabled ? m_SinglePassParams.pNormalModeParams : m_SinglePassParams.pComparisonModeParams1);
        break;
    case Algorithm::SINGLE_PASS_BOX_FILTER:
        ExecuteSinglePassBoxFilter(pCmdList,
                                   !m_ComparisonModeEnabled ? L"BoxFilter" : L"BoxFilter Compare1",
                                   !m_ComparisonModeEnabled ?
                                       m_SinglePassParams.pNormalModeParams : m_SinglePassParams.pComparisonModeParams1,
                                   m_CurrentGaussianSigma1,
                                   m_CurrentKernelSize1,
                                   GetFloatPrecision(m_CurrentFpMath1));
        break;
    case Algorithm::MULTI_PASS_SEPARABLE_FILTER:
        ExecuteMultiPassFilter(pCmdList,
                               !m_ComparisonModeEnabled ? L"MultiPassFilter" : L"MultiPassFilter Compare1",
                               !m_ComparisonModeEnabled
                                   ? ParameterSetPair(m_MultiPassParams.pPass1NormalModeParams,
                                                      m_MultiPassParams.pPass2NormalModeParams)
                                   : ParameterSetPair(m_MultiPassParams.pPass1ComparisonModeParams1,
                                                      m_MultiPassParams.pPass2ComparisonModeParams1),
                               m_CurrentGaussianSigma1,
                               m_CurrentKernelSize1,
                               GetFloatPrecision(m_CurrentFpMath1));
        break;
    case Algorithm::MULTI_PASS_SEPARABLE_FILTER_TRANSPOSE:
        ExecuteMultiPassTransposeFilter(pCmdList,
                                        !m_ComparisonModeEnabled ? L"MultiPassTranposeFilter" : L"MultiPassTransposeFilter Compare1",
                                        !m_ComparisonModeEnabled
                                            ? ParameterSetPair(m_MultiPassTransposeParams.pPass1NormalModeParams,
                                                               m_MultiPassTransposeParams.pPass2NormalModeParams)
                                            : ParameterSetPair(m_MultiPassTransposeParams.pPass1ComparisonModeParams1,
                                                               m_MultiPassTransposeParams.pPass2ComparisonModeParams1),
                                        m_CurrentGaussianSigma1,
                                        m_CurrentKernelSize1,
                                        GetFloatPrecision(m_CurrentFpMath1));
        break;
    case Algorithm::FIDELITYFX_BLUR_GAUSSIAN:
    default:
        ExecuteBlurEffect(pCmdList,
                          !m_ComparisonModeEnabled ? L"FFX Blur" : L"FFX Blur Compare1",
                          m_BlurContext1,
                          !m_ComparisonModeEnabled ? TexturePair(m_pInput, m_pOutput) : TexturePair(m_pInput, m_pComparisonOutput1),
                          GetGaussianSigmaPermutation(m_CurrentGaussianSigma1),
                          GetKernelSize(m_CurrentKernelSize1));
        break;
    }

    if (m_ComparisonModeEnabled)
    {
        barriers[0] = Barrier::Transition(m_pComparisonOutput1->GetResource(),
            ResourceState::UnorderedAccess,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);

        ResourceBarrier(pCmdList, 1, barriers.data());

        switch (static_cast<Algorithm>(m_CurrentAlgorithm2))
        {
        case Algorithm::NONE:
            ExecutePassThrough(pCmdList,
                               L"None Compare2",
                               m_SinglePassParams.pComparisonModeParams2);
            break;
        case Algorithm::SINGLE_PASS_BOX_FILTER:
            ExecuteSinglePassBoxFilter(pCmdList,
                                       L"BoxFilter Compare2",
                                       m_SinglePassParams.pComparisonModeParams2,
                                       m_CurrentGaussianSigma2,
                                       m_CurrentKernelSize2,
                                       GetFloatPrecision(m_CurrentFpMath2));
            break;
        case Algorithm::MULTI_PASS_SEPARABLE_FILTER:
            ExecuteMultiPassFilter(pCmdList,
                                   L"MultiPassFilter Compare2",
                                   ParameterSetPair(m_MultiPassParams.pPass1ComparisonModeParams2,
                                                    m_MultiPassParams.pPass2ComparisonModeParams2),
                                   m_CurrentGaussianSigma2,
                                   m_CurrentKernelSize2,
                                   GetFloatPrecision(m_CurrentFpMath2));
            break;
        case Algorithm::MULTI_PASS_SEPARABLE_FILTER_TRANSPOSE:
            ExecuteMultiPassTransposeFilter(pCmdList,
                                            L"MultiPassTransposeFilter Compare2",
                                            ParameterSetPair(m_MultiPassTransposeParams.pPass1ComparisonModeParams2, m_MultiPassTransposeParams.pPass2ComparisonModeParams2),
                                   m_CurrentGaussianSigma2,
                                   m_CurrentKernelSize2,
                                   GetFloatPrecision(m_CurrentFpMath2));
            break;
        case Algorithm::FIDELITYFX_BLUR_GAUSSIAN:
        default:
            ExecuteBlurEffect(pCmdList,
                              L"FFX Blur Compare2",
                              m_BlurContext2,
                              TexturePair(m_pInput, m_pComparisonOutput2),
                              GetGaussianSigmaPermutation(m_CurrentGaussianSigma2),
                              GetKernelSize(m_CurrentKernelSize2));
            break;
        }

        barriers[0] = Barrier::Transition(m_pComparisonOutput2->GetResource(),
            ResourceState::UnorderedAccess,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);

        ResourceBarrier(pCmdList, 1, barriers.data());

        ExecuteComparisonPass(pCmdList);
    }

    barriers[0] = Barrier::Transition(m_pOutput->GetResource(),
        ResourceState::UnorderedAccess,
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);

    ResourceBarrier(pCmdList, 1, barriers.data());
}

void BlurRenderModule::OnResize(const ResolutionInfo& resInfo)
{
    if (!ModuleEnabled())
        return;

    DestroyBlurContexts();
    CreateBlurContexts();
}

void BlurRenderModule::UpdateConstants(uint32_t width, uint32_t height, ParameterSet* pParameterSet)
{
    Constants constants;
    constants.Width  = width;
    constants.Height = height;

    BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(constants), &constants);
    pParameterSet->UpdateRootConstantBuffer(&bufferInfo, 0);
}

static void ComputeDispatchDimensions(
    uint32_t imageWidth, uint32_t imageHeight,
    uint32_t& outDispatchX, uint32_t& outDispatchY, uint32_t& outDispatchZ)
{
    const int THREAD_GROUP_WORK_REGION_DIM = 8;  // 8x8 = 64 px region

    outDispatchX = DivideRoundingUp(imageWidth, THREAD_GROUP_WORK_REGION_DIM);
    outDispatchY = DivideRoundingUp(imageHeight, THREAD_GROUP_WORK_REGION_DIM);
    outDispatchZ = 1;
}

void BlurRenderModule::ExecutePassThrough(
    CommandList*   pCmdList,
    const wchar_t* pProfile,
    ParameterSet*  pParamSet)
{
    GPUScopedProfileCapture marker(pCmdList, pProfile);

    UpdateConstants(m_pInput->GetDesc().Width, m_pInput->GetDesc().Height, pParamSet);

    pParamSet->Bind(pCmdList, m_pPassThroughPipeline);

    SetPipelineState(pCmdList, m_pPassThroughPipeline);

    uint32_t dispatchX = 0u;
    uint32_t dispatchY = 0u;
    uint32_t dispatchZ = 0u;
    ComputeDispatchDimensions(
        m_pInput->GetDesc().Width, m_pInput->GetDesc().Height,
        dispatchX, dispatchY, dispatchZ);

    Dispatch(pCmdList, dispatchX, dispatchY, dispatchZ);
}

void BlurRenderModule::ExecuteSinglePassBoxFilter(
    CommandList*          pCmdList,
    const wchar_t*        pProfile,
    ParameterSet*         pParamSet,
    int32_t               kernelPerm,
    int32_t               kernelSize,
    FfxBlurFloatPrecision floatPrecision)
{
    GPUScopedProfileCapture marker(pCmdList, pProfile);

    PipelineObject* pPipelineObj = nullptr;

    uint32_t pipelineIndex = static_cast<uint32_t>((kernelPerm * m_KernelSizesCount) + kernelSize);
    if (floatPrecision == FFX_BLUR_FLOAT_PRECISION_32BIT)
        pPipelineObj = m_SinglePassBoxFilterPipelinesFp32[pipelineIndex];
    else
        pPipelineObj = m_SinglePassBoxFilterPipelinesFp16[pipelineIndex];

    UpdateConstants(m_pInput->GetDesc().Width, m_pInput->GetDesc().Height, pParamSet);

    pParamSet->Bind(pCmdList, pPipelineObj);

    SetPipelineState(pCmdList, pPipelineObj);

    uint32_t dispatchX = 0u;
    uint32_t dispatchY = 0u;
    uint32_t dispatchZ = 0u;
    ComputeDispatchDimensions(
        m_pInput->GetDesc().Width, m_pInput->GetDesc().Height,
        dispatchX, dispatchY, dispatchZ);

    Dispatch(pCmdList, dispatchX, dispatchY, dispatchZ);
}

void BlurRenderModule::ExecuteMultiPassFilter(
    CommandList*          pCmdList,
    const wchar_t*        pProfile,
    ParameterSetPair&     paramSets,
    int32_t               kernelPerm,
    int32_t               kernelSize,
    FfxBlurFloatPrecision floatPrecision)
{
    GPUScopedProfileCapture marker(pCmdList, pProfile);

    PipelineObject* pPass1PipelineObj = nullptr;
    PipelineObject* pPass2PipelineObj = nullptr;

    uint32_t pipelineIndex = static_cast<uint32_t>((kernelPerm * m_KernelSizesCount) + kernelSize);
    if (floatPrecision == FFX_BLUR_FLOAT_PRECISION_32BIT)
    {
        pPass1PipelineObj = m_MultiPassSeparableFilterPipelinesFp32[pipelineIndex * 2];
        pPass2PipelineObj = m_MultiPassSeparableFilterPipelinesFp32[(pipelineIndex * 2) + 1];
    }
    else
    {
        pPass1PipelineObj = m_MultiPassSeparableFilterPipelinesFp16[pipelineIndex * 2];
        pPass2PipelineObj = m_MultiPassSeparableFilterPipelinesFp16[(pipelineIndex * 2) + 1];
    }

    ExecuteTwoPassFilter(pCmdList, pPass1PipelineObj, pPass2PipelineObj, paramSets, m_pPass1Output);
}

void BlurRenderModule::ExecuteMultiPassTransposeFilter(
    CommandList*          pCmdList,
    const wchar_t*        pProfile,
    ParameterSetPair&     paramSets,
    int32_t               kernelPerm,
    int32_t               kernelSize,
    FfxBlurFloatPrecision floatPrecision)
{
    GPUScopedProfileCapture marker(pCmdList, pProfile);

    PipelineObject* pPipelineObj = nullptr;

    uint32_t pipelineIndex = static_cast<uint32_t>((kernelPerm * m_KernelSizesCount) + kernelSize);
    if (floatPrecision == FFX_BLUR_FLOAT_PRECISION_32BIT)
        pPipelineObj = m_MultiPassSeparableTransposeFilterPipelinesFp32[pipelineIndex];
    else
        pPipelineObj = m_MultiPassSeparableTransposeFilterPipelinesFp16[pipelineIndex];

    ExecuteTwoPassFilter(pCmdList, pPipelineObj, pPipelineObj, paramSets, m_pTransposePass1Output);
}

void BlurRenderModule::ExecuteTwoPassFilter(
    CommandList*      pCmdList,
    PipelineObject*   pPass1PipelineObj,
    PipelineObject*   pPass2PipelineObj,
    ParameterSetPair& paramSets,
    const Texture*    pPass1Output)
{
    Barrier rtBarrier = Barrier::Transition(pPass1Output->GetResource(),
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
        ResourceState::UnorderedAccess);

    ResourceBarrier(pCmdList, 1, &rtBarrier);

    UpdateConstants(m_pInput->GetDesc().Width, m_pInput->GetDesc().Height, paramSets.first);

    paramSets.first->Bind(pCmdList, pPass1PipelineObj);

    SetPipelineState(pCmdList, pPass1PipelineObj);

    uint32_t dispatchX = 0u;
    uint32_t dispatchY = 0u;
    uint32_t dispatchZ = 0u;
    ComputeDispatchDimensions(m_pInput->GetDesc().Width, m_pInput->GetDesc().Height, dispatchX, dispatchY, dispatchZ);

    Dispatch(pCmdList, dispatchX, dispatchY, dispatchZ);

    rtBarrier =
        Barrier::Transition(pPass1Output->GetResource(),
            ResourceState::UnorderedAccess,
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);

    ResourceBarrier(pCmdList, 1, &rtBarrier);

    UpdateConstants(pPass1Output->GetDesc().Width, pPass1Output->GetDesc().Height, paramSets.second);

    paramSets.second->Bind(pCmdList, pPass2PipelineObj);

    SetPipelineState(pCmdList, pPass2PipelineObj);

    ComputeDispatchDimensions(pPass1Output->GetDesc().Width, pPass1Output->GetDesc().Height, dispatchX, dispatchY, dispatchZ);

    Dispatch(pCmdList, dispatchX, dispatchY, dispatchZ);
}

void BlurRenderModule::ExecuteBlurEffect(
    CommandList*             pCmdList,
    const wchar_t*           pProfile,
    FfxBlurContext&          blurContext,
    const TexturePair&       inputOutputPair,
    FfxBlurKernelPermutation kernelPermutation,
    FfxBlurKernelSize        kernelSize)
{
    GPUScopedProfileCapture marker(pCmdList, pProfile);

    FfxBlurDispatchDescription desc = {};

    desc.commandList = SDKWrapper::ffxGetCommandList(pCmdList);

    desc.kernelPermutation = kernelPermutation;
    desc.kernelSize = kernelSize;

    desc.input = SDKWrapper::ffxGetResource(inputOutputPair.first->GetResource(), L"BLUR_InputSrc", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    desc.inputAndOutputSize.width  = desc.input.description.width;
    desc.inputAndOutputSize.height = desc.input.description.height;

    desc.output = SDKWrapper::ffxGetResource(inputOutputPair.second->GetResource(), L"BLUR_Output", FFX_RESOURCE_STATE_UNORDERED_ACCESS);

    ffxBlurContextDispatch(&blurContext, &desc);

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);
}

void BlurRenderModule::ExecuteComparisonPass(CommandList* pCmdList)
{
    GPUScopedProfileCapture marker(pCmdList, L"ComparisonPass");

    ComparisonConstants constants;
    constants.Width      = m_pOutput->GetDesc().Width;
    constants.Height     = m_pOutput->GetDesc().Height;
    constants.DiffFactor = m_DiffFactor;

    BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(constants), &constants);
    m_pComparisonPipelineParams->UpdateRootConstantBuffer(&bufferInfo, 0);

    m_pComparisonPipelineParams->Bind(pCmdList, m_pComparisonPipeline);

    SetPipelineState(pCmdList, m_pComparisonPipeline);

    uint32_t dispatchX = 0u;
    uint32_t dispatchY = 0u;
    uint32_t dispatchZ = 0u;
    ComputeDispatchDimensions(m_pInput->GetDesc().Width, m_pInput->GetDesc().Height, dispatchX, dispatchY, dispatchZ);

    Dispatch(pCmdList, dispatchX, dispatchY, dispatchZ);
}

BlurRenderModule::ParameterSets::~ParameterSets()
{
    delete pNormalModeParams;
    delete pComparisonModeParams1;
    delete pComparisonModeParams2;
}

BlurRenderModule::MultiPassParameterSets::~MultiPassParameterSets()
{
    delete pPass1NormalModeParams;
    delete pPass2NormalModeParams;
    delete pPass1ComparisonModeParams1;
    delete pPass2ComparisonModeParams1;
    delete pPass1ComparisonModeParams2;
    delete pPass2ComparisonModeParams2;
}
