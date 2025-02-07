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

#include "runtimeshaderrecompilerrendermodule.h"

#include "core/framework.h"
#include "render/device.h"

#include "render/backend_shader_reloader/backend_shader_reloader.h"

using namespace cauldron;

RuntimeShaderRecompilerRenderModule::RuntimeShaderRecompilerRenderModule()
    : RenderModule(L"RuntimeShaderRecompilerRenderModule")
{
    backend_shader_reloader::Init();
}

RuntimeShaderRecompilerRenderModule::~RuntimeShaderRecompilerRenderModule()
{
    // Can't call backend_shader_reloader::Shutdown() because it needs to happen after all other SDK dependent code is killed,
    // which there are some modules used by the Framework that get destroyed after this RenderModule, so calling it here causes
    // runtime exceptions for those modules.
    // But, it's not necessary to call backend_shader_reloader::Shutdown(), because it unsets the backend SDK function pointers
    // and unloads the backend DLL, which will happen automatically when the app is killed anyway, and if this destructor is being
    // called then we are trying to shutdown the application.
}

void RuntimeShaderRecompilerRenderModule::Init(const json& initData)
{
#if SUPPORT_RUNTIME_SHADER_RECOMPILE
    SetModuleReady(true);
#else // SUPPORT_RUNTIME_SHADER_RECOMPILE
    // SUPPORT_RUNTIME_SHADER_RECOMPILE is not defined then this module does nothing.
    SetModuleReady(false);
    SetModuleEnabled(false);
#endif
}

void RuntimeShaderRecompilerRenderModule::AddReloadCallbacks(
    std::function<void(void)> preReloadCallback,
    std::function<void(void)> postReloadCallback)
{
#if SUPPORT_RUNTIME_SHADER_RECOMPILE
    ReloaderCallbacks reloader = {
        preReloadCallback,
        postReloadCallback
    };

    m_ReloaderCallbacks.push_back(reloader);

    m_EnableRebuild = true;
#endif // SUPPORT_RUNTIME_SHADER_RECOMPILE
}

void RuntimeShaderRecompilerRenderModule::OnPreFrame()
{
    if (m_RebuildClicked)
    {
        GetFramework()->GetDevice()->FlushAllCommandQueues();

        m_RebuildClicked = false;

        // Call all preReloadCallbacks
        for (auto& reloader : m_ReloaderCallbacks)
        {
            reloader.preReloadCallback();
        }

        try
        {
            Log::Write(LOGLEVEL_TRACE, L"Rebuilding shaders...");

            backend_shader_reloader::RebuildShaders();

            m_buildStatusDescription = "Build Succeeded!";
            Log::Write(LOGLEVEL_TRACE, L"Shader rebuild completed successfully!");
        }
        catch (std::runtime_error e)
        {
            m_buildStatusDescription = "Build Failed (see log for errors).";

            Log::Write(LOGLEVEL_TRACE, L"Failed to rebuild shaders.");
            Log::Write(LOGLEVEL_TRACE, L"------------------------");
            std::string errors = e.what();
            Log::Write(LOGLEVEL_TRACE, StringToWString(errors).c_str());
            Log::Write(LOGLEVEL_TRACE, L"------------------------");
        }

        // Call all postReloadCallbacks
        for (auto& reloader : m_ReloaderCallbacks)
        {
            reloader.postReloadCallback();
        }
    }
}
