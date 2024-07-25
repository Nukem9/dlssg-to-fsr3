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

#if defined(_VK)

#include "render/vk/commandlist_vk.h"
#include "render/vk/device_vk.h"
#include "render/vk/profiler_vk.h"

#include "core/framework.h"
#include "misc/assert.h"

namespace cauldron
{
    Profiler* Profiler::CreateProfiler(bool enableCPUProfiling /* = true */, bool enableGPUProfiling /* = true */)
    {
        return new ProfilerInternal(enableCPUProfiling, enableGPUProfiling);
    }

    ProfilerInternal::ProfilerInternal(bool enableCPUProfiling, bool enableGPUProfiling) : 
        Profiler(enableCPUProfiling, enableGPUProfiling)
    {
        if (m_GPUProfilingEnabled)
        {
            const uint32_t backBufferCount = static_cast<uint32_t>(GetConfig()->BackBufferCount);

            VkQueryPoolCreateInfo queryPoolCreateInfo = {};
            queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            queryPoolCreateInfo.pNext = nullptr;
            queryPoolCreateInfo.flags = 0;
            queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryPoolCreateInfo.queryCount = s_MAX_TIMESTAMPS_PER_FRAME * backBufferCount;
            queryPoolCreateInfo.pipelineStatistics = 0;

            DeviceInternal* pDevice = GetDevice()->GetImpl();
            VkResult res = vkCreateQueryPool(pDevice->VKDevice(), &queryPoolCreateInfo, nullptr, &m_QueryPool);
            CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS && m_QueryPool != VK_NULL_HANDLE, L"Unable to create the query pool");

            pDevice->SetResourceName(VK_OBJECT_TYPE_QUERY_POOL, (uint64_t)m_QueryPool, "Query Pool");
        }
    }

    ProfilerInternal::~ProfilerInternal()
    {
        if (m_QueryPool != VK_NULL_HANDLE)
            vkDestroyQueryPool(GetDevice()->GetImpl()->VKDevice(), m_QueryPool, nullptr);
    }

    void ProfilerInternal::BeginEvent(CommandList* pCmdList, const wchar_t* label)
    {
        if (pCmdList != nullptr)
        {
            std::string strLabel = WStringToString(label);
            VkDebugUtilsLabelEXT debugLabel = {};
            debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            debugLabel.pNext = nullptr;
            debugLabel.pLabelName = strLabel.c_str();
            // not to saturated red
            debugLabel.color[0] = 1.0f;
            debugLabel.color[1] = 0.14f;
            debugLabel.color[2] = 0.14f;
            debugLabel.color[3] = 1.0f;
            GetDevice()->GetImpl()->GetCmdBeginDebugUtilsLabel()(pCmdList->GetImpl()->VKCmdBuffer(), &debugLabel);
        }
    }

    void ProfilerInternal::EndEvent(CommandList* pCmdList)
    {
        if (pCmdList != nullptr)
            GetDevice()->GetImpl()->GetCmdEndDebugUtilsLabel()(pCmdList->GetImpl()->VKCmdBuffer());
    }

    bool ProfilerInternal::InsertTimeStamp(CommandList* pCmdList)
    {
        if (pCmdList != nullptr)
        {
            CauldronAssert(ASSERT_WARNING, m_TimeStampCount < s_MAX_TIMESTAMPS_PER_FRAME, L"Too many timestamps");
            if (m_TimeStampCount < s_MAX_TIMESTAMPS_PER_FRAME)
            {
                uint32_t query = (m_CurrentFrame * s_MAX_TIMESTAMPS_PER_FRAME) + m_TimeStampCount;
                vkCmdWriteTimestamp(pCmdList->GetImpl()->VKCmdBuffer(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPool, query);
                ++m_TimeStampCount;

                return true;
            }
        }

        return true;
    }

    uint32_t ProfilerInternal::RetrieveTimeStamps(CommandList* pCmdList, uint64_t* pQueries, size_t maxCount, uint32_t numTimeStamps)
    {
        CauldronAssert(ASSERT_CRITICAL, pQueries != nullptr, L"Invalid queries buffer");
        bool success = false;
        if (numTimeStamps > 0)
        {
            VkResult res = vkGetQueryPoolResults(GetDevice()->GetImpl()->VKDevice(),
                m_QueryPool,
                m_CurrentFrame * s_MAX_TIMESTAMPS_PER_FRAME,
                numTimeStamps,
                sizeof(uint64_t) * maxCount,
                pQueries,
                sizeof(uint64_t),
                VK_QUERY_RESULT_64_BIT);

            success = (res == VK_SUCCESS);
        }

        vkCmdResetQueryPool(pCmdList->GetImpl()->VKCmdBuffer(), m_QueryPool, m_CurrentFrame * s_MAX_TIMESTAMPS_PER_FRAME, s_MAX_TIMESTAMPS_PER_FRAME);

        return success ? numTimeStamps : 0;
    }

} // namespace cauldron

#endif // #if defined(_VK)
