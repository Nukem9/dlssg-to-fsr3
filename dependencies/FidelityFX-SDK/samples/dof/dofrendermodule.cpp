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

#include "dofrendermodule.h"

#include "core/backend_interface.h"
#include "core/components/cameracomponent.h"
#include "core/framework.h"
#include "core/uimanager.h"
#include "core/scene.h"
#include "render/device.h"
#include "render/dynamicresourcepool.h"
#include "render/parameterset.h"
#include "render/pipelineobject.h"
#include "render/profiler.h"
#include "render/renderdefines.h"

#include <limits>

using namespace cauldron;

void DoFRenderModule::Init(const json&)
{
    // Fetch needed resource
    m_pColorTarget = GetFramework()->GetColorTargetForCallback(GetName());
    m_pDepthTarget = GetFramework()->GetRenderTexture(L"DepthTarget");

    // UI elements
    m_UISection = GetUIManager()->RegisterUIElements("Depth of Field", UISectionType::Sample);

    m_UISection->RegisterUIElement<UISlider<float>>("Aperture", m_Aperture, 0.0f, 0.1f);
    m_UISection->RegisterUIElement<UISlider<float>>("Sensor Size", m_SensorSize, 0.0f, 0.1f);
    m_UIFocusDist = m_UISection->RegisterUIElement<UISlider<float>>("Focus Distance", m_FocusDist, 0.01f, 120.0f);
    std::function<void(bool, bool)> callback = [this](bool cur, bool old) {
        UpdateDofContext(false);
        UpdateDofContext(true);
    };
    std::function<void(int32_t, int32_t)> callbackInt = [this, callback](int32_t, int32_t) { callback(true, true); };
    std::function<void(float, float)> callbackFloat = [this, callback](float, float) { callback(true, true); };
    m_UISection->RegisterUIElement<UISlider<int32_t>>("Quality", m_Quality, 1, 50, callbackInt);
    m_UISection->RegisterUIElement<UISlider<float>>("Blur Size Limit", m_CocLimit, 0.0f, 1.0f, callbackFloat);
    m_UISection->RegisterUIElement<UICheckBox>("Enable Kernel Ring Merging", m_EnableRingMerge, callback);

    // Init effect
    SetupFidelityFxInterface();

    // Create the context
    UpdateDofContext(true);

    GetFramework()->ConfigureRuntimeShaderRecompiler(
        [this](void) { DestroyDofContext(); },
        [this](void) {
            SetupFidelityFxInterface();
            UpdateDofContext(true);
        });

    SetModuleReady(true);
    SetModuleEnabled(true);
}

void DoFRenderModule::SetupFidelityFxInterface()
{
    if (m_InitializationParameters.backendInterface.scratchBuffer != nullptr)
        free(m_InitializationParameters.backendInterface.scratchBuffer);

    const size_t scratchBufferSize = SDKWrapper::ffxGetScratchMemorySize(FFX_DOF_CONTEXT_COUNT);
    void*        scratchBuffer     = calloc(scratchBufferSize, 1u);
    FfxErrorCode errorCode =
        SDKWrapper::ffxGetInterface(&m_InitializationParameters.backendInterface, GetDevice(), scratchBuffer, scratchBufferSize, FFX_DOF_CONTEXT_COUNT);
    CAULDRON_ASSERT(errorCode == FFX_OK);
    CauldronAssert(ASSERT_CRITICAL, m_InitializationParameters.backendInterface.fpGetSDKVersion(&m_InitializationParameters.backendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 2),
        L"FidelityFX DOF 1.1 sample requires linking with a 1.1.2 version SDK backend");
    CauldronAssert(ASSERT_CRITICAL, ffxDofGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 1, 0),
                       L"FidelityFX DOF 1.1 sample requires linking with a 1.1 version FidelityFX DOF library");
                       
    m_InitializationParameters.backendInterface.fpRegisterConstantBufferAllocator(&m_InitializationParameters.backendInterface, SDKWrapper::ffxAllocateConstantBuffer);
}

DoFRenderModule::~DoFRenderModule()
{
    DestroyDofContext();
    // Destroy the FidelityFX interface memory
    free(m_InitializationParameters.backendInterface.scratchBuffer);
}

void DoFRenderModule::DestroyDofContext()
{
    // Destroy the context
    UpdateDofContext(false);
}

void DoFRenderModule::UpdateDofContext(bool enabled)
{
    if (enabled)
    {
        // Setup parameters
        m_InitializationParameters.flags = 0;
        // We output to the same texture as the input.
        m_InitializationParameters.flags |= FFX_DOF_OUTPUT_PRE_INIT;
        if (!m_EnableRingMerge)
            m_InitializationParameters.flags |= FFX_DOF_DISABLE_RING_MERGE;
        if (GetConfig()->InvertedDepth)
            m_InitializationParameters.flags |= FFX_DOF_REVERSE_DEPTH;
        m_InitializationParameters.resolution.width  = GetFramework()->GetResolutionInfo().RenderWidth;
        m_InitializationParameters.resolution.height = GetFramework()->GetResolutionInfo().RenderHeight;
        m_InitializationParameters.quality           = static_cast<uint32_t>(m_Quality);
        m_InitializationParameters.cocLimitFactor    = m_CocLimit;

        ffxDofContextCreate(&m_DofContext, &m_InitializationParameters);
    }
    else
    {
        // Flush anything out of the pipes before destroying the context
        GetDevice()->FlushAllCommandQueues();

        ffxDofContextDestroy(&m_DofContext);
    }
}

void DoFRenderModule::OnResize(const ResolutionInfo& resInfo)
{
    if (!ModuleEnabled())
        return;

    // Need to recreate the context on resource resize
    UpdateDofContext(false);  // Destroy
    UpdateDofContext(true);   // Re-create
}

void DoFRenderModule::UpdateUI(double deltaTime)
{
    // update UI (min/max focus depth)
    BoundingBox bb = GetScene()->GetBoundingBox();
    if (!bb.IsEmpty())
    {
        CameraComponent* pCamera = GetScene()->GetCurrentCamera();
        Vec3 eyePos  = pCamera->GetCameraPos();
        Vec3 viewDir = normalize(-pCamera->GetInverseView().getCol2().getXYZ());
        Vec3 p[2]    = {bb.GetMin().getXYZ() - eyePos, bb.GetMax().getXYZ() - eyePos};
        // Min is 0.1, even if distances are negative (when the entire scene is behind the camera)
        float maxViewZ = 0.1f;
        for (size_t ix = 0; ix < 2; ix++)
            for (size_t iy = 0; iy < 2; iy++)
                for (size_t iz = 0; iz < 2; iz++)
                    maxViewZ = std::max(maxViewZ, (float)dot(viewDir, Vec3(p[ix].getX(), p[iy].getY(), p[iz].getZ())));

        // add 20% leeway to allow having the whole scene out of focus.
        maxViewZ *= 1.2f;
        // Prevent the slider from snapping around
        maxViewZ = std::max(maxViewZ, m_FocusDist);

        m_UIFocusDist->SetMax(maxViewZ);
    }
}

void DoFRenderModule::Execute(double deltaTime, CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FFX DOF");
    CameraComponent*        pCamera = GetScene()->GetCurrentCamera();
    const Mat4&             proj    = pCamera->GetProjection();

    FfxDofDispatchDescription dispatchParams{};
    dispatchParams.commandList = SDKWrapper::ffxGetCommandList(pCmdList);
    float conversion           = 0.5f * (float)m_InitializationParameters.resolution.width / m_SensorSize;
    float focalLength          = m_SensorSize / (2.f * std::tanf(pCamera->GetFovX() * 0.5f));

    // UI focus value is positive, but in the coordinate system, negative z is visible. So negate focus distance for calculation.
    // Matrix element indexes are column, row and zero-based. So proj34 parameter becomes getElem(3,2).
    dispatchParams.cocScale =
        ffxDofCalculateCocScale(m_Aperture, -m_FocusDist, focalLength, conversion, proj.getElem(2, 2), proj.getElem(3, 2), proj.getElem(2, 3));
    dispatchParams.cocBias =
        ffxDofCalculateCocBias(m_Aperture, -m_FocusDist, focalLength, conversion, proj.getElem(2, 2), proj.getElem(3, 2), proj.getElem(2, 3));
    dispatchParams.color  = SDKWrapper::ffxGetResource(m_pColorTarget->GetResource(), L"DoF_InputColor", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParams.depth  = SDKWrapper::ffxGetResource(m_pDepthTarget->GetResource(), L"DoF_InputDepth", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchParams.output = SDKWrapper::ffxGetResource(m_pColorTarget->GetResource(), L"DoF_Output", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    CAULDRON_ASSERT(FFX_OK == ffxDofContextDispatch(&m_DofContext, &dispatchParams));

    // FidelityFX contexts modify the set resource view heaps, so set the cauldron one back
    SetAllResourceViewHeaps(pCmdList);
}
