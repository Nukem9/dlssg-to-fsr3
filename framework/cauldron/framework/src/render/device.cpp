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

#include "render/device.h"
#include "render/swapchain.h"
#include "core/framework.h"

namespace cauldron
{
    Device::Device()
    {
        
    }

    Device::~Device()
    {

    }

    // Flush all device queues
    void Device::FlushAllCommandQueues()
    {
        // Flush all queues
        FlushQueue(CommandQueue::Graphics);
        FlushQueue(CommandQueue::Compute);
        FlushQueue(CommandQueue::Copy);
    }

    CommandList* Device::BeginFrame()
    {
        // Clear the active command lists for this frame
        uint8_t currentBufferIndex = GetFramework()->GetSwapChain()->GetBackBufferIndex();
        m_pActiveCommandList = CreateCommandList(L"DeviceGraphicsCmdList", CommandQueue::Graphics);

        // Set all resource view heaps for the frame
        SetAllResourceViewHeaps(m_pActiveCommandList);

        return m_pActiveCommandList;
    }

    void Device::EndFrame()
    {
        // Close the command list for this frame
        CloseCmdList(m_pActiveCommandList);
        
        // Execute all submission command lists (only 1 for now)
        std::vector<CommandList*> cmdLists;
        cmdLists.push_back(std::move(m_pActiveCommandList));
        uint64_t signalValue = ExecuteCommandLists(cmdLists, CommandQueue::Graphics, false, true);

        // Asynchronously delete the active command list in the background once it's cleared the graphics queue
        GPUExecutionPacket* pInflightPacket = new GPUExecutionPacket(cmdLists, signalValue);
        GetTaskManager()->AddTask(Task(std::bind(&Device::DeleteCommandListAsync, this, std::placeholders::_1), reinterpret_cast<void*>(pInflightPacket)));

        // Make sure no one tries to do anything with this
        m_pActiveCommandList = nullptr;
    }

    void Device::SubmitCmdListBatch(std::vector<CommandList*>& cmdLists, CommandQueue queueType, bool isFirstSubmissionOfFrame)
    {
        uint64_t signalValue = ExecuteCommandLists(cmdLists, queueType, isFirstSubmissionOfFrame, false);

        // Asynchronously delete the active command list in the background once it's cleared the graphics queue
        GPUExecutionPacket* pInflightPacket = new GPUExecutionPacket(cmdLists, signalValue);
        GetTaskManager()->AddTask(Task(std::bind(&Device::DeleteCommandListAsync, this, std::placeholders::_1), reinterpret_cast<void*>(pInflightPacket)));
    }

    void Device::DeleteCommandListAsync(void* pInFlightGPUInfo)
    {
        GPUExecutionPacket* pInflightPacket = static_cast<GPUExecutionPacket*>(pInFlightGPUInfo);

        // Wait until the command lists are processed
        WaitOnQueue(pInflightPacket->CompletionID, CommandQueue::Graphics);

        // Delete them to release the allocators
        for (auto cmdListIter = pInflightPacket->CmdLists.begin(); cmdListIter != pInflightPacket->CmdLists.end(); ++cmdListIter)
            delete* cmdListIter;
        pInflightPacket->CmdLists.clear();
        delete pInflightPacket;
    }

} // namespace cauldron
