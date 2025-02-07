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

#include "breadcrumbsrendermodule.h"

#include "core/backend_interface.h"
#include "core/framework.h"
#include "core/loaders/textureloader.h"
#include "render/device.h"
#include "render/dynamicresourcepool.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rasterview.h"
#include "render/renderdefines.h"
#include "render/uploadheap.h"
#include "render/swapchain.h"

#include <array>
#include <limits>

using namespace cauldron;

BreadcrumbsRenderModule::BreadcrumbsRenderModule()
    : RenderModule(L"BreadcrumbsRenderModule")
{
}

BreadcrumbsRenderModule::~BreadcrumbsRenderModule()
{
    if (m_pParams)
        delete m_pParams;
    if (m_pPipeline)
        delete m_pPipeline;
    if (m_pRootSig)
        delete m_pRootSig;

    // Destroy Breadcrumbs context
    if (m_BreadContextCreated)
        ffxBreadcrumbsContextDestroy(&m_BreadContext);

    // Release the scratch buffer memory
    if (m_BackendScratchBuffer)
        free(m_BackendScratchBuffer);
}

void BreadcrumbsRenderModule::Init(const json& initData)
{
    m_pRenderTarget = GetFramework()->GetColorTargetForCallback(GetName());
    m_pRasterView = GetRasterViewAllocator()->RequestRasterView(m_pRenderTarget, ViewDimension::Texture2D);
    GetDevice()->RegisterDeviceRemovedCallback(BreadcrumbsRenderModule::ProcessDeviceRemovedEvent, &m_BreadContext);

    FfxBreadcrumbsContextDescription contextDesc = {};

    // Initialize the FFX backend
    size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(FFX_BREADCRUMBS_CONTEXT_COUNT);
    m_BackendScratchBuffer = calloc(scratchBufferSize, 1);
    FfxErrorCode errorCode = SDKWrapper::ffxGetInterface(&contextDesc.backendInterface, GetDevice(),
        m_BackendScratchBuffer, scratchBufferSize, FFX_BREADCRUMBS_CONTEXT_COUNT);
    CAULDRON_ASSERT(errorCode == FFX_OK);
    CauldronAssert(ASSERT_CRITICAL, contextDesc.backendInterface.fpGetSDKVersion(&contextDesc.backendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
        L"FidelityFX Breadcrumbs 1.0 sample requires linking with a 1.1.2 version SDK backend");
    CauldronAssert(ASSERT_CRITICAL, ffxBreadcrumbsGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 0, 0),
                       L"FidelityFX Breadcrumbs 1.0 sample requires linking with a 1.0 version FidelityFX Breadcrumbs library");
                       
    contextDesc.backendInterface.fpRegisterConstantBufferAllocator(&contextDesc.backendInterface, SDKWrapper::ffxAllocateConstantBuffer);


    // Create Breadcrumbs context
    contextDesc.flags = FFX_BREADCRUMBS_PRINT_FINISHED_LISTS | FFX_BREADCRUMBS_PRINT_NOT_STARTED_LISTS
        | FFX_BREADCRUMBS_PRINT_FINISHED_NODES | FFX_BREADCRUMBS_PRINT_NOT_STARTED_NODES;
    contextDesc.frameHistoryLength = static_cast<uint32_t>(GetFramework()->GetSwapChain()->GetBackBufferCount() * 2);
    contextDesc.maxMarkersPerMemoryBlock = 3;
    contextDesc.usedGpuQueuesCount = 1;
    contextDesc.pUsedGpuQueues = &m_GpuQueue;
    contextDesc.allocCallbacks.fpAlloc = malloc;
    contextDesc.allocCallbacks.fpRealloc = realloc;
    contextDesc.allocCallbacks.fpFree = free;
    errorCode = ffxBreadcrumbsContextCreate(&m_BreadContext, &contextDesc);
    CAULDRON_ASSERT(errorCode == FFX_OK);
    m_BreadContextCreated = true;

    // Create root signature
    RootSignatureDesc rootSigDesc = {};
    rootSigDesc.AddConstantBufferView(0, ShaderBindStage::Vertex, 1);
    m_pRootSig = RootSignature::CreateRootSignature(L"BreadcrumbsEffect_RootSignature", rootSigDesc);

    // Create pipeline
    PipelineDesc psoDesc = {};
    psoDesc.SetRootSignature(m_pRootSig);
    psoDesc.AddRasterFormats(GetFramework()->GetColorTargetForCallback(GetName())->GetFormat());
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);

    DefineList defines = {};
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"crash_vs.hlsl", L"mainVS", ShaderModel::SM6_0, &defines));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"simple_ps.hlsl", L"mainPS", ShaderModel::SM6_0, &defines));

    m_pPipeline = PipelineObject::CreatePipelineObject(L"BreadcrumbsEffect_Pipeline", psoDesc);

    // Register pipeline for Breadcrumbs
    FfxBreadcrumbsPipelineStateDescription pipelineDesc = {};
    pipelineDesc.pipeline = SDKWrapper::ffxGetPipeline(m_pPipeline);
    pipelineDesc.name = { "Basic pipeline", true };
    pipelineDesc.vertexShader = { "EndlessLoopVS", true };
    pipelineDesc.pixelShader = { "SolidColorPS", true };
    errorCode = ffxBreadcrumbsRegisterPipeline(&m_BreadContext, &pipelineDesc);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // Setup crashing buffer set
    m_pParams = ParameterSet::CreateParameterSet(m_pRootSig);
    m_pParams->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(uint32_t), 0);

    SetModuleReady(true);
}

void BreadcrumbsRenderModule::Execute(double deltaTime, cauldron::CommandList* pCmdList)
{
    // Crash case: infinite loop in single vertex shader invocation
    uint32_t crashLoopCount = 0;
    // Wait for crash frame to not miss the crash point due to too fast execution
    if (GetFramework()->GetFrameID() == m_CrashFrame - 2)
        GetDevice()->FlushAllCommandQueues();
    if (GetFramework()->GetFrameID() == m_CrashFrame)
        crashLoopCount = UINT32_MAX;

    // Transition render target
    Barrier barrier = Barrier::Transition(m_pRenderTarget->GetResource(), 
                                          ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
                                          ResourceState::RenderTargetResource);
    ResourceBarrier(pCmdList, 1, &barrier);

    // Begin new frame
    FfxErrorCode errorCode = ffxBreadcrumbsStartFrame(&m_BreadContext);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // Register command list before using breadcrumbs on it
    FfxBreadcrumbsCommandListDescription listDesc = {};
    listDesc.commandList = SDKWrapper::ffxGetCommandList(pCmdList);
    listDesc.queueType = m_GpuQueue;
    listDesc.name = { "Sample command list", true };
    listDesc.pipeline = nullptr;
    listDesc.submissionIndex = 0;
    errorCode = ffxBreadcrumbsRegisterCommandList(&m_BreadContext, &listDesc);
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // Create tag for main part of rendering
    const FfxBreadcrumbsNameTag mainTag = { "Main rendering", true };
    errorCode = ffxBreadcrumbsBeginMarker(&m_BreadContext, SDKWrapper::ffxGetCommandList(pCmdList), FFX_BREADCRUMBS_MARKER_PASS, &mainTag);
    CAULDRON_ASSERT(errorCode == FFX_OK);
    {
        // Perform simple clear
        const FfxBreadcrumbsNameTag clearTag = { "Reset current backbuffer contents", true };
        errorCode = ffxBreadcrumbsBeginMarker(&m_BreadContext, SDKWrapper::ffxGetCommandList(pCmdList), FFX_BREADCRUMBS_MARKER_CLEAR_RENDER_TARGET, &clearTag);
        CAULDRON_ASSERT(errorCode == FFX_OK);
        {
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            ClearRenderTarget(pCmdList, &m_pRasterView->GetResourceView(), clearColor);
        }
        errorCode = ffxBreadcrumbsEndMarker(&m_BreadContext, SDKWrapper::ffxGetCommandList(pCmdList));
        CAULDRON_ASSERT(errorCode == FFX_OK);

        BeginRaster(pCmdList, 1, &m_pRasterView);

        BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(uint32_t), &crashLoopCount);
        m_pParams->UpdateRootConstantBuffer(&bufferInfo, 0);
        m_pParams->Bind(pCmdList, m_pPipeline);

        // Set and register pipeline state for future markers
        SetPipelineState(pCmdList, m_pPipeline);
        errorCode = ffxBreadcrumbsSetPipeline(&m_BreadContext, SDKWrapper::ffxGetCommandList(pCmdList), SDKWrapper::ffxGetPipeline(m_pPipeline));
        CAULDRON_ASSERT(errorCode == FFX_OK);

        const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
        SetViewportScissorRect(pCmdList, 0, 0, resInfo.DisplayWidth, resInfo.DisplayHeight, 0.f, 1.f);
        SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

        // Perform simple triangle render
        const FfxBreadcrumbsNameTag drawTag = { "Draw simple triangle", true };
        errorCode = ffxBreadcrumbsBeginMarker(&m_BreadContext, SDKWrapper::ffxGetCommandList(pCmdList), FFX_BREADCRUMBS_MARKER_DRAW_INSTANCED, &drawTag);
        CAULDRON_ASSERT(errorCode == FFX_OK);
        {
            DrawInstanced(pCmdList, 3);
        }
        errorCode = ffxBreadcrumbsEndMarker(&m_BreadContext, SDKWrapper::ffxGetCommandList(pCmdList));
        CAULDRON_ASSERT(errorCode == FFX_OK);

        EndRaster(pCmdList);
    }
    // End top level marker
    errorCode = ffxBreadcrumbsEndMarker(&m_BreadContext, SDKWrapper::ffxGetCommandList(pCmdList));
    CAULDRON_ASSERT(errorCode == FFX_OK);

    // Transition render target back
    barrier = Barrier::Transition(m_pRenderTarget->GetResource(),
                                  ResourceState::RenderTargetResource,
                                  ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
    ResourceBarrier(pCmdList, 1, &barrier);
}

void BreadcrumbsRenderModule::ProcessDeviceRemovedEvent(void* data)
{
    FfxBreadcrumbsMarkersStatus markerStatus = {};
    FfxErrorCode result = ffxBreadcrumbsPrintStatus((FfxBreadcrumbsContext*)data, &markerStatus);
    CauldronAssert(ASSERT_CRITICAL, result == FFX_OK, L"Failed to retrieve markers buffer!");

    std::ofstream fout("breadcrumbs_sample_dumpfile.txt", std::ios::binary);
    CauldronAssert(ASSERT_WARNING, fout.good(), L"Failed to create \"breadcrumbs_sample.txt\"!");

    if (fout.good())
    {
        fout.write(markerStatus.pBuffer, markerStatus.bufferSize);
        fout.close();
    }
    // Freeing markers buffer with same functions as provided in FfxBreadcrumbsContextDescription::allocCallbacks::fpFree
    FFX_SAFE_FREE(markerStatus.pBuffer, free);
}
