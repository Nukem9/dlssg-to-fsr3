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

#include "spdrendermodule.h"

#include "core/backend_interface.h"
#include "core/contentmanager.h"
#include "core/framework.h"
#include "core/loaders/textureloader.h"
#include "core/uimanager.h"
#include "render/device.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rasterview.h"
#include "render/renderdefines.h"

// Pull in common structures for SPD
#include "shaders/spd_common.h"

#include <limits>

using namespace cauldron;
using namespace std::experimental;

void SPDRenderModule::Init(const json& initData)
{
    // Fetch needed resource
    m_pColorTarget = GetFramework()->GetColorTargetForCallback(GetName());
    m_pColorRasterView = GetRasterViewAllocator()->RequestRasterView(m_pColorTarget, ViewDimension::Texture2D);

    // Register UI for SPD
    UISection* uiSection = GetUIManager()->RegisterUIElements("Downsampler", UISectionType::Sample);

    const char* downsamplers[] = { "Multipass PS", "Multipass CS", "SPD CS" };
    const char* loadOptions[] = { "Load", "Linear Sampler" };
    const char* waveOptions[] = { "LocalDataShare", "WaveOps" };
    const char* mathOptions[] = { "Non-Packed", "Packed" };
    const char* sliceOptions[] = { "0", "1", "2", "3", "4", "5" };

    std::vector<const char*> comboOptions;

    // Add down sampler combo
    comboOptions.assign(downsamplers, downsamplers + _countof(downsamplers));
    uiSection->RegisterUIElement<UICombo>(
        "Downsampler options",
        m_DownsamplerUsed,
        std::move(comboOptions),
        [this](int32_t cur, int32_t old) {
            if (cur != old)
            {
                UpdateSPDContext(false);
                UpdateSPDContext(true);
            }
        }
    );

    // Use the same callback for all option changes, which will always destroy/create the context
    std::function<void(int32_t, int32_t)> optionChangeCallback = [this](int32_t, int32_t) {
        if (m_ContextCreated)
        {
            // Refresh
            UpdateSPDContext(false);
            UpdateSPDContext(true);
        }
    };

    // Add load/linear combo
    comboOptions.assign(loadOptions, loadOptions + _countof(loadOptions));
    uiSection->RegisterUIElement<UICombo>("SPD Load / Linear", m_SPDLoadLinear, std::move(comboOptions), optionChangeCallback);

    // Add wave op combo
    comboOptions.assign(waveOptions, waveOptions + _countof(waveOptions));
    uiSection->RegisterUIElement<UICombo>("SPD Wave Interop", m_SPDWaveInterop, std::move(comboOptions), optionChangeCallback);

    // Add math combo
    comboOptions.assign(mathOptions, mathOptions + _countof(mathOptions));
    uiSection->RegisterUIElement<UICombo>("SPD Math", m_SPDMath, std::move(comboOptions), optionChangeCallback);

    // Add a combo for the slice to view (assumes a cubemap, if ever we are viewing a 2d texture, disable UI)
    comboOptions.assign(sliceOptions, sliceOptions + _countof(sliceOptions));
    uiSection->RegisterUIElement<UICombo>("Slice to View", (int32_t&)m_ViewSlice, std::move(comboOptions));

    GetFramework()->ConfigureRuntimeShaderRecompiler(
        [this](void) { DestroyFfxContext(); }, [this](void) { InitFfxContext(); });

    // Initialize common resources that aren't pipeline dependent
    m_LinearSamplerDesc.Filter = FilterFunc::MinMagLinearMipPoint;
    m_LinearSamplerDesc.MaxLOD = std::numeric_limits<float>::max();
    m_LinearSamplerDesc.MaxAnisotropy = 1;

    // Pipelines will be initialized after the texture is loaded

    // Using AllowRenderTarget + AllowUnorderedAccess on the same resource is usually
    // frowned upon for performance reasons, but we are doing it here in the interest of re-using a resource
    TextureLoadCompletionCallbackFn completionCallback = 
        [this](const std::vector<const Texture*>& textures, void* additionalParams = nullptr) { 
        this->TextureLoadComplete(textures, additionalParams); 
    };

    filesystem::path texturePath = L"..\\media\\Textures\\SPD\\spd_cubemap.dds";
    GetContentManager()->LoadTexture(TextureLoadInfo(texturePath, true, 1.f, 
        ResourceFlags::AllowRenderTarget | ResourceFlags::AllowUnorderedAccess), 
        completionCallback);
}

SPDRenderModule::~SPDRenderModule()
{
    DestroyFfxContext();
    
    // Delete pipeline objects and resources
    for (int32_t i = 0; i < static_cast<int32_t>(DownsampleTechnique::Count); ++i)
    {
        for (auto paramIter = m_PipelineSets[i].ParameterSets.begin(); paramIter != m_PipelineSets[i].ParameterSets.end(); ++paramIter)
            delete (*paramIter);
        m_PipelineSets[i].ParameterSets.clear();
        delete m_PipelineSets[i].pPipelineObj;
        delete m_PipelineSets[i].pRootSignature;
    }

    // Delete verification pipeline set
    for (ParameterSet* pParamSet : m_VerificationSet.ParameterSets)
        delete pParamSet;
    m_VerificationSet.ParameterSets.clear();
    delete m_VerificationSet.pPipelineObj;
    delete m_VerificationSet.pRootSignature;
    m_pColorRasterView = nullptr; // Don't own this memory

    // Clear out our raster views
    m_RasterViews.clear();

    // Don't own this memory
    m_pCubeTexture = nullptr;
}

void SPDRenderModule::DestroyFfxContext()
{
    // Tear down SPD context
    UpdateSPDContext(false);

    // Destroy the FidelityFX interface memory
    if (m_InitializationParameters.backendInterface.scratchBuffer != nullptr)
    {
        free(m_InitializationParameters.backendInterface.scratchBuffer);
        m_InitializationParameters.backendInterface.scratchBuffer = nullptr;
    }
}

void SPDRenderModule::InitTraditionalDSPipeline(bool computeDownsample)
{
    ShaderBindStage shaderStage = (computeDownsample) ? ShaderBindStage::Compute : ShaderBindStage::Pixel;
    int32_t pipelineID = (computeDownsample) ? static_cast<int32_t>(DownsampleTechnique::CSDownsample) : static_cast<int32_t>(DownsampleTechnique::PSDownsample);

    // Create root signature
    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, shaderStage, 1);
    signatureDesc.AddTextureSRVSet(0, shaderStage, 1);
    signatureDesc.AddStaticSamplers(0, shaderStage, 1, &m_LinearSamplerDesc);

    std::wstring rootName;

    if (computeDownsample)
    {
        signatureDesc.AddTextureUAVSet(0, shaderStage, 1);
        rootName = L"SPD_DownsampleCS_RootSignature";
    }
    else
        rootName = L"SPD_DownsamplePS_RootSignature";

    m_PipelineSets[pipelineID].pRootSignature = RootSignature::CreateRootSignature(rootName.c_str(), signatureDesc);

    // Setup the pipeline object
    std::wstring pipelineName;
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_PipelineSets[pipelineID].pRootSignature);

    // Setup the shaders to build on the pipeline object
    if (computeDownsample)
    {
        pipelineName = L"SPD_DownsampleCS_PipelineObj";
        psoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"spd_cs_downsampler.hlsl", L"mainCS", ShaderModel::SM6_0, nullptr));
    }
    else
    {
        pipelineName = L"SPD_DownsamplePS_PipelineObj";

        psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"fullscreen.hlsl", L"FullscreenVS", ShaderModel::SM6_0, nullptr));
        psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"spd_ps_downsampler.hlsl", L"mainPS", ShaderModel::SM6_0, nullptr));

        // Setup remaining information and build
        psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
        psoDesc.AddRasterFormats(m_pCubeTexture->GetFormat());   // Use the first raster set, as we just want the format and they are all the same
    }

    m_PipelineSets[pipelineID].pPipelineObj = PipelineObject::CreatePipelineObject(pipelineName.c_str(), psoDesc);

    // Setup parameter sets
    // Now that the texture is loaded, create raster passes, and initialize the different down sample techniques
    const TextureDesc& desc = m_pCubeTexture->GetDesc();

    uint32_t paramSetCount = (computeDownsample) ? desc.MipLevels - 1 : desc.DepthOrArraySize * (desc.MipLevels - 1);
    m_PipelineSets[pipelineID].ParameterSets.clear();
    m_PipelineSets[pipelineID].ParameterSets.resize(paramSetCount);

    for (uint32_t slice = 0; slice < desc.DepthOrArraySize; ++slice)
    {
        for (uint32_t mip = 0; mip < desc.MipLevels - 1; ++mip)
        {
            uint32_t offset = slice * (desc.MipLevels - 1) + mip;

            if (!slice || !computeDownsample)
            {
                ParameterSet* pParamSet = ParameterSet::CreateParameterSet(m_PipelineSets[pipelineID].pRootSignature);
                pParamSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SPDDownsampleInfo), 0);

                // If this is the CS version we also need to setup the UAV for writes to the same parameter set
                if (!computeDownsample)
                    pParamSet->SetTextureSRV(m_pCubeTexture, ViewDimension::Texture2DArray, 0, mip, 1, slice);
                else
                {
                    pParamSet->SetTextureSRV(m_pCubeTexture, ViewDimension::Texture2DArray, 0, mip, desc.DepthOrArraySize, 0);
                    pParamSet->SetTextureUAV(m_pCubeTexture, ViewDimension::Texture2DArray, 0, mip + 1, desc.DepthOrArraySize, 0);
                }

                m_PipelineSets[pipelineID].ParameterSets[offset] = pParamSet;
            }
        }
    }
}

void SPDRenderModule::InitVerificationPipeline()
{
    // Initialize the verification pipeline set
    RootSignatureDesc verificationSignatureDesc;
    verificationSignatureDesc.AddConstantBufferView(0, ShaderBindStage::VertexAndPixel, 1);
    verificationSignatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1);
    verificationSignatureDesc.AddStaticSamplers(0, ShaderBindStage::Pixel, 1, &m_LinearSamplerDesc);
    m_VerificationSet.pRootSignature = RootSignature::CreateRootSignature(L"SPD_VerificationSignature", verificationSignatureDesc);

    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_VerificationSet.pRootSignature);
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"spd_verify_results.hlsl", L"MainVS", ShaderModel::SM6_0, nullptr));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"spd_verify_results.hlsl", L"MainPS", ShaderModel::SM6_0, nullptr));
    // Setup remaining information and build
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
    psoDesc.AddRasterFormats(m_pColorTarget->GetFormat());
    m_VerificationSet.pPipelineObj = PipelineObject::CreatePipelineObject(L"SPD_VerificationPipeline", psoDesc);

    // Setup parameter sets
    const TextureDesc& desc = m_pCubeTexture->GetDesc();

    m_VerificationSet.ParameterSets.clear();
    m_VerificationSet.ParameterSets.resize(1);
    ParameterSet* pParamSet = ParameterSet::CreateParameterSet(m_VerificationSet.pRootSignature);
    pParamSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SPDVerifyConstants), 0);
    pParamSet->SetTextureSRV(m_pCubeTexture, ViewDimension::Texture2DArray, 0);
    m_VerificationSet.ParameterSets[0] = pParamSet;
}

void SPDRenderModule::UpdateSPDContext(bool enabled)
{
    if (enabled && !m_ContextCreated)
    {
        // Setup all the parameters for this SPD run
        m_InitializationParameters.flags = 0;   // Reset
        m_InitializationParameters.flags |= m_SPDLoadLinear ? FFX_SPD_SAMPLER_LINEAR : FFX_SPD_SAMPLER_LOAD;
        m_InitializationParameters.flags |= m_SPDWaveInterop ? FFX_SPD_WAVE_INTEROP_WAVE_OPS : FFX_SPD_WAVE_INTEROP_LDS;
        m_InitializationParameters.flags |= m_SPDMath ? FFX_SPD_MATH_PACKED : FFX_SPD_MATH_NONPACKED;

        ffxSpdContextCreate(&m_Context, &m_InitializationParameters);

        m_ContextCreated = true;
        
    }
    else if (!enabled && m_ContextCreated)
    {
        // Flush anything out of the pipes before destroying the context
        GetDevice()->FlushAllCommandQueues();

        ffxSpdContextDestroy(&m_Context);

        m_ContextCreated = false;
    }
}

void SPDRenderModule::TextureLoadComplete(const std::vector<const Texture*>& textureList, void*)
{
    // Cube map for SPD
    m_pCubeTexture = textureList[0];

    // Will need to reference texture information to create appropriate views
    const TextureDesc& desc = m_pCubeTexture->GetDesc();

    int32_t psID = static_cast<int32_t>(DownsampleTechnique::PSDownsample);

    // Now that the texture is loaded, create raster passes, and initialize the different down sample techniques
    m_RasterViews.resize(desc.DepthOrArraySize * (desc.MipLevels - 1));
    for (uint32_t slice = 0; slice < desc.DepthOrArraySize; ++slice)
    {
        for (uint32_t mip = 0; mip < desc.MipLevels - 1; ++mip)
        {
            // Setup raster sets for the pixel shader down sample
            int32_t resourceOffset = slice * (desc.MipLevels - 1) + mip;
            m_RasterViews[resourceOffset] = GetRasterViewAllocator()->RequestRasterView(m_pCubeTexture, ViewDimension::Texture2DArray, mip + 1, 1, slice);
        }
    }

    // Init all pipelines for the various down sample modes
    InitTraditionalDSPipeline(false);   // PS version
    InitTraditionalDSPipeline(true);    // CS version
    InitVerificationPipeline();         // Result verification pipeline set

    InitFfxContext();

    // We are now ready for use
    SetModuleReady(true);
}

void SPDRenderModule::InitFfxContext()
{
    // Initialize the FFX backend
    const size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(FFX_SPD_CONTEXT_COUNT);
    void* scratchBuffer = calloc(scratchBufferSize, 1u);
    FfxErrorCode errorCode = SDKWrapper::ffxGetInterface(&m_InitializationParameters.backendInterface, GetDevice(), scratchBuffer, scratchBufferSize, FFX_SPD_CONTEXT_COUNT);
    CAULDRON_ASSERT(errorCode == FFX_OK);
    CauldronAssert(ASSERT_CRITICAL, m_InitializationParameters.backendInterface.fpGetSDKVersion(&m_InitializationParameters.backendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
        L"FidelityFX SPD 2.1 sample requires linking with a 1.1.2 version SDK backend");
    CauldronAssert(ASSERT_CRITICAL, ffxSpdGetEffectVersion() == FFX_SDK_MAKE_VERSION(2, 2, 0),
                       L"FidelityFX SPD 2.1 sample requires linking with a 2.2 version FidelityFX SPD library");

    m_InitializationParameters.backendInterface.fpRegisterConstantBufferAllocator(&m_InitializationParameters.backendInterface, SDKWrapper::ffxAllocateConstantBuffer);
    // Init SPD
    UpdateSPDContext(m_DownsamplerUsed == static_cast<int32_t>(DownsampleTechnique::SPDDownsample));
}

void SPDRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX SPD");

    // Pick the right application based on down sampling technique
    DownsampleTechnique technique = static_cast<DownsampleTechnique>(m_DownsamplerUsed);
    switch (technique)
    {
    case DownsampleTechnique::PSDownsample:
        ExecutePSDownsample(deltaTime, pCmdList);
        break;
    case DownsampleTechnique::CSDownsample:
        ExecuteCSDownsample(deltaTime, pCmdList);
        break;
    case DownsampleTechnique::SPDDownsample:
    default:
        ExecuteSPDDownsample(deltaTime, pCmdList);
        break;
    }        

    // Render the verification quads
    ExecuteVerificationQuads(deltaTime, pCmdList);
}

void SPDRenderModule::OnResize(const ResolutionInfo& resInfo)
{
    if (!ModuleEnabled())
        return;

    // Refresh
    UpdateSPDContext(false);
    UpdateSPDContext(true);
}

void SPDRenderModule::ExecuteVerificationQuads(double deltaTime, CommandList* pCmdList)
{
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

    // Barrier the color target to render
    Barrier rtBarrier = Barrier::Transition(m_pColorTarget->GetResource(),
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
        ResourceState::RenderTargetResource);
    ResourceBarrier(pCmdList, 1, &rtBarrier);

    // Begin raster into cube map mip face
    BeginRaster(pCmdList, 1, &m_pColorRasterView);

    // Allocate a dynamic constant buffer and set
    SPDVerifyConstants verityConst = { (uint32_t)m_VerificationSet.ParameterSets.size(), m_ViewSlice, 1.f / GetFramework()->GetAspectRatio(), 0 };
    BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SPDVerifyConstants), &verityConst);
    m_VerificationSet.ParameterSets[0]->UpdateRootConstantBuffer(&bufferInfo, 0);

    // Bind all parameters
    m_VerificationSet.ParameterSets[0]->Bind(pCmdList, m_VerificationSet.pPipelineObj);

    // Set pipeline and draw
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

    Viewport vp = { 0.f, 0.f, resInfo.fDisplayWidth(), resInfo.fDisplayHeight(), 0.f, 1.f };
    SetViewport(pCmdList, &vp);
    Rect scissorRect = { 0, 0, resInfo.RenderWidth, resInfo.RenderHeight };
    SetScissorRects(pCmdList, 1, &scissorRect);

    SetPipelineState(pCmdList, m_VerificationSet.pPipelineObj);
    CauldronAssert(ASSERT_CRITICAL, m_pCubeTexture->GetDesc().MipLevels < SPD_MAX_MIP_LEVELS, L"SPD Shader only can't represent mip. Please grow SPD_MAX_MIP_LEVELS");
    DrawInstanced(pCmdList, 6, m_pCubeTexture->GetDesc().MipLevels);  // Each mip will represent another quad instance

    // End raster into cube map mip face
    EndRaster(pCmdList);

    rtBarrier = Barrier::Transition(m_pColorTarget->GetResource(),
        ResourceState::RenderTargetResource,
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
    ResourceBarrier(pCmdList, 1, &rtBarrier);
}

void SPDRenderModule::ExecutePSDownsample(double deltaTime, cauldron::CommandList* pCmdList)
{
    PipelineSet& pipeline = m_PipelineSets[static_cast<size_t>(DownsampleTechnique::PSDownsample)];

    // Applies to all
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

    // Down sample each face/mip individually
    const TextureDesc& desc = m_pCubeTexture->GetDesc();
    for (uint32_t slice = 0; slice < desc.DepthOrArraySize; ++slice)
    {
        for (uint32_t mip = 0; mip < desc.MipLevels - 1; ++mip)
        {
            int32_t resourceOffset = slice * (desc.MipLevels - 1) + mip;

            // Barrier the face in/out
            Barrier rtBarrier = Barrier::Transition(m_pCubeTexture->GetResource(), 
                ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
                ResourceState::RenderTargetResource, 
                slice * desc.MipLevels + mip + 1);
            ResourceBarrier(pCmdList, 1, &rtBarrier);

            // Begin raster into cube map mip face
            BeginRaster(pCmdList, 1, &m_RasterViews[resourceOffset]);

            // Allocate a dynamic constant buffer and set
            SPDDownsampleInfo constants = { desc.Width >> (mip +1), desc.Height >> (mip + 1), // OutSize
                                            1.f / static_cast<float>(desc.Width >> mip), 1.f / static_cast<float>(desc.Height >> mip), // InvSize
                                            0, 0, 0, 0 };   // Padding
            BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SPDDownsampleInfo), &constants);
            pipeline.ParameterSets[resourceOffset]->UpdateRootConstantBuffer(&bufferInfo, 0);

            // Bind all parameters
            pipeline.ParameterSets[resourceOffset]->Bind(pCmdList, pipeline.pPipelineObj);

            // Set pipeline and draw
            SetViewportScissorRect(pCmdList, 0, 0, desc.Width >> (mip + 1), desc.Height >> (mip + 1), 0.f, 1.f);
            SetPipelineState(pCmdList, pipeline.pPipelineObj);
            DrawInstanced(pCmdList, 3);

            // End raster into cube map mip face
            EndRaster(pCmdList);

            rtBarrier = Barrier::Transition(m_pCubeTexture->GetResource(), 
                ResourceState::RenderTargetResource,
                ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
                slice * desc.MipLevels + mip + 1);
            ResourceBarrier(pCmdList, 1, &rtBarrier);
        }
    }
}

void SPDRenderModule::ExecuteCSDownsample(double deltaTime, cauldron::CommandList* pCmdList)
{
    PipelineSet& pipeline = m_PipelineSets[static_cast<size_t>(DownsampleTechnique::CSDownsample)];

    // Down sample each face/mip individually
    const TextureDesc& desc = m_pCubeTexture->GetDesc();

    for (int32_t slice = 0; slice < (int32_t)desc.DepthOrArraySize ; ++slice)
    {
        for (uint32_t mip = 0; mip < desc.MipLevels - 1; ++mip)
        {
            // Barrier the face in/out
            Barrier rtBarrier = Barrier::Transition(m_pCubeTexture->GetResource(), 
                ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource,
                ResourceState::UnorderedAccess, mip + 1);
            ResourceBarrier(pCmdList, 1, &rtBarrier);

            // Allocate a dynamic constant buffer and set
            SPDDownsampleInfo constants = { desc.Width >> (mip + 1), desc.Height >> (mip + 1), // OutSize
                                            1.f / static_cast<float>(desc.Width >> mip), 1.f / static_cast<float>(desc.Height >> mip), // InvSize
                                            slice, 0, 0, 0 };   // Slice + Padding
            BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SPDDownsampleInfo), &constants);
            pipeline.ParameterSets[mip]->UpdateRootConstantBuffer(&bufferInfo, 0);
            // Bind all parameters
            pipeline.ParameterSets[mip]->Bind(pCmdList, pipeline.pPipelineObj);

            // Set pipeline and dispatch
            SetPipelineState(pCmdList, pipeline.pPipelineObj);
            uint32_t dispatchX = DivideRoundingUp(desc.Width >> (mip + 1), 8);
            uint32_t dispatchY = DivideRoundingUp(desc.Height >> (mip + 1), 8);
            uint32_t dispatchZ = 1;
            Dispatch(pCmdList, dispatchX, dispatchY, dispatchZ);

            rtBarrier = Barrier::Transition(m_pCubeTexture->GetResource(), 
                ResourceState::UnorderedAccess,
                ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
                mip + 1);
            ResourceBarrier(pCmdList, 1, &rtBarrier);
        }
    }
}

void SPDRenderModule::ExecuteSPDDownsample(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"SPD");

    FfxSpdDispatchDescription dispatchParameters = {};
    dispatchParameters.commandList = SDKWrapper::ffxGetCommandList(pCmdList);
    dispatchParameters.resource    = SDKWrapper::ffxGetResource(m_pCubeTexture->GetResource(), L"SPD_Downsample_Resource", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ, FFX_RESOURCE_USAGE_ARRAYVIEW);

    FfxErrorCode errorCode = ffxSpdContextDispatch(&m_Context, &dispatchParameters);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);
}
