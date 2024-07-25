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

#include "cacaosample.h"
#include "misc/fileio.h"
#include "rendermoduleregistry.h"
#include <core/uimanager.h>
#include <lighting/lightingrendermodule.h>
#include <cacao/cacaorendermodule.h>
#include <render/rendermodules/tonemapping/tonemappingrendermodule.h>
#include "render/device.h"

using namespace cauldron;

// Read in sample-specific configuration parameters.
// Cauldron defaults may also be overridden at this point
void CacaoSample::ParseSampleConfig()
{
    json sampleConfig;
    CauldronAssert(ASSERT_CRITICAL, ParseJsonFile(L"configs/cacaoconfig.json", sampleConfig), L"Could not parse JSON file %ls", L"cacaoconfig.json");

    // Get the sample configuration
    json configData = sampleConfig["FidelityFX CACAO"];

    // Let the framework parse all the "known" options for us
    ParseConfigData(configData);
}

void CacaoSample::ParseSampleCmdLine(const wchar_t* cmdLine)
{
    // Process any command line parameters the sample looks for here
}

// Register sample's render modules so the factory can spawn them
void CacaoSample::RegisterSampleModules()
{
    // Init all pre-registered render modules
    rendermodule::RegisterAvailableRenderModules();

    RenderModuleFactory::RegisterModule<CACAORenderModule>("CACAORenderModule");
}

// Sample initialization point
void CacaoSample::DoSampleInit()
{
    m_pLightingRenderModule = static_cast<LightingRenderModule*>(GetFramework()->GetRenderModule("LightingRenderModule"));
    m_pCACAORenderModule    = static_cast<CACAORenderModule*>(GetFramework()->GetRenderModule("CACAORenderModule"));
    m_pToneMappingRenderModule = static_cast<ToneMappingRenderModule*>(GetFramework()->GetRenderModule("ToneMappingRenderModule"));
    m_UseCACAO                 = false;
    m_OutputCacaoDirectly      = false;
    m_UIUseCACAO               = true;
    m_UIOutputCacaoDirectly    = true;
    m_UIUseCACAOEnabler        = true;

    UISection* uiSection = GetUIManager()->RegisterUIElements("FFX CACAO", UISectionType::Sample);
    if (uiSection)
    {
        uiSection->RegisterUIElement<UICheckBox>("Output CACAO Directly", m_UIOutputCacaoDirectly);
        uiSection->RegisterUIElement<UICheckBox>("Use CACAO", m_UIUseCACAO, m_UIUseCACAOEnabler);
        m_pCACAORenderModule->InitUI(uiSection);
    }
}

// Do any app-specific (global) updates here
// This is called prior to components/render module updates
void CacaoSample::DoSampleUpdates(double deltaTime)
{
    const bool outputStateChanged = m_UIOutputCacaoDirectly != m_OutputCacaoDirectly;
    const bool useStateChanged    = m_UIUseCACAO != m_UseCACAO;
    const bool stateChanged       = outputStateChanged || useStateChanged;

    if (stateChanged)
        GetDevice()->FlushAllCommandQueues();

    if (outputStateChanged)
    {
        m_OutputCacaoDirectly = m_UIOutputCacaoDirectly;
        m_UIUseCACAO          = true;
        m_UIUseCACAOEnabler   = !m_UIOutputCacaoDirectly;
        m_pCACAORenderModule->SetOutputToCallbackTarget(!m_OutputCacaoDirectly);
        m_pLightingRenderModule->EnableModule(!m_OutputCacaoDirectly);
    }

    if (useStateChanged)
    {
        m_UseCACAO = m_UIUseCACAO;
        m_pCACAORenderModule->EnableModule(m_UseCACAO);
    }

}

// Handle any changes that need to occur due to applicate resize
// NOTE: Cauldron with auto-resize internal resources
void CacaoSample::DoSampleResize(const ResolutionInfo& resInfo)
{

}
