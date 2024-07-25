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

#include "core/taskmanager.h"
#include "core/contentmanager.h"
#include "core/framework.h"
#include "misc/assert.h"

#include <functional>

namespace cauldron
{
    TaskManager::TaskManager()
    {
    }

    TaskManager::~TaskManager()
    {
    }

    int32_t TaskManager::Init(uint32_t threadPoolSize)
    {
        std::function<void()> pTaskHandler = [this]() { this->TaskExecutor(); };

        for (uint32_t i = 0; i < threadPoolSize; ++i)
            m_ThreadPool.push_back(std::thread(pTaskHandler));

        return 0;
    }

    void TaskManager::Shutdown()
    {
        // Before shutting down, ensure no loading is going on in the background, as it can hang
        while (GetContentManager()->IsCurrentlyLoading()) {}

        // Flag all threads to shutdown
        {
            std::unique_lock<std::mutex> lock(m_CriticalSection);
            m_ShuttingDown = true;
            m_QueueCondition.notify_all();
        }

        // Wait for all threads to be done
        std::vector<std::thread>::iterator iter = m_ThreadPool.begin();
        while (iter != m_ThreadPool.end())
        {
            iter->join();
            iter = m_ThreadPool.erase(iter);
        }
    }

    void TaskManager::AddTask(Task& newTask) 
    { 
        std::unique_lock<std::mutex> lock(m_CriticalSection);
        m_TaskQueue.push(std::move(newTask));

        // Wake a single thread to pick up the task
        m_QueueCondition.notify_one();
    }

    void TaskManager::AddTaskList(std::queue<Task>& newTaskList)
    {
        std::unique_lock<std::mutex> lock(m_CriticalSection);
        while (newTaskList.size())
        {
            m_TaskQueue.push(std::move(newTaskList.front()));
            newTaskList.pop();
        }

        // Wake up all threads to pick up as many concurrent tasks as possible
        m_QueueCondition.notify_all();
    }

    // Runs for each thread and executes any waiting tasks when available
    void TaskManager::TaskExecutor()
    {
        while (true)
        {
            Task taskToExecute;
            {
                // Try to unload a task to execute
                std::unique_lock<std::mutex> lock(m_CriticalSection);
                m_QueueCondition.wait(lock, [this] { return !this->m_TaskQueue.empty() || m_ShuttingDown; });    // Sleep until a task is available to execute or we are shutting down

                if (m_ShuttingDown)
                    break;

                // Get the task (if not shutting down
                taskToExecute = m_TaskQueue.front();
                m_TaskQueue.pop();
            }

            while (taskToExecute.pTaskFunction)
            {
                // Execute the task
                taskToExecute.pTaskFunction(taskToExecute.pTaskParam);

                // When we are done, if there was a completion callback, tick it down and execute if needed
                if (taskToExecute.pTaskCompletionCallback)
                {
                    // If this was the last task on which we were waiting, execute the completion task now
                    if (--taskToExecute.pTaskCompletionCallback->TaskCount == 0)
                    {
                        auto callbackMemPtr = taskToExecute.pTaskCompletionCallback;
                        taskToExecute = taskToExecute.pTaskCompletionCallback->CompletionTask;
                        delete callbackMemPtr;
                        continue;
                    }
                }

                // No completion task to run, fetch another task or sleep
                break;
            }
        }
    }

} // namespace cauldron
