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

#include "render/profiler.h"
#include "render/device.h"
#include "core/framework.h"
#include "misc/assert.h"
#include "render/commandlist.h"

namespace cauldron
{
    /////////////////////////////////////////////////////////////////////////
    // Profiler
    /////////////////////////////////////////////////////////////////////////
    Profiler::Profiler(bool enableCPUProfiling, bool enableGPUProfiling)
        : m_CPUProfilingEnabled(enableCPUProfiling)
        , m_GPUProfilingEnabled(enableGPUProfiling)
    {
        const uint32_t backBufferCount = static_cast<uint32_t>(GetConfig()->BackBufferCount);

        m_CPUTimings.resize(backBufferCount);
        m_GPUTimings.resize(backBufferCount);

        if (m_GPUProfilingEnabled)
        {
            m_GPUTimingInfos.resize(backBufferCount);
            m_GPUTimeStampCounts.resize(backBufferCount);
        }
    }

    const std::vector<TimingInfo>& Profiler::GetCPUTimings() const
    {
        // When fetching timings, we always want those from the last frame (current frame timings incomplete)
        uint32_t frameID = (m_CurrentFrame == 0) ? GetConfig()->BackBufferCount - 1 : m_CurrentFrame - 1;
        return m_CPUTimings[frameID];
    }

    const std::vector<TimingInfo>& Profiler::GetGPUTimings() const
    {
        // When fetching timings, we always want those from the last frame (current frame timings incomplete)
        uint32_t frameID = (m_CurrentFrame == 0) ? GetConfig()->BackBufferCount - 1 : m_CurrentFrame - 1;
        return m_GPUTimings[frameID];
    }

    ProfileCapture Profiler::Begin(CommandList* pCmdList, const wchar_t* label)
    {
        ProfileCapture capture;
        capture.CPUIndex = BeginCPU(label).CPUIndex;
        capture.GPUIndex = BeginGPU(pCmdList, label).GPUIndex;
        return capture;
    }

    void Profiler::End(CommandList* pCmdList, ProfileCapture capture)
    {
        EndCPU(capture);
        EndGPU(pCmdList, capture);
    }

    ProfileCapture Profiler::BeginCPU(const wchar_t* label)
    {
        ProfileCapture capture;
        if (m_CPUProfilingEnabled)
        {
            TimingInfo t;
            t.Label = label;
            t.StartTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());

            m_CurrentCPUTimings.push_back(std::move(t));
            capture.CPUIndex = static_cast<uint32_t>(m_CurrentCPUTimings.size()) - 1;
        }
        return capture;
    }

    void Profiler::EndCPU(ProfileCapture capture)
    {
        if (m_CPUProfilingEnabled)
        {
            CauldronAssert(ASSERT_WARNING, m_CurrentCPUTimings.size() > capture.CPUIndex, L"There is no CPU timing to end");
            m_CurrentCPUTimings[capture.CPUIndex].EndTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
        }
    }

    ProfileCapture Profiler::BeginGPU(CommandList* pCmdList, const wchar_t* label)
    {
        ProfileCapture capture;
        if (m_GPUProfilingEnabled)
        {
            BeginEvent(pCmdList, label);
            if (InsertTimeStamp(pCmdList))
            {
                std::vector<GPUTimingInfo>& infos = m_GPUTimingInfos[m_CurrentFrame];
                infos.push_back({ label, m_TimeStampCount - 1, 0 });
                capture.GPUIndex = static_cast<uint32_t>(infos.size()) - 1;
            }
        }
        return capture;
    }

    void Profiler::EndGPU(CommandList* pCmdList, ProfileCapture capture)
    {
        if (m_GPUProfilingEnabled)
        {
            EndEvent(pCmdList);
            CauldronAssert(ASSERT_WARNING, m_GPUTimingInfos[m_CurrentFrame].size() > capture.GPUIndex, L"There is no GPU timing to end");
            if (InsertTimeStamp(pCmdList))
            {
                m_GPUTimingInfos[m_CurrentFrame][capture.GPUIndex].EndIndex = m_TimeStampCount - 1;
            }
        }
    }

    void Profiler::BeginCPUFrame()
    {
        // Update the frame for which we are gathering data
        m_CurrentFrame = (m_CurrentFrame + 1) % GetConfig()->BackBufferCount;

        // Save the CPU timings of the previous frame in the correct vector
        if (m_CPUProfilingEnabled)
            CollectCPUTimings();
    }

    void Profiler::BeginGPUFrame(CommandList* pCmdList)
    {
        // Save the GPU timing of the previous frame using the same command list
        if (m_GPUProfilingEnabled)
            CollectGPUTimings(pCmdList);
    }

    void Profiler::EndFrame(CommandList* pCmdList)
    {
        // Nothing to do to end the CPU frame
        if (m_GPUProfilingEnabled)
            EndFrameGPU(pCmdList);
    }

    void Profiler::EndFrameGPU(CommandList* pCmdList)
    {
        // Save the number of events of this frame
        m_GPUTimeStampCounts[m_CurrentFrame] = m_TimeStampCount;

        // Reset the number of timestamps
        m_TimeStampCount = 0;
    }

    void Profiler::CollectCPUTimings()
    {
        // By the time we collect CPU timings, the frame ID has changed, so we need to use the last frame's ID
        uint32_t frameID = (m_CurrentFrame == 0) ? GetConfig()->BackBufferCount - 1 : m_CurrentFrame - 1;

        // Populate the frame timings from the current (last frame) capture timings
        std::vector<TimingInfo>& latestCPUTimings = m_CPUTimings[frameID];
        latestCPUTimings.clear();
        latestCPUTimings.swap(m_CurrentCPUTimings);
        m_CurrentCPUTimings.clear();

        // Calculate frame tick
        m_LatestCPUFrameCount = 0;
        if (latestCPUTimings.size())
        {
            m_LatestCPUFrameCount = (latestCPUTimings[latestCPUTimings.size() - 1].EndTime - latestCPUTimings[0].StartTime).count();
        }
    }

    void Profiler::CollectGPUTimings(CommandList* pCmdList)
    {
        // By the time we collect GPU timings, the frame ID has changed. Unlike the CPU timings, GPU timings
        // have to be synchronized with the GPU queue. At this point, we should have waited for the swapchain
        // to be available, which means the timings are resolved for the frame that last used this frame ID.
        uint32_t frameID = m_CurrentFrame;

        std::vector<TimingInfo>& latestGPUTimings = m_GPUTimings[frameID];

        // Fetch results from internal GPU timings for the last frame
        std::vector<GPUTimingInfo>& latestGPUTimingInfo = m_GPUTimingInfos[frameID];
        const uint32_t numTimeStamps = m_GPUTimeStampCounts[frameID];
        uint64_t queries[s_MAX_TIMESTAMPS_PER_FRAME];
        uint32_t retrievedTimeStamps = RetrieveTimeStamps(pCmdList, queries, s_MAX_TIMESTAMPS_PER_FRAME, numTimeStamps);

        // Get the GPU counter frequency for the graphics queue (in order to do conversion to chrono timer)
        // If tick frequency is less than 1000000000 (Nanoseconds), scale all timings to nanoseconds
        static uint64_t s_TickFrequency = GetDevice()->QueryPerformanceFrequency(CommandQueue::Graphics);
        static uint64_t s_TickMultiplier = g_NanosecondsPerSecond / s_TickFrequency;
        CauldronAssert(ASSERT_ERROR, s_TickFrequency <= g_NanosecondsPerSecond, L"Profiler currently only supports counters of nanosecond granularity or less. Timings will be inaccurate.");

        // Clear out old entries
        latestGPUTimings.clear();

        if (numTimeStamps > 0)
        {
            if (retrievedTimeStamps == numTimeStamps)
            {
                const size_t numTimings = latestGPUTimingInfo.size();
                latestGPUTimings.reserve(numTimings);

                for (auto it = latestGPUTimingInfo.begin(); it != latestGPUTimingInfo.end(); ++it)
                {
                    TimingInfo timingInfo;
                    timingInfo.Label = std::move(it->Label);
                    timingInfo.StartTime = std::chrono::nanoseconds(queries[it->StartIndex] * s_TickMultiplier);
                    timingInfo.EndTime = std::chrono::nanoseconds(queries[it->EndIndex] * s_TickMultiplier);

                    // Emplace the timing information now that it's been properly converted
                    latestGPUTimings.emplace_back(timingInfo);
                }
            }
            else
            {
                TimingInfo timingInfo;
                timingInfo.Label = L"GPU counters are invalid";
                timingInfo.StartTime = std::chrono::nanoseconds(0);
                timingInfo.EndTime = std::chrono::nanoseconds(0);
                latestGPUTimings.reserve(1);
                latestGPUTimings.emplace_back(timingInfo);
            }
        }

        // Calculate frame tick
        m_LatestGPUFrameCount = 0;
        if (latestGPUTimings.size())
        {
            m_LatestGPUFrameCount = (latestGPUTimings[latestGPUTimings.size() - 1].EndTime - latestGPUTimings[0].StartTime).count();
        }

        m_GPUTimingInfos[frameID].clear();
        m_GPUTimeStampCounts[frameID] = 0;
    }

    /////////////////////////////////////////////////////////////////////////
    // Scoped captures
    /////////////////////////////////////////////////////////////////////////
    CPUScopedProfileCapture::CPUScopedProfileCapture(const wchar_t* label)
        : ScopedProfileCaptureBase()
    {
        m_Capture = GetProfiler()->BeginCPU(label);
    }
    CPUScopedProfileCapture::~CPUScopedProfileCapture()
    {
        GetProfiler()->EndCPU(m_Capture);
    }

    GPUScopedProfileCapture::GPUScopedProfileCapture(CommandList* pCmdList, const wchar_t* label)
        : ScopedProfileCaptureBase()
        , m_pCommandList(pCmdList)
    {
        m_Capture = GetProfiler()->BeginGPU(pCmdList, label);
    }
    GPUScopedProfileCapture::~GPUScopedProfileCapture()
    {
        GetProfiler()->EndGPU(m_pCommandList, m_Capture);
    }

    ScopedProfileCapture::ScopedProfileCapture(CommandList* pCmdList, const wchar_t* label)
        : ScopedProfileCaptureBase()
        , m_pCommandList(pCmdList)
    {
        m_Capture = GetProfiler()->Begin(pCmdList, label);
    }
    ScopedProfileCapture::~ScopedProfileCapture()
    {
        GetProfiler()->End(m_pCommandList, m_Capture);
    }
} // namespace cauldron
