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

#include "FrameInterpolationSwapchainVK_Helpers.h"

#ifdef _WIN32
#include <dwmapi.h>
#endif  // #ifdef _WIN32

void waitForPerformanceCount(const int64_t targetCount)
{
    int64_t currentCount = 0;
    do
    {
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentCount));
    } while (currentCount < targetCount);
}

VkResult VulkanQueue::submit(VkCommandBuffer commandBuffer, SubmissionSemaphores& semaphoresToWait, SubmissionSemaphores& semaphoresToSignal, VkFence fence)
{
    VkSubmitInfo submitInfo         = {};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = semaphoresToWait.count;
    submitInfo.pWaitSemaphores      = semaphoresToWait.semaphores;
    submitInfo.pWaitDstStageMask    = semaphoresToWait.waitStages;
    submitInfo.signalSemaphoreCount = semaphoresToSignal.count;
    submitInfo.pSignalSemaphores    = semaphoresToSignal.semaphores;

    if (commandBuffer == VK_NULL_HANDLE)
    {
        submitInfo.commandBufferCount = 0;
        submitInfo.pCommandBuffers    = nullptr;
    }
    else
    {
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;
    }

    VkTimelineSemaphoreSubmitInfo timelineSemaphoreSubmitInfo = {};
    if (semaphoresToSignal.count == 0 && semaphoresToWait.count == 0)
    {
        submitInfo.pNext = nullptr;
    }
    else
    {
        timelineSemaphoreSubmitInfo.sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineSemaphoreSubmitInfo.pNext                     = nullptr;
        timelineSemaphoreSubmitInfo.waitSemaphoreValueCount   = semaphoresToWait.count;
        timelineSemaphoreSubmitInfo.pWaitSemaphoreValues      = semaphoresToWait.values;
        timelineSemaphoreSubmitInfo.signalSemaphoreValueCount = semaphoresToSignal.count;
        timelineSemaphoreSubmitInfo.pSignalSemaphoreValues    = semaphoresToSignal.values;

        submitInfo.pNext = &timelineSemaphoreSubmitInfo;
    }

    VkResult res = VK_SUCCESS;
    if (submitFunc != nullptr)
        res = submitFunc(1, &submitInfo, fence);
    else
        res = vkQueueSubmit(queue, 1, &submitInfo, fence);

    semaphoresToWait.reset();
    semaphoresToSignal.reset();

    return res;
}

VkResult VulkanQueue::submit(VkCommandBuffer commandBuffer, VkSemaphore timelineSemaphore, uint64_t signalValue)
{
    SubmissionSemaphores semaphoresToWait;
    SubmissionSemaphores semaphoresToSignal;

    semaphoresToSignal.add(timelineSemaphore, signalValue);

    return submit(commandBuffer, semaphoresToWait, semaphoresToSignal);
}

