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

#if defined(_WINDOWS)
    
#include "core/win/framework_win.h"
#include "core/win/uibackend_win.h"

#include "core/loaders/textureloader.h"
#include "core/scene.h"
#include "core/uimanager.h"

#include "render/device.h"
#include "render/dynamicresourcepool.h"
#include "render/rendermodules/ui/uirendermodule.h"
#include "render/rendermodules/fpslimiter/fpslimiterrendermodule.h"
#include "render/profiler.h"
#include "render/swapchain.h"

#include "render/rendermodules/rsr/runtimeshaderrecompilerrendermodule.h"

// For windows DPI scaling fetching
#include <shellscalingapi.h>

// Needed to process windows input by ImGui
#include "imgui/backends/imgui_impl_win32.h"

// Needs to be declared outside of the cauldron namespace or it won't resolve properly
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace cauldron
{
    UIBackend* UIBackend::CreateUIBackend()
    {
        return new UIBackendInternal();
    }

    UIBackendInternal::UIBackendInternal() :
        UIBackend()
    {
        // Init ImGui basics
        m_pImGuiContext = ImGui::CreateContext();
        CauldronAssert(ASSERT_ERROR, m_pImGuiContext, L"Could not create ImGui context, UI will be disabled.");

        // Set ImGui style
        ImGui::StyleColorsDark();

        // Init windows/graphics back end for rendering with ImGui
        ImGuiIO& io = ImGui::GetIO();
        CauldronAssert(ASSERT_CRITICAL, io.ImeWindowHandle == 0 && io.BackendRendererUserData == nullptr && io.BackendPlatformUserData == nullptr, L"Already initialized a platform or rendering back end!");

        // On windows, we will use the win32 backend as it requires hijacking input and a few other things.
        // However, we will not use the rendering backend and elect to have a cauldron specific back end for rendering
        ImGui_ImplWin32_Init(GetFramework()->GetImpl()->GetHWND());

        // If in development mode, disabe the ini file (as it's really annoying when working on UI development)
        if (GetConfig()->DeveloperMode)
            io.IniFilename = nullptr;

        // Use a custom cauldron rendering back end
        io.BackendRendererName = "imgui_impl_cauldron";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

        // Load font to use (handled on a background thread)
        std::function<void(void*)> loadFont = [this](void*) { this->LoadUIFont(); };
        std::function<void(void*)> loadCompleteCallback = [this](void*) { this->UIFontLoadComplete(); };
        TaskCompletionCallback* pCompletionCallback = new TaskCompletionCallback(Task(loadCompleteCallback));
        Task fontLoadTask(loadFont, nullptr, pCompletionCallback);
        GetTaskManager()->AddTask(fontLoadTask);
    }

    UIBackendInternal::~UIBackendInternal()
    {
        // Shutdown render back end
        ImGuiIO& io = ImGui::GetIO();
        io.BackendRendererName = nullptr;
        io.BackendRendererUserData = nullptr;

        // Shut down windows back end
        ImGui_ImplWin32_Shutdown();

        // Destroy the ImGui context
        ImGui::DestroyContext(m_pImGuiContext);
    }

    void UIBackendInternal::PlatformUpdate(double deltaTime)
    {
        // Do windows-specific updates
        ImGui_ImplWin32_NewFrame();
    }

    bool UIBackendInternal::MessageHandler(const void* pMessage)
    {
        const MessagePacket* pMsgPacket = reinterpret_cast<const MessagePacket*>(pMessage);
        if (pMessage)
            return ImGui_ImplWin32_WndProcHandler(pMsgPacket->WndHandle, pMsgPacket->Msg, pMsgPacket->WParam, pMsgPacket->LParam);
        return false;
    }

    void UIBackendInternal::LoadUIFont()
    {
        ImGuiIO& io = ImGui::GetIO();

        // Fix up font size based on scale factor
        DEVICE_SCALE_FACTOR scaleFactor = GetScaleFactorForDevice(DEVICE_PRIMARY);
        float textScale = scaleFactor / 100.f;

        // Get default (embedded) font
        ImFontConfig font_cfg;
        font_cfg.SizePixels = GetConfig()->FontSize * textScale;
        io.Fonts->AddFontDefault(&font_cfg);

        // Fetch the font data and put it in a memory texture data block for copy to texture
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        MemTextureDataBlock* pDataBlock = new MemTextureDataBlock(reinterpret_cast<char*>(pixels));

        // Create the font texture
        TextureDesc fontDesc = TextureDesc::Tex2D(L"UIFontTexture", ResourceFormat::RGBA8_UNORM, width, height, 1, 1);
        m_pFontTexture = GetDynamicResourcePool()->CreateTexture(&fontDesc, ResourceState::CopyDest);
        CauldronAssert(ASSERT_ERROR, m_pFontTexture, L"Could not create the font texture for UI");

        if (m_pFontTexture)
        {
            // Explicitly cast away const during data copy
            const_cast<Texture*>(m_pFontTexture)->CopyData(pDataBlock);

            // Once done, auto-enqueue a barrier for start of next frame so it's usable
            Barrier textureTransition = Barrier::Transition(m_pFontTexture->GetResource(), ResourceState::CopyDest, ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource);
            GetDevice()->ExecuteResourceTransitionImmediate(1, &textureTransition);
        }

        delete pDataBlock;
    }

    void UIBackendInternal::UIFontLoadComplete()
    {
        // Make sure everything has been initialized before trying to fetch the ui render module
        while (!GetFramework()->IsRunning()) {}

        // Pass along the initialized font texture to the UI render module for parameter binding
        RenderModule*  pRenderModule = GetFramework()->GetRenderModule("UIRenderModule");


        CauldronAssert(ASSERT_CRITICAL, pRenderModule, L"Could not find UI render module to load font into");
        static_cast<UIRenderModule*>(pRenderModule)->SetFontResourceTexture(m_pFontTexture);

        // Initialization is now complete and UI back end is ready to be used
        m_BackendReady.store(true);
    }

    // Begins UI updates for frame
    void UIBackendInternal::BeginUIUpdates()
    {
        // Trigger start of a new frame
        ImGui::NewFrame();
    }

    // Ends UI updates for frame
    void UIBackendInternal::EndUIUpdates()
    {
        // Done with updates, call render now as any further processing will imply
        // ImGui data handling is done (including input processing)
        ImGui::Render();
    }

    // Helper to build filter buttons on the output UI
    void UIBackendInternal::OutputFilterButton(char* pString, uint32_t filterIndex, int32_t borderSize, bool sameLine/*=false*/)
    {
        if (sameLine)
            ImGui::SameLine();

        bool filterEnabled = m_FilterEnabled[filterIndex];
        if (filterEnabled)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, static_cast<float>(borderSize));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 1));
        }

        if (ImGui::SmallButton(pString))
            m_FilterEnabled[filterIndex] = !m_FilterEnabled[filterIndex];

        if (filterEnabled)
        {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
    }

    // Build out the tabbed window dialog which represent all registered UI elements
    void UIBackendInternal::BuildTabbedDialog(Vec2 uiscale)
    {
        // Setup tab frame work (internal elements provided by registered components (except for the scene tab)
        ImGui::SetNextWindowSize(ImVec2(s_UITabDialogWidth * uiscale.getX(), s_UITabDialogHeight * uiscale.getY()), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(s_UIDialogXSpacing * uiscale.getX(), s_UIDialogYSpacing * uiscale.getY()), ImGuiCond_FirstUseEver);
        ImGui::Begin("Main Interface (F1 to toggle)", nullptr, ImGuiWindowFlags_NoCollapse);
        {
            ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
            if (ImGui::BeginTabBar("CauldronTabs", tab_bar_flags))
            {
                if (ImGui::BeginTabItem("General"))
                {
                    // Build the general tab
                    BuildGeneralTab();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Scene"))
                {
                    // Build the scene representation UI
                    BuildSceneTab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Anti-Lag 2"))
                {
                    bool enabled = GetFramework()->GetDevice()->GetAntiLag2Enabled();

                    if (!GetFramework()->GetDevice()->GetAntiLag2FeatureSupported())
                        ImGui::BeginDisabled();

                    if (ImGui::Checkbox("Anti-Lag 2 Enabled", &enabled))
                    {
                        GetFramework()->GetDevice()->SetAntiLag2Enabled(enabled);
                    }

                    if (enabled)
                    {
                        static bool limiter      = false;
                        static int  limiterValue = 0;
                        ImGui::Checkbox("Framerate Limiter Enabled", &limiter);

                        if (!limiter)
                            ImGui::BeginDisabled();

                        ImGui::SliderInt("Max FPS", &limiterValue, 50, 300);

                        GetFramework()->GetDevice()->SetAntiLag2FramerateLimiter(limiter ? limiterValue : 0);

                        const FPSLimiterRenderModule* limiterModule = (const FPSLimiterRenderModule*)GetFramework()->GetRenderModule("FPSLimiterRenderModule");
                        if (limiterModule && limiterModule->IsFPSLimited())
                            ImGui::Text("(You need to disable the main framerate limiter first)");

                        if (!limiter)
                        {
                            ImGui::EndDisabled();
                            GetFramework()->GetDevice()->SetAntiLag2FramerateLimiter(0);
                        }
                    }

                    if (!GetFramework()->GetDevice()->GetAntiLag2FeatureSupported())
                        ImGui::EndDisabled();

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::End();
        }
    }

    // Builds the performance dialog
    void UIBackendInternal::BuildPerfDialog(Vec2 uiscale)
    {
        // Setup tab frame work (internal elements provided by registered components (except for the scene tab)
        const Framework* pFwrk = GetFramework();
        const Device* pDevice = GetDevice();

        // Gather stats for the frame and keep track in min/max
        constexpr size_t c_NumFrames = 128;
        static float s_CPUFrameTimes[c_NumFrames] = { 0 };
        static float s_GPUFrameTimes[c_NumFrames] = { 0 };

        // Gather stats for averages
        struct TimingTotals
        {
            std::chrono::nanoseconds TotalTime;
            std::wstring             Label;
        };
        static std::vector<TimingTotals>   s_CPUTotals = {};
        static std::vector<TimingTotals>   s_GPUTotals = {};
        static int64_t                    s_CPUFrameTotals = 0;
        static int64_t                    s_GPUFrameTotals = 0;
        static int64_t                    s_NumGatheredFrames = 0;

        // Track highest frame rate and determine the max value of the graph based on the measured highest value
        static float s_HighestRecentGPUTime = 0.0f;
        static float s_HighestRecentCPUTime = 0.0f;
        constexpr int32_t c_FrameTimeGraphMaxFPS[] = { 800, 240, 120, 90, 60, 45, 30, 15, 10, 5, 4, 3, 2, 1 };
        // All frame information is in nanoseconds, conversion needed to whatever scale used (default to milliseconds)
        static float  s_FrameTimeGraphMaxCPUValues[_countof(c_FrameTimeGraphMaxFPS)] = { 0 };
        static float  s_FrameTimeGraphMaxGPUValues[_countof(c_FrameTimeGraphMaxFPS)] = { 0 };
        for (int32_t i = 0; i < _countof(c_FrameTimeGraphMaxFPS); ++i) { s_FrameTimeGraphMaxCPUValues[i] = s_FrameTimeGraphMaxGPUValues[i] = static_cast<float>(g_NanosecondsPerSecond) / c_FrameTimeGraphMaxFPS[i]; }

        // Scrolling data and average FPS computing
        const std::vector<TimingInfo>& cpuTimings = GetProfiler()->GetCPUTimings();
        const std::vector<TimingInfo>& gpuTimings = GetProfiler()->GetGPUTimings();
        const int64_t CPUTickCount = GetProfiler()->GetCPUFrameTicks();
        const int64_t GPUTickCount = GetProfiler()->GetGPUFrameTicks();

        const bool cpuTimeStampsAvailable = cpuTimings.size() > 1;
        const bool gpuTimeStampsAvailable = gpuTimings.size() > 1;

        // Copy over previous frame times
        for (uint32_t i = 0; i < c_NumFrames - 1; ++i)
        {
            if (cpuTimeStampsAvailable)
                s_CPUFrameTimes[i] = s_CPUFrameTimes[i + 1];
            if (gpuTimeStampsAvailable)
                s_GPUFrameTimes[i] = s_GPUFrameTimes[i + 1];
        }

        // Track new frame times and update maximums
        if (cpuTimeStampsAvailable)
        {
            s_CPUFrameTimes[c_NumFrames - 1] = (float)CPUTickCount;
            s_HighestRecentCPUTime = std::max<float>(s_HighestRecentCPUTime, s_CPUFrameTimes[c_NumFrames - 1]);
        }

        if (gpuTimeStampsAvailable)
        {
            s_GPUFrameTimes[c_NumFrames - 1] = (float)GPUTickCount;
            s_HighestRecentGPUTime = std::max<float>(s_HighestRecentGPUTime, s_GPUFrameTimes[c_NumFrames - 1]);
        }

        // Update runtime averages
        if (!s_NumGatheredFrames)
        {
            // Initialize the results with what we have to date
            s_CPUTotals.resize(cpuTimings.size());
            s_GPUTotals.resize(gpuTimings.size());

            s_CPUFrameTotals = CPUTickCount;
            s_GPUFrameTotals = GPUTickCount;
            for (size_t i = 0; i < cpuTimings.size(); ++i)
            {
                s_CPUTotals[i].TotalTime = cpuTimings[i].EndTime - cpuTimings[i].StartTime;
                s_CPUTotals[i].Label = cpuTimings[i].Label;
            }
            for (size_t i = 0; i < gpuTimings.size(); ++i)
            {
                s_GPUTotals[i].TotalTime = gpuTimings[i].EndTime - gpuTimings[i].StartTime;
                s_GPUTotals[i].Label = gpuTimings[i].Label;
            }
            s_NumGatheredFrames = 1;
        }
        else
        {
            // Update totals, and reset if data layout has changed (which occurs when user plays with options that alters
            // the rendering flow of the sample)
            bool dataValid = s_CPUTotals.size() == cpuTimings.size();

            if (dataValid)
            {
                // Start with CPU
                s_CPUFrameTotals += CPUTickCount;
                for (size_t i = 0; i < cpuTimings.size(); ++i)
                {
                    if (s_CPUTotals[i].Label == cpuTimings[i].Label)
                    {
                        s_CPUTotals[i].TotalTime += cpuTimings[i].EndTime - cpuTimings[i].StartTime;
                    }
                    else
                    {
                        dataValid = false;
                        break;
                    }
                }
            }
            
            // If still good, do GPU
            dataValid = dataValid && (s_GPUTotals.size() == gpuTimings.size());
            if (dataValid)
            {
                s_GPUFrameTotals += GPUTickCount;
                for (size_t i = 0; i < gpuTimings.size(); ++i)
                {
                    if (s_GPUTotals[i].Label == gpuTimings[i].Label)
                    {
                        s_GPUTotals[i].TotalTime += gpuTimings[i].EndTime - gpuTimings[i].StartTime;
                    }
                    else
                    {
                        dataValid = false;
                        break;
                    }
                }
            }

            if (dataValid)
            {
                ++s_NumGatheredFrames;
            }
            else
            {
                s_NumGatheredFrames = 0;
                CauldronWarning(L"Resetting gathered performance results averages due to change in render flow from user interaction.");
            }
        }

        // Use slowest between gpu & cpu frame times as FPS tracker
        const float& frameTimeNanoSecondsCPU = s_CPUFrameTimes[c_NumFrames - 1];
        const float& frameTimeNanoSecondsGPU = s_GPUFrameTimes[c_NumFrames - 1];
        const float  frameTimeMicroSeconds = frameTimeNanoSecondsCPU * 0.001f;
        const float  frameTimeMilliSeconds = frameTimeMicroSeconds * 0.001f;
        const int32_t fpsCPU = cpuTimeStampsAvailable ? static_cast<int32_t>(g_NanosecondsPerSecond / frameTimeNanoSecondsCPU) : 0;
        const int32_t fpsGPU = gpuTimeStampsAvailable ? static_cast<int32_t>(g_NanosecondsPerSecond / frameTimeNanoSecondsGPU) : 0;
        const int32_t fps = std::min(fpsGPU, fpsCPU);
        static bool s_ShowMilliSeconds = true;
        static bool s_ShowGPUTimes = true;

        const ResolutionInfo& resInfo      = pFwrk->GetResolutionInfo();
        float outputHeight = static_cast<float>(resInfo.DisplayHeight - ((s_UIDialogYSpacing * 3 + s_UITabDialogHeight) * uiscale.getY()));
        ImGui::SetNextWindowSize(ImVec2(s_UIPerfDialogWidth * uiscale.getX(), outputHeight), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(s_UIDialogXSpacing * uiscale.getX(), (s_UIDialogYSpacing * 2 + s_UITabDialogHeight) * uiscale.getY()), ImGuiCond_FirstUseEver);
        ImGui::Begin("Performance (F2 to toggle)", nullptr, ImGuiWindowFlags_NoCollapse);
        {
            if (GetFramework()->UpscalerEnabled())
            {
                ImGui::Text("Render Res  : %ix%i", resInfo.RenderWidth, resInfo.RenderHeight);
                ImGui::Text("Upscale Res : %ix%i", resInfo.UpscaleWidth, resInfo.UpscaleHeight);
                ImGui::Text("Display Res : %ix%i", resInfo.DisplayWidth, resInfo.DisplayHeight);
            }
            else
                ImGui::Text("Resolution  : %ix%i", resInfo.DisplayWidth, resInfo.DisplayHeight);

            ImGui::Text("API         : %S", pDevice->GetGraphicsAPI());
            ImGui::Text("GPU         : %S", pDevice->GetDeviceName());
            ImGui::Text("Driver      : %S", pDevice->GetDriverVersion());
            ImGui::Text("CPU         : %S", pFwrk->GetCPUName());

            if (GetFramework()->FrameInterpolationEnabled())
            {
                static std::chrono::nanoseconds cpuTimestamp  = cpuTimings[0].EndTime;
                static uint64_t                 renderFrames  = GetFramework()->GetFrameID();
                static uint64_t                 displayFrames = 0;  // GetFramework()->GetSwapChain()->GetLastPresentCount();
                static uint64_t                 renderFPS     = 0;
                static uint64_t                 displayFPS    = 0;
                if ((cpuTimings[0].EndTime - cpuTimestamp).count() > 1000000000)
                {
                    // minus one to exclude the current frame from measurement,
                    // as we're on the next frame AFTER the 1 second we wanted to measure
                    renderFPS    = GetFramework()->GetFrameID() - renderFrames - 1;
                    renderFrames = GetFramework()->GetFrameID();

                    UINT lastPresentCount = 0;
                    GetFramework()->GetSwapChain()->GetLastPresentCount(&lastPresentCount);
                    displayFPS    = lastPresentCount - displayFrames - 1;
                    displayFrames = lastPresentCount;

                    cpuTimestamp = cpuTimings[0].EndTime;
                }

                double monitorRefreshRate = 0.0;
                GetFramework()->GetSwapChain()->GetRefreshRate(&monitorRefreshRate);

                ImGui::Text("Render FPS  : %d", renderFPS);
                ImGui::Text("Display FPS : %d", displayFPS);
                ImGui::Text("RefreshRate : %.1f", monitorRefreshRate);
                ImGui::Text("CPU time    : %.2f ms", frameTimeNanoSecondsCPU / 1000000.f);
                ImGui::Text("GPU time    : %.2f ms", frameTimeNanoSecondsGPU / 1000000.f);
            }
            else
            {
                ImGui::Text("FPS         : %d (%.2f ms)", fps, frameTimeMilliSeconds);
            }

            if (ImGui::CollapsingHeader("Timing Information", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Button(s_ShowMilliSeconds ? "ms to us" : "us to ms"))
                    s_ShowMilliSeconds = !s_ShowMilliSeconds;
                ImGui::SameLine();
                if (ImGui::Button(s_ShowGPUTimes ? "gpu to cpu time" : "cpu to gpu time"))
                    s_ShowGPUTimes = !s_ShowGPUTimes;

                // Leave a gap
                ImGui::Spacing();

                const CauldronConfig* pConfig = GetConfig();
                if (pConfig->CPUValidationEnabled || pConfig->GPUValidationEnabled)
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "WARNING: Validation layer(s) enabled, perf numbers inaccurate");

                // Plot the frame times according to what we want to see (CPU or GPU)
                uint32_t frameTimeGraphMaxValue = 0;
                if (s_ShowGPUTimes)
                {
                    for (int32_t i = 0; i < _countof(s_FrameTimeGraphMaxGPUValues); ++i)
                    {
                        if (s_HighestRecentGPUTime < s_FrameTimeGraphMaxGPUValues[i]) // sFrameTimeGraphMaxGPUValues are in increasing order
                        {
                            frameTimeGraphMaxValue = std::min(static_cast<int32_t>(_countof(s_FrameTimeGraphMaxGPUValues) - 1), i + 1);
                            break;
                        }
                    }
                    ImGui::PlotLines("", s_GPUFrameTimes, c_NumFrames, 0, "GPU frame time (us)", 0.0f, s_FrameTimeGraphMaxGPUValues[frameTimeGraphMaxValue], ImVec2(0, 40));

                    // Display frame time separately as it's recorded over multiple cmd lists and can't be done together in VK
                    {
                        float valueAvg = 0.f;
                        if (s_NumGatheredFrames)
                        {
                            int64_t avg = s_GPUFrameTotals / s_NumGatheredFrames;
                            valueAvg = s_ShowMilliSeconds ? avg * 0.000001f : avg * 0.001f;
                        }

                        float value = s_ShowMilliSeconds ? GPUTickCount * 0.000001f : GPUTickCount * 0.001f;
                        const char* pStrUnit = s_ShowMilliSeconds ? "ms" : "us";
                        ImGui::Text("%-24.24S: %7.2f %s", L"GPU Frame (total)", value, pStrUnit);
                        ImGui::SameLine();
                        ImGui::Text("  avg: %7.2f %s", valueAvg, pStrUnit);
                    }

                    // Display captured GPU times
                    for (int i = 0; i < gpuTimings.size(); ++i)
                    {
                        float valueAvg = 0.f;
                        if (s_NumGatheredFrames)
                        {
                            int64_t nanoCountTotal = s_GPUTotals[i].TotalTime.count();
                            int64_t avg = nanoCountTotal / s_NumGatheredFrames;
                            valueAvg = s_ShowMilliSeconds ? avg * 0.000001f : avg * 0.001f;
                        }

                        int64_t nanoCount = gpuTimings[i].GetDuration().count();
                        float value = s_ShowMilliSeconds ? nanoCount * 0.000001f : nanoCount * 0.001f;
                        const char* pStrUnit = s_ShowMilliSeconds ? "ms" : "us";
                        // Will print only up to 24 characters left aligned from ':' by 24 characters, and write timings to 2 decimals right aligned by 7 characters
                        ImGui::Text("%-24.24S: %7.2f %s", gpuTimings[i].Label.c_str(), value, pStrUnit);
                        ImGui::SameLine();
                        ImGui::Text("  avg: %7.2f %s", valueAvg, pStrUnit);
                    }
                }
                else
                {
                    for (int32_t i = 0; i < _countof(s_FrameTimeGraphMaxCPUValues); ++i)
                    {
                        if (s_HighestRecentCPUTime < s_FrameTimeGraphMaxCPUValues[i]) // sFrameTimeGraphMaxCPUValues are in increasing order
                        {
                            frameTimeGraphMaxValue = std::min(static_cast<int32_t>(_countof(s_FrameTimeGraphMaxCPUValues) - 1), i + 1);
                            break;
                        }
                    }
                    ImGui::PlotLines("", s_CPUFrameTimes, c_NumFrames, 0, "CPU frame time (us)", 0.0f, s_FrameTimeGraphMaxCPUValues[frameTimeGraphMaxValue], ImVec2(0, 40));

                    // Display frame time separately as it's recorded over multiple cmd lists and can't be done together in VK
                    {
                        float valueAvg = 0.f;
                        if (s_NumGatheredFrames)
                        {
                            int64_t avg = s_CPUFrameTotals / s_NumGatheredFrames;
                            valueAvg = s_ShowMilliSeconds ? avg * 0.000001f : avg * 0.001f;
                        }

                        float value = s_ShowMilliSeconds ? CPUTickCount * 0.000001f : CPUTickCount * 0.001f;
                        const char* pStrUnit = s_ShowMilliSeconds ? "ms" : "us";
                        ImGui::Text("%-24.24S: %7.2f %s", L"CPU Frame (total)", value, pStrUnit);
                        ImGui::SameLine();
                        ImGui::Text("  avg: %7.2f %s", valueAvg, pStrUnit);
                    }

                    // Display captured CPU times
                    for (int i = 0; i < cpuTimings.size(); ++i)
                    {
                        float valueAvg = 0.f;
                        if (s_NumGatheredFrames)
                        {
                            int64_t nanoCountTotal = s_CPUTotals[i].TotalTime.count();
                            int64_t avg = nanoCountTotal / s_NumGatheredFrames;
                            valueAvg = s_ShowMilliSeconds ? avg * 0.000001f : avg * 0.001f;
                        }

                        int64_t nanoCount = cpuTimings[i].GetDuration().count();
                        float value = s_ShowMilliSeconds ? nanoCount * 0.000001f : nanoCount * 0.001f;
                        const char* pStrUnit = s_ShowMilliSeconds ? "ms" : "us";
                        ImGui::Text("%-18S: %7.2f %s", cpuTimings[i].Label.c_str(), value, pStrUnit);
                        ImGui::SameLine();
                        ImGui::Text("  avg: %7.2f %s", valueAvg, pStrUnit);
                    }
                }
            }
        }
        ImGui::End();   // Profiler window
    }

    // Builds the output log dialog
    void UIBackendInternal::BuildOutputDialog(Vec2 uiscale)
    {
        // Setup first time use location (UI always done at display res)
        const ResolutionInfo& resInfo      = GetFramework()->GetResolutionInfo();
        float outputWidth = resInfo.fDisplayWidth() - ((s_UIDialogXSpacing * 3 + s_UIPerfDialogWidth) * uiscale.getX());
        float outputHeight = resInfo.fDisplayHeight() - ((s_UIDialogYSpacing * 3 + s_UITabDialogHeight) * uiscale.getY());
        ImGui::SetNextWindowSize(ImVec2(outputWidth, outputHeight), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2((s_UIDialogXSpacing * 2 + s_UIPerfDialogWidth) * uiscale.getX(), (s_UIDialogYSpacing * 2 + s_UITabDialogHeight) * uiscale.getY()), ImGuiCond_FirstUseEver);
        ImGui::Begin("Output (F3 to toggle)", nullptr, ImGuiWindowFlags_NoCollapse);
        {
            // Query the number of each type of message we have for filtering options
            std::array<uint32_t, LOGLEVEL_COUNT> levelCountArray;
            Log::QueryMessageCounts(levelCountArray);
            char filterText[50];

            // Calculate button border based on resolution
            int32_t borderSize = std::min(1, static_cast<int32_t>(2 * uiscale.getX()));

            // Add ability to filter messages
            sprintf_s(filterText, 50, "%d Traces", levelCountArray[0]);
            OutputFilterButton(filterText, 0, borderSize);

            sprintf_s(filterText, 50, "%d Debug", levelCountArray[1]);
            OutputFilterButton(filterText, 1, borderSize, true);

            sprintf_s(filterText, 50, "%d Info", levelCountArray[2]);
            OutputFilterButton(filterText, 2, borderSize, true);

            sprintf_s(filterText, 50, "%d Warnings", levelCountArray[3]);
            OutputFilterButton(filterText, 3, borderSize, true);

            sprintf_s(filterText, 50, "%d Errors", levelCountArray[4]);
            OutputFilterButton(filterText, 4, borderSize, true);

            sprintf_s(filterText, 50, "%d Fatals", levelCountArray[5]);
            OutputFilterButton(filterText, 5, borderSize, true);

            ImGui::SameLine();
            if (ImGui::SmallButton("Reset Filters"))
                memset(m_FilterEnabled, 1, sizeof(bool) * LOGLEVEL_COUNT);

            // Separate from actual text printing
            ImGui::Separator();

            // Reserve enough left-over height for 1 separator + 1 input text
            const float footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() / 2.f;
            ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeightToReserve), false, ImGuiWindowFlags_HorizontalScrollbar);

            // Gather up the messages to print based on filters
            int32_t msgMask = 0;
            for (int32_t i = 0; i < LOGLEVEL_COUNT; ++i)
            {
                if (m_FilterEnabled[i])
                    msgMask |= 1 << i;
            }

            // Print all wanted messages
            std::vector<LogMessageEntry> messages;
            Log::GetMessages(messages, msgMask);
            for (auto msgIter = messages.begin(); msgIter != messages.end(); ++msgIter)
            {
                ImVec4 color;
                bool hasColor;
                switch (msgIter->LogPriority)
                {
                case LogLevel::LOGLEVEL_DEBUG:
                    color = ImVec4(0.4f, 0.4f, 1.f, 1.f);
                    hasColor = true;
                    break;
                case LogLevel::LOGLEVEL_WARNING:
                    color = ImVec4(1.0f, 0.9f, 0.4f, 1.f);
                    hasColor = true;
                    break;
                case LogLevel::LOGLEVEL_ERROR:
                case LogLevel::LOGLEVEL_FATAL:
                    color = ImVec4(1.0f, 0.4f, 0.4f, 1.f);
                    hasColor = true;
                    break;
                default:
                    hasColor = false;
                }

                if (hasColor)
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(WStringToString(msgIter->LogMessage).c_str());
                if (hasColor)
                    ImGui::PopStyleColor();

                // Auto scroll down if needed
                if (m_OutputAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndChild();  // scroll reservation

            ImGui::End();   // End output window
        }
    }

    // Builds the general tab
    void UIBackendInternal::BuildGeneralTab()
    {
        static RuntimeShaderRecompilerRenderModule* pShaderCompilerRM = static_cast<RuntimeShaderRecompilerRenderModule*>(GetFramework()->GetRenderModule("RuntimeShaderRecompilerRenderModule"));
        if (pShaderCompilerRM && pShaderCompilerRM->RebuildEnabled())
        {
            if (ImGui::CollapsingHeader("Shader re-compile", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (pShaderCompilerRM->RebuildEnabled())
                {
                    if (ImGui::Button("Compile Modified Shaders"))
                    {
                        pShaderCompilerRM->ScheduleRebuild();
                    }

                    // Display the build status description
                    ImGui::Text("Build Status:");
                    ImGui::Text(pShaderCompilerRM->GetBuildStatusDescription());
                }
                else
                {
                    ImGui::Text("Runtime shader recompile not configured.");
                }
            }
        }

        if(GetConfig()->IsAnyInCodeCaptureEnabled())
        {
            if (ImGui::CollapsingHeader("Capture", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if(GetConfig()->EnableRenderDocCapture)
                {
                    if(ImGui::Button("RenderDoc"))
                    {
                        GetFramework()->TakeRenderDocCapture();
                    }
                }

                if(GetFramework()->GetConfig()->EnablePixCapture)
                {
                    if(ImGui::Button("Pix"))
                    {
                        GetFramework()->TakePixCapture();
                    }
                }
            }
        }

        // Get all the ui sections and elements from the ui manager
        for (const auto& sectionIter : GetUIManager()->GetGeneralLayout())
        {
            const auto& section = sectionIter.second;
            if (section->Shown() &&
                ImGui::CollapsingHeader(section->GetSectionName(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                // Iterate through all elements
                for (const auto& elementIter : section->GetElements())
                {
                    const auto& element = elementIter.second;
                    if (element->IsShown())
                    {
                        if (element->SameLine())
                            ImGui::SameLine();

                        ImGui::BeginDisabled(!element->Enabled());

                        element->BuildUI();

                        ImGui::EndDisabled();
                    }
                }
            }

            // Add a space between sections
            ImGui::Spacing();
        }
    }

    void EntityEntry(const Entity* pEntity)
    {
        if (pEntity->GetChildCount())
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);

            if (ImGui::TreeNode("%s", WStringToString(pEntity->GetName()).c_str()))
            {
                const auto& children = pEntity->GetChildren();
                for (auto childItr = children.begin(); childItr != children.end(); ++childItr)
                    EntityEntry(*childItr);

                ImGui::TreePop();
            }
        }
        else
        {
            // Render disabled style is not active
            if (!pEntity->IsActive()) {
                ImGui::BeginDisabled(true);
            }
                
            ImGui::Text("%S", pEntity->GetName());

            if (!pEntity->IsActive()) {
                ImGui::EndDisabled();
            }
                
        }
    }

    // Builds the scene tab
    void UIBackendInternal::BuildSceneTab()
    {
        // Fetch the scene entities to list
        const std::vector<const Entity*>& sceneEntities = GetScene()->GetSceneEntities();

        // For now, just list the name of all loaded entities until we decide what to do with this
        if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (auto entityIter = sceneEntities.begin(); entityIter != sceneEntities.end(); ++entityIter)
                EntityEntry(*entityIter);
        }
    }

    void UIText::BuildUI()
    {
        ImGui::Text(GetDesc());
    }

    void UIButton::BuildUI()
    {
        if (ImGui::Button(GetDesc()))
        {
            m_Callback();
        }
    }

    void UICheckBox::BuildUI()
    {
        bool cur = GetData();

        if (ImGui::Checkbox(GetDesc(), &cur))
        {
            SetData(cur);
        }
    }

    void UIRadioButton::BuildUI()
    {
        CauldronError(L"Not yet implemented! Add support to ImGuiBackendCommon::BuildGeneralTab()");
    }

    void UICombo::BuildUI()
    {
        int32_t cur = GetData();

        if (ImGui::Combo(GetDesc(), &cur, m_Option.data(), (int32_t)m_Option.size()))
        {
            SetData(cur);
        }
    }

    void UISlider<int32_t>::BuildUI()
    {
        int32_t cur = GetData();

        if (ImGui::SliderInt(GetDesc(), &cur, m_MinValue, m_MaxValue, m_Format))
        {
            SetData(cur);
        }
    }

    void UISlider<float>::BuildUI()
    {
        float cur = GetData();

        if (ImGui::SliderFloat(GetDesc(), &cur, m_MinValue, m_MaxValue, m_Format))
        {
            SetData(cur);
        }
    }

    void UISeparator::BuildUI()
    {
        ImGui::Separator();
    }

} // namespace cauldron

#endif // defined(_WINDOWS)
