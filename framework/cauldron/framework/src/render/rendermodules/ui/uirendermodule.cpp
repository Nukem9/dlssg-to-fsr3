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

#include "render/rendermodules/ui/uirendermodule.h"
#include "core/framework.h"
#include "core/uimanager.h"
#include "core/inputmanager.h"

#include "render/color_conversion.h"
#include "render/dynamicresourcepool.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/rasterview.h"
#include "render/rootsignature.h"
#include "render/swapchain.h"
#include "render/texture.h"
#include "shaders/shadercommon.h"

namespace cauldron
{
    constexpr uint32_t g_NumThreadX = 8;
    constexpr uint32_t g_NumThreadY = 8;

    void UIRenderModule::Init(const json& initData)
    {
        //////////////////////////////////////////////////////////////////////////
        // Register UI elements for magnifier
        static constexpr float s_MAGNIFICATION_AMOUNT_MIN = 1.0f;
        static constexpr float s_MAGNIFICATION_AMOUNT_MAX = 32.0f;
        static constexpr float s_MAGNIFIER_RADIUS_MIN = 0.01f;
        static constexpr float s_MAGNIFIER_RADIUS_MAX = 0.85f;

        UISection* uiSection = GetUIManager()->RegisterUIElements("Magnifier");
        if (uiSection)
        {
            m_MagnifierEnabledPtr = uiSection->RegisterUIElement<UICheckBox>("Show Magnifier (M or Middle Mouse Button)", m_MagnifierEnabled);
            m_LockMagnifierPositionPtr = uiSection->RegisterUIElement<UICheckBox>("Lock Position (L)", m_LockMagnifierPosition, m_MagnifierEnabled);
            uiSection->RegisterUIElement<UISlider<float>>(
                "Screen Size",
                m_MagnifierCBData.MagnifierScreenRadius,
                s_MAGNIFIER_RADIUS_MIN,
                s_MAGNIFIER_RADIUS_MAX,
                m_MagnifierEnabled);
            uiSection->RegisterUIElement<UISlider<float>>(
                "Magnification",
                m_MagnifierCBData.MagnificationAmount,
                s_MAGNIFICATION_AMOUNT_MIN,
                s_MAGNIFICATION_AMOUNT_MAX,
                m_MagnifierEnabled);
        }

        //////////////////////////////////////////////////////////////////////////
        // Render Target
        // Get the proper UI color targets
        m_pRenderTarget = GetFramework()->GetRenderTexture(L"SwapChainProxy");
        CauldronAssert(ASSERT_CRITICAL, m_pRenderTarget, L"Couldn't find the render target for UIRenderModule.");

        m_pUiOnlyRenderTarget[0] = GetFramework()->GetRenderTexture(L"UITarget0");
        m_pUiOnlyRenderTarget[1] = GetFramework()->GetRenderTexture(L"UITarget1");

        m_pUIRasterView = GetRasterViewAllocator()->RequestRasterView(m_pRenderTarget, ViewDimension::Texture2D);

        if (GetFramework()->GetRenderTexture(L"HudlessTarget0"))
        {
            m_pHudLessRenderTarget[0] = GetFramework()->GetRenderTexture(L"HudlessTarget0");
            m_pHudLessRenderTarget[1] = GetFramework()->GetRenderTexture(L"HudlessTarget1");

            m_pHudLessRasterView[0] = GetRasterViewAllocator()->RequestRasterView(m_pHudLessRenderTarget[0], ViewDimension::Texture2D);
            m_pHudLessRasterView[1] = GetRasterViewAllocator()->RequestRasterView(m_pHudLessRenderTarget[1], ViewDimension::Texture2D);

            RootSignatureDesc signatureDesc;
            signatureDesc.AddConstantBufferView(0, ShaderBindStage::Pixel, 1);
            signatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1);

            m_pHudLessRootSignature = RootSignature::CreateRootSignature(L"HudLessBlit_RootSignature", signatureDesc);

            m_pHudLessParameters = ParameterSet::CreateParameterSet(m_pHudLessRootSignature);
            m_pHudLessParameters->SetTextureSRV(m_pHudLessRenderTarget[0], ViewDimension::Texture2D, 0);

            // Setup the pipeline object
            PipelineDesc psoDesc;
            psoDesc.SetRootSignature(m_pHudLessRootSignature);

            // Setup the shaders to build on the pipeline object
            psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"fullscreen.hlsl", L"FullscreenVS", ShaderModel::SM6_0, nullptr));
            psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"HudLessBlit.hlsl", L"BlitPS", ShaderModel::SM6_0, nullptr));

            // Setup remaining information and build
            psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
            psoDesc.AddRasterFormats(m_pHudLessRenderTarget[0]->GetFormat());

            m_pHudLessPipelineObj = PipelineObject::CreatePipelineObject(L"HudLessBlit_PipelineObj", psoDesc);
        }

        if (m_pUiOnlyRenderTarget[0])
            m_pUiOnlyRasterView[0] = GetRasterViewAllocator()->RequestRasterView(m_pUiOnlyRenderTarget[0], ViewDimension::Texture2D);
        if (m_pUiOnlyRenderTarget[1])
            m_pUiOnlyRasterView[1] = GetRasterViewAllocator()->RequestRasterView(m_pUiOnlyRenderTarget[1], ViewDimension::Texture2D);

        // Temp intermediate RT for magnifier render pass
        TextureDesc           desc    = m_pRenderTarget->GetDesc();
        const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
        desc.Width                    = resInfo.DisplayWidth;
        desc.Height                   = resInfo.DisplayHeight;
        desc.Name                     = L"Magnifier_Intermediate_Color";
        m_pRenderTargetTemp           = GetDynamicResourcePool()->CreateRenderTexture(
            &desc, [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
                desc.Width  = displayWidth;
                desc.Height = displayHeight;
            });

        //////////////////////////////////////////////////////////////////////////
        // Create sampler - both magnifier and UI can use a point sampler
        SamplerDesc samplerDesc;
        samplerDesc.Filter = FilterFunc::MinMagMipPoint;
        samplerDesc.AddressU = AddressMode::Border;
        samplerDesc.AddressV = AddressMode::Border;
        samplerDesc.AddressW = AddressMode::Border;
        std::vector<SamplerDesc> samplers;
        samplers.push_back(samplerDesc);

        //////////////////////////////////////////////////////////////////////////
        // Root signatures

        // UI
        RootSignatureDesc uiSignatureDesc;
        uiSignatureDesc.AddConstantBufferView(0, ShaderBindStage::Vertex, 1);
        uiSignatureDesc.AddConstantBufferView(1, ShaderBindStage::Pixel, 1);
        uiSignatureDesc.AddTextureSRVSet(0, ShaderBindStage::Pixel, 1);
        uiSignatureDesc.AddStaticSamplers(0, ShaderBindStage::Pixel, (uint32_t)samplers.size(), samplers.data());

        m_pUIRootSignature = RootSignature::CreateRootSignature(L"UIRenderModule_UIRootSignature", uiSignatureDesc);

        // Magnifier
        RootSignatureDesc magSignatureDesc;
        magSignatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1); // Upscaler
        magSignatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1); // MagnifierCB
        magSignatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1);       // ColorTarget
        magSignatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);       // ColorTargetTemp

        m_pMagnifierRootSignature = RootSignature::CreateRootSignature(L"UIRenderModule_MagnifierRootSignature", magSignatureDesc);

        //////////////////////////////////////////////////////////////////////////
        // Setup the pipeline objects (one each for LDR/HDR modes)

        //----------------------------------------------------
        // UI
        PipelineDesc uiPsoDesc;
        uiPsoDesc.SetRootSignature(m_pUIRootSignature);

        // Setup the shaders to build on the pipeline object
        DefineList defineList;
        defineList.insert(std::make_pair(L"NUM_THREAD_X", std::to_wstring(g_NumThreadX)));
        defineList.insert(std::make_pair(L"NUM_THREAD_Y", std::to_wstring(g_NumThreadY)));
        {
            PipelineDesc uiPsoDesc;
            uiPsoDesc.SetRootSignature(m_pUIRootSignature);

            uiPsoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"ui.hlsl", L"uiVS", ShaderModel::SM6_0, &defineList));
            uiPsoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"ui.hlsl", L"uiPS", ShaderModel::SM6_0, &defineList));

            // Setup vertex elements
            std::vector<InputLayoutDesc> vertexAttributes;
            vertexAttributes.push_back(InputLayoutDesc(VertexAttributeType::Position, ResourceFormat::RG32_FLOAT, 0, 0));
            vertexAttributes.push_back(InputLayoutDesc(VertexAttributeType::Texcoord0, ResourceFormat::RG32_FLOAT, 0, 8));
            vertexAttributes.push_back(InputLayoutDesc(VertexAttributeType::Color0, ResourceFormat::RGBA8_UNORM, 0, 16));
            uiPsoDesc.AddInputLayout(vertexAttributes);

            // Setup blend and depth states
            BlendDesc              blendDesc = {true,
                                                Blend::SrcAlpha,
                                                Blend::InvSrcAlpha,
                                                BlendOp::Add,
                                                Blend::One,
                                                Blend::InvSrcAlpha,
                                                BlendOp::Add,
                                                static_cast<uint32_t>(ColorWriteMask::All)};
            std::vector<BlendDesc> blends;
            blends.push_back(blendDesc);
            uiPsoDesc.AddBlendStates(blends, false, false);
            DepthDesc depthDesc;
            uiPsoDesc.AddDepthState(&depthDesc);  // Use defaults

            RasterDesc rasterDesc;
            rasterDesc.CullingMode = CullMode::None;
            uiPsoDesc.AddRasterStateDescription(&rasterDesc);

            // Setup remaining information and build
            uiPsoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);

            uiPsoDesc.AddRasterFormats(m_pRenderTarget->GetFormat());
            m_pUIPipelineObj = PipelineObject::CreatePipelineObject(L"UI_PipelineObj", uiPsoDesc);
        }

        {
            PipelineDesc uiPsoDesc;
            uiPsoDesc.SetRootSignature(m_pUIRootSignature);

            uiPsoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"ui.hlsl", L"uiVS", ShaderModel::SM6_0, &defineList));
            uiPsoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"ui.hlsl", L"uiPS", ShaderModel::SM6_0, &defineList));

            // Setup vertex elements
            std::vector<InputLayoutDesc> vertexAttributes;
            vertexAttributes.push_back(InputLayoutDesc(VertexAttributeType::Position, ResourceFormat::RG32_FLOAT, 0, 0));
            vertexAttributes.push_back(InputLayoutDesc(VertexAttributeType::Texcoord0, ResourceFormat::RG32_FLOAT, 0, 8));
            vertexAttributes.push_back(InputLayoutDesc(VertexAttributeType::Color0, ResourceFormat::RGBA8_UNORM, 0, 16));
            uiPsoDesc.AddInputLayout(vertexAttributes);

            // Setup blend and depth states
            BlendDesc              blendDesc = {true,
                                                Blend::SrcAlpha,
                                                Blend::InvSrcAlpha,
                                                BlendOp::Add,
                                                Blend::One,
                                                Blend::InvSrcAlpha,
                                                BlendOp::Add,
                                                static_cast<uint32_t>(ColorWriteMask::All)};
            std::vector<BlendDesc> blends;
            blends.push_back(blendDesc);
            uiPsoDesc.AddBlendStates(blends, false, false);
            DepthDesc depthDesc;
            uiPsoDesc.AddDepthState(&depthDesc);  // Use defaults

            RasterDesc rasterDesc;
            rasterDesc.CullingMode = CullMode::None;
            uiPsoDesc.AddRasterStateDescription(&rasterDesc);

            // Setup remaining information and build
            uiPsoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);

            uiPsoDesc.AddRasterFormats(m_pRenderTarget->GetFormat());
            m_pAsyncPipelineObj = PipelineObject::CreatePipelineObject(L"UI_AsyncPipelineObj", uiPsoDesc);
        }

        //----------------------------------------------------
        // Magnifier
        PipelineDesc magPsoDesc;
        magPsoDesc.SetRootSignature(m_pMagnifierRootSignature);

        // Setup the shaders to build on the pipeline object
        magPsoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"ui.hlsl", L"MagnifierCS", ShaderModel::SM6_0, &defineList));

        m_pMagnifierPipelineObj = PipelineObject::CreatePipelineObject(L"Magnifier_PipelineObj", magPsoDesc);

        //////////////////////////////////////////////////////////////////////////
        // Setup parameter sets

        // UI
        m_pUIParameters = ParameterSet::CreateParameterSet(m_pUIRootSignature);
        m_pUIParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(UIVertexBufferConstants), 0);
        m_pUIParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(HDRCBData), 1);

        // Magnifier
        m_pMagnifierParameters = ParameterSet::CreateParameterSet(m_pMagnifierRootSignature);
        m_pMagnifierParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(UpscalerInformation), 0);
        m_pMagnifierParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(MagnifierCBData), 1);
        m_pMagnifierParameters->SetTextureSRV(m_pRenderTarget, ViewDimension::Texture2D, 0);
        m_pMagnifierParameters->SetTextureUAV(m_pRenderTargetTemp, ViewDimension::Texture2D, 0);
    }

    UIRenderModule::~UIRenderModule()
    {
        delete m_pHudLessRootSignature;
        delete m_pHudLessPipelineObj;
        delete m_pHudLessParameters;

        delete m_pUIRootSignature;
        delete m_pMagnifierRootSignature;

        delete m_pUIPipelineObj;
        delete m_pAsyncPipelineObj;
        delete m_pMagnifierPipelineObj;

        delete m_pUIParameters;
        delete m_pMagnifierParameters;

        // This may or may not have been assigned, but delete just in case
        delete m_BufferedRenderParams;
    }

    void UIRenderModule::UpdateUI(double deltaTime)
    {
        // Do input updates for magnifier
        // Always use SetData to trigger callback.
        const InputState& inputState = GetInputManager()->GetInputState();
        if (inputState.GetKeyUpState(Key_M) || inputState.GetMouseButtonUpState(Mouse_MButton))
        {
            m_MagnifierEnabledPtr->SetData(!m_MagnifierEnabled);
        }

        if (inputState.GetKeyUpState(Key_L))
        {
            m_LockMagnifierPositionPtr->SetData(!m_LockMagnifierPosition);
        }
    }

    void UIRenderModule::Execute(double deltaTime, CommandList* pCmdList)
    {
        GPUScopedProfileCapture UIMarker(pCmdList, L"UI");

        // If upscaler is enabled, we NEED to have hit the upscaler by now!
        CauldronAssert(ASSERT_WARNING,
                       GetFramework()->GetUpscalingState() != UpscalerState::PreUpscale,
                       L"Upscale state is still PreUpscale when reaching UIRendermodule. This should not be the case.");

        // And fetch all the data ImGUI pushed this frame for UI rendering
        ImDrawData* pUIDrawData = ImGui::GetDrawData();

        // If there's nothing to do, return
        if (!m_MagnifierEnabled && !pUIDrawData)
            return;

        uint32_t rtWidth, rtHeight;
        float    widthScale, heightScale;
        GetFramework()->GetUpscaledRenderInfo(rtWidth, rtHeight, widthScale, heightScale);
        UpscalerInformation upscaleConst = {Vec4((float)rtWidth, (float)rtHeight, widthScale, heightScale)};


        // Check if enabled
        if (m_MagnifierEnabled)
        {
            // Setup constant buffer data and update magnifier to stay centered on screen
            UpdateMagnifierParams();
            BufferAddressInfo bufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(MagnifierCBData), &m_MagnifierCBData);
            BufferAddressInfo upscaleInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(UpscalerInformation), &upscaleConst);

            // Render modules expect resources coming in/going out to be in a shader read state
            {
                Barrier barrier;
                barrier = Barrier::Transition(
                    m_pRenderTargetTemp->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::UnorderedAccess);
                ResourceBarrier(pCmdList, 1, &barrier);
            }

            // Bind everything
            m_pMagnifierParameters->UpdateRootConstantBuffer(&upscaleInfo, 0);
            m_pMagnifierParameters->UpdateRootConstantBuffer(&bufferInfo, 1);
            m_pMagnifierParameters->Bind(pCmdList, m_pMagnifierPipelineObj);

            // Set pipeline and draw
            SetPipelineState(pCmdList, m_pMagnifierPipelineObj);

            const uint32_t numGroupX = DivideRoundingUp(m_pRenderTarget->GetDesc().Width, g_NumThreadX);
            const uint32_t numGroupY = DivideRoundingUp(m_pRenderTarget->GetDesc().Height, g_NumThreadY);
            Dispatch(pCmdList, numGroupX, numGroupY, 1);

            // Copy intermediate target into original render target
            {
                std::vector<Barrier> barriers;

                barriers.push_back(Barrier::Transition(m_pRenderTargetTemp->GetResource(), ResourceState::UnorderedAccess, ResourceState::CopySource));
                barriers.push_back(Barrier::Transition(
                    m_pRenderTarget->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::CopyDest));
                ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

                TextureCopyDesc desc(m_pRenderTargetTemp->GetResource(), m_pRenderTarget->GetResource());
                CopyTextureRegion(pCmdList, &desc);

                barriers[0] = Barrier::Transition(
                    m_pRenderTargetTemp->GetResource(), ResourceState::CopySource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
                ResourceBarrier(pCmdList, 1, barriers.data());
            }

            // If we aren't drawing UI data, then end raster and transition the render target back to read
            if (!pUIDrawData || m_AsyncRender)
            {
                {
                    Barrier barrier;
                    barrier = Barrier::Transition(m_pRenderTarget->GetResource(),
                                                  ResourceState::CopyDest,
                                                  ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
                    ResourceBarrier(pCmdList, 1, &barrier);
                }
            }
        }

        if (pUIDrawData)
        {
            RenderParams renderParams{};
            renderParams.vtxBuffer.resize(pUIDrawData->TotalVtxCount);
            renderParams.idxBuffer.resize(pUIDrawData->TotalIdxCount);

            // Copy data in
            ImDrawVert* pVtx = (ImDrawVert*)renderParams.vtxBuffer.data();
            ImDrawIdx*  pIdx = (ImDrawIdx*)renderParams.idxBuffer.data();
            for (int n = 0; n < pUIDrawData->CmdListsCount; n++)
            {
                const ImDrawList* pIMCmdList = pUIDrawData->CmdLists[n];
                memcpy(pVtx, pIMCmdList->VtxBuffer.Data, pIMCmdList->VtxBuffer.Size * sizeof(ImDrawVert));
                memcpy(pIdx, pIMCmdList->IdxBuffer.Data, pIMCmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
                pVtx += pIMCmdList->VtxBuffer.Size;
                pIdx += pIMCmdList->IdxBuffer.Size;
            }

            // Setup constant buffer data
            float left   = 0.0f;
            float right  = ImGui::GetIO().DisplaySize.x;
            float bottom = ImGui::GetIO().DisplaySize.y;
            float top    = 0.0f;
            Mat4  mvp(Vec4(2.f / (right - left), 0.f, 0.f, 0.f),
                     Vec4(0.f, 2.f / (top - bottom), 0.f, 0.f),
                     Vec4(0.f, 0.f, 0.5f, 0.f),
                     Vec4((right + left) / (left - right), (top + bottom) / (bottom - top), 0.5f, 1.f));

            renderParams.matrix = mvp;

            m_HDRCBData.MonitorDisplayMode  = GetFramework()->GetSwapChain()->GetSwapChainDisplayMode();
            m_HDRCBData.DisplayMaxLuminance = GetFramework()->GetSwapChain()->GetHDRMetaData().MaxLuminance;

            // Scene dependent
            ColorSpace inputColorSpace = ColorSpace_REC709;

            // Display mode dependent
            // Both FSHDR_2084 and HDR10_2084 take rec2020 value
            // Difference is FSHDR needs to be gamut mapped using monitor primaries and then gamut converted to rec2020
            ColorSpace outputColorSpace = ColorSpace_REC2020;
            SetupGamutMapperMatrices(inputColorSpace, outputColorSpace, &m_HDRCBData.ContentToMonitorRecMatrix);

            renderParams.hdr = m_HDRCBData;

            // Render command lists
            uint32_t vtxOffset = 0;
            uint32_t idxOffset = 0;
            for (int32_t n = 0; n < pUIDrawData->CmdListsCount; n++)
            {
                const ImDrawList* pIMCmdList = pUIDrawData->CmdLists[n];
                for (int32_t cmdIndex = 0; cmdIndex < pIMCmdList->CmdBuffer.Size; cmdIndex++)
                {
                    const ImDrawCmd* pDrawCmd = &pIMCmdList->CmdBuffer[cmdIndex];
                    CauldronAssert(ASSERT_ERROR, !pDrawCmd->UserCallback, L"ImGui User Callback is not supported!");
                    // Project scissor/clipping rectangles into framebuffer space
                    ImVec2 clipOffset = pUIDrawData->DisplayPos;
                    ImVec2 clipMin(pDrawCmd->ClipRect.x - clipOffset.x, pDrawCmd->ClipRect.y - clipOffset.y);
                    ImVec2 clipMax(pDrawCmd->ClipRect.z - clipOffset.x, pDrawCmd->ClipRect.w - clipOffset.y);
                    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                        continue;
                    Rect scissorRect = {
                        static_cast<uint32_t>(clipMin.x), static_cast<uint32_t>(clipMin.y), static_cast<uint32_t>(clipMax.x), static_cast<uint32_t>(clipMax.y)};

                    RenderCommand cmd{};
                    cmd.scissor    = scissorRect;
                    cmd.indexCount = pDrawCmd->ElemCount;
                    cmd.startIndex = idxOffset;
                    cmd.baseVertex = vtxOffset;
                    renderParams.commands.push_back(cmd);

                    idxOffset += pDrawCmd->ElemCount;
                }
                vtxOffset += pIMCmdList->VtxBuffer.Size;
            }

            if (m_AsyncRender)
            {
                // Store value for later/different thread.
                RenderParams* pParams = new RenderParams{renderParams};
                pParams               = m_AsyncChannel.exchange(pParams);
                // No one else has this pointer, therefore we should delete it.
                delete pParams;
            }
            else
            {
                const cauldron::GPUResource* rt = m_bRenderToTexture ? m_pUiOnlyRenderTarget[m_curUiTextureIndex]->GetResource() : m_pRenderTarget->GetResource();
                ResourceState prevState = rt->GetCurrentResourceState();
                Barrier       rtBarrier = Barrier::Transition(rt, prevState, ResourceState::RenderTargetResource);
                ResourceBarrier(pCmdList, 1, &rtBarrier);

                if (m_bCopyHudLessTexture)
                {
                    BlitBackbufferToHudLess(pCmdList);
                }

                // Select the render target
                const RasterView* pRTRasterView = (m_bRenderToTexture ? m_pUiOnlyRasterView[m_curUiTextureIndex] : m_pUIRasterView);
                Render(pCmdList, &pRTRasterView->GetResourceView(), &renderParams);

                // Render modules expect resources coming in/going out to be in a shader read state
                rtBarrier.SourceState = rtBarrier.DestState;
                rtBarrier.DestState   = ResourceState::ShaderResource;
                ResourceBarrier(pCmdList, 1, &rtBarrier);

                if (m_MagnifierEnabled && m_bRenderToTexture)
                {
                    Barrier rtBarrier = Barrier::Transition(m_pRenderTarget->GetResource(), ResourceState::CopyDest, ResourceState::ShaderResource);
                    ResourceBarrier(pCmdList, 1, &rtBarrier);
                }
            } 
        }
    }

    void UIRenderModule::Render(CommandList* pCmdList, const ResourceViewInfo* pRTViewInfo, RenderParams* pParams)
    {
        if (m_bRenderToTexture)
        {
            float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            ClearRenderTarget(pCmdList, &m_pUiOnlyRasterView[m_curUiTextureIndex]->GetResourceView(), clearColor);
        }

        BeginRaster(pCmdList, 1, pRTViewInfo);

        // UI RM always done at display resolution
        const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
        Viewport              vp      = {0.f, 0.f, resInfo.fDisplayWidth(), resInfo.fDisplayHeight(), 0.f, 1.f};
        SetViewport(pCmdList, &vp);
        SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

        // Pick the right pipeline / raster set / render target
        PipelineObject* pPipelineObj = m_AsyncRender || m_bRenderToTexture ? m_pAsyncPipelineObj : m_pUIPipelineObj;

        // Get buffers for vertices and indices
        char*             pVertices = nullptr;
        BufferAddressInfo vertBufferInfo =
            GetDynamicBufferPool()->AllocVertexBuffer((uint32_t)pParams->vtxBuffer.size(), sizeof(ImDrawVert), reinterpret_cast<void**>(&pVertices));
        memcpy(pVertices, pParams->vtxBuffer.data(), pParams->vtxBuffer.size() * sizeof(ImDrawVert));
        char*             pIndices = nullptr;
        BufferAddressInfo idxBufferInfo =
            GetDynamicBufferPool()->AllocIndexBuffer((uint32_t)pParams->idxBuffer.size(), sizeof(ImDrawIdx), reinterpret_cast<void**>(&pIndices));
        memcpy(pIndices, pParams->idxBuffer.data(), pParams->idxBuffer.size() * sizeof(ImDrawIdx));

        BufferAddressInfo bufferInfo    = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(Mat4), &pParams->matrix);
        BufferAddressInfo hdrbufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(HDRCBData), &pParams->hdr);

        // Update constant buffer
        m_pUIParameters->UpdateRootConstantBuffer(&bufferInfo, 0);
        m_pUIParameters->UpdateRootConstantBuffer(&hdrbufferInfo, 1);

        // Bind all parameters
        m_pUIParameters->Bind(pCmdList, pPipelineObj);

        // Set pipeline and draw
        SetPipelineState(pCmdList, pPipelineObj);
        SetVertexBuffers(pCmdList, 0, 1, &vertBufferInfo);
        SetIndexBuffer(pCmdList, &idxBufferInfo);

        for (auto& cmd : pParams->commands)
        {
            SetScissorRects(pCmdList, 1, &cmd.scissor);
            DrawIndexedInstanced(pCmdList, cmd.indexCount, 1, cmd.startIndex, cmd.baseVertex, 0);
        }

        EndRaster(pCmdList);
    }

    void UIRenderModule::ExecuteAsync(CommandList* pCmdList, const ResourceViewInfo* pRTViewInfo)
    {
        // Get parameters from async channel
        RenderParams* pParams = m_AsyncChannel.exchange(nullptr);
        // Params are buffered here so that they can be re-used for interpolated frames
        if (pParams)
        {
            delete m_BufferedRenderParams;
            m_BufferedRenderParams = pParams;
        }
        else
        {
            pParams = m_BufferedRenderParams;
        }
        if (pParams)
            Render(pCmdList, pRTViewInfo, pParams);
    }

    void UIRenderModule::SetFontResourceTexture(const Texture* pFontTexture)
    {
        // Bind the font texture to the parameter set
        m_pUIParameters->SetTextureSRV(pFontTexture, ViewDimension::Texture2D, 0);

        // We are now ready for use
        SetModuleReady(true);
    }

    void UIRenderModule::SetAsyncRender(bool async)
    {
        m_AsyncRender = async;
    }

    void UIRenderModule::SetRenderToTexture(bool renderToTexture)
    {
        m_bRenderToTexture = renderToTexture;
    }

    void UIRenderModule::SetCopyHudLessTexture(bool copyHudLessTexture)
    {
        m_bCopyHudLessTexture = copyHudLessTexture;
    }

    void UIRenderModule::SetUiSurfaceIndex(uint32_t uiTextureIndex)
    {
        m_curUiTextureIndex = uiTextureIndex;
    }

    void UIRenderModule::UpdateMagnifierParams()
    {
        // If we are locked, use locked position for mouse pos
        if (m_LockMagnifierPosition)
        {
            m_MagnifierCBData.MousePosX = m_LockedMagnifierPositionX;
            m_MagnifierCBData.MousePosY = m_LockedMagnifierPositionY;
        }
        // Otherwise poll input state, and update the last lock position
        else
        {
            const InputState& inputState = GetInputManager()->GetInputState();
            m_MagnifierCBData.MousePosX = static_cast<int32_t>(inputState.Mouse.AxisState[Mouse_XAxis]);
            m_MagnifierCBData.MousePosY = static_cast<int32_t>(inputState.Mouse.AxisState[Mouse_YAxis]);
            m_LockedMagnifierPositionX = m_MagnifierCBData.MousePosX;
            m_LockedMagnifierPositionY = m_MagnifierCBData.MousePosY;
        }

        // UI RM always uses display resolution
        const ResolutionInfo& resInfo    = GetFramework()->GetResolutionInfo();
        m_MagnifierCBData.ImageWidth     = resInfo.DisplayWidth;
        m_MagnifierCBData.ImageHeight    = resInfo.DisplayHeight;
        m_MagnifierCBData.BorderColorRGB = m_LockMagnifierPosition ? Vec4(0.72f, 0.002f, 0.0f, 1.f) : Vec4(0.002f, 0.72f, 0.0f, 1.f);

        const int32_t imageSize[2] = { static_cast<int>(m_MagnifierCBData.ImageWidth), static_cast<int>(m_MagnifierCBData.ImageHeight) };
        const int32_t& width = imageSize[0];
        const int32_t& height = imageSize[1];

        const int32_t radiusInPixelsMagifier = static_cast<int>(m_MagnifierCBData.MagnifierScreenRadius * height);
        const int32_t radiusInPixelsMagifiedArea = static_cast<int>(m_MagnifierCBData.MagnifierScreenRadius * height / m_MagnifierCBData.MagnificationAmount);

        const bool bCirclesAreOverlapping = radiusInPixelsMagifiedArea + radiusInPixelsMagifier > std::sqrt(m_MagnifierCBData.MagnifierOffsetX * m_MagnifierCBData.MagnifierOffsetX + m_MagnifierCBData.MagnifierOffsetY * m_MagnifierCBData.MagnifierOffsetY);

        if (bCirclesAreOverlapping) // Don't let the two circles overlap
        {
            m_MagnifierCBData.MagnifierOffsetX = radiusInPixelsMagifiedArea + radiusInPixelsMagifier + 1;
            m_MagnifierCBData.MagnifierOffsetY = radiusInPixelsMagifiedArea + radiusInPixelsMagifier + 1;
        }

        // Try to move the magnified area to be fully on screen, if possible
        const int32_t* pMousePos[2] = { &m_MagnifierCBData.MousePosX, &m_MagnifierCBData.MousePosY };
        int32_t* pMagnifierOffset[2] = { &m_MagnifierCBData.MagnifierOffsetX, &m_MagnifierCBData.MagnifierOffsetY };
        for (int32_t i = 0; i < 2; ++i)
        {
            const bool bMagnifierOutOfScreenRegion = *pMousePos[i] + *pMagnifierOffset[i] + radiusInPixelsMagifier > imageSize[i] ||
                *pMousePos[i] + *pMagnifierOffset[i] - radiusInPixelsMagifier < 0;
            if (bMagnifierOutOfScreenRegion)
            {
                if (!(*pMousePos[i] - *pMagnifierOffset[i] + radiusInPixelsMagifier > imageSize[i]
                    || *pMousePos[i] - *pMagnifierOffset[i] - radiusInPixelsMagifier < 0))
                {
                    // Flip offset if possible
                    *pMagnifierOffset[i] = -*pMagnifierOffset[i];
                }
                else
                {
                    // Otherwise clamp
                    if (*pMousePos[i] + *pMagnifierOffset[i] + radiusInPixelsMagifier > imageSize[i])
                        *pMagnifierOffset[i] = imageSize[i] - *pMousePos[i] - radiusInPixelsMagifier;
                    if (*pMousePos[i] + *pMagnifierOffset[i] - radiusInPixelsMagifier < 0)
                        *pMagnifierOffset[i] = -*pMousePos[i] + radiusInPixelsMagifier;
                }
            }
        }
    }

    void UIRenderModule::BlitBackbufferToHudLess(CommandList* pCmdList)
    {
        GPUScopedProfileCapture SwapchainMarker(pCmdList, L"Copy Hudless");

        // Cauldron resources need to be transitioned app-side to avoid confusion in states internally
        // Render modules expect resources coming in/going out to be in a shader read state
        ResourceState state0     = m_pRenderTarget->GetResource()->GetCurrentResourceState();
        ResourceState state1     = m_pHudLessRenderTarget[m_curUiTextureIndex]->GetResource()->GetCurrentResourceState();
        Barrier       barriers[] = {Barrier::Transition(m_pRenderTarget->GetResource(), state0, ResourceState::PixelShaderResource), 
                                    Barrier::Transition(m_pHudLessRenderTarget[m_curUiTextureIndex]->GetResource(), state1, ResourceState::RenderTargetResource)};
        ResourceBarrier(pCmdList, _countof(barriers), barriers);

        BeginRaster(pCmdList, 1, &m_pHudLessRasterView[m_curUiTextureIndex]);

        m_pHudLessParameters->SetTextureSRV(m_pRenderTarget, ViewDimension::Texture2D, 0);

        // Bind all the parameters
        m_pHudLessParameters->Bind(pCmdList, m_pHudLessPipelineObj);

        // Swap chain RM always done at display res
        const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
        SetViewportScissorRect(pCmdList, 0, 0, resInfo.DisplayWidth, resInfo.DisplayHeight, 0.f, 1.f);

        // Set pipeline and draw
        SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);
        SetPipelineState(pCmdList, m_pHudLessPipelineObj);

        DrawInstanced(pCmdList, 3);

        EndRaster(pCmdList);

        // Revert resource states
        for (int i = 0; i < _countof(barriers); ++i)
            std::swap(barriers[i].SourceState, barriers[i].DestState);
        ResourceBarrier(pCmdList, _countof(barriers), barriers);
    }

} // namespace cauldron
