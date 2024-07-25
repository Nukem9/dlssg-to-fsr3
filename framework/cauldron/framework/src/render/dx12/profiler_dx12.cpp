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

#if defined(_DX12)
#include "render/dx12/commandlist_dx12.h"
#include "render/dx12/device_dx12.h"
#include "render/dx12/profiler_dx12.h"

#include "core/framework.h"
#include "misc/assert.h"

// PIX instrumentation is only enabled if one of the preprocessor symbols USE_PIX, DBG, _DEBUG, PROFILE, or PROFILE_BUILD is defined.
// ref: https://devblogs.microsoft.com/pix/winpixeventruntime/
#ifndef USE_PIX
#define USE_PIX // Should enable it at anytime, as we already have a runtime switch for this purpose
#endif // #ifndef USE_PIX
#include "pix/pix3.h"

#include "dxheaders/include/directx/d3dx12.h"

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

            D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
            queryHeapDesc.Count = s_MAX_TIMESTAMPS_PER_FRAME * backBufferCount;
            queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
            queryHeapDesc.NodeMask = 0;
            CauldronThrowOnFail(GetDevice()->GetImpl()->DX12Device()->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_pQueryHeap)));

            CauldronThrowOnFail(
                GetDevice()->GetImpl()->DX12Device()->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
                    D3D12_HEAP_FLAG_NONE,
                    &CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint64_t) * backBufferCount * s_MAX_TIMESTAMPS_PER_FRAME),
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    nullptr,
                    IID_PPV_ARGS(&m_pBuffer))
            );
        }
    }

    void ProfilerInternal::BeginEvent(CommandList* pCmdList, const wchar_t* label)
    {
        if (pCmdList != nullptr)
        {
            PIXBeginEvent(pCmdList->GetImpl()->DX12CmdList(), PIX_COLOR(255, 36, 36), label);

            // Use AGS markers too if it's enabled
            AGSContext* pContext = const_cast<AGSContext*>(GetDevice()->GetImpl()->GetAGSContext());
            if (pContext)
            {
                agsDriverExtensionsDX12_PushMarker(pContext, pCmdList->GetImpl()->DX12CmdList(), WStringToString(label).c_str());
            }
        }
    }

    void ProfilerInternal::EndEvent(CommandList* pCmdList)
    {
        if (pCmdList != nullptr)
        {
            PIXEndEvent(pCmdList->GetImpl()->DX12CmdList());

            // Use AGS markers too if it's enabled
            AGSContext* pContext = const_cast<AGSContext*>(GetDevice()->GetImpl()->GetAGSContext());
            if (pContext)
            {
                agsDriverExtensionsDX12_PopMarker(pContext, pCmdList->GetImpl()->DX12CmdList());
            }
        }
    }

    bool ProfilerInternal::InsertTimeStamp(CommandList* pCmdList)
    {
        if (pCmdList != nullptr)
        {
            CauldronAssert(ASSERT_WARNING, m_TimeStampCount < s_MAX_TIMESTAMPS_PER_FRAME, L"Too many timestamps");
            if (m_TimeStampCount < s_MAX_TIMESTAMPS_PER_FRAME)
            {
                UINT query = (m_CurrentFrame * s_MAX_TIMESTAMPS_PER_FRAME) + m_TimeStampCount;
                pCmdList->GetImpl()->DX12CmdList()->EndQuery(m_pQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, query);
                ++m_TimeStampCount;

                return true;
            }
        }

        return true;
    }

    uint32_t ProfilerInternal::RetrieveTimeStamps(CommandList* pCmdList, uint64_t* pQueries, size_t maxCount, uint32_t numTimeStamps)
    {
        CauldronAssert(ASSERT_CRITICAL, pQueries != nullptr, L"Invalid queries buffer");
        if (numTimeStamps > 0)
        {
            uint32_t frameID = m_CurrentFrame;
            D3D12_RANGE range;
            range.Begin = frameID * s_MAX_TIMESTAMPS_PER_FRAME;
            range.End   = frameID * s_MAX_TIMESTAMPS_PER_FRAME + numTimeStamps * sizeof(UINT64);

            UINT64* pTimeStampBuffer = NULL;
            HRESULT res = m_pBuffer->Map(0, &range, reinterpret_cast<void**>(&pTimeStampBuffer));

            if (res == S_OK)
            {
                memcpy(pQueries, pTimeStampBuffer + range.Begin, numTimeStamps * sizeof(UINT64));
                
                CD3DX12_RANGE writtenRange(0, 0);
                m_pBuffer->Unmap(0, &writtenRange);
                
                return numTimeStamps;
            }
        }

        return 0;

    }

    void ProfilerInternal::EndFrameGPU(CommandList* pCmdList)
    {
        pCmdList->GetImpl()->DX12CmdList()->ResolveQueryData(
            m_pQueryHeap.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            m_CurrentFrame * s_MAX_TIMESTAMPS_PER_FRAME,
            static_cast<UINT>(m_TimeStampCount),
            m_pBuffer.Get(),
            m_CurrentFrame * s_MAX_TIMESTAMPS_PER_FRAME * sizeof(UINT64));

        Profiler::EndFrameGPU(pCmdList);
    }

} // namespace cauldron

#endif // #if defined(_DX12)
