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

#include "core/uimanager.h"
#include "render/rendermodule.h"

#include <functional>
#include <string>
#include <vector>

/**
* @class RuntimeShaderRecompilerRenderModule
*
* The runtime shader recompiler render module uses the backend_shader_reloader library
* to trigger rebuilds of backend shader code at runtime.
*
* @ingroup CauldronRender
*/
class RuntimeShaderRecompilerRenderModule : public cauldron::RenderModule
{
public:
    /**
    * @brief   Construction.
    */
    RuntimeShaderRecompilerRenderModule();

    /**
    * @brief   Destruction.
    */
    virtual ~RuntimeShaderRecompilerRenderModule();

    /**
    * @brief   Initialization function.
    */
    void Init(const json& initData) override;

    /**
    * @brief   This render module does not need to Execute.
    */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override {}

    /**
    * @brief   Adds the specified pre and post reload callbacks to the set of callbacks called
    *          before a rebuild happens and after a rebuild has finished.
    *          The preReloadCallback should destroy the FFX interface and any currently active
    *          FFX Components' contexts.
    *          The postReloadCallback should re-initialize the FFX interface and re-create
    *          all previously active Components' contexts.
    */
    void AddReloadCallbacks(
        std::function<void(void)> preReloadCallback,
        std::function<void(void)> postReloadCallback);

    /**
    * @brief   AddReloadCallbacks must be called at least once for rebuilding to be enabled.
    */
    bool RebuildEnabled() const { return m_EnableRebuild; }

    virtual void OnPreFrame() override;

    void ScheduleRebuild()
    {
        m_RebuildClicked         = true;
        m_buildStatusDescription = "Building";
    }

    const char* GetBuildStatusDescription() const { return m_buildStatusDescription.c_str(); }

private:
    // No copy, No move
    NO_COPY(RuntimeShaderRecompilerRenderModule)
    NO_MOVE(RuntimeShaderRecompilerRenderModule)

    bool m_EnableRebuild = false;

    struct ReloaderCallbacks
    {
        /// Callback function called before the backend dll is unloaded.
        std::function<void(void)> preReloadCallback;
        /// Callback function called after the backend dll is unloaded.
        std::function<void(void)> postReloadCallback;
    };
    /// List of callbacks used for pre and post runtime shader recompilation.
    std::vector<ReloaderCallbacks> m_ReloaderCallbacks;

    std::string m_buildStatusDescription = "";
    bool m_RebuildClicked = false;
};
