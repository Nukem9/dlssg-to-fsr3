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

#include <chrono>
#include <string>
#include <vector>
#include <limits>

namespace cauldron
{
    /// Everything in the engine is nanosecond based
    ///
    /// @ingroup CauldronRender
    constexpr uint64_t g_NanosecondsPerSecond = 1000000000;

    class CommandList;

    
    /// Structure holding the information of a timing capture
    ///
    /// @ingroup CauldronRender
    struct TimingInfo
    {
        std::wstring Label;                     ///< Label for timing marker information.
        std::chrono::nanoseconds    StartTime;  ///< Start time for the marker (in nanoseconds).
        std::chrono::nanoseconds    EndTime;    ///< End time for the marker (in nanoseconds).

        TimingInfo() = default;

        TimingInfo(const wchar_t* name)
            : Label(name)
        {
        }

        /// Get the duration in nanoseconds (can be converted later if needed)
        ///
        std::chrono::nanoseconds GetDuration() const
        {
            return (EndTime - StartTime);
        }
    };

    /// Profiling capture identification structure
    ///
    /// @ingroup CauldronRender
    struct ProfileCapture
    {
        uint32_t CPUIndex = UINT32_MAX;     ///< The index of the CPU timing information.
        uint32_t GPUIndex = UINT32_MAX;     ///< The index of the GPU timing information.
    };

    /**
     * @class Profiler
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> representation of the CPU/GPU profiler.
     * NOTE: This class is not thread safe.
     *
     * @ingroup CauldronRender
     */
    class Profiler
    {
    public:

        /// Maximum number of timestamps that can be captured per frame.
        /// 
        static const uint32_t s_MAX_TIMESTAMPS_PER_FRAME = 256;

        /**
         * @brief   Profiler instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static Profiler* CreateProfiler(bool enableCPUProfiling = true, bool enableGPUProfiling = true);

        /**
         * @brief   Destruction with default behavior.
         */
        virtual ~Profiler() = default;

        /**
         * @brief   Returns true if CPU profiling is enabled.
         */
        bool IsCPUProfilingEnabled() const { return m_CPUProfilingEnabled; }

        /**
         * @brief   Returns true if GPU profiling is enabled..
         */
        bool IsGPUProfilingEnabled() const { return m_GPUProfilingEnabled; }

        /**
         * @brief   Returns the vector of CPU timing information.
         */
        const std::vector<TimingInfo>& GetCPUTimings() const;

        /**
         * @brief   Returns the vector of GPU timing information.
         */
        const std::vector<TimingInfo>& GetGPUTimings() const;

        /**
         * @brief   Returns the CPU tick count for the frame.
         */
        int64_t GetCPUFrameTicks() const { return m_LatestCPUFrameCount; }

        /**
         * @brief   Returns the GPU tick count for the frame.
         */
        int64_t GetGPUFrameTicks() const { return m_LatestGPUFrameCount; }

        /**
         * @brief   Starts a new CPU timing frame and retrieves the CPU timings for past frames.
         */
        void BeginCPUFrame();

        /**
         * @brief   Starts a new GPU timing frame and retrieves the GPU timestamps for past frames.
         */
        void BeginGPUFrame(CommandList* pCmdList);

        /**
         * @brief   Ends the current frame capture.
         */
        void EndFrame(CommandList* pCmdList);

        /**
         * @brief   Begins a capture on both CPU and GPU.
         */
        ProfileCapture Begin(CommandList* pCmdList, const wchar_t* label);

        /**
         * @brief   Ends a capture on both CPU and GPU.
         */
        void End(CommandList* pCmdList, ProfileCapture capture);

        /**
         * @brief   Begins a capture on CPU only.
         */
        ProfileCapture BeginCPU(const wchar_t* label);

        /**
         * @brief   Ends a capture on CPU only.
         */
        void EndCPU(ProfileCapture capture);

        /**
         * @brief   Begins a capture on GPU only.
         */
        ProfileCapture BeginGPU(CommandList* pCmdList, const wchar_t* label);

        /**
         * @brief   Ends a capture on GPU only.
         */
        void EndGPU(CommandList* pCmdList, ProfileCapture capture);

    protected:
        Profiler(bool enableCPUProfiling, bool enableGPUProfiling);
        virtual void EndFrameGPU(CommandList* pCmdList);

        virtual void BeginEvent(CommandList* pCmdList, const wchar_t* label) = 0;
        virtual void EndEvent(CommandList* pCmdList) = 0;
        virtual bool InsertTimeStamp(CommandList* pCmdList) = 0;

        // fills the buffer of timestamps and returns the number of written timestamps
        virtual uint32_t RetrieveTimeStamps(CommandList* pCmdList, uint64_t* pQueries, size_t maxCount, uint32_t numTimeStamps) = 0;

    private:
        NO_COPY(Profiler)
        NO_MOVE(Profiler)
        Profiler() = delete;

        void CollectCPUTimings();
        void CollectGPUTimings(CommandList* pCmdList);

        // Internal members (general)
    protected:
        bool     m_CPUProfilingEnabled = true;
        bool     m_GPUProfilingEnabled = true;
        uint32_t m_CurrentFrame        = 0;
        uint32_t m_TimeStampCount      = 0;

        // Timing information for GPU/CPU frames
        std::vector<std::vector<TimingInfo>> m_CPUTimings;
        std::vector<std::vector<TimingInfo>> m_GPUTimings;

        // Members for CPU/GPU timings
    private:
        std::vector<TimingInfo>     m_CurrentCPUTimings;
        int64_t                     m_LatestCPUFrameCount = 0;
        int64_t                     m_LatestGPUFrameCount = 0;


    private:
        // Internal GPU timing tracking
        struct GPUTimingInfo
        {
            std::wstring Label;
            uint64_t     StartIndex = 0;
            uint64_t     EndIndex = 0;
        };

        std::vector<std::vector<GPUTimingInfo>> m_GPUTimingInfos;
        std::vector<uint32_t>                   m_GPUTimeStampCounts;

    };

    /**
     * @class ScopedProfileCaptureBase
     *
     * Convenience calss to perform scoped profiling captures.
     *
     * @ingroup CauldronRender
     */
    class ScopedProfileCaptureBase
    {
    protected:
        ScopedProfileCaptureBase() = default;
        virtual ~ScopedProfileCaptureBase() = default;

        ProfileCapture m_Capture;
    };

    /**
     * @class CPUScopedProfileCapture
     *
     * Convenience class to perform scoped CPU profiling captures.
     *
     * @ingroup CauldronRender
     */
    class CPUScopedProfileCapture : public ScopedProfileCaptureBase
    {
    public:
        CPUScopedProfileCapture(const wchar_t* label);
        virtual ~CPUScopedProfileCapture();
    };

    /**
     * @class GPUScopedProfileCapture
     *
     * Convenience class to perform scoped GPU profiling captures.
     *
     * @ingroup CauldronRender
     */
    class GPUScopedProfileCapture : public ScopedProfileCaptureBase
    {
    public:
        GPUScopedProfileCapture(CommandList* pCmdList, const wchar_t* label);
        virtual ~GPUScopedProfileCapture();

    private:
        CommandList* m_pCommandList;
    };

    /**
     * @class ScopedProfileCapture
     *
     * Convenience class to perform scoped GPU and CPU profiling captures.
     *
     * @ingroup CauldronRender
     */
    class ScopedProfileCapture : public ScopedProfileCaptureBase
    {
    public:
        ScopedProfileCapture(CommandList* pCmdList, const wchar_t* label);
        virtual ~ScopedProfileCapture();

    private:
        CommandList* m_pCommandList;
    };

} // namespace cauldron
