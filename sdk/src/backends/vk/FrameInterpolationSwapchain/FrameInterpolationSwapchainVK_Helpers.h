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

#include <vulkan/vulkan.h>

#include <FidelityFX/host/ffx_assert.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>

#include <Windows.h>
#include <synchapi.h>


void waitForPerformanceCount(const int64_t targetCount);


struct SubmissionSemaphores
{
    static const uint32_t Capacity = 6;

    VkSemaphore          semaphores[Capacity];
    uint64_t             values[Capacity];
    VkPipelineStageFlags waitStages[Capacity];
    uint32_t             count;

    SubmissionSemaphores()
    {
        reset();
    }

    bool isEmpty()
    {
        return count == 0;
    }

    void reset()
    {
        count = 0;
    }

    void add(VkSemaphore semaphore, uint64_t value = 0)
    {
        if (semaphore != VK_NULL_HANDLE)
        {
            FFX_ASSERT_MESSAGE(count < Capacity, "[FrameInterpolationSwapchainVK] SubmissionSemaphores capacity exceeded. Please increase it.");
            if (count < Capacity)
            {
                semaphores[count] = semaphore;
                values[count]     = value;
                waitStages[count] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                ++count;
            }
        }
    }
};

struct VulkanQueue : public VkQueueInfoFFX
{
    void operator=(const VkQueueInfoFFX& info)
    {
        queue       = info.queue;
        familyIndex = info.familyIndex;
        submitFunc  = info.submitFunc;
    } 

    void reset()
    {
        queue       = VK_NULL_HANDLE;
        familyIndex = 0;
        submitFunc  = nullptr;
    }

    VkResult submit(VkCommandBuffer       commandBuffer,
                    SubmissionSemaphores& semaphoresToWait,
                    SubmissionSemaphores& semaphoresToSignal,
                    VkFence               fence = VK_NULL_HANDLE);
    VkResult submit(VkCommandBuffer commandBuffer, VkSemaphore timelineSemaphore, uint64_t signalValue);
};

class VkCommands
{
    VkDevice        device                  = VK_NULL_HANDLE;
    VulkanQueue     queue                   = {};
    VkCommandPool   commandPool             = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer           = VK_NULL_HANDLE;
    VkSemaphore     semaphore               = VK_NULL_HANDLE;
    uint64_t        availableSemaphoreValue = 0;

public:
    void release(const VkAllocationCallbacks* pAllocator = nullptr)
    {
        if (device != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            vkDestroyCommandPool(device, commandPool, pAllocator);
            vkDestroySemaphore(device, semaphore, pAllocator);
        }

        device        = VK_NULL_HANDLE;
        commandBuffer = VK_NULL_HANDLE;
        commandPool   = VK_NULL_HANDLE;
        semaphore     = VK_NULL_HANDLE;
    }

public:
    ~VkCommands()
    {
        release();
    }

    bool initiated()
    {
        return commandPool != VK_NULL_HANDLE;
    }

    bool verify(VkDevice inputDevice, uint32_t queueFamily)
    {
        VkResult res = initiated() ? VK_SUCCESS : VK_ERROR_UNKNOWN;

        // TODO: set names

        if (res != VK_SUCCESS)
        {
            device = inputDevice;

            // command pools
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.pNext                   = nullptr;
            poolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex        = queueFamily;

            res = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

            if (res == VK_SUCCESS)
            {
                // command buffers
                VkCommandBufferAllocateInfo allocInfo = {};
                allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandPool                 = commandPool;
                allocInfo.commandBufferCount          = 1;

                res = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
            }
            if (res == VK_SUCCESS)
            {
                // create timeline semaphore
                VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = {};
                semaphoreTypeCreateInfo.sType                     = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
                semaphoreTypeCreateInfo.pNext                     = nullptr;
                semaphoreTypeCreateInfo.semaphoreType             = VK_SEMAPHORE_TYPE_TIMELINE;
                semaphoreTypeCreateInfo.initialValue              = availableSemaphoreValue;

                VkSemaphoreCreateInfo semaphoreCreateInfo = {};
                semaphoreCreateInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                semaphoreCreateInfo.pNext                 = &semaphoreTypeCreateInfo;
                semaphoreCreateInfo.flags                 = 0;

                res = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore);
            }
        }
        if (res != VK_SUCCESS)
        {
            release();
        }

        return res == VK_SUCCESS;
    }

    void occupy(VulkanQueue inputQueue, const char* name)
    {
        availableSemaphoreValue++;
        queue = inputQueue;

        // TODO: set names
    }

    VkCommandBuffer reset()
    {
        VkResult res = vkResetCommandPool(device, commandPool, 0);
        if (res == VK_SUCCESS)
        {
            res = vkResetCommandBuffer(commandBuffer, 0);
            if (res == VK_SUCCESS)
            {
            }
        }

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext                    = nullptr;
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo         = nullptr;

        res = vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    VkCommandBuffer list()
    {
        return commandBuffer;
    }

    VkResult execute()
    {
        VkResult res = vkEndCommandBuffer(commandBuffer);
        if (res != VK_SUCCESS)
            return res;
        
        return queue.submit(commandBuffer, semaphore, availableSemaphoreValue);
    }

    VkResult execute(SubmissionSemaphores& semaphoresToWait, SubmissionSemaphores& semaphoresToSignal)
    {
        VkResult res = vkEndCommandBuffer(commandBuffer);
        if (res != VK_SUCCESS)
            return res;
        
        semaphoresToSignal.add(semaphore, availableSemaphoreValue);
        return queue.submit(commandBuffer, semaphoresToWait, semaphoresToSignal);
    }

    VkResult drop()
    {
        VkResult res = vkEndCommandBuffer(commandBuffer);
        if (res != VK_SUCCESS)
            return res;
        
        return queue.submit(VK_NULL_HANDLE, semaphore, availableSemaphoreValue);
    }

    bool available()
    {
        uint64_t value = 0;
        if (vkGetSemaphoreCounterValue(device, semaphore, &value) == VK_SUCCESS)
            return value >= availableSemaphoreValue;
        return false;
    }
};

template <size_t NumFamilies, size_t Capacity>
class VulkanCommandPool
{
public:
private:
    CRITICAL_SECTION criticalSection                 = {};
    uint32_t         queueFamilyIndices[NumFamilies] = {};
    VkCommands       buffer[NumFamilies][Capacity]   = {};

public:
    VulkanCommandPool()
    {
        InitializeCriticalSection(&criticalSection);
        for (size_t familyIndex = 0; familyIndex < NumFamilies; familyIndex++)
            queueFamilyIndices[familyIndex] = UINT32_MAX;
    }

    ~VulkanCommandPool()
    {
        EnterCriticalSection(&criticalSection);

        for (size_t familyIndex = 0; familyIndex < NumFamilies; familyIndex++)
        {
            for (size_t idx = 0; idx < Capacity; idx++)
            {
                auto& cmds = buffer[familyIndex][idx];
                while (cmds.initiated() && !cmds.available())
                {
                    // wait for list to be idling
                }
                cmds.release();
            }

            queueFamilyIndices[familyIndex] = UINT32_MAX;
        }

        LeaveCriticalSection(&criticalSection);

        DeleteCriticalSection(&criticalSection);
    }

    VkCommands* get(VkDevice device, VulkanQueue queue, const char* name)
    {
        EnterCriticalSection(&criticalSection);

        uint32_t familyIndex = 0;
        // find family index
        for (; familyIndex < NumFamilies; familyIndex++)
        {
            if (queueFamilyIndices[familyIndex] == queue.familyIndex)
            {
                break;
            }
            else if (queueFamilyIndices[familyIndex] == UINT32_MAX)
            {
                queueFamilyIndices[familyIndex] = queue.familyIndex;
                break;
            }
        }

        FFX_ASSERT(familyIndex < NumFamilies);

        VkCommands* pCommands = nullptr;
        
        for (size_t idx = 0; idx < Capacity && (pCommands == nullptr); idx++)
        {
            auto& cmds = buffer[familyIndex][idx];
            if (cmds.verify(device, queue.familyIndex) && cmds.available())
            {
                pCommands = &cmds;
            }
        }

        FFX_ASSERT(pCommands);

        pCommands->occupy(queue, name);

        LeaveCriticalSection(&criticalSection);

        return pCommands;
    }
};

template <const int Size, typename Type = double>
struct SimpleMovingAverage
{
    Type         history[Size] = {};
    unsigned int idx           = 0;
    unsigned int updateCount   = 0;

    Type getAverage()
    {
        if (updateCount < Size)
            return 0.0;

        Type         average    = 0.f;
        unsigned int iterations = (updateCount >= Size) ? Size : updateCount;

        if (iterations > 0)
        {
            for (size_t i = 0; i < iterations; i++)
            {
                average += history[i];
            }
            average /= iterations;
        }

        return average;
    }

    Type getVariance()
    {
        if (updateCount < Size)
            return 0.0;

        Type         average    = getAverage();
        Type         variance   = 0.f;
        unsigned int iterations = (updateCount >= Size) ? Size : updateCount;

        if (iterations > 0)
        {
            for (size_t i = 0; i < iterations; i++)
            {
                variance += (history[i] - average) * (history[i] - average);
            }
            variance /= iterations;
        }

        return sqrt(variance);
    }

    void reset()
    {
        updateCount = 0;
        idx         = 0;
    }

    void update(Type newValue)
    {
        history[idx] = newValue;
        idx          = (idx + 1) % Size;
        updateCount++;
    }
};
