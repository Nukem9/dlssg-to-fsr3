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

#include <condition_variable>
#include <mutex>

namespace cauldron
{
    /**
     * @class ThreadSafeRingBuffer
     *
     * Thread-safe ring buffer used to back logging system.
     *
     * @ingroup CauldronMisc
     */
    template<typename T, size_t CAPACITY>
    class ThreadSafeRingBuffer
    {
    public:

        /**
         * @brief   Construction with custom initialization.
         */
        ThreadSafeRingBuffer()
            : m_data()
            , m_startIndex{ 0 }
            , m_size{ 0 }
            , m_closed{ false }
            , m_lock()
            , m_cv()
        {
        }

        /**
         * @brief   Ring buffer destruction.
         */
        ~ThreadSafeRingBuffer()
        {
            // stop incoming pushes
            Close();
            {
                std::lock_guard<std::mutex> lg(m_lock);
                m_startIndex = 0;
                m_size = 0;
            }
            m_cv.notify_all();
        }

        /**
         * @brief   Closes the ring buffer (takes no more entries).
         */
        void Close()
        {
            m_closed = true;
            m_cv.notify_one();
        }

        /**
         * @brief   Queries if the ring buffer is empty.
         */
        bool Empty() const
        {
            return m_size == 0;
        }

        /**
         * @brief   Queries if the ring buffer is full (at max capacity).
         */
        bool Full() const
        {
            return m_size == CAPACITY;
        }

        /**
         * @brief   Pops an item off the top of the ring buffer. Blocking until the buffer has an element or is closed.
         */
        bool Pop(T& item)
        {
            bool hasItem = false;
            {
                // only one push or pop happens at the same time
                std::unique_lock<std::mutex> lk(m_lock);
                m_cv.wait(lk, [this] { return this->m_size > 0 || this->m_closed; });
                hasItem = m_size > 0;
                if (hasItem)
                {
                    item = std::move(m_data[m_startIndex]);
                    --m_size;
                    m_startIndex = (m_startIndex + 1) % CAPACITY;
                }
            }
            m_cv.notify_one();
            return hasItem;
        }

        /**
         * @brief   Pushes an item onto the ring buffer. Blocking until there is enough space in the ring buffer if at capacity.
         */
        void PushBack(T&& item)
        {
            {
                // only one push or pop happens at the same time
                std::unique_lock<std::mutex> lk(m_lock);
                m_cv.wait(lk, [this] { return this->m_closed || this->m_size < CAPACITY; });
                if (!m_closed)
                {
                    CauldronAssert(ASSERT_CRITICAL, m_size < CAPACITY, L"Ring buffer is full");
                    size_t index = (m_startIndex + m_size) % CAPACITY;
                    m_data[index] = std::move(item);
                    ++m_size;
                }
            }
            m_cv.notify_one();
        }

    private:
        T m_data[CAPACITY];
        size_t m_startIndex;
        size_t m_size;
        bool m_closed;
        std::mutex m_lock;  // lock to push/pop items
        std::condition_variable m_cv;
    };
}
