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

/// @defgroup SDKSample Samples
/// This module contains reference documentation for the FidelityFX SDK samples. This includes detailed information on the collection of samples within the FidelityFX SDK as well as documentation for the FidelityFX Cauldron Framework used as a backbone for these samples.
///

/// @defgroup SDKEffects Effect samples
/// The FidelityFX SDK contains a number of samples which demonstrate how to use Effects in the SDK. The sub-modules here include reference documentation for each sample.
///
/// @ingroup SDKSample

/// @defgroup Cauldron Cauldron
/// FidelityFX Cauldron Framework is a library for rapid prototyping using either the Vulkan® or DirectX® 12 APIs. The effects in the FidelityFX SDK are built around the FidelityFX Cauldron Framework.
///
/// This module contains reference documentation for the Cauldron framework.
///
/// @ingroup SDKSample

/// @defgroup CauldronCore Core
/// FidelityFX Cauldron Framework Core reference documentation
///
/// @ingroup Cauldron

/// @defgroup CauldronMisc Miscellaneous
/// FidelityFX Cauldron Framework Misc reference documentation
///
/// @ingroup Cauldron

/// @defgroup CauldronRender Render
/// FidelityFX Cauldron Framework Render reference documentation
///
/// @ingroup Cauldron

#include "misc/helpers.h"
#include "render/commandlist.h"
#include "render/particle.h"
#include "render/rendermodule.h"
#include "shaders/shadercommon.h"
#include "core/taskmanager.h"

// No way to forward declare this that I can tell
#include "json/json.h"
using json = nlohmann::ordered_json;

#include <chrono>
#include <map>
#include <set>
#include <unordered_set>
#include <xstring>
#include <functional>

namespace cauldron
{
    /**
     * @struct RenderResourceInformation
     *
     * Resource information used to create auto-generated resources.
     * 
     * @ingroup CauldronCore
     */
    struct RenderResourceInformation
    {
        ResourceFormat Format;  ///< The resource (texture) format
        bool AllowUAV;          ///< True if resource requires UAV usage
        bool RenderResolution;  ///< If true, resize function for the resource should always recreate to render resolution (and not display)
    };

    /**
     * @struct RenderModuleInfo
     *
     * Contains information needed for individual render module initialization.
     *
     * @ingroup CauldronCore
     */
    struct RenderModuleInfo
    {
        std::string     Name = "";      ///< Rendermodule name
        json            InitOptions;    ///< Initialization options from json to configure the module at init time
    };

    /**
     * @struct CauldronConfig
     *
     * Represents the configuration used to initiate the current cauldron run.
     * Framework will be initialzed according to contents of CauldronConfig + any
     * command line overrides applied.
     *
     * @ingroup CauldronCore
     */
    struct CauldronConfig
    {
        //////////////////////////////////////////////////////////////////////////
        // Binary options

        // Validation
        bool CPUValidationEnabled : 1;
        bool GPUValidationEnabled : 1;

        // Features
        bool VRSTier1 : 1;
        bool VRSTier2 : 1;
        bool RT_1_0 : 1;
        bool RT_1_1 : 1;
        bool FP16 : 1;
        bool ShaderStorageBufferArrayNonUniformIndexing : 1;

        // Presentation
        bool Vsync : 1;
        bool Fullscreen    : 1;

        // Other options
        bool DeveloperMode : 1;
        bool DebugShaders : 1;
        bool AGSEnabled : 1;
        bool StablePowerState : 1;
        bool InvertedDepth : 1;
        bool AntiLag2 : 1;

        // RenderDoc
        bool EnableRenderDocCapture : 1;
        // Pix
        bool EnablePixCapture : 1;

        // Override Scene Samplers
        bool OverrideSceneSamplers : 1;

        // Perf Dump
        bool EnableBenchmark : 1;
        bool BenchmarkAppend : 1;
        bool BenchmarkJson   : 1;

        // Screen shot
        bool TakeScreenshot : 1;

        // FPS limiter
        bool LimitFPS : 1;
        bool GPULimitFPS : 1;

        // Acceleration Structure
        bool BuildRayTracingAccelerationStructure : 1;

        //////////////////////////////////////////////////////////////////////////
        // Non-binary data

        std::string MotionVectorGeneration = "";

        // FPS limiter
        uint32_t LimitedFrameRate = 240;

        // Presentation
        uint8_t  BackBufferCount = 2;
        uint32_t Width = 1920;
        uint32_t Height = 1080;
        uint32_t FontSize = 13;

        // Allocation sizes
        uint64_t UploadHeapSize        = 100 * 1024 * 1024;
        uint32_t DynamicBufferPoolSize = 2 * 1024 * 1024;
        uint32_t GPUResourceViewCount  = 10000;
        uint32_t CPUResourceViewCount  = 100;
        uint32_t CPURenderViewCount    = 100;
        uint32_t CPUDepthViewCount     = 100;
        uint32_t GPUSamplerViewCount   = 100;

        // DisplayMode
        DisplayMode CurrentDisplayMode = DisplayMode::DISPLAYMODE_LDR;

        ResourceFormat SwapChainFormat = ResourceFormat::Unknown;

        // Requested minimum shader model
        ShaderModel MinShaderModel = ShaderModel::SM5_1;

        // List of scenes loaded at startup and content creation callbacks from elsewhere in the framework
        std::vector<Task> ContentCreationTasks = {};

        /**
         * @struct StartupContentDef
         *
         * Represents the content to auto-load as part of the sample startup process.
         * Also contains initialization values for various frame work components such
         * as image-based lighting factors and exposure settings.
         *
         * @ingroup CauldronCore
         */
        struct StartupContentDef
        {
            std::vector<std::wstring>           Scenes = {};                                                    ///< The gltf scenes to load on startup
            std::wstring                        Camera = L"";                                                   ///< The camera to set as active at startup
            float                               SceneExposure = 1.f;                                            ///< Scene exposure setting at startup for content
            std::vector<ParticleSpawnerDesc>    ParticleSpawners = {};                                          ///< Particle systems to create for the sample
            
            std::wstring                        DiffuseIBL = L"..\\media\\IBL\\mud_road_puresky_Diffuse.dds";   ///< Diffuse reflection map to use for sample
            std::wstring                        SpecularIBL = L"..\\media\\IBL\\mud_road_puresky_Specular.dds"; ///< Specular reflection map to use for sample
            std::wstring                        SkyMap = L"..\\media\\IBL\\mud_road_puresky_Specular.dds";      ///< Environment map used for rendering
            float                               IBLFactor   = 0.55f;                                            ///< IBL factor to apply for sample

        } StartupContent;

        // Perf Output
        uint32_t                      BenchmarkFrameDuration = -1;
        std::wstring                  BenchmarkPath = L"";
        float                         BenchmarkDeviationFilterFactor = 1.0f;

        std::vector<std::pair<std::wstring, std::wstring>> BenchmarkPermutationOptions = {};

        // App identifier
        std::wstring                  AppName = L"";

        // Screenshot name (for use with perf output when specified)
        std::experimental::filesystem::path ScreenShotFileName = L"";

        void Validate() const;
        const wchar_t* GetAliasedResourceName(const wchar_t* resourceName) const;
        RenderResourceInformation GetRenderResourceInformation(const wchar_t* resourceName) const;

        const bool IsAnyInCodeCaptureEnabled() const { return EnableRenderDocCapture || EnablePixCapture; }

    private:
        friend class Framework;   // Only the framework parser can modify render resources/modules directly

        // Render resources
        std::map<std::wstring, RenderResourceInformation> RenderResources        = {};
        std::map<std::wstring, std::wstring>              RenderResourceMappings = {};

        // Render modules
        std::vector<RenderModuleInfo> RenderModules = {};
    };

    // Resolution update func (defined when enabling an upscaler)
    typedef std::function<ResolutionInfo(uint32_t, uint32_t)> ResolutionUpdateFunc;

    // Execution callback func
    typedef std::function<void(double, CommandList*)> ExecuteCallback;
    typedef std::pair<std::wstring, std::pair<RenderModule*, ExecuteCallback>> ExecutionTuple;

    /**
     * @struct FrameworkInitParams
     *
     * Used to pass application parameters to the framework for initialization.
     *
     * @ingroup CauldronCore
     */
    struct FrameworkInitParams
    {
        wchar_t* Name;                  ///< Application name to use
        wchar_t* CmdLine;               ///< Command line parameters from application instance
        void*    AdditionalParams;      ///< Additional parameters from application instance
    };

    /**
     * @class FrameworkImpl
     *
     * Base class from which implementations need to inherit.
     * Pull in platform versions for access to <c><i>FrameworkInteral</i></c>.
     *
     * @ingroup CauldronCore
     */
    class FrameworkImpl
    {
    public:
        FrameworkImpl(Framework* pFramework) : m_pFramework(pFramework) {}
        virtual ~FrameworkImpl() = default;

        virtual void Init() = 0;
        virtual int32_t Run() = 0;
        virtual void    PreRun() = 0;
        virtual void    PostRun() = 0;
        virtual void    Shutdown() = 0;

    protected:
        Framework*  m_pFramework = nullptr;

    private:
        FrameworkImpl() = delete;
        NO_COPY(FrameworkImpl)
        NO_MOVE(FrameworkImpl)
    };

    // Forward declare internal implementation
    class ComponentMgr;
    class ContentManager;
    class Device;
    class DynamicBufferPool;
    class DynamicResourcePool;
    class FrameworkInternal;
    class InputManager;
    class Profiler;
    class RasterViewAllocator;
    class ResourceResizedListener;
    class ResourceViewAllocator;
    class Scene;
    class ShadowMapResourcePool;
    class SwapChain;
    class TaskManager;
    class UIManager;
    class UploadHeap;

    /**
     * @class Framework
     *
     * Sample-backing framework. Is responsible for application setup and tear down based
     * on runtime configuration + command line overrides.
     * 
     * Main framework instance holds all required constructs to handle 
     * application updates and rendering. Samples are offered override points
     * to modify sample configuration or run-time behavior.
     *
     * @ingroup CauldronCore
     */
    class Framework
    {
    public:
        
        /**
         * @brief   Framework initialization. Override to modify/override framework initialization.
         */
        virtual void Init();

        /**
         * @brief   Framework pre-runtime callback. Called before entering main loop 
         *          after framework/sample has been initialized. Override to modify/override 
         *          framework pre-run.
         */
        virtual void PreRun();

        /**
         * @brief   Framework post-runtime callback. Called after exiting main loop
         *          prior to framework/sample termination. Override to modify/override
         *          framework post run.
         */
        virtual void PostRun();

        /**
         * @brief   Framework shutdown. Called to clean everything up when we are done. 
         *          Override to modify/override framework shutdown.
         */
        virtual void Shutdown();

        /**
         * @brief   Framework run callback. Calls implementation's run cycle (which calls <c><i>MainLoop()</i></c>.
         */
        int32_t Run();

        /**
         * @brief   Framework main loop callback. Executes a frame's worth of work.
         */
        void MainLoop();

        /**
         * @brief   Internal implementation accessor.
         */
        const FrameworkInternal* GetImpl() const { return m_pImpl; }

        /**
         * @brief   ParseSampleConfig(). Override in sample to modify application configuration.
         */
        virtual void ParseSampleConfig() { }

        /**
         * @brief   ParseSampleCmdLine(). Override in sample to modify application configuration.
         */
        virtual void ParseSampleCmdLine(const wchar_t* cmdLine) { }

        /**
         * @brief   RegisterSampleModules(). Override in sample to 
         *          register additional render modules and components.
         */
        virtual void RegisterSampleModules() {}

        /**
         * @brief   DoSampleInit(). Override in sample to modify application initialization.
         */
        virtual void DoSampleInit() {}

        /**
         * @brief   DoSampleUpdates(). Override in sample to perform additional sample updates.
         */
        virtual void DoSampleUpdates(double deltaTime) {}

        /**
         * @brief   DoSampleResize(). Override in sample to handle application resize changes.
         */
        virtual void DoSampleResize(const ResolutionInfo& resInfo) {}

        /**
         * @brief   DoSampleShutdown(). Override in sample to modify application shutdown.
         */
        virtual void DoSampleShutdown() {}

        /**
         * @brief   Utility function to parse all known options from config data.
         */
        void ParseConfigData(const json& jsonConfigData);

        /**
         * @brief   Retrieves the <c><i>TaskManager</i></c> instance.
         */
        const TaskManager* GetTaskManager() const { return m_pTaskManager; }
        TaskManager* GetTaskManager() { return m_pTaskManager; }

        /**
         * @brief   Retrieves the <c><i>Scene</i></c> instance.
         */
        const Scene* GetScene() const { return m_pScene; }
        Scene* GetScene() { return m_pScene; }

        /**
         * @brief   Retrieves the <c><i>InputManager</i></c> instance.
         */
        const InputManager* GetInputMgr() const { return m_pInputManager; }
        InputManager* GetInputMgr() { return m_pInputManager; }

        /**
         * @brief   Retrieves the <c><i>ContentManager</i></c> instance.
         */
        const ContentManager* GetContentManager() const { return m_pContentManager; }
        ContentManager* GetContentManager() { return m_pContentManager; }

        /**
         * @brief   Retrieves the <c><i>Profiler</i></c> instance.
         */
        const Profiler* GetProfiler() const { return m_pProfiler; }
        Profiler* GetProfiler() { return m_pProfiler; }

        /**
         * @brief   Retrieves the <c><i>Device</i></c> instance.
         */
        const Device* GetDevice() const { return m_pDevice; }
        Device* GetDevice() { return m_pDevice; }

        /**
         * @brief   Retrieves the <c><i>ResourceViewAllocator</i></c> instance.
         */
        const ResourceViewAllocator* GetResourceViewAllocator() const { return m_pResourceViewAllocator; }
        ResourceViewAllocator* GetResourceViewAllocator() { return m_pResourceViewAllocator; }

        /**
         * @brief   Retrieves the <c><i>RasterViewAllocator</i></c> instance.
         */
        const RasterViewAllocator* GetRasterViewAllocator() const { return m_pRasterViewAllocator; }
        RasterViewAllocator* GetRasterViewAllocator() { return m_pRasterViewAllocator; }

        /**
         * @brief   Retrieves the <c><i>SwapChain</i></c> instance.
         */
        const SwapChain* GetSwapChain() const { return m_pSwapChain; }
        SwapChain* GetSwapChain() { return m_pSwapChain; }

        /**
         * @brief   Retrieves the <c><i>UIManager</i></c> instance.
         */
        const UIManager* GetUIManager() const { return m_pUIManager; }
        UIManager* GetUIManager() { return m_pUIManager; }

        /**
         * @brief   Retrieves the <c><i>UploadHeap</i></c> instance.
         */
        const UploadHeap* GetUploadHeap() const { return m_pUploadHeap; }
        UploadHeap* GetUploadHeap() { return m_pUploadHeap; }

        /**
         * @brief   Retrieves the <c><i>DynamicBufferPool</i></c> instance.
         */
        const DynamicBufferPool* GetDynamicBufferPool() const { return m_pDynamicBufferPool; }
        DynamicBufferPool* GetDynamicBufferPool() { return m_pDynamicBufferPool; }

        /**
         * @brief   Retrieves the <c><i>DynamicResourcePool</i></c> instance.
         */
        const DynamicResourcePool* GetDynamicResourcePool() const { return m_pDynamicResourcePool; }
        DynamicResourcePool* GetDynamicResourcePool() { return m_pDynamicResourcePool; }

        /**
         * @brief   Retrieves the <c><i>ShadowMapResourcePool</i></c> instance.
         */
        const ShadowMapResourcePool* GetShadowMapResourcePool() const { return m_pShadowMapResourcePool; }
        ShadowMapResourcePool* GetShadowMapResourcePool() { return m_pShadowMapResourcePool; }

        /**
         * @brief   Gets the active <c><i>CommandList</i></c> for the frame.
         */
        CommandList* GetActiveCommandList() { CauldronAssert(ASSERT_CRITICAL, m_pCmdListForFrame, L"Framework: Trying to get active command list outside of BeginFrame/EndFrame."); return m_pCmdListForFrame; }

        /**
         * @brief   Retrieves the requested <c><i>Texture</i></c> instance.
         *          Note: This should not be called after application initialization.
         */
        const Texture* GetRenderTexture(const wchar_t* name) const;

        /**
         * @brief   Retrieves the requested <c><i>RenderModule</i></c> instance.
         *          Note: This should not be called after application initialization.
         */
        RenderModule* GetRenderModule(const char* name);

        /**
         * @brief   Retrieves the <c><i>CauldronConfig</i></c> used to setup the current run.
         */
        const CauldronConfig* GetConfig() const { return &m_Config; }

        /**
         * @brief   Retrieves the application's name.
         */
        const wchar_t* GetName() const { return m_Name.c_str(); }

        /**
         * @brief   Retrieves the command line arguments passed to the application.
         */
        const wchar_t* GetCmdLine() const { return m_CmdLine.c_str(); }

        /**
         * @brief   Retrieves name of the CPU the application is running on.
         */
        const wchar_t* GetCPUName() const { return m_CPUName.c_str(); }

        /**
         * @brief   Initializes the application configuration.
         */
        void InitConfig();

        // Used to support upscaling and dynamic resolution changes
        /**
         * @brief   Retrieves current <c><i>ResolutionInfo</i></c> data.
         */
        const ResolutionInfo& GetResolutionInfo() const { return m_ResolutionInfo; }

        void UpdateRenderResolution(uint32_t renderWidth, uint32_t renderHeight)
        {
            m_ResolutionInfo.RenderWidth = renderWidth;
            m_ResolutionInfo.RenderHeight = renderHeight;
        }

        /**
         * @brief   Enables or disabled upscaling in the application.
         */
        void EnableUpscaling(bool enabled, ResolutionUpdateFunc func = nullptr);

        /**
         * @brief   Processes application resize. Will initiate resource recreation and binding processes.
         */
        void ResizeEvent();

        /**
         * @brief   Processes application focus lost event.
         */
        void FocusLostEvent();

        /**
         * @brief   Processes application focus gained event.
         */
        void FocusGainedEvent();

        /**
         * @brief   Used to set the current internal upsample state.
         */
        void SetUpscalingState(UpscalerState state) { m_UpscalingState = state;}

        /**
         * @brief   Gets the current internal upsample state.
         */
        UpscalerState GetUpscalingState() const { return !m_UpscalerEnabled ? UpscalerState::None : m_UpscalingState; }

        /**
         * @brief   Query if upsampling has been enabled.
         */
        bool UpscalerEnabled() const { return m_UpscalerEnabled; }

        void EnableFrameInterpolation(bool enabled);
        
        bool FrameInterpolationEnabled() const { return m_FrameInterpolationEnabled; }

        /**
         * @brief   Overrides the default tonemapper.
         */
        void SetTonemapper(const RenderModule* pToneMapper) { m_pToneMapper = pToneMapper; }

        /**
         * @brief   Fetches the proper resource to use as a color target given the callback's ordering.
         */
        const Texture* GetColorTargetForCallback(const wchar_t* callbackOrModuleName);

        /**
         * @brief   Inserts a new execution callback for the runtime.
         */
        void RegisterExecutionCallback(std::wstring insertionName, bool preInsertion, ExecutionTuple callbackTuple);

        /**
         * @brief   Gets the current aspect ratio for the application display.
         */
        float    GetAspectRatio() const { return m_ResolutionInfo.GetDisplayAspectRatio(); }

        /**
         * @brief   Gets parameters needed by upscalers.
         */
        void     GetUpscaledRenderInfo(uint32_t& width, uint32_t& height, float& renderWidthRatio, float& renderHeightRatio) const;

        /**
         * @brief   Gets the current frame time slice.
         */
        double   GetDeltaTime() const { return m_DeltaTime; }

        /**
         * @brief   Gets the current frame ID.
         */
        uint64_t GetFrameID() const { return m_FrameID; }

        /**
         * @brief   Query whether the sample is currently running.
         */
        bool     IsRunning() const { return m_Running.load(); }

        /**
         * @brief   Fetches the main thread's ID. Used to enforce performance warnings on select functionality.
         */
        const std::thread::id MainThreadID() const { return m_MainThreadID; }

        /**
         * @brief   Registers a component manager for the defined Comp type with the framework.
         */
        template <typename Comp>
        void RegisterComponentManager();

        /**
         * @brief   Unregisters all Components and RenderModules. Called at application shutdown.
         */
        void UnRegisterComponentsAndRenderModules();

        /**
         * @brief   Used to register for resource load callbacks.
         */
        void AddResizableResourceDependence(ResourceResizedListener* pListener);

        /**
         * @brief   Used to unregister from resource load callbacks.
         */
        void RemoveResizableResourceDependence(ResourceResizedListener* pListener);

        /**
         * @brief   Takes a RenderDoc capture if option is enabled. Will open the capture in a new/running instance of RenderDoc.
         */
        void TakeRenderDocCapture() { m_RenderDocCaptureState = FrameCaptureState::CaptureRequested; }

        /**
         * @brief   Takes a PIX capture if option is enabled. Will open the capture in a new/running instance of PIX.
         */
        void TakePixCapture() { m_PixCaptureState = FrameCaptureState::CaptureRequested; }

        /**
         * @brief   Enqueues a content creation task to be executed once the framework is done initializing.
         */
        void AddContentCreationTask(Task& task) { m_Config.ContentCreationTasks.push_back(task); }

        /**
         * @brief   Configures the specified callbacks for runtime shader recompile/reload.
         *          These functions are called pre and post FFX shader reload.
         *          The preReloadCallback should destroy the FFX interface and any currently active
         *          FFX Components' contexts.
         *          The postReloadCallback should re-initialize the FFX interface and re-create
         *          all previously active Components' contexts.
         */
        void ConfigureRuntimeShaderRecompiler(
                                      std::function<void(void)> preReloadCallback,
                                      std::function<void(void)> postReloadCallback);

    private:
        friend class FrameworkInternal;
        Framework() = delete;

        // No Copy, No Move
        NO_COPY(Framework);
        NO_MOVE(Framework);

        int CreateRenderResources();
        void RegisterComponentsAndModules();
        RenderModule* GetRenderModule(uint32_t order);
        bool AreDependenciesPresent(const std::set<std::string>&, const std::set<std::string>&) const;

    private:
        FrameworkInternal*                      m_pImpl = nullptr;

        // This variable will be used to ensure we aren't doing things on the wrong thread. There are certain operations we never want to do on the main thread
        std::thread::id                         m_MainThreadID;

        // RenderDoc
        void*                                   m_RenderDocApi = nullptr;

        enum class FrameCaptureState
        {
            None = 0,
            CaptureRequested,
            CaptureStarted,
        };

    protected:
        Framework(const FrameworkInitParams* pInitParams);
        virtual ~Framework();

        void ParseConfigFile(const wchar_t* configFileName);
        void ParseCmdLine(const wchar_t* cmdLine);

        void BeginFrame();
        void EndFrame();

        // Members
        CauldronConfig          m_Config = {};
        std::wstring            m_Name;
        std::wstring            m_ConfigFileName;
        std::wstring            m_CmdLine;
        std::wstring            m_CPUName = L"Not Set";
        ResolutionInfo          m_ResolutionInfo            = {1920, 1080, 1920, 1080, 1920, 1080};
        ResolutionInfo          m_BenchmarkResolutionInfo   = {1920, 1080, 1920, 1080, 1920, 1080};
        UpscalerState           m_UpscalingState = UpscalerState::None;
        ResolutionUpdateFunc    m_ResolutionUpdaterFn = nullptr;
        bool                    m_UpscalerEnabled = false;
        bool                    m_FrameInterpolationEnabled = false;
        std::atomic_bool        m_Running = false;
        FrameCaptureState       m_RenderDocCaptureState = FrameCaptureState::None;
        FrameCaptureState       m_PixCaptureState       = FrameCaptureState::None;

        // Task Manager for background tasks
        TaskManager*            m_pTaskManager = nullptr;

        // Scene to hold loaded entities for interacting with
        Scene*                  m_pScene = nullptr;

        // Content manager to handle all the things we load
        ContentManager*         m_pContentManager = nullptr;

        // Time/Frame management
        std::chrono::time_point<std::chrono::system_clock> m_LoadingStartTime;
        std::chrono::time_point<std::chrono::system_clock> m_LastFrameTime;
        double                  m_DeltaTime = 0.0;
        uint64_t                m_FrameID   = -1;               // Start at -1 so that the first frame is 0 (as we increment on begin frame)
        CommandList*            m_pCmdListForFrame = nullptr;  // Valid between Begin/EndFrame only

        CommandList*                                       m_pDeviceCmdListForFrame = nullptr;    // Valid between Begin/EndFrame only
        std::vector<CommandList*>                          m_vecCmdListsForFrame;                 // Valid between Begin/EndFrame only

        // Profiling/Perf data
        Profiler*               m_pProfiler = nullptr;
        struct PerfStats
        {
            std::wstring             Label;
            std::chrono::nanoseconds              min, max, total;
            size_t                                refinedSize;
            std::vector<std::chrono::nanoseconds> timings;
        };
        std::vector<PerfStats>                             m_CpuPerfStats{}, m_GpuPerfStats{};
        int64_t                                            m_PerfFrameCount{INT64_MIN};
        std::chrono::time_point<std::chrono::steady_clock> m_StartTime;
        std::chrono::time_point<std::chrono::steady_clock> m_StopTime = std::chrono::time_point<std::chrono::steady_clock>::max();

        // Graphics sub-systems
        Device*                 m_pDevice                = nullptr;
        ResourceViewAllocator*  m_pResourceViewAllocator = nullptr;
        RasterViewAllocator*    m_pRasterViewAllocator   = nullptr;
        SwapChain*              m_pSwapChain             = nullptr;
        UploadHeap*             m_pUploadHeap            = nullptr;
        DynamicBufferPool*      m_pDynamicBufferPool     = nullptr;
        DynamicResourcePool*    m_pDynamicResourcePool   = nullptr;
        ShadowMapResourcePool*  m_pShadowMapResourcePool = nullptr;
        InputManager*           m_pInputManager          = nullptr;
        UIManager*              m_pUIManager             = nullptr;

        // list of instances that need to react to the change of size
        std::mutex m_ResourceResizeMutex;
        std::unordered_set<ResourceResizedListener*> m_ResourceResizedListeners;

        // Render modules and components
        const RenderModule*                     m_pToneMapper = nullptr;
        std::vector<RenderModule*>              m_RenderModules = {};
        std::vector<ExecutionTuple>             m_ExecutionCallbacks = {};
        std::map<std::wstring, ComponentMgr*>   m_ComponentManagers = {};
    };

    /**
    * @brief   Main runtime execution callback.
    */
    int RunFramework(Framework* pFramework);

    /**
    * @brief   Retrieves the current <c><i>Framework</i></c> instance.
    */
    Framework* GetFramework();

    /**
    * @brief   Retrieves the current <c><i>CauldronConfig</i></c> for the running instance.
    */
    const CauldronConfig*  GetConfig();

    /**
    * @brief   Retrieves the current <c><i>TaskManager</i></c> instance.
    */
    TaskManager* GetTaskManager();

    /**
    * @brief   Retrieves the current <c><i>ContentManager</i></c> instance.
    */
    ContentManager* GetContentManager();

    /**
    * @brief   Retrieves the current <c><i>Profiler</i></c> instance.
    */
    Profiler* GetProfiler();

    /**
    * @brief   Retrieves the current <c><i>Device</i></c> instance.
    */
    Device* GetDevice();

    /**
    * @brief   Retrieves the current <c><i>ResourceViewAllocator</i></c> instance.
    */
    ResourceViewAllocator* GetResourceViewAllocator();

    /**
    * @brief   Retrieves the current <c><i>RasterViewAllocator</i></c> instance.
    */
    RasterViewAllocator* GetRasterViewAllocator();

    /**
    * @brief   Retrieves the current <c><i>SwawpChain</i></c> instance.
    */
    SwapChain* GetSwapChain();

    /**
    * @brief   Retrieves the current <c><i>UploadHeap</i></c> instance.
    */
    UploadHeap* GetUploadHeap();

    /**
    * @brief   Retrieves the current <c><i>DynamicBufferPool</i></c> instance.
    */
    DynamicBufferPool* GetDynamicBufferPool();

    /**
    * @brief   Retrieves the current <c><i>DynamicResourcePool</i></c> instance.
    */
    DynamicResourcePool* GetDynamicResourcePool();

    /**
    * @brief   Retrieves the current <c><i>Scene</i></c> instance.
    */
    Scene* GetScene();

    /**
    * @brief   Retrieves the current <c><i>InputManager</i></c> instance.
    */
    InputManager* GetInputManager();

    /**
    * @brief   Retrieves the current <c><i>UIManager</i></c> instance.
    */
    UIManager* GetUIManager();

} // namespace cauldron
