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

#include "fsrapirendermodule.h"
#include "render/rendermodules/ui/uirendermodule.h"
#include "render/rasterview.h"
#include "render/dynamicresourcepool.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/rootsignature.h"
#include "render/swapchain.h"
#include "render/resourceviewallocator.h"
#include "core/backend_interface.h"
#include "core/scene.h"
#include "misc/assert.h"
#include "render/profiler.h"
#include "translucency/translucencyrendermodule.h"
#include "core/win/framework_win.h"

#if defined(FFX_API_DX12)
#include <ffx_api/dx12/ffx_api_dx12.hpp>
#include "render/dx12/device_dx12.h"
#include "render/dx12/commandlist_dx12.h"
#elif defined(FFX_API_VK)
#include <ffx_api/vk/ffx_api_vk.hpp>
#include "render/vk/device_vk.h"
#include "render/vk/commandlist_vk.h"
#include "render/vk/swapchain_vk.h"
#endif  // FFX_API_DX12

#include <functional>

using namespace std;
using namespace cauldron;

void RestoreApplicationSwapChain(bool recreateSwapchain = true);

void FSRRenderModule::Init(const json& initData)
{
    m_pTAARenderModule   = static_cast<TAARenderModule*>(GetFramework()->GetRenderModule("TAARenderModule"));
    m_pTransRenderModule = static_cast<TranslucencyRenderModule*>(GetFramework()->GetRenderModule("TranslucencyRenderModule"));
    m_pToneMappingRenderModule = static_cast<ToneMappingRenderModule*>(GetFramework()->GetRenderModule("ToneMappingRenderModule"));
    CauldronAssert(ASSERT_CRITICAL, m_pTAARenderModule, L"FidelityFX FSR Sample: Error: Could not find TAA render module.");
    CauldronAssert(ASSERT_CRITICAL, m_pTransRenderModule, L"FidelityFX FSR Sample: Error: Could not find Translucency render module.");
    CauldronAssert(ASSERT_CRITICAL, m_pToneMappingRenderModule, L"FidelityFX FSR Sample: Error: Could not find Tone Mapping render module.");

    // Fetch needed resources
    m_pColorTarget           = GetFramework()->GetColorTargetForCallback(GetName());
    m_pTonemappedColorTarget = GetFramework()->GetRenderTexture(L"SwapChainProxy");
    m_pDepthTarget           = GetFramework()->GetRenderTexture(L"DepthTarget");
    m_pMotionVectors         = GetFramework()->GetRenderTexture(L"GBufferMotionVectorRT");
    m_pDistortionField[0]    = GetFramework()->GetRenderTexture(L"DistortionField0");
    m_pDistortionField[1]    = GetFramework()->GetRenderTexture(L"DistortionField1");
    m_pReactiveMask          = GetFramework()->GetRenderTexture(L"ReactiveMask");
    m_pCompositionMask       = GetFramework()->GetRenderTexture(L"TransCompMask");
    CauldronAssert(ASSERT_CRITICAL, m_pMotionVectors && m_pDistortionField[0] && m_pDistortionField[1] && m_pReactiveMask && m_pCompositionMask, L"Could not get one of the needed resources for FSR Rendermodule.");

    // Get a CPU resource view that we'll use to map the render target to
    GetResourceViewAllocator()->AllocateCPURenderViews(&m_pRTResourceView);

    // Create render resolution opaque render target to use for auto-reactive mask generation
    TextureDesc desc = m_pColorTarget->GetDesc();
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    desc.Width = resInfo.RenderWidth;
    desc.Height = resInfo.RenderHeight;
    desc.Name = L"FSR_OpaqueTexture";
    m_pOpaqueTexture = GetDynamicResourcePool()->CreateRenderTexture(&desc, [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight)
        {
            desc.Width = renderingWidth;
            desc.Height = renderingHeight;
        });

    // Register additional exports for translucency pass
    BlendDesc      reactiveCompositionBlend = {
        true, Blend::InvDstColor, Blend::One, BlendOp::Add, Blend::One, Blend::Zero, BlendOp::Add, static_cast<uint32_t>(ColorWriteMask::Red)};

    OptionalTransparencyOptions transOptions;
    transOptions.OptionalTargets.push_back(std::make_pair(m_pReactiveMask, reactiveCompositionBlend));
    transOptions.OptionalTargets.push_back(std::make_pair(m_pCompositionMask, reactiveCompositionBlend));
    transOptions.OptionalAdditionalOutputs = L"float ReactiveTarget : SV_TARGET1; float CompositionTarget : SV_TARGET2;";
    transOptions.OptionalAdditionalExports =
        L"float hasAnimatedTexture = 0.f; output.ReactiveTarget = ReactiveMask; output.CompositionTarget = max(Alpha, hasAnimatedTexture);";

    // Add additional exports for FSR to translucency pass
    m_pTransRenderModule->AddOptionalTransparencyOptions(transOptions);

    // Create temporary texture to copy color into before upscale
    {
        TextureDesc desc = m_pColorTarget->GetDesc();
        desc.Name        = L"UpscaleIntermediateTarget";
        desc.Width       = m_pColorTarget->GetDesc().Width;
        desc.Height      = m_pColorTarget->GetDesc().Height;

        m_pTempTexture = GetDynamicResourcePool()->CreateRenderTexture(
            &desc, [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
                desc.Width  = displayWidth;
                desc.Height = displayHeight;
            });
        CauldronAssert(ASSERT_CRITICAL, m_pTempTexture, L"Couldn't create intermediate texture.");
    }
   
    // Create raster views on the reactive mask and composition masks (for clearing and rendering)
    m_RasterViews.resize(2);
    m_RasterViews[0] = GetRasterViewAllocator()->RequestRasterView(m_pReactiveMask, ViewDimension::Texture2D);
    m_RasterViews[1] = GetRasterViewAllocator()->RequestRasterView(m_pCompositionMask, ViewDimension::Texture2D);

    // Set our render resolution function as that to use during resize to get render width/height from display width/height
    m_pUpdateFunc = [this](uint32_t displayWidth, uint32_t displayHeight) { return this->UpdateResolution(displayWidth, displayHeight); };

    //////////////////////////////////////////////////////////////////////////
    // Register additional execution callbacks during the frame

    // Register a post-lighting callback to copy opaque texture
    ExecuteCallback callbackPreTrans = [this](double deltaTime, CommandList* pCmdList) {
        this->PreTransCallback(deltaTime, pCmdList);
    };
    ExecutionTuple callbackPreTransTuple = std::make_pair(L"FSRRenderModule::PreTransCallback", std::make_pair(this, callbackPreTrans));
    GetFramework()->RegisterExecutionCallback(L"LightingRenderModule", false, callbackPreTransTuple);

    // Register a post-transparency callback to generate reactive mask
    ExecuteCallback callbackPostTrans = [this](double deltaTime, CommandList* pCmdList) {
        this->PostTransCallback(deltaTime, pCmdList);
    };
    ExecutionTuple callbackPostTransTuple = std::make_pair(L"FSRRenderModule::PostTransCallback", std::make_pair(this, callbackPostTrans));
    GetFramework()->RegisterExecutionCallback(L"TranslucencyRenderModule", false, callbackPostTransTuple);

    m_curUiTextureIndex     = 0;
    
    // Get the proper UI color target
    m_pUiTexture[0] = GetFramework()->GetRenderTexture(L"UITarget0");
    m_pUiTexture[1] = GetFramework()->GetRenderTexture(L"UITarget1");
    
    // Create FrameInterpolationSwapchain
    // Separate from FSR generation so it can be done when the engine creates the swapchain
    // should not be created and destroyed with FSR, as it requires a switch to windowed mode

    
#if defined(FFX_API_DX12)

    m_FrameInterpolationAvailable = true;
    m_AsyncComputeAvailable       = true;

#elif defined(FFX_API_VK)

    const cauldron::FIQueue* pAsyncComputeQueue = cauldron::GetDevice()->GetImpl()->GetFIAsyncComputeQueue();
    const cauldron::FIQueue* pPresentQueue      = cauldron::GetDevice()->GetImpl()->GetFIPresentQueue();
    const cauldron::FIQueue* pImageAcquireQueue = cauldron::GetDevice()->GetImpl()->GetFIImageAcquireQueue();

    m_FrameInterpolationAvailable = pPresentQueue->queue != VK_NULL_HANDLE && pImageAcquireQueue->queue != VK_NULL_HANDLE;
    m_AsyncComputeAvailable       = m_FrameInterpolationAvailable && pAsyncComputeQueue->queue != VK_NULL_HANDLE;

#endif  // defined(FFX_API_DX12)

    if (!m_FrameInterpolationAvailable)
    {
        m_FrameInterpolation = false;
        s_uiRenderMode       = 0;  // no UI handling
        CauldronWarning(L"Frame interpolation isn't available on this device.");
    }
    if (!m_AsyncComputeAvailable)
    {
        m_EnableAsyncCompute        = false;
        m_PendingEnableAsyncCompute = false;
        m_AllowAsyncCompute         = false;

        CauldronWarning(L"Async compute Frame interpolation isn't available on this device.");
    }

    if (m_FrameInterpolationAvailable)
    {
#if defined(FFX_API_DX12)
        IDXGISwapChain4* dxgiSwapchain = GetSwapChain()->GetImpl()->DX12SwapChain();
        dxgiSwapchain->AddRef();
        cauldron::GetSwapChain()->GetImpl()->SetDXGISwapChain(nullptr);

        ffx::CreateContextDescFrameGenerationSwapChainForHwndDX12 createSwapChainDesc{};
        dxgiSwapchain->GetHwnd(&createSwapChainDesc.hwnd);
        DXGI_SWAP_CHAIN_DESC1 desc1;
        dxgiSwapchain->GetDesc1(&desc1);
        createSwapChainDesc.desc = &desc1;
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc;
        dxgiSwapchain->GetFullscreenDesc(&fullscreenDesc);
        createSwapChainDesc.fullscreenDesc = &fullscreenDesc;
        dxgiSwapchain->GetParent(IID_PPV_ARGS(&createSwapChainDesc.dxgiFactory));
        createSwapChainDesc.gameQueue = GetDevice()->GetImpl()->DX12CmdQueue(cauldron::CommandQueue::Graphics);

        dxgiSwapchain->Release();
        dxgiSwapchain = nullptr;
        createSwapChainDesc.swapchain = &dxgiSwapchain;

        ffx::ReturnCode retCode = ffx::CreateContext(m_SwapChainContext, nullptr, createSwapChainDesc);
        CauldronAssert(ASSERT_CRITICAL, retCode == ffx::ReturnCode::Ok, L"Couldn't create the ffxapi fg swapchain (dx12): %d", (uint32_t)retCode);
        createSwapChainDesc.dxgiFactory->Release();

        cauldron::GetSwapChain()->GetImpl()->SetDXGISwapChain(dxgiSwapchain);

        // In case the app is handling Alt-Enter manually we need to update the window association after creating a different swapchain
        IDXGIFactory7* factory = nullptr;
        if (SUCCEEDED(dxgiSwapchain->GetParent(IID_PPV_ARGS(&factory))))
        {
            factory->MakeWindowAssociation(cauldron::GetFramework()->GetImpl()->GetHWND(), DXGI_MWA_NO_WINDOW_CHANGES);
            factory->Release();
        }

        dxgiSwapchain->Release();

        // Lets do the same for HDR as well as it will need to be re initialized since swapchain was re created
        cauldron::GetSwapChain()->SetHDRMetadataAndColorspace();

#elif defined(FFX_API_VK)

        // Create frameinterpolation swapchain
        cauldron::SwapChain* pSwapchain       = cauldron::GetFramework()->GetSwapChain();
        VkSwapchainKHR       currentSwapchain = pSwapchain->GetImpl()->VKSwapChain();

        ffx::CreateContextDescFrameGenerationSwapChainVK createSwapChainDesc{};
        createSwapChainDesc.physicalDevice        = cauldron::GetDevice()->GetImpl()->VKPhysicalDevice();
        createSwapChainDesc.device                = cauldron::GetDevice()->GetImpl()->VKDevice();
        createSwapChainDesc.swapchain             = &currentSwapchain;
        createSwapChainDesc.createInfo            = *cauldron::GetFramework()->GetSwapChain()->GetImpl()->GetCreateInfo();
        createSwapChainDesc.allocator             = nullptr;
        createSwapChainDesc.gameQueue.queue       = cauldron::GetDevice()->GetImpl()->VKCmdQueue(cauldron::CommandQueue::Graphics);
        createSwapChainDesc.gameQueue.familyIndex = cauldron::GetDevice()->GetImpl()->VKCmdQueueFamily(cauldron::CommandQueue::Graphics);
        createSwapChainDesc.gameQueue.submitFunc  = nullptr;  // this queue is only used in vkQueuePresentKHR, hence doesn't need a callback

        createSwapChainDesc.asyncComputeQueue.queue       = pAsyncComputeQueue->queue;
        createSwapChainDesc.asyncComputeQueue.familyIndex = pAsyncComputeQueue->family;
        createSwapChainDesc.asyncComputeQueue.submitFunc  = nullptr;

        createSwapChainDesc.presentQueue.queue       = pPresentQueue->queue;
        createSwapChainDesc.presentQueue.familyIndex = pPresentQueue->family;
        createSwapChainDesc.presentQueue.submitFunc  = nullptr;

        createSwapChainDesc.imageAcquireQueue.queue       = pImageAcquireQueue->queue;
        createSwapChainDesc.imageAcquireQueue.familyIndex = pImageAcquireQueue->family;
        createSwapChainDesc.imageAcquireQueue.submitFunc  = nullptr;

        // make sure swapchain is not holding a ref to real swapchain
        cauldron::GetFramework()->GetSwapChain()->GetImpl()->SetVKSwapChain(VK_NULL_HANDLE);

        auto convertQueueInfo = [](VkQueueInfoFFXAPI queueInfo) {
            VkQueueInfoFFX info;
            info.queue       = queueInfo.queue;
            info.familyIndex = queueInfo.familyIndex;
            info.submitFunc  = queueInfo.submitFunc;
            return info;
        };

        VkFrameInterpolationInfoFFX frameInterpolationInfo = {};
        frameInterpolationInfo.device                      = createSwapChainDesc.device;
        frameInterpolationInfo.physicalDevice              = createSwapChainDesc.physicalDevice;
        frameInterpolationInfo.pAllocator                  = createSwapChainDesc.allocator;
        frameInterpolationInfo.gameQueue                   = convertQueueInfo(createSwapChainDesc.gameQueue);
        frameInterpolationInfo.asyncComputeQueue           = convertQueueInfo(createSwapChainDesc.asyncComputeQueue);
        frameInterpolationInfo.presentQueue                = convertQueueInfo(createSwapChainDesc.presentQueue);
        frameInterpolationInfo.imageAcquireQueue           = convertQueueInfo(createSwapChainDesc.imageAcquireQueue);

        ffx::ReturnCode retCode = ffx::CreateContext(m_SwapChainContext, nullptr, createSwapChainDesc);

        ffx::QueryDescSwapchainReplacementFunctionsVK replacementFunctions{};
        ffx::Query(m_SwapChainContext, replacementFunctions);
        cauldron::GetDevice()->GetImpl()->SetSwapchainMethodsAndContext(nullptr,
                                                                        nullptr,
                                                                        replacementFunctions.pOutGetSwapchainImagesKHR,
                                                                        replacementFunctions.pOutAcquireNextImageKHR,
                                                                        replacementFunctions.pOutQueuePresentKHR,
                                                                        replacementFunctions.pOutSetHdrMetadataEXT,
                                                                        replacementFunctions.pOutCreateSwapchainFFXAPI,
                                                                        replacementFunctions.pOutDestroySwapchainFFXAPI,
                                                                        nullptr,
                                                                        replacementFunctions.pOutGetLastPresentCountFFXAPI,
                                                                        m_SwapChainContext,
                                                                        &frameInterpolationInfo);

        // Set frameinterpolation swapchain to engine
        cauldron::GetFramework()->GetSwapChain()->GetImpl()->SetVKSwapChain(currentSwapchain, true);
#endif  // defined(FFX_API_DX12)
    }

    // Fetch hudless texture resources
    m_pHudLessTexture[0] = GetFramework()->GetRenderTexture(L"HudlessTarget0");
    m_pHudLessTexture[1] = GetFramework()->GetRenderTexture(L"HudlessTarget1");

    // Start disabled as this will be enabled externally
    cauldron::RenderModule::SetModuleEnabled(false);

    {
        // Register upscale method picker picker
        UISection* uiSection = GetUIManager()->RegisterUIElements("Upscaling", UISectionType::Sample);
        InitUI(uiSection);
    }
    //////////////////////////////////////////////////////////////////////////
    // Finish up init

    // That's all we need for now
    SetModuleReady(true);

    SwitchUpscaler(m_UiUpscaleMethod);
}

FSRRenderModule::~FSRRenderModule()
{
    // Destroy the FSR context
    UpdateFSRContext(false);

    if (m_SwapChainContext != nullptr)
    {
        // Restore the application's swapchain
        ffx::DestroyContext(m_SwapChainContext);
        RestoreApplicationSwapChain(false);
    }
}

void FSRRenderModule::EnableModule(bool enabled)
{
    // If disabling the render module, we need to disable the upscaler with the framework
    if (!enabled)
    {
        // Toggle this now so we avoid the context changes in OnResize
        SetModuleEnabled(enabled);

        // Destroy the FSR context
        UpdateFSRContext(false);

        if (GetFramework()->UpscalerEnabled())
            GetFramework()->EnableUpscaling(false);
        if (GetFramework()->FrameInterpolationEnabled())
            GetFramework()->EnableFrameInterpolation(false);

        UIRenderModule* uimod = static_cast<UIRenderModule*>(GetFramework()->GetRenderModule("UIRenderModule"));
        uimod->SetAsyncRender(false);
        uimod->SetRenderToTexture(false);
        uimod->SetCopyHudLessTexture(false);

        CameraComponent::SetJitterCallbackFunc(nullptr);
    }
    else
    {
        UIRenderModule* uimod = static_cast<UIRenderModule*>(GetFramework()->GetRenderModule("UIRenderModule"));
        uimod->SetAsyncRender(s_uiRenderMode == 2);
        uimod->SetRenderToTexture(s_uiRenderMode == 1);
        uimod->SetCopyHudLessTexture(s_uiRenderMode == 3);

        // Setup everything needed when activating FSR
        // Will also enable upscaling
        UpdatePreset(nullptr);

        // Toggle this now so we avoid the context changes in OnResize
        SetModuleEnabled(enabled);

        // Create the FSR context
        UpdateFSRContext(true);

        if (m_UpscaleMethod == Upscaler_FSRAPI)
        {
            // Set the jitter callback to use
            CameraJitterCallback jitterCallback = [this](Vec2& values) {
                // Increment jitter index for frame
                ++m_JitterIndex;

                // Update FSR jitter for built in TAA
                const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

                ffx::ReturnCode                     retCode;
                int32_t                             jitterPhaseCount;
                ffx::QueryDescUpscaleGetJitterPhaseCount getJitterPhaseDesc{};
                getJitterPhaseDesc.displayWidth   = resInfo.DisplayWidth;
                getJitterPhaseDesc.renderWidth    = resInfo.RenderWidth;
                getJitterPhaseDesc.pOutPhaseCount = &jitterPhaseCount;

                retCode = ffx::Query(m_UpscalingContext, getJitterPhaseDesc);
                CauldronAssert(ASSERT_CRITICAL, retCode == ffx::ReturnCode::Ok,
                    L"ffxQuery(FSR_GETJITTERPHASECOUNT) returned %d", retCode);

                ffx::QueryDescUpscaleGetJitterOffset getJitterOffsetDesc{};
                getJitterOffsetDesc.index                              = m_JitterIndex;
                getJitterOffsetDesc.phaseCount                         = jitterPhaseCount;
                getJitterOffsetDesc.pOutX                              = &m_JitterX;
                getJitterOffsetDesc.pOutY                              = &m_JitterY;

                retCode = ffx::Query(m_UpscalingContext, getJitterOffsetDesc);

                CauldronAssert(ASSERT_CRITICAL, retCode == ffx::ReturnCode::Ok,
                    L"ffxQuery(FSR_GETJITTEROFFSET) returned %d", retCode);

                values = Vec2(-2.f * m_JitterX / resInfo.RenderWidth, 2.f * m_JitterY / resInfo.RenderHeight);
            };
            CameraComponent::SetJitterCallbackFunc(jitterCallback);
        }

        ClearReInit();
    }

    // Show or hide UI elements for active upscaler
    for (auto& i : m_UIElements)
    {
        i->Show(enabled);
    }
}

FfxErrorCode waitCallback(wchar_t* fenceName, uint64_t fenceValueToWaitFor)
{
    CAUDRON_LOG_DEBUG(L"waiting on '%ls' with value %llu", fenceName, fenceValueToWaitFor);
    return FFX_API_RETURN_OK;
}

void FSRRenderModule::InitUI(UISection* pUISection)
{
    std::vector<const char*> comboOptions = {"Native", "FSR (ffxapi)"};
    pUISection->RegisterUIElement<UICombo>("Method", (int32_t&)m_UiUpscaleMethod, std::move(comboOptions), [this](int32_t cur, int32_t old) { SwitchUpscaler(cur); });

    // get version info from ffxapi
    ffx::QueryDescGetVersions versionQuery{};
    versionQuery.createDescType = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
#ifdef FFX_API_DX12
    versionQuery.device = GetDevice()->GetImpl()->DX12Device();
#endif
    uint64_t versionCount = 0;
    versionQuery.outputCount = &versionCount;
    ffxQuery(nullptr, &versionQuery.header);

    std::vector<const char*> versionNames;
    m_FsrVersionIds.resize(versionCount);
    versionNames.resize(versionCount);
    versionQuery.versionIds = m_FsrVersionIds.data();
    versionQuery.versionNames = versionNames.data();
    ffxQuery(nullptr, &versionQuery.header);

    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICombo>(
        "FSR Version", (int32_t&)m_FsrVersionIndex, std::move(versionNames), [this](int32_t, int32_t) { m_NeedReInit = true; }, false));

    // Setup scale preset options
    std::vector<const char*> presetComboOptions = { "Native AA (1.0x)", "Quality (1.5x)", "Balanced (1.7x)", "Performance (2x)", "Ultra Performance (3x)", "Custom" };
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICombo>(
        "Scale Preset", (int32_t&)m_ScalePreset, std::move(presetComboOptions), m_IsNonNative, [this](int32_t cur, int32_t old) { UpdatePreset(&old); }, false));
    
    // Setup mip bias
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UISlider<float>>(
        "Mip LOD Bias",
        m_MipBias,
        -5.f, 0.f,
        [this](float cur, float old) {
            UpdateMipBias(&old);
        },
        false
    ));

    // Setup scale factor (disabled for all but custom)
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UISlider<float>>(
        "Custom Scale",
        m_UpscaleRatio,
        1.f, 3.f,
        m_UpscaleRatioEnabled,
        [this](float cur, float old) {
            UpdateUpscaleRatio(&old);
        },
        false
    ));

    m_UIElements.emplace_back(pUISection->RegisterUIElement<UISlider<float>>(
        "Letterbox size", m_LetterboxRatio, 0.1f, 1.f, [this](float cur, float old) { UpdateUpscaleRatio(&old); }, false));

    m_UIElements.emplace_back(pUISection->RegisterUIElement<UIButton>("Reset Upscaling", [this]() { m_ResetUpscale = true; }));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Draw upscaler debug view", m_DrawUpscalerDebugView, nullptr, false));

    // Reactive mask
    std::vector<const char*> maskComboOptions{ "Disabled", "Manual Reactive Mask Generation", "Autogen FSR2 Helper Function" };
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICombo>("Reactive Mask Mode", (int32_t&)m_MaskMode, std::move(maskComboOptions), m_EnableMaskOptions, nullptr, false));

    // Use mask
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Use Transparency and Composition Mask", m_UseMask, m_EnableMaskOptions, nullptr, false));

    // Sharpening
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("RCAS Sharpening", m_RCASSharpen, nullptr, false, false));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UISlider<float>>("Sharpness", m_Sharpness, 0.f, 1.f, m_RCASSharpen, nullptr, false));

    //Set Upscaler CB KeyValue post context creation
    std::vector<const char*>        configureUpscaleKeyLabels = { "fVelocity" };
    
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICombo>(
        "Upscaler CB Key to set",
        m_UpscalerCBKey,
        configureUpscaleKeyLabels,
        m_EnableMaskOptions,
        nullptr,
        m_EnableMaskOptions));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UISlider<float>>(
        "Upscaler CB Value to set",
        m_UpscalerCBValue,
        0.f, 1.f,
        m_EnableMaskOptions,
        [this](float, float) { SetUpscaleConstantBuffer(m_UpscalerCBKey, m_UpscalerCBValue); },
        m_EnableMaskOptions));

    // Frame interpolation
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Frame Interpolation", m_FrameInterpolation, m_FrameInterpolationAvailable,
        [this](bool, bool)
        {
            m_OfUiEnabled = m_FrameInterpolation && s_enableSoftwareMotionEstimation;
            
            GetFramework()->EnableFrameInterpolation(m_FrameInterpolation);
            
            // Ask main loop to re-initialize.
            m_NeedReInit = true;
        }, 
        false));

    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Support Async Compute", m_PendingEnableAsyncCompute,
        m_AsyncComputeAvailable,
        [this](bool, bool) 
        {
            // Ask main loop to re-initialize.
            m_NeedReInit = true;
        }, 
        false));

    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Allow async compute", m_AllowAsyncCompute, m_PendingEnableAsyncCompute, nullptr, false));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Use callback", m_UseCallback, m_FrameInterpolation, nullptr, false));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Use Distortion Field Input", m_UseDistortionField, m_FrameInterpolation, nullptr, false));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Draw frame generation tear lines", m_DrawFrameGenerationDebugTearLines, m_FrameInterpolation, nullptr, false));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Draw frame generation pacing lines", m_DrawFrameGenerationDebugPacingLines, m_FrameInterpolation, nullptr, false));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Draw frame generation reset indicators", m_DrawFrameGenerationDebugResetIndicators, m_FrameInterpolation, nullptr, false));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Draw frame generation debug view", m_DrawFrameGenerationDebugView, m_FrameInterpolation, nullptr, false));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("Present interpolated only", m_PresentInterpolatedOnly, m_FrameInterpolation, nullptr, false));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UIButton>("Reset Frame Interpolation", m_FrameInterpolation, [this]() { m_ResetFrameInterpolation = true; }));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UIButton>("Simulate present skip", m_FrameInterpolation, [this]() { m_SimulatePresentSkip = true; }));
    
    std::vector<const char*>        uiRenderModeLabels = { "No UI handling (not recommended)", "UiTexture", "UiCallback", "Pre-Ui Backbuffer" };
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICombo>(
        "UI Composition Mode",
        s_uiRenderMode,
        uiRenderModeLabels, 
        m_FrameInterpolation,
        [this](int32_t, int32_t) 
        {
            UIRenderModule* uimod = static_cast<UIRenderModule*>(GetFramework()->GetRenderModule("UIRenderModule"));
            uimod->SetAsyncRender(s_uiRenderMode == 2);
            uimod->SetRenderToTexture(s_uiRenderMode == 1);
            uimod->SetCopyHudLessTexture(s_uiRenderMode == 3);
            // Need to recreate the FSR context
            EnableModule(false);
            EnableModule(true);
        },
        false));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>("DoubleBuffer UI resource in swapchain", m_DoublebufferInSwapchain, m_FrameInterpolation, nullptr, false));

    std::vector<const char*>        waitCallbackModeLabels = { "nullptr", "CAUDRON_LOG_DEBUG(\"waitCallback\")"};
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICombo>(
        "WaitCallback Mode",
        m_waitCallbackMode,
        waitCallbackModeLabels,
        m_EnableWaitCallbackModeUI,
        [this](int32_t, int32_t)
        {
#if defined(FFX_API_DX12)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueDX12 m_swapchainKeyValueConfig{};
#elif defined(FFX_API_VK)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueVK m_swapchainKeyValueConfig{};
#endif
            m_swapchainKeyValueConfig.key = FFX_API_CONFIGURE_FG_SWAPCHAIN_KEY_WAITCALLBACK;
            if (m_waitCallbackMode == 0)
            {
                m_swapchainKeyValueConfig.ptr = nullptr;
            }
            else if (m_waitCallbackMode == 1)
            {
                m_swapchainKeyValueConfig.ptr = waitCallback;
            }
            ffx::Configure(m_SwapChainContext, m_swapchainKeyValueConfig);

        },
        m_EnableMaskOptions));


    m_UIElements.emplace_back(pUISection->RegisterUIElement<UISlider<float>>(
        "Frame Pacing safetyMarginInMs",
        m_SafetyMarginInMs,
        0.0f, 1.0f,
        m_FrameInterpolation,
        [this](float, float) {
#if defined(FFX_API_DX12)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueDX12 m_swapchainKeyValueConfig{};
#elif defined(FFX_API_VK)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueVK m_swapchainKeyValueConfig{};
#endif
            m_swapchainKeyValueConfig.key = FFX_API_CONFIGURE_FG_SWAPCHAIN_KEY_FRAMEPACINGTUNING;
            m_swapchainKeyValueConfig.ptr = &framePacingTuning;

            framePacingTuning.safetyMarginInMs = m_SafetyMarginInMs;

            ffx::Configure(m_SwapChainContext, m_swapchainKeyValueConfig);
        },
        m_FrameInterpolation));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UISlider<float>>(
        "Frame Pacing varianceFactor",
        m_VarianceFactor,
        0.0f, 1.0f,
        m_FrameInterpolation,
        [this](float, float) {
#if defined(FFX_API_DX12)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueDX12 m_swapchainKeyValueConfig{};
#elif defined(FFX_API_VK)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueVK m_swapchainKeyValueConfig{};
#endif
            m_swapchainKeyValueConfig.key = FFX_API_CONFIGURE_FG_SWAPCHAIN_KEY_FRAMEPACINGTUNING;
            m_swapchainKeyValueConfig.ptr = &framePacingTuning;

            framePacingTuning.varianceFactor = m_VarianceFactor;

            ffx::Configure(m_SwapChainContext, m_swapchainKeyValueConfig);
        },
        m_FrameInterpolation));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>(
        "Frame Pacing allowHybridSpin", 
        m_AllowHybridSpin, 
        m_FrameInterpolation,
        [this](bool, bool) {
#if defined(FFX_API_DX12)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueDX12 m_swapchainKeyValueConfig{};
#elif defined(FFX_API_VK)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueVK m_swapchainKeyValueConfig{};
#endif
            m_swapchainKeyValueConfig.key = FFX_API_CONFIGURE_FG_SWAPCHAIN_KEY_FRAMEPACINGTUNING;
            m_swapchainKeyValueConfig.ptr = &framePacingTuning;

            framePacingTuning.allowHybridSpin = m_AllowHybridSpin;

            ffx::Configure(m_SwapChainContext, m_swapchainKeyValueConfig);
        }));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UISlider<int32_t>>(
        "hybridSpinTime in timer resolution units",
        (int32_t&) m_HybridSpinTime,
        0, 10,
        m_FrameInterpolation,
        [this](int32_t, int32_t) {
#if defined(FFX_API_DX12)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueDX12 m_swapchainKeyValueConfig{};
#elif defined(FFX_API_VK)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueVK m_swapchainKeyValueConfig{};
#endif
            m_swapchainKeyValueConfig.key = FFX_API_CONFIGURE_FG_SWAPCHAIN_KEY_FRAMEPACINGTUNING;
            m_swapchainKeyValueConfig.ptr = &framePacingTuning;

            framePacingTuning.hybridSpinTime = m_HybridSpinTime;

            ffx::Configure(m_SwapChainContext, m_swapchainKeyValueConfig);
        },
        m_FrameInterpolation));
    m_UIElements.emplace_back(pUISection->RegisterUIElement<UICheckBox>(
        "allowWaitForSingleObjectOnFence",
        m_AllowWaitForSingleObjectOnFence,
        m_FrameInterpolation,
        [this](bool, bool) {
#if defined(FFX_API_DX12)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueDX12 m_swapchainKeyValueConfig{};
#elif defined(FFX_API_VK)
            ffx::ConfigureDescFrameGenerationSwapChainKeyValueVK m_swapchainKeyValueConfig{};
#endif
            m_swapchainKeyValueConfig.key = FFX_API_CONFIGURE_FG_SWAPCHAIN_KEY_FRAMEPACINGTUNING;
            m_swapchainKeyValueConfig.ptr = &framePacingTuning;

            framePacingTuning.allowWaitForSingleObjectOnFence = m_AllowWaitForSingleObjectOnFence;

            ffx::Configure(m_SwapChainContext, m_swapchainKeyValueConfig);
        }));
    EnableModule(true);
}

void FSRRenderModule::SwitchUpscaler(int32_t newUpscaler)
{
    // Flush everything out of the pipe before disabling/enabling things
    GetDevice()->FlushAllCommandQueues();

    if (ModuleEnabled())
        EnableModule(false);

    // 0 = native, 1 = FFXAPI
    SetFilter(newUpscaler);
    switch (newUpscaler)
    {
    case 0:
        m_pTAARenderModule->EnableModule(false);
        m_pToneMappingRenderModule->EnableModule(true);
        m_EnableMaskOptions = false;
        break;
    case 1:
        ClearReInit();
        // Also disable TAA render module
        m_pTAARenderModule->EnableModule(false);
        m_pToneMappingRenderModule->EnableModule(true);
        m_EnableMaskOptions = true;
        break;
    default:
        CauldronCritical(L"Unsupported upscaler requested.");
        break;
    }

    m_EnableWaitCallbackModeUI = m_EnableMaskOptions && m_FrameInterpolationAvailable;

    m_UpscaleMethod = newUpscaler;

    // Enable the new one
    EnableModule(true);
    ClearReInit();
}

void FSRRenderModule::UpdatePreset(const int32_t* pOldPreset)
{
    switch (m_ScalePreset)
    {
    case FSRScalePreset::NativeAA:
        m_UpscaleRatio = 1.0f;
        break;
    case FSRScalePreset::Quality:
        m_UpscaleRatio = 1.5f;
        break;
    case FSRScalePreset::Balanced:
        m_UpscaleRatio = 1.7f;
        break;
    case FSRScalePreset::Performance:
        m_UpscaleRatio = 2.0f;
        break;
    case FSRScalePreset::UltraPerformance:
        m_UpscaleRatio = 3.0f;
        break;
    case FSRScalePreset::Custom:
    default:
        // Leave the upscale ratio at whatever it was
        break;
    }

    // Update whether we can update the custom scale slider
    m_UpscaleRatioEnabled = (m_ScalePreset == FSRScalePreset::Custom);

    // Update mip bias
    float oldValue = m_MipBias;
    if (m_ScalePreset != FSRScalePreset::Custom)
        m_MipBias = cMipBias[static_cast<uint32_t>(m_ScalePreset)];
    else
        m_MipBias = m_MipBias = CalculateMipBias(m_UpscaleRatio);
    UpdateMipBias(&oldValue);

    // Update resolution since rendering ratios have changed
    GetFramework()->EnableUpscaling(true, m_pUpdateFunc);

    GetFramework()->EnableFrameInterpolation(m_FrameInterpolation);
}

void FSRRenderModule::UpdateUpscaleRatio(const float* pOldRatio)
{
    // Disable/Enable FSR since resolution ratios have changed
    GetFramework()->EnableUpscaling(true, m_pUpdateFunc);
}

void FSRRenderModule::UpdateMipBias(const float* pOldBias)
{
    // Update the scene MipLODBias to use
    GetScene()->SetMipLODBias(m_MipBias);
}

void FSRRenderModule::FfxMsgCallback(uint32_t type, const wchar_t* message)
{
    if (type == FFX_API_MESSAGE_TYPE_ERROR)
    {
        CauldronError(L"FSR_API_DEBUG_ERROR: %ls", message);
    }
    else if (type == FFX_API_MESSAGE_TYPE_WARNING)
    {
        CauldronWarning(L"FSR_API_DEBUG_WARNING: %ls", message);
    }
}

ffxReturnCode_t FSRRenderModule::UiCompositionCallback(ffxCallbackDescFrameGenerationPresent* params)
{
    if (s_uiRenderMode != 2)
        return FFX_API_RETURN_ERROR_PARAMETER;

    // Get a handle to the UIRenderModule for UI composition (only do this once as there's a search cost involved)
    if (!m_pUIRenderModule)
    {
        m_pUIRenderModule = static_cast<UIRenderModule*>(GetFramework()->GetRenderModule("UIRenderModule"));
        CauldronAssert(ASSERT_CRITICAL, m_pUIRenderModule, L"Could not get a handle to the UIRenderModule.");
    }
    
    // Wrap everything in cauldron wrappers to allow backend agnostic execution of UI render.
    CommandList* pCmdList = CommandList::GetWrappedCmdListFromSDK(L"UI_CommandList", CommandQueue::Graphics, params->commandList);
    SetAllResourceViewHeaps(pCmdList); // Set the resource view heaps to the wrapped command list

    ResourceState rtResourceState = SDKWrapper::GetFrameworkState((FfxResourceStates)params->outputSwapChainBuffer.state);
    ResourceState bbResourceState = SDKWrapper::GetFrameworkState((FfxResourceStates)params->currentBackBuffer.state);

    TextureDesc rtDesc = SDKWrapper::GetFrameworkTextureDescription(params->outputSwapChainBuffer.description);
    TextureDesc bbDesc = SDKWrapper::GetFrameworkTextureDescription(params->currentBackBuffer.description);

    GPUResource* pRTResource = GPUResource::GetWrappedResourceFromSDK(L"UI_RenderTarget", params->outputSwapChainBuffer.resource, &rtDesc, rtResourceState);
    GPUResource* pBBResource = GPUResource::GetWrappedResourceFromSDK(L"BackBuffer", params->currentBackBuffer.resource, &bbDesc, bbResourceState);
    
    std::vector<Barrier> barriers;
    barriers = {Barrier::Transition(pRTResource, rtResourceState, ResourceState::CopyDest),
                Barrier::Transition(pBBResource, bbResourceState, ResourceState::CopySource)};
    ResourceBarrier(pCmdList, (uint32_t)barriers.size(), barriers.data());
        
    TextureCopyDesc copyDesc(pBBResource, pRTResource);
    CopyTextureRegion(pCmdList, &copyDesc);
    
    barriers[0].SourceState = barriers[0].DestState;
    barriers[0].DestState   = ResourceState::RenderTargetResource;
    swap(barriers[1].SourceState, barriers[1].DestState);
    ResourceBarrier(pCmdList, (uint32_t)barriers.size(), barriers.data());

    // Create and set RTV, required for async UI render.
    FfxApiResourceDescription rdesc = params->outputSwapChainBuffer.description;

    TextureDesc rtResourceDesc = TextureDesc::Tex2D(L"UI_RenderTarget",
                                                    SDKWrapper::GetFrameworkSurfaceFormat((FfxSurfaceFormat)rdesc.format),
                                                    rdesc.width,
                                                    rdesc.height,
                                                    rdesc.depth,
                                                    rdesc.mipCount,
                                                    ResourceFlags::AllowRenderTarget);
    m_pRTResourceView->BindTextureResource(pRTResource, rtResourceDesc, ResourceViewType::RTV, ViewDimension::Texture2D, 0, 1, 0);
    
    m_pUIRenderModule->ExecuteAsync(pCmdList, &m_pRTResourceView->GetViewInfo(0));

    ResourceBarrier(pCmdList, 1, &Barrier::Transition(pRTResource, ResourceState::RenderTargetResource, rtResourceState));

    // Clean up wrapped resources for the frame
    delete pBBResource;
    delete pRTResource;
    delete pCmdList;

    return FFX_API_RETURN_OK;
}

void FSRRenderModule::UpdateFSRContext(bool enabled)
{
    if (enabled)
    {
        const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
        static bool s_InvertedDepth = GetConfig()->InvertedDepth;
        // Backend creation (for both FFXAPI contexts, FG and Upscale)
#if defined(FFX_API_DX12)
        ffx::CreateBackendDX12Desc backendDesc{};
        backendDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
        backendDesc.device = GetDevice()->GetImpl()->DX12Device();
#elif defined(FFX_API_VK)
        DeviceInternal *device = GetDevice()->GetImpl();
        ffx::CreateBackendVKDesc backendDesc{};
        backendDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK;
        backendDesc.vkDevice = device->VKDevice();
        backendDesc.vkPhysicalDevice = device->VKPhysicalDevice();
        backendDesc.vkDeviceProcAddr = vkGetDeviceProcAddr;
#endif  // defined(FFX_API_DX12)

        if (m_UpscaleMethod == Upscaler_FSRAPI)
        {
            ffx::CreateContextDescUpscale createFsr{};
            
            createFsr.maxUpscaleSize = {resInfo.DisplayWidth, resInfo.DisplayHeight};
            createFsr.maxRenderSize  = {resInfo.DisplayWidth, resInfo.DisplayHeight};
            createFsr.flags = FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
            if (s_InvertedDepth)
            {
                createFsr.flags |= FFX_UPSCALE_ENABLE_DEPTH_INVERTED | FFX_UPSCALE_ENABLE_DEPTH_INFINITE;
            }
            createFsr.flags |= FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE;

            // Do eror checking in debug
    #if defined(_DEBUG)
            createFsr.flags |= FFX_UPSCALE_ENABLE_DEBUG_CHECKING;
            createFsr.fpMessage = &FSRRenderModule::FfxMsgCallback;
    #endif  // #if defined(_DEBUG)

            // Create the FSR context
            {
                ffx::ReturnCode retCode;
                // lifetime of this must last until after CreateContext call!
                ffx::CreateContextDescOverrideVersion versionOverride{};
                if (m_FsrVersionIndex < m_FsrVersionIds.size())
                {
                    versionOverride.versionId = m_FsrVersionIds[m_FsrVersionIndex];
                    retCode = ffx::CreateContext(m_UpscalingContext, nullptr, createFsr, backendDesc, versionOverride);
                }
                else
                {
                    retCode = ffx::CreateContext(m_UpscalingContext, nullptr, createFsr, backendDesc);
                }

                CauldronAssert(ASSERT_CRITICAL, retCode == ffx::ReturnCode::Ok, L"Couldn't create the ffxapi upscaling context: %d", (uint32_t)retCode);
            }
            FfxApiEffectMemoryUsage gpuMemoryUsageUpscaler;
            ffx::QueryDescUpscaleGetGPUMemoryUsage upscalerGetGPUMemoryUsage{};
            upscalerGetGPUMemoryUsage.gpuMemoryUsageUpscaler = &gpuMemoryUsageUpscaler;

            ffx::Query(m_UpscalingContext, upscalerGetGPUMemoryUsage);

            CAUDRON_LOG_INFO(L"Upscaler Context VRAM totalUsageInBytes %f MB aliasableUsageInBytes %f MB", gpuMemoryUsageUpscaler.totalUsageInBytes / 1048576.f, gpuMemoryUsageUpscaler.aliasableUsageInBytes / 1048576.f);

        }

        // Create the FrameGen context
        if (m_FrameInterpolationAvailable)
        {
            ffx::CreateContextDescFrameGeneration createFg{};
            createFg.displaySize = { resInfo.DisplayWidth, resInfo.DisplayHeight };
            createFg.maxRenderSize = { resInfo.DisplayWidth, resInfo.DisplayHeight };
            if (s_InvertedDepth)
                createFg.flags |= FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED | FFX_FRAMEGENERATION_ENABLE_DEPTH_INFINITE;
            createFg.flags |= FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE;

            m_EnableAsyncCompute = m_PendingEnableAsyncCompute;
            if (m_EnableAsyncCompute)
            {
                createFg.flags |= FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT;
            }

            createFg.backBufferFormat = SDKWrapper::GetFfxSurfaceFormat(GetFramework()->GetSwapChain()->GetSwapChainFormat());
            ffx::ReturnCode retCode;
            if (s_uiRenderMode == 3)
            {
                ffx::CreateContextDescFrameGenerationHudless createFgHudless{};
                createFgHudless.hudlessBackBufferFormat = SDKWrapper::GetFfxSurfaceFormat(m_pHudLessTexture[0]->GetResource()->GetTextureResource()->GetFormat());
                // create the context. We can reuse the backend description. TODO: this relies on an implementation detail we may not want to expose.
                retCode = ffx::CreateContext(m_FrameGenContext, nullptr, createFg, backendDesc, createFgHudless);
            }
            else
            {
                // create the context. We can reuse the backend description. TODO: this relies on an implementation detail we may not want to expose.
                retCode = ffx::CreateContext(m_FrameGenContext, nullptr, createFg, backendDesc);
            }
            
            CauldronAssert(ASSERT_CRITICAL, retCode == ffx::ReturnCode::Ok, L"Couldn't create the ffxapi framegen context: %d", (uint32_t)retCode);

            void* ffxSwapChain;
#if defined(FFX_API_DX12)
            ffxSwapChain = GetSwapChain()->GetImpl()->DX12SwapChain();
#elif defined(FFX_API_VK)
            ffxSwapChain = GetSwapChain()->GetImpl()->VKSwapChain();
#endif  // defined(FFX_API_DX12)

            // Configure frame generation
            FfxApiResource hudLessResource = SDKWrapper::ffxGetResourceApi(m_pHudLessTexture[m_curUiTextureIndex]->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);

            m_FrameGenerationConfig.frameGenerationEnabled  = false;
            m_FrameGenerationConfig.frameGenerationCallback = [](ffxDispatchDescFrameGeneration* params, void* pUserCtx) -> ffxReturnCode_t
            {
                return ffxDispatch(reinterpret_cast<ffxContext*>(pUserCtx), &params->header);
            };
            m_FrameGenerationConfig.frameGenerationCallbackUserContext = &m_FrameGenContext;
            if (s_uiRenderMode == 2)
            {
                m_FrameGenerationConfig.presentCallback = [](ffxCallbackDescFrameGenerationPresent* params, void* self) -> auto { return reinterpret_cast<FSRRenderModule*>(self)->UiCompositionCallback(params); };
                m_FrameGenerationConfig.presentCallbackUserContext = this;
            }
            else
            {
                m_FrameGenerationConfig.presentCallback = nullptr;
                m_FrameGenerationConfig.presentCallbackUserContext = nullptr;
            }
            m_FrameGenerationConfig.swapChain               = ffxSwapChain;
            m_FrameGenerationConfig.HUDLessColor            = (s_uiRenderMode == 3) ? hudLessResource : FfxApiResource({});

            m_FrameGenerationConfig.frameID = m_FrameID;

            retCode = ffx::Configure(m_FrameGenContext, m_FrameGenerationConfig);
            CauldronAssert(ASSERT_CRITICAL, retCode == ffx::ReturnCode::Ok, L"Couldn't create the ffxapi upscaling context: %d", (uint32_t)retCode);
            
            FfxApiEffectMemoryUsage gpuMemoryUsageFrameGeneration;
            ffx::QueryDescFrameGenerationGetGPUMemoryUsage frameGenGetGPUMemoryUsage{};
            frameGenGetGPUMemoryUsage.gpuMemoryUsageFrameGeneration = &gpuMemoryUsageFrameGeneration;
            ffx::Query(m_FrameGenContext, frameGenGetGPUMemoryUsage);

            CAUDRON_LOG_INFO(L"FrameGeneration Context VRAM totalUsageInBytes %f MB aliasableUsageInBytes %f MB", gpuMemoryUsageFrameGeneration.totalUsageInBytes / 1048576.f, gpuMemoryUsageFrameGeneration.aliasableUsageInBytes / 1048576.f);


            FfxApiEffectMemoryUsage gpuMemoryUsageFrameGenerationSwapchain;
#if defined(FFX_API_DX12)
            ffx::QueryFrameGenerationSwapChainGetGPUMemoryUsageDX12 frameGenSwapchainGetGPUMemoryUsage{};
            frameGenSwapchainGetGPUMemoryUsage.gpuMemoryUsageFrameGenerationSwapchain = &gpuMemoryUsageFrameGenerationSwapchain;
            ffx::Query(m_SwapChainContext, frameGenSwapchainGetGPUMemoryUsage);
#elif defined(FFX_API_VK)
            ffx::QueryFrameGenerationSwapChainGetGPUMemoryUsageVK frameGenSwapchainGetGPUMemoryUsage{};
            frameGenSwapchainGetGPUMemoryUsage.gpuMemoryUsageFrameGenerationSwapchain = &gpuMemoryUsageFrameGenerationSwapchain;
            ffx::Query(m_SwapChainContext, frameGenSwapchainGetGPUMemoryUsage);
#endif  // defined(FFX_API_DX12)
            CAUDRON_LOG_INFO(L"Swapchain Context VRAM totalUsageInBytes %f MB aliasableUsageInBytes %f MB", gpuMemoryUsageFrameGenerationSwapchain.totalUsageInBytes / 1048576.f, gpuMemoryUsageFrameGenerationSwapchain.aliasableUsageInBytes / 1048576.f);
        }
    }
    else if (m_FrameInterpolationAvailable)
    {
        void* ffxSwapChain;
#if defined(FFX_API_DX12)
        ffxSwapChain = GetSwapChain()->GetImpl()->DX12SwapChain();
#elif defined(FFX_API_VK)
        ffxSwapChain = GetSwapChain()->GetImpl()->VKSwapChain();
#endif  // defined(FFX_API_DX12)

        // disable frame generation before destroying context
        // also unset present callback, HUDLessColor and UiTexture to have the swapchain only present the backbuffer
        m_FrameGenerationConfig.frameGenerationEnabled = false;
        m_FrameGenerationConfig.swapChain              = ffxSwapChain;
        m_FrameGenerationConfig.presentCallback        = nullptr;
        m_FrameGenerationConfig.HUDLessColor           = FfxApiResource({});
        ffx::Configure(m_FrameGenContext, m_FrameGenerationConfig);

#if defined(FFX_API_DX12)
        ffx::ConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12 uiConfig{};
        uiConfig.uiResource = {};
        uiConfig.flags = 0;
        ffx::Configure(m_SwapChainContext, uiConfig);
#elif defined(FFX_API_VK)
        ffx::ConfigureDescFrameGenerationSwapChainRegisterUiResourceVK uiConfig{};
        uiConfig.uiResource = {};
        ffx::Configure(m_SwapChainContext, uiConfig);
#endif  // defined(FFX_API_DX12)

        // Destroy the contexts
        if (m_UpscalingContext)
        {
            ffx::DestroyContext(m_UpscalingContext);
            m_UpscalingContext = nullptr;
        }
        ffx::DestroyContext(m_FrameGenContext);
    }
}

void FSRRenderModule::SetUpscaleConstantBuffer(uint64_t key, float value)
{
    ffx::ConfigureDescUpscaleKeyValue m_upscalerKeyValueConfig{};
    m_upscalerKeyValueConfig.key = key;
    m_upscalerKeyValueConfig.ptr = &value;
    ffx::Configure(m_UpscalingContext, m_upscalerKeyValueConfig);
}

ResolutionInfo FSRRenderModule::UpdateResolution(uint32_t displayWidth, uint32_t displayHeight)
{
    return {static_cast<uint32_t>((float)displayWidth / m_UpscaleRatio * m_LetterboxRatio),
            static_cast<uint32_t>((float)displayHeight / m_UpscaleRatio * m_LetterboxRatio),
            static_cast<uint32_t>((float)displayWidth * m_LetterboxRatio),
            static_cast<uint32_t>((float)displayHeight * m_LetterboxRatio),
            displayWidth, displayHeight };
}

void FSRRenderModule::OnPreFrame()
{
    if (NeedsReInit())
    {
        GetDevice()->FlushAllCommandQueues();

        // Need to recreate the FSR context
        EnableModule(false);
        EnableModule(true);

        ClearReInit();
    }
}

void FSRRenderModule::OnResize(const ResolutionInfo& resInfo)
{
    if (!ModuleEnabled())
        return;

    // Need to recreate the FSR context on resource resize
    UpdateFSRContext(false);   // Destroy
    UpdateFSRContext(true);    // Re-create

    // Reset jitter index
    m_JitterIndex = 0;
}

void FSRRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    if (m_pHudLessTexture[m_curUiTextureIndex]->GetResource()->GetCurrentResourceState() != ResourceState::NonPixelShaderResource)
    {
        Barrier barrier = Barrier::Transition(m_pHudLessTexture[m_curUiTextureIndex]->GetResource(),
                                              m_pHudLessTexture[m_curUiTextureIndex]->GetResource()->GetCurrentResourceState(),
                                              ResourceState::NonPixelShaderResource);
        ResourceBarrier(pCmdList, 1, &barrier);
    }

    if (m_pUiTexture[m_curUiTextureIndex]->GetResource()->GetCurrentResourceState() != ResourceState::ShaderResource)
    {
        std::vector<Barrier> barriers = {Barrier::Transition(m_pUiTexture[m_curUiTextureIndex]->GetResource(),
                                                             m_pUiTexture[m_curUiTextureIndex]->GetResource()->GetCurrentResourceState(),
                                                             ResourceState::ShaderResource)};
        ResourceBarrier(pCmdList, (uint32_t)barriers.size(), barriers.data());
    }

    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX API FSR Upscaler");
    const ResolutionInfo&   resInfo = GetFramework()->GetResolutionInfo();
    CameraComponent*        pCamera = GetScene()->GetCurrentCamera();

    GPUResource* pSwapchainBackbuffer = GetFramework()->GetSwapChain()->GetBackBufferRT()->GetCurrentResource();
    FfxApiResource backbuffer            = SDKWrapper::ffxGetResourceApi(pSwapchainBackbuffer, FFX_API_RESOURCE_STATE_PRESENT);

    // copy input source to temp so that the input and output texture of the upscalers is different 
    {
        std::vector<Barrier> barriers;
        barriers.push_back(Barrier::Transition(
            m_pTempTexture->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::CopyDest));
        barriers.push_back(Barrier::Transition(
            m_pColorTarget->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::CopySource));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
    }

    {
        GPUScopedProfileCapture sampleMarker(pCmdList, L"CopyToTemp");

        TextureCopyDesc desc(m_pColorTarget->GetResource(), m_pTempTexture->GetResource());
        CopyTextureRegion(pCmdList, &desc);
    }

    {
        std::vector<Barrier> barriers;
        barriers.push_back(Barrier::Transition(
            m_pTempTexture->GetResource(), ResourceState::CopyDest, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
        barriers.push_back(Barrier::Transition(
            m_pColorTarget->GetResource(), ResourceState::CopySource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
        ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
    }

    // Note, inverted depth and display mode are currently handled statically for the run of the sample.
    // If they become changeable at runtime, we'll need to modify how this information is queried
    static bool s_InvertedDepth = GetConfig()->InvertedDepth;

    // Upscale the scene first
    if (m_UpscaleMethod == Upscaler_Native)
    {
        // Native, nothing to do
    }

    if (m_UpscaleMethod == Upscaler_FSRAPI)
    {
        // FFXAPI
        // All cauldron resources come into a render module in a generic read state (ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource)
        ffx::DispatchDescUpscale dispatchUpscale{};

#if defined(FFX_API_DX12)
        dispatchUpscale.commandList = pCmdList->GetImpl()->DX12CmdList();
#elif defined(FFX_API_VK)
        dispatchUpscale.commandList = pCmdList->GetImpl()->VKCmdBuffer();
#endif  // defined(FFX_API_DX12)
        dispatchUpscale.color         = SDKWrapper::ffxGetResourceApi(m_pTempTexture->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchUpscale.depth         = SDKWrapper::ffxGetResourceApi(m_pDepthTarget->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchUpscale.motionVectors = SDKWrapper::ffxGetResourceApi(m_pMotionVectors->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchUpscale.exposure      = SDKWrapper::ffxGetResourceApi(nullptr, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchUpscale.output        = SDKWrapper::ffxGetResourceApi(m_pColorTarget->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);

        if (m_MaskMode != FSRMaskMode::Disabled)
        {
            dispatchUpscale.reactive = SDKWrapper::ffxGetResourceApi(m_pReactiveMask->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        }
        else
        {
            dispatchUpscale.reactive = SDKWrapper::ffxGetResourceApi(nullptr, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        }

        if (m_UseMask)
        {
            dispatchUpscale.transparencyAndComposition =
                SDKWrapper::ffxGetResourceApi(m_pCompositionMask->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        }
        else
        {
            dispatchUpscale.transparencyAndComposition = SDKWrapper::ffxGetResourceApi(nullptr, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        }

        // Jitter is calculated earlier in the frame using a callback from the camera update
        dispatchUpscale.jitterOffset.x      = -m_JitterX;
        dispatchUpscale.jitterOffset.y      = -m_JitterY;
        dispatchUpscale.motionVectorScale.x = resInfo.fRenderWidth();
        dispatchUpscale.motionVectorScale.y = resInfo.fRenderHeight();
        dispatchUpscale.reset               = m_ResetUpscale || GetScene()->GetCurrentCamera()->WasCameraReset();
        dispatchUpscale.enableSharpening    = m_RCASSharpen;
        dispatchUpscale.sharpness           = m_Sharpness;

        // Cauldron keeps time in seconds, but FSR expects milliseconds
        dispatchUpscale.frameTimeDelta = static_cast<float>(deltaTime * 1000.f);

        dispatchUpscale.preExposure        = GetScene()->GetSceneExposure();
        dispatchUpscale.renderSize.width   = resInfo.RenderWidth;
        dispatchUpscale.renderSize.height  = resInfo.RenderHeight;
        dispatchUpscale.upscaleSize.width  = resInfo.UpscaleWidth;
        dispatchUpscale.upscaleSize.height = resInfo.UpscaleHeight;

        // Setup camera params as required
        dispatchUpscale.cameraFovAngleVertical = pCamera->GetFovY();

        if (s_InvertedDepth)
        {
            dispatchUpscale.cameraFar  = pCamera->GetNearPlane();
            dispatchUpscale.cameraNear = FLT_MAX;
        }
        else
        {
            dispatchUpscale.cameraFar  = pCamera->GetFarPlane();
            dispatchUpscale.cameraNear = pCamera->GetNearPlane();
        }

        dispatchUpscale.flags = 0;
        dispatchUpscale.flags |= m_DrawUpscalerDebugView ? FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW : 0;

        ffx::ReturnCode retCode = ffx::Dispatch(m_UpscalingContext, dispatchUpscale);
        CauldronAssert(ASSERT_CRITICAL, !!retCode, L"Dispatching FSR upscaling failed: %d", (uint32_t)retCode);
    }

    if (m_FrameInterpolationAvailable)
    {
        ffx::DispatchDescFrameGenerationPrepare dispatchFgPrep{};

#if defined(FFX_API_DX12)
        dispatchFgPrep.commandList = pCmdList->GetImpl()->DX12CmdList();
#elif defined(FFX_API_VK)
        dispatchFgPrep.commandList = pCmdList->GetImpl()->VKCmdBuffer();
#endif  // defined(FFX_API_DX12)
        dispatchFgPrep.depth         = SDKWrapper::ffxGetResourceApi(m_pDepthTarget->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchFgPrep.motionVectors = SDKWrapper::ffxGetResourceApi(m_pMotionVectors->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchFgPrep.flags         = 0;

        dispatchFgPrep.jitterOffset.x      = -m_JitterX;
        dispatchFgPrep.jitterOffset.y      = -m_JitterY;
        dispatchFgPrep.motionVectorScale.x = resInfo.fRenderWidth();
        dispatchFgPrep.motionVectorScale.y = resInfo.fRenderHeight();

        // Cauldron keeps time in seconds, but FSR expects milliseconds
        dispatchFgPrep.frameTimeDelta = static_cast<float>(deltaTime * 1000.f);

        dispatchFgPrep.renderSize.width       = resInfo.RenderWidth;
        dispatchFgPrep.renderSize.height      = resInfo.RenderHeight;
        dispatchFgPrep.cameraFovAngleVertical = pCamera->GetFovY();

        if (s_InvertedDepth)
        {
            dispatchFgPrep.cameraFar  = pCamera->GetNearPlane();
            dispatchFgPrep.cameraNear = FLT_MAX;
        }
        else
        {
            dispatchFgPrep.cameraFar  = pCamera->GetFarPlane();
            dispatchFgPrep.cameraNear = pCamera->GetNearPlane();
        }
        dispatchFgPrep.viewSpaceToMetersFactor = 0.f;
        dispatchFgPrep.frameID                 = m_FrameID;

        // Update frame generation config
        FfxApiResource hudLessResource =
            SDKWrapper::ffxGetResourceApi(m_pHudLessTexture[m_curUiTextureIndex]->GetResource(), FFX_API_RESOURCE_STATE_COMPUTE_READ);

        m_FrameGenerationConfig.frameGenerationEnabled = m_FrameInterpolation;
        m_FrameGenerationConfig.flags                  = 0;
        m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugTearLines ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES : 0;
        m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugResetIndicators ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS : 0;
        m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugPacingLines ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_PACING_LINES : 0;
        m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugView ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW : 0;
        dispatchFgPrep.flags                        = m_FrameGenerationConfig.flags;  // TODO: maybe these should be distinct flags?
        m_FrameGenerationConfig.HUDLessColor        = (s_uiRenderMode == 3) ? hudLessResource : FfxApiResource({});
        m_FrameGenerationConfig.allowAsyncWorkloads = m_AllowAsyncCompute && m_EnableAsyncCompute;
        // assume symmetric letterbox
        m_FrameGenerationConfig.generationRect.left   = (resInfo.DisplayWidth - resInfo.UpscaleWidth) / 2;
        m_FrameGenerationConfig.generationRect.top    = (resInfo.DisplayHeight - resInfo.UpscaleHeight) / 2;
        m_FrameGenerationConfig.generationRect.width  = resInfo.UpscaleWidth;
        m_FrameGenerationConfig.generationRect.height = resInfo.UpscaleHeight;
        if (m_UseCallback)
        {
            m_FrameGenerationConfig.frameGenerationCallback = [](ffxDispatchDescFrameGeneration* params, void* pUserCtx) -> ffxReturnCode_t {
                return ffxDispatch(reinterpret_cast<ffxContext*>(pUserCtx), &params->header);
            };
            m_FrameGenerationConfig.frameGenerationCallbackUserContext = &m_FrameGenContext;
        }
        else
        {
            m_FrameGenerationConfig.frameGenerationCallback            = nullptr;
            m_FrameGenerationConfig.frameGenerationCallbackUserContext = nullptr;
        }
        m_FrameGenerationConfig.onlyPresentGenerated = m_PresentInterpolatedOnly;
        m_FrameGenerationConfig.frameID              = m_FrameID;

        void* ffxSwapChain;
#if defined(FFX_API_DX12)
        ffxSwapChain = GetSwapChain()->GetImpl()->DX12SwapChain();
#elif defined(FFX_API_VK)
        ffxSwapChain = GetSwapChain()->GetImpl()->VKSwapChain();
#endif  // defined(FFX_API_DX12)
        m_FrameGenerationConfig.swapChain = ffxSwapChain;
        ffx::ReturnCode retCode           = ffx::ReturnCode::ErrorParameter;
        if (m_UseDistortionField)
        {
            ffx::ConfigureDescFrameGenerationRegisterDistortionFieldResource dfConfig{};
            dfConfig.distortionField =
                SDKWrapper::ffxGetResourceApi(m_pDistortionField[m_curUiTextureIndex]->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
            retCode = ffx::Configure(m_FrameGenContext, m_FrameGenerationConfig, dfConfig);
        }
        else
        {
            retCode = ffx::Configure(m_FrameGenContext, m_FrameGenerationConfig);
        }

        CauldronAssert(ASSERT_CRITICAL, !!retCode, L"Configuring FSR FG failed: %d", (uint32_t)retCode);

        retCode = ffx::Dispatch(m_FrameGenContext, dispatchFgPrep);
        CauldronAssert(ASSERT_CRITICAL, !!retCode, L"Dispatching FSR FG (upscaling data) failed: %d", (uint32_t)retCode);

        FfxApiResource uiColor =
            (s_uiRenderMode == 1) ? SDKWrapper::ffxGetResourceApi(m_pUiTexture[m_curUiTextureIndex]->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ)
                                  : FfxApiResource({});

#if defined(FFX_API_DX12)
        ffx::ConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12 uiConfig{};
        uiConfig.uiResource = uiColor;
        uiConfig.flags      = m_DoublebufferInSwapchain ? FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING : 0;
        ffx::Configure(m_SwapChainContext, uiConfig);
#elif defined(FFX_API_VK)
        ffx::ConfigureDescFrameGenerationSwapChainRegisterUiResourceVK uiConfig{};
        uiConfig.uiResource = uiColor;
        uiConfig.flags      = m_DoublebufferInSwapchain ? FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING : 0;
        ffx::Configure(m_SwapChainContext, uiConfig);
#endif  // defined(FFX_API_DX12)
    }

    // Dispatch frame generation, if not using the callback
    if (m_FrameInterpolation && !m_UseCallback)
    {
        ffx::DispatchDescFrameGeneration dispatchFg{};

        dispatchFg.presentColor = backbuffer;
        dispatchFg.numGeneratedFrames = 1;

        // assume symmetric letterbox
        dispatchFg.generationRect.left = (resInfo.DisplayWidth - resInfo.UpscaleWidth) / 2;
        dispatchFg.generationRect.top = (resInfo.DisplayHeight - resInfo.UpscaleHeight) / 2;
        dispatchFg.generationRect.width = resInfo.UpscaleWidth;
        dispatchFg.generationRect.height = resInfo.UpscaleHeight;

#if defined(FFX_API_DX12)
        ffx::QueryDescFrameGenerationSwapChainInterpolationCommandListDX12 queryCmdList{};
        queryCmdList.pOutCommandList = &dispatchFg.commandList;
        ffx::Query(m_SwapChainContext, queryCmdList);

        ffx::QueryDescFrameGenerationSwapChainInterpolationTextureDX12 queryFiTexture{};
        queryFiTexture.pOutTexture = &dispatchFg.outputs[0];
        ffx::Query(m_SwapChainContext, queryFiTexture);
#elif defined(FFX_API_VK)
        ffx::QueryDescFrameGenerationSwapChainInterpolationCommandListVK queryCmdList{};
        queryCmdList.pOutCommandList = &dispatchFg.commandList;
        ffx::Query(m_SwapChainContext, queryCmdList);

        ffx::QueryDescFrameGenerationSwapChainInterpolationTextureVK queryFiTexture{};
        queryFiTexture.pOutTexture = &dispatchFg.outputs[0];
        ffx::Query(m_SwapChainContext, queryFiTexture);
#endif  // defined(FFX_API_DX12)

        dispatchFg.frameID = m_FrameID;
        dispatchFg.reset = m_ResetFrameInterpolation;

        ffx::ReturnCode retCode = ffx::Dispatch(m_FrameGenContext, dispatchFg);
        
        CauldronAssert(ASSERT_CRITICAL, !!retCode, L"Dispatching Frame Generation failed: %d", (uint32_t)retCode);
    }

    m_FrameID += uint64_t(1 + m_SimulatePresentSkip);
    m_SimulatePresentSkip = false;

    m_ResetUpscale = false;
    m_ResetFrameInterpolation = false;

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);

    // We are now done with upscaling
    GetFramework()->SetUpscalingState(UpscalerState::PostUpscale);
}

void FSRRenderModule::PreTransCallback(double deltaTime, CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"Pre-Trans (FSR)");

    std::vector<Barrier> barriers;
    barriers.push_back(Barrier::Transition(m_pReactiveMask->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::RenderTargetResource));
    barriers.push_back(Barrier::Transition(m_pCompositionMask->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::RenderTargetResource));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    // We need to clear the reactive and composition masks before any translucencies are rendered into them
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ClearRenderTarget(pCmdList, &m_RasterViews[0]->GetResourceView(), clearColor);
    ClearRenderTarget(pCmdList, &m_RasterViews[1]->GetResourceView(), clearColor);

    barriers.clear();
    barriers.push_back(Barrier::Transition(m_pReactiveMask->GetResource(), ResourceState::RenderTargetResource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(m_pCompositionMask->GetResource(), ResourceState::RenderTargetResource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    // update index for UI doublebuffering
    UIRenderModule* uimod = static_cast<UIRenderModule*>(GetFramework()->GetRenderModule("UIRenderModule"));
    m_curUiTextureIndex = (++m_curUiTextureIndex) & 1;
    uimod->SetUiSurfaceIndex(m_curUiTextureIndex);

    //update index for distortion texture doublebuffering
    m_pToneMappingRenderModule->SetDoubleBufferedTextureIndex(m_curUiTextureIndex);

    if (m_MaskMode != FSRMaskMode::Auto)
        return;

    barriers.clear();
    barriers.push_back(Barrier::Transition(m_pColorTarget->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::CopySource));
    barriers.push_back(Barrier::Transition(m_pOpaqueTexture->GetResource(), ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, ResourceState::CopyDest));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());

    // Copy the color render target before we apply translucency
    TextureCopyDesc copyColor = TextureCopyDesc(m_pColorTarget->GetResource(), m_pOpaqueTexture->GetResource());
    CopyTextureRegion(pCmdList, &copyColor);

    barriers.clear();
    barriers.push_back(Barrier::Transition(m_pColorTarget->GetResource(), ResourceState::CopySource, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    barriers.push_back(Barrier::Transition(m_pOpaqueTexture->GetResource(), ResourceState::CopyDest, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource));
    ResourceBarrier(pCmdList, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void FSRRenderModule::PostTransCallback(double deltaTime, CommandList* pCmdList)
{
    if ((m_MaskMode != FSRMaskMode::Auto) || (m_UpscaleMethod!= 1))
        return;

    GPUScopedProfileCapture sampleMarker(pCmdList, L"Gen Reactive Mask (FSR API)");

    ffx::DispatchDescUpscaleGenerateReactiveMask dispatchDesc{};
#if defined(FFX_API_DX12)
    dispatchDesc.commandList   = pCmdList->GetImpl()->DX12CmdList();
#elif defined(FFX_API_VK)
    dispatchDesc.commandList   = pCmdList->GetImpl()->VKCmdBuffer();
#endif  // defined(FFX_API_DX12)
    dispatchDesc.colorOpaqueOnly = SDKWrapper::ffxGetResourceApi(m_pOpaqueTexture->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchDesc.colorPreUpscale = SDKWrapper::ffxGetResourceApi(m_pColorTarget->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchDesc.outReactive = SDKWrapper::ffxGetResourceApi(m_pReactiveMask->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    dispatchDesc.renderSize.width = resInfo.RenderWidth;
    dispatchDesc.renderSize.height = resInfo.RenderHeight;

    // The following are all hard-coded in the original FSR2 sample. Should these be exposed?
    dispatchDesc.scale = 1.f;
    dispatchDesc.cutoffThreshold = 0.2f;
    dispatchDesc.binaryValue = 0.9f;
    dispatchDesc.flags = FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_TONEMAP |
                         FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_THRESHOLD |
                         FFX_UPSCALE_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX;

    ffx::ReturnCode retCode = ffx::Dispatch(m_UpscalingContext, dispatchDesc);
    CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"ffxDispatch(FSR_GENERATEREACTIVEMASK) failed with %d", (uint32_t)retCode);

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);
}

// Copy of ffxRestoreApplicationSwapChain from backend_interface, which is not built for this sample.
#if defined(FFX_API_DX12)
void RestoreApplicationSwapChain(bool recreateSwapchain)
{
    cauldron::SwapChain* pSwapchain  = cauldron::GetSwapChain();
    IDXGISwapChain4*     pSwapChain4 = pSwapchain->GetImpl()->DX12SwapChain();
    pSwapChain4->AddRef();

    IDXGIFactory7*      factory   = nullptr;
    ID3D12CommandQueue* pCmdQueue = cauldron::GetDevice()->GetImpl()->DX12CmdQueue(cauldron::CommandQueue::Graphics);

    // Setup a new swapchain for HWND and set it to cauldron
    IDXGISwapChain1* pSwapChain1 = nullptr;
    if (SUCCEEDED(pSwapChain4->GetParent(IID_PPV_ARGS(&factory))))
    {
        cauldron::GetSwapChain()->GetImpl()->SetDXGISwapChain(nullptr);

        // safe data since release will destroy the swapchain (and we need it distroyed before we can create the new one)
        HWND windowHandle = pSwapchain->GetImpl()->DX12SwapChainDesc().OutputWindow;
        DXGI_SWAP_CHAIN_DESC1 desc1 = pSwapchain->GetImpl()->DX12SwapChainDesc1();
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC  fsDesc = pSwapchain->GetImpl()->DX12SwapChainFullScreenDesc();

        pSwapChain4->Release();

        // check if window is still valid or if app is shutting down bc window was closed
        if (recreateSwapchain && IsWindow(windowHandle))
        {
            if (SUCCEEDED(factory->CreateSwapChainForHwnd(pCmdQueue, windowHandle, &desc1, &fsDesc, nullptr, &pSwapChain1)))
            {
                if (SUCCEEDED(pSwapChain1->QueryInterface(IID_PPV_ARGS(&pSwapChain4))))
                {
                    cauldron::GetSwapChain()->GetImpl()->SetDXGISwapChain(pSwapChain4);
                    pSwapChain4->Release();
                }
                pSwapChain1->Release();
            }
            factory->MakeWindowAssociation(cauldron::GetFramework()->GetImpl()->GetHWND(), DXGI_MWA_NO_WINDOW_CHANGES);
        }

        factory->Release();
    }
    return;
}

#elif defined(FFX_API_VK)
void RestoreApplicationSwapChain(bool recreateSwapchain)
{
    const VkSwapchainCreateInfoKHR* pCreateInfo = cauldron::GetSwapChain()->GetImpl()->GetCreateInfo();
    VkSwapchainKHR                  swapchain   = cauldron::GetSwapChain()->GetImpl()->VKSwapChain();
    cauldron::GetSwapChain()->GetImpl()->SetVKSwapChain(VK_NULL_HANDLE);
    cauldron::GetDevice()->GetImpl()->DestroySwapchainKHR(swapchain, nullptr);
    cauldron::GetDevice()->GetImpl()->SetSwapchainMethodsAndContext();  // reset all
    swapchain = VK_NULL_HANDLE;
    if (recreateSwapchain)
    {
        VkResult res = cauldron::GetDevice()->GetImpl()->CreateSwapchainKHR(pCreateInfo, nullptr, &swapchain);
        if (res == VK_SUCCESS)
        {
            // Swapchain creation can fail when this function is called when closing the application. In that case, just exit silently
            cauldron::GetSwapChain()->GetImpl()->SetVKSwapChain(swapchain);
        }
    }
}
#endif
