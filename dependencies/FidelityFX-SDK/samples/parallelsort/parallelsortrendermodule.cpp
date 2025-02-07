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

#include "parallelsortrendermodule.h"
#include "shaders/parallelsort_common.h"

#include "core/backend_interface.h"
#include "core/contentmanager.h"
#include "core/framework.h"
#include "core/loaders/textureloader.h"
#include "core/uimanager.h"
#include "render/copyresource.h"
#include "render/device.h"
#include "render/dynamicresourcepool.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rasterview.h"
#include "render/renderdefines.h"
#include "render/rootsignature.h"
#include "render/uploadheap.h"
#include "render/swapchain.h"

#include <random>

using namespace cauldron;
using namespace std::experimental;

const uint32_t ParallelSortRenderModule::s_NumKeys[ResSize_Count] = { 1920 * 1080, 2560 * 1440, 3840 * 2160 };

void ParallelSortRenderModule::Init(const json& initData)
{
    InitFfxContext();

    GetFramework()->ConfigureRuntimeShaderRecompiler(
        [this](void) { DestroyFfxContext(); }, [this](void) { InitFfxContext(); });

    // Register UI for Parallel Sort
    UISection* uiSection = GetUIManager()->RegisterUIElements("Parallel Sort", UISectionType::Sample);

    // Add resolution sizes combo
    std::vector<const char*> resolutionSizes = { "1920x1080", "2560x1440", "3840x2160" };
    uiSection->RegisterUIElement<UICombo>("Buffer resolutions", (int32_t&)m_ParallelSortResolutions, std::move(resolutionSizes));

    // Add output visualization selector
    uiSection->RegisterUIElement<UICheckBox>("Render Sorted Keys", m_ParallelSortRenderSortedKeys);

    // Use the same callback for all option changes, which will always destroy/create the context
    std::function<void(bool,bool)> optionChangeCallback = [this](bool cur, bool old) {
        // Refresh
        ResetParallelSortContext();
    };

    // Add sort payload checkbox
    uiSection->RegisterUIElement<UICheckBox>("Sort Payload", m_ParallelSortPayload, optionChangeCallback);

    // Add indirect execution checkbox
    uiSection->RegisterUIElement<UICheckBox>("Use Indirect Execution", m_ParallelSortIndirectExecution, optionChangeCallback);

    //////////////////////////////////////////////////////////////////////////
    // Generate unsorted key data
    std::vector<uint32_t> keyData[ResSize_Count];
    for (int32_t i = 0; i < ResSize_Count; ++i) {
        // Size the buffer accordingly
        keyData[i].resize(s_NumKeys[i]);
        // Populate the buffers with linear access index
        std::iota(keyData[i].begin(), keyData[i].end(), 0);
        // Shuffle the data
        std::shuffle(keyData[i].begin(), keyData[i].end(), std::mt19937{ std::random_device{}() });
    }

    // Create the unsorted buffers at each resolution, as well as the copy resource for each
    static wchar_t* s_Names[ResSize_Count] = { L"Unsorted 1080p Key Buffer", L"Unsorted 2K Key Buffer", L"Unsorted 4K Key Buffer" };
    for (int32_t i = 0; i < ResSize_Count; ++i)
    {   
        // Unsorted
        BufferDesc bufferDesc = BufferDesc::Data(s_Names[i], static_cast<uint32_t>(keyData[i].size() * sizeof(uint32_t)), static_cast<uint32_t>(sizeof(uint32_t)));
        m_pUnsortedBuffers[i] = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, static_cast<ResourceState>(ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));        
        
        // Copy resource
        const CopyResource::SourceData sourceData{CopyResource::SourceData::Type::BUFFER, keyData[i].size() * sizeof(uint32_t), keyData[i].data()};
        m_pCopyResources[i] = CopyResource::CreateCopyResource(m_pUnsortedBuffers[i]->GetResource(), &sourceData, ResourceState::CopySource);
    }

    // We will use a single max-sized buffer to do key/payload sorts in
    BufferDesc bufferDesc = BufferDesc::Data(L"SortKeyBuffer", s_NumKeys[ResSize_Count-1] * sizeof(uint32_t), sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
    m_pKeysToSort = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, static_cast<ResourceState>(ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    bufferDesc = BufferDesc::Data(L"SortPayloadBuffer", s_NumKeys[ResSize_Count - 1] * sizeof(uint32_t), sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
    m_pPayloadToSort = GetDynamicResourcePool()->CreateBuffer(&bufferDesc, static_cast<ResourceState>(ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));

    // Create the pipeline and signature for verification pass
    m_pRenderTarget = GetFramework()->GetColorTargetForCallback(GetName());
    CauldronAssert(ASSERT_CRITICAL, m_pRenderTarget != nullptr, L"Couldn't find or create the render target for ParallelSortRenderModule.");
    m_pRasterView = GetRasterViewAllocator()->RequestRasterView(m_pRenderTarget, ViewDimension::Texture2D);

    RootSignatureDesc signatureDesc;
    signatureDesc.AddConstantBufferView(0, ShaderBindStage::Pixel, 1);
    signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1);
    signatureDesc.AddBufferSRVSet(1, ShaderBindStage::Pixel, 1);

    m_pRootSignature = RootSignature::CreateRootSignature(L"ParallelSortVerification_RootSignature", signatureDesc);

    // Setup parameter set to use
    m_pParameters = ParameterSet::CreateParameterSet(m_pRootSignature);
    m_pParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(ParallelSortVerifyCBData), 0);

    // Setup the pipeline object
    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_pRootSignature);
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"parallelsort_verify.hlsl", L"FullscreenVS", ShaderModel::SM6_0));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"parallelsort_verify.hlsl", L"RenderSortValidationPS", ShaderModel::SM6_0));

    // Setup blend and depth states
    std::vector<BlendDesc> blends;
    blends.push_back(BlendDesc());      // Use defaults
    psoDesc.AddBlendStates(blends, false, false);
    DepthDesc depthDesc;
    psoDesc.AddDepthState(&depthDesc);  // Use defaults

    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
    psoDesc.AddRasterFormats(m_pRenderTarget->GetFormat());

    m_pPipelineObj = PipelineObject::CreatePipelineObject(L"ParallelSortVerification_PipelineObj", psoDesc);

    // Load the texture data from which to create the texture
    filesystem::path texturePath1080p = L"..\\media\\Textures\\ParallelSort\\parallelsort_validate1080p.png";

    filesystem::path texturePath2K = L"..\\media\\Textures\\ParallelSort\\parallelsort_validate2K.png";

    filesystem::path texturePath4K = L"..\\media\\Textures\\ParallelSort\\parallelsort_validate4K.png";

    TextureLoadCompletionCallbackFn completionCallback = [this](const std::vector<const Texture*>& textures, void* additionalParams = nullptr) { this->TextureLoadComplete(textures, additionalParams); };
    GetContentManager()->LoadTextures(
        {
            TextureLoadInfo(texturePath1080p,   false, 1.f, ResourceFlags::AllowUnorderedAccess),
            TextureLoadInfo(texturePath2K,      false, 1.f, ResourceFlags::AllowUnorderedAccess),
            TextureLoadInfo(texturePath4K,      false, 1.f, ResourceFlags::AllowUnorderedAccess),
        },
        completionCallback);

    while (!ModuleReady()) {}
}

void ParallelSortRenderModule::InitFfxContext()
{
    // Setup FidelityFX interface.
    {
        const size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(FFX_PARALLELSORT_CONTEXT_COUNT);
        void*        scratchBuffer     = calloc(scratchBufferSize, 1u);
        FfxErrorCode errorCode =
            SDKWrapper::ffxGetInterface(&m_InitializationParameters.backendInterface, GetDevice(), scratchBuffer, scratchBufferSize, FFX_PARALLELSORT_CONTEXT_COUNT);
        CAULDRON_ASSERT(errorCode == FFX_OK);
        CauldronAssert(ASSERT_CRITICAL, m_InitializationParameters.backendInterface.fpGetSDKVersion(&m_InitializationParameters.backendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
            L"FidelityFX ParallelSort 2.1 sample requires linking with a 1.1.2 version SDK backend");
        CauldronAssert(ASSERT_CRITICAL, ffxParallelSortGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 3, 0),
                           L"FidelityFX ParallelSort 2.1 sample requires linking with a 1.3 version FidelityFX ParallelSort library");
                           
        m_InitializationParameters.backendInterface.fpRegisterConstantBufferAllocator(&m_InitializationParameters.backendInterface, SDKWrapper::ffxAllocateConstantBuffer);
    }

    // Create the Parallel Sort context
    {
        m_InitializationParameters.flags = 0;
        m_InitializationParameters.flags |= m_ParallelSortIndirectExecution ? FFX_PARALLELSORT_INDIRECT_SORT : 0;
        m_InitializationParameters.flags |= m_ParallelSortPayload ? FFX_PARALLELSORT_PAYLOAD_SORT : 0;

        // Highest resolution is the max number of keys we'll ever sort
        m_InitializationParameters.maxEntries = s_NumKeys[ResSize_Count - 1];

        FfxErrorCode errorCode = ffxParallelSortContextCreate(&m_ParallelSortContext, &m_InitializationParameters);
        CAULDRON_ASSERT(errorCode == FFX_OK);
    }
}

ParallelSortRenderModule::~ParallelSortRenderModule()
{
    DestroyFfxContext();

    // Delete copy resources
    for (int32_t i = 0; i < ResSize_Count; ++i) {
        delete m_pCopyResources[i];
    }

    // Delete other resources
    delete m_pPipelineObj;
    delete m_pRootSignature;
    delete m_pParameters;
}

void ParallelSortRenderModule::DestroyFfxContext()
{
    // Flush anything out of the pipes before destroying the context
    GetDevice()->FlushAllCommandQueues();

    // Destroy the Parallel Sort context
    FfxErrorCode errorCode = ffxParallelSortContextDestroy(&m_ParallelSortContext);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // Destroy backing memory for the backend
    if (m_InitializationParameters.backendInterface.scratchBuffer != nullptr)
    {
        free(m_InitializationParameters.backendInterface.scratchBuffer);
        m_InitializationParameters.backendInterface.scratchBuffer = nullptr;
    }
}

void ParallelSortRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX PARALLEL SORT");
    Barrier barrier;

    // Copy unsorted keys into the key buffer to sort
    // (Note: We don't really care about what's in the payload as we are just interested in the time it takes to copy)
    BufferCopyDesc copyDesc = BufferCopyDesc(m_pCopyResources[m_ParallelSortResolutions]->GetResource(),
                                             m_pKeysToSort->GetResource());
    barrier = Barrier::Transition(m_pKeysToSort->GetResource(), 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
        ResourceState::CopyDest);
    ResourceBarrier(pCmdList, 1, &barrier);
    
    CopyBufferRegion(pCmdList, &copyDesc);
    
    barrier =Barrier::Transition(m_pKeysToSort->GetResource(), 
        ResourceState::CopyDest, 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
    ResourceBarrier(pCmdList, 1, &barrier);

    // Copy unsorted keys into unsorted buffers first time through
    static bool sCopied[ResSize_Count] = { false };
    if (!sCopied[m_ParallelSortResolutions])
    {
        sCopied[m_ParallelSortResolutions] = true;

        copyDesc = BufferCopyDesc(m_pCopyResources[m_ParallelSortResolutions]->GetResource(),
                                    m_pUnsortedBuffers[m_ParallelSortResolutions]->GetResource());

        // Barrier the destination from pixel/compute read to copy dest
        barrier = Barrier::Transition(m_pUnsortedBuffers[m_ParallelSortResolutions]->GetResource(), 
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
            ResourceState::CopyDest);
        ResourceBarrier(pCmdList, 1, &barrier);

        CopyBufferRegion(pCmdList, &copyDesc);

        // Barrier the destination from copy dest to pixel/compute read
        barrier = Barrier::Transition(m_pUnsortedBuffers[m_ParallelSortResolutions]->GetResource(), 
            ResourceState::CopyDest, 
            ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
        ResourceBarrier(pCmdList, 1, &barrier);
    }
    
    // Dispatch parallel sort to do the sorting
    FfxParallelSortDispatchDescription dispatchDesc = {};
    dispatchDesc.commandList    = SDKWrapper::ffxGetCommandList(pCmdList);
    dispatchDesc.keyBuffer      = SDKWrapper::ffxGetResource(m_pKeysToSort->GetResource(), L"ParallelSort_KeyBuffer", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchDesc.payloadBuffer  = SDKWrapper::ffxGetResource(m_pPayloadToSort->GetResource(), L"ParallelSort_PayloadBuffer", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchDesc.numKeysToSort  = s_NumKeys[m_ParallelSortResolutions];
    
    FfxErrorCode errorCode = ffxParallelSortContextDispatch(&m_ParallelSortContext, &dispatchDesc);
    CAULDRON_ASSERT(errorCode == FFX_OK);
    
    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);

    // Render verification pass

    // Set render target for rasterization
    barrier = Barrier::Transition(m_pRenderTarget->GetResource(), 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
        ResourceState::RenderTargetResource);
    ResourceBarrier(pCmdList, 1, &barrier);
    BeginRaster(pCmdList, 1, &m_pRasterView);

    // Set current validation texture to parameter set
    m_pParameters->SetTextureSRV(m_pValidationTextures[m_ParallelSortResolutions], ViewDimension::Texture2D, 0);

    // Set source buffer for keys used in verification
    m_pParameters->SetBufferSRV(m_ParallelSortRenderSortedKeys ? m_pKeysToSort : m_pUnsortedBuffers[m_ParallelSortResolutions], 1);

    // Constant buffer parameters
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    static const uint32_t SortWidths[] = { 1920, 2560, 3840 };
    static const uint32_t SortHeights[] = { 1080, 1440, 2160 };
    
    ParallelSortVerifyCBData constData;
    constData.Width = resInfo.DisplayWidth;
    constData.Height = resInfo.DisplayHeight;
    constData.SortWidth = SortWidths[m_ParallelSortResolutions];
    constData.SortHeight = SortHeights[m_ParallelSortResolutions];

    BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(ParallelSortVerifyCBData), &constData);
    m_pParameters->UpdateRootConstantBuffer(&bufferInfo, 0);

    // Update and bind parameter set
    m_pParameters->Bind(pCmdList, m_pPipelineObj);

    Viewport vp = { 0.f, 0.f, resInfo.fDisplayWidth(), resInfo.fDisplayHeight(), 0.f, 1.f };
    SetViewport(pCmdList, &vp);

    Rect scissorRect = { 0, 0, resInfo.DisplayWidth, resInfo.DisplayHeight };
    SetScissorRects(pCmdList, 1, &scissorRect);

    // Set pipeline and draw
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);
    SetPipelineState(pCmdList, m_pPipelineObj);

    DrawInstanced(pCmdList, 3);

    EndRaster(pCmdList);

    barrier = Barrier::Transition(m_pRenderTarget->GetResource(), 
        ResourceState::RenderTargetResource, 
        ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
    ResourceBarrier(pCmdList, 1, &barrier);
}

void ParallelSortRenderModule::TextureLoadComplete(const std::vector<const Texture*>& textureList, void*)
{
    CauldronAssert(ASSERT_CRITICAL, textureList.size() == ResSize_Count, 
        L"Expected loaded texture list of size %d. Something was changed without fully updating code.", ResSize_Count);
    for (int32_t i = 0; i < ResSize_Count; ++i) {
        m_pValidationTextures[i] = textureList[i];
    }

    // We are now ready for use
    SetModuleReady(true);
}

void ParallelSortRenderModule::ResetParallelSortContext()
{
    // Flush anything out of the pipes before destroying the context
    GetDevice()->FlushAllCommandQueues();

    FfxErrorCode errorCode = ffxParallelSortContextDestroy(&m_ParallelSortContext);
    CAULDRON_ASSERT(errorCode == FFX_OK);
    
    // Setup all the parameters for this parallel sort run
    m_InitializationParameters.flags = 0;
    m_InitializationParameters.flags |= m_ParallelSortIndirectExecution ? FFX_PARALLELSORT_INDIRECT_SORT : 0;
    m_InitializationParameters.flags |= m_ParallelSortPayload ? FFX_PARALLELSORT_PAYLOAD_SORT : 0;
    
    errorCode = ffxParallelSortContextCreate(&m_ParallelSortContext, &m_InitializationParameters);
    CAULDRON_ASSERT(errorCode == FFX_OK);
}

void ParallelSortRenderModule::OnResize(const ResolutionInfo& resInfo)
{
    if (!ModuleEnabled())
        return;

    // Refresh
    ResetParallelSortContext();
}
