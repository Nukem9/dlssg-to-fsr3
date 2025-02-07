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

#include "misc/helpers.h"
#include "render/renderdefines.h"

#include <mutex>
#include <queue>

namespace cauldron
{
    struct Barrier;
    struct BufferCopyDesc;
    class CommandList;
    class DeviceInternal;
    class SwapChain;
    struct SwapChainCreationParams;
    class Texture;
    struct TextureCopyDesc;

    /// A structure representing variable shading rate information
    ///
    /// @ingroup CauldronRender
    struct VariableShadingRateInfo
    {
        VariableShadingMode VariableShadingMode;    ///< The <c><i>VariableShadingMode</i></c> to use.
        ShadingRate         BaseShadingRate;        ///< The <c><i>ShadingRate</i></c> to use.
        const Texture*      pShadingRateImage;      ///< The shading rate texture to use.
        ShadingRateCombiner Combiners[2];           ///< The <c><i>ShadingRateCombiner</i></c>s to use.
        uint32_t            ShadingRateTileWidth;   ///< The shading rate tile width.
        uint32_t            ShadingRateTileHeight;  ///< The shading rate tile height.
    };

    /// Callback to be used when detecting device removed error while presenting the frame
    ///
    /// @ingroup CauldronRender
    typedef void (*DeviceRemovedCallback)(void* customData);

    /**
     * @class Device
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of the rendering device.
     *
     * @ingroup CauldronRender
     */
    class Device
    {
    public:

        /**
         * @brief   Device instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static Device* CreateDevice();

        /**
         * @brief   Destruction.
         */
        virtual ~Device();

        /**
         * @brief   Queries if a requested feature is supported.
         */
        bool FeatureSupported(DeviceFeature requestedFeature) const { return (bool)(requestedFeature & m_SupportedFeatures); }

        /**
         * @brief   Returns a feature information structure for features supporting feature queries.
         */
        virtual void GetFeatureInfo(DeviceFeature feature, void* pFeatureInfo) = 0;

        /**
         * @brief   Queries the highest supported shader model on the current device.
         */
        ShaderModel MaxSupportedShaderModel() const { return m_MaxSupportedShaderModel; }

        /**
         * @brief   Gets the minimum wave lane count information for the current device.
         */
        uint32_t GetMinWaveLaneCount() const { return m_MinWaveLaneCount; }

        /**
         * @brief   Gets the maximum wave lane count information for the current device.
         */
        uint32_t GetMaxWaveLaneCount() const { return m_MaxWaveLaneCount; }

        /**
         * @brief   Flush all command queues.
         */
        void FlushAllCommandQueues();
        
        /**
         * @brief   Runs the device through frame initialization, and returns a CommandList
         *          to record into for the duration of the frame.
         */
        CommandList* BeginFrame();

        /**
         * @brief   Ends the current frame. Also closes the current frame's CommandList and submits it to the device for execution.
         */
        void EndFrame();

        /**
         * @brief   Submits batch of command lists to the device for execution.
         */
        void SubmitCmdListBatch(std::vector<CommandList*>& cmdLists, CommandQueue queueType, bool isFirstSubmissionOfFrame);

        /**
         * @brief   Gets the current device name.
         */
        const wchar_t* GetDeviceName() const { return m_DeviceName.c_str(); }

        /**
         * @brief   Gets the current driver version (requires AGS be enabled).
         */
        const wchar_t* GetDriverVersion() const { return m_DriverVersion.c_str(); }

        /**
         * @brief   Gets the graphics API in use.
         */
        const wchar_t* GetGraphicsAPI() const { return m_GraphicsAPI.c_str(); }

        /**
         * @brief   Gets a short-form name for the used API
         */
        const wchar_t* GetGraphicsAPIShort() const { return m_GraphicsAPIShort.c_str(); }

        /**
         * @brief   Gets a pretty-fied string to use to represent device name and API
         */
        const wchar_t* GetGraphicsAPIPretty() const { return m_GraphicsAPIPretty.c_str(); }

        /**
         * @brief   Gets the current graphics API version (requires AGS be enabled).
         */
        const wchar_t* GetGraphicsAPIVersion() const { return m_GraphicsAPIVersion.c_str(); }

        /**
         * @brief   Flush the specified queue.
         */
        virtual void FlushQueue(CommandQueue queueType) = 0;

        /**
         * @brief   Query the performance counter frequency on a given queue.
         */
        virtual uint64_t QueryPerformanceFrequency(CommandQueue queueType) = 0;

        /**
         * @brief   Creates a CommanList.
         */
        virtual CommandList* CreateCommandList(const wchar_t* name, CommandQueue queueType) = 0;

        /**
         * @brief   Creates a SwapChain.
         */
        virtual void CreateSwapChain(SwapChain*& pSwapChain, const SwapChainCreationParams& params, CommandQueue queueType) = 0;

        /**
         * @brief   For SwapChain present and signaling (for synchronization).
         */
        virtual uint64_t PresentSwapChain(SwapChain* pSwapChain) = 0;

        /**
         * @brief   Used to signal a command queue.
         */
        virtual uint64_t SignalQueue(CommandQueue queueType) = 0;

        /**
         * @brief   Used to query the last completed signal on the command queue.
         */
        virtual uint64_t QueryLastCompletedValue(CommandQueue queueType) = 0;

        /**
         * @brief   Used to wait until a signal value has been processed.
         */
        virtual void WaitOnQueue(uint64_t waitValue, CommandQueue queueType) const = 0;
        
        /**
         * @brief   Execute the provided command lists, returns a signal ID that can be used to query completion.
         */
        virtual uint64_t ExecuteCommandLists(std::vector<CommandList*>& cmdLists, CommandQueue queuType, bool isFirstSubmissionOfFrame = false, bool isLastSubmissionOfFrame = false) = 0;

        /**
         * @brief   Similar to execute command list, but will wait until completion.
         */
        virtual void ExecuteCommandListsImmediate(std::vector<CommandList*>& cmdLists, CommandQueue queuType) = 0;

        /**
         * @brief   Transition a resource in place (blocking call).
         */
        virtual void ExecuteResourceTransitionImmediate(uint32_t barrierCount, const Barrier* pBarriers) = 0;

        /**
         * @brief   Copy to a texture resource in place (blocking call).
         */
        virtual void ExecuteTextureResourceCopyImmediate(uint32_t resourceCopyCount, const TextureCopyDesc* pCopyDescs) = 0;

        /**
         * @brief   Set VariableShadingRateInfo to use.
         */
        void SetVRSInfo(const VariableShadingRateInfo& variableShadingRateInfo) { m_VariableShadingRateInfo = variableShadingRateInfo; }

        /**
         * @brief   Gets the currently set VariableShadingRateInfo.
         */
        const VariableShadingRateInfo* GetVRSInfo() const { return &m_VariableShadingRateInfo; }

        /**
         * @brief   Sets callback to call when device removed event occurs during presenting backbuffer.
         */
        void RegisterDeviceRemovedCallback(DeviceRemovedCallback callback, void* customData) { m_DeviceRemovedCallback = callback; m_DeviceRemovedCustomData = customData; }

        /**
         * @brief   Gets whether Anti-Lag 2.0 is available on the system.
         */
        bool GetAntiLag2FeatureSupported() const { return m_AntiLag2Supported; }

        /**
         * @brief   Gets the current state of Anti-Lag 2.0.
         */
        bool GetAntiLag2Enabled() const { return m_AntiLag2Enabled; }

        /**
         * @brief   Sets the current state of Anti-Lag 2.0.
         */
        void SetAntiLag2Enabled(bool enable) { m_AntiLag2Enabled = enable; }

        /**
         * @brief   Sets the framerate limiter for Anti-Lag 2.0. Zero disables the limiter.
         */
        void SetAntiLag2FramerateLimiter(uint32_t maxFPS) { m_AntiLag2FramerateLimiter = maxFPS; }

        /**
         * @brief   Update Anti-Lag 2.0 state.
         */
        virtual void UpdateAntiLag2() = 0;

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        virtual const DeviceInternal* GetImpl() const = 0;
        virtual DeviceInternal* GetImpl() = 0;

    protected:
        
        Device();
        void DeleteCommandListAsync(void* pInFlightGPUInfo);

    private:        
        
        // No copy, No move
        NO_COPY(Device)
        NO_MOVE(Device)

    protected:
        
        DeviceFeature    m_SupportedFeatures = DeviceFeature::None;
        // VK doesn't support these queries, set to min without RT and 32 waves for now
        ShaderModel m_MaxSupportedShaderModel = ShaderModel::SM6_2; 
        uint32_t    m_MinWaveLaneCount        = 32;
        uint32_t    m_MaxWaveLaneCount        = 32;

        VariableShadingRateInfo m_VariableShadingRateInfo;
        
        // Graphics command lists
        CommandList*    m_pActiveCommandList = nullptr;
        // Todo: Add async compute command lists

        std::wstring    m_DeviceName = L"Not Set";
        std::wstring    m_DriverVersion = L"Not Set";
        std::wstring    m_GraphicsAPI = L"Not Set";
        std::wstring    m_GraphicsAPIShort = L"Not Set";
        std::wstring    m_GraphicsAPIPretty  = L"Not Set";
        std::wstring    m_GraphicsAPIVersion = L"Not Set";

        // Callback for receiving device removed error while presenting the frame
        DeviceRemovedCallback m_DeviceRemovedCallback = nullptr;
        void*                 m_DeviceRemovedCustomData = nullptr;

        bool     m_AntiLag2Supported        = false;
        bool     m_AntiLag2Enabled          = false;
        uint32_t m_AntiLag2FramerateLimiter = 0;
    };

} // namespace cauldron
