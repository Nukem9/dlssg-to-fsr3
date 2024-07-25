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

#include "misc/log.h"
#include "misc/assert.h"

#include <cstdarg>
#include <iostream>
#include <sstream>

#ifdef WIN32
#include <debugapi.h>
#endif

namespace cauldron
{
    //////////////////////////////////////////////////////////////////////////
    // Helpers

    int GetFormattedLength(const wchar_t* format, ...)
    {
        va_list vl;
        va_start(vl, format);
        int len = _vscwprintf(L" (%ls: %d)", vl);
        va_end(vl);
        return len;
    }

    int PrintInBuffer(wchar_t* data, int length, const wchar_t* format, ...)
    {
        va_list vl;
        va_start(vl, format);
        int len = _vsnwprintf_s(data, length, length, format, vl);
        va_end(vl);
        return len;
    }

    void PrintMessage(std::wostream& os, const cauldron::MessageBuffer& msg)
    {
        time_t t = msg.Time();
        tm ts;
        localtime_s(&ts, &t);
        wchar_t time_buf[16];
        wcsftime(time_buf, 16, L"[%H:%M:%S]", &ts);

        os << time_buf;

        switch (msg.Level()) {
        case cauldron::LogLevel::LOGLEVEL_TRACE:
            os << L"[Trace]   ";
            break;
        case cauldron::LogLevel::LOGLEVEL_DEBUG:
            os << L"[Debug]   ";
            break;
        case cauldron::LogLevel::LOGLEVEL_INFO:
            os << L"[Info]    ";
            break;
        case cauldron::LogLevel::LOGLEVEL_WARNING:
            os << L"[Warning] ";
            break;
        case cauldron::LogLevel::LOGLEVEL_ERROR:
            os << L"[Error]   ";
            break;
        case cauldron::LogLevel::LOGLEVEL_FATAL:
            os << L"[Fatal]   ";
            break;
        default:
            os << L"[Unknown] ";
            break;
        }

        os << msg.Data() << std::endl;
    }

    //////////////////////////////////////////////////////////////////////////
    // MessageBuffer
    
    MessageBuffer::MessageBuffer()
        : MessageBuffer(0, LogLevel::LOGLEVEL_TRACE, 0)
    {
    }

    MessageBuffer::MessageBuffer(size_t length, LogLevel level, time_t t)
        : m_length(length)
        , m_level(level)
        , m_time(t)
        , m_static()
        , m_dynamic(length > s_STATIC_BUFFER_SIZE ? length : 0)
    {
    }

    MessageBuffer::MessageBuffer(MessageBuffer& other)
        : m_length{ other.m_length }
        , m_level{ other.m_level }
        , m_time{ other.m_time }
        , m_static()
        , m_dynamic(0)
    {
        if (m_length > s_STATIC_BUFFER_SIZE)
            m_dynamic = other.m_dynamic;
        else
            memcpy(m_static, other.m_static, m_length * sizeof(wchar_t));
    }

    MessageBuffer::~MessageBuffer()
    {
    }

    MessageBuffer& MessageBuffer::operator=(MessageBuffer&& other) noexcept
    {
        m_length = other.m_length;
        m_level = other.m_level;
        m_time = other.m_time;
        m_dynamic = std::move(other.m_dynamic);
        if (m_length <= s_STATIC_BUFFER_SIZE)
            memcpy(m_static, other.m_static, m_length * sizeof(wchar_t));

        return *this;
    }

    wchar_t* MessageBuffer::Data()
    {
        if (m_length > s_STATIC_BUFFER_SIZE)
            return m_dynamic.data();
        else
            return m_static;
    }

    const wchar_t* MessageBuffer::Data() const
    {
        if (m_length > s_STATIC_BUFFER_SIZE)
            return m_dynamic.data();
        else
            return m_static;
    }

    //////////////////////////////////////////////////////////////////////////
    // Log
    
    Log* Log::s_pLogInstance = nullptr;

    int Log::InitLogSystem(const wchar_t* filename)
    {
        // Create an instance of the log system if non already exists
        if (s_pLogInstance == nullptr)
        {
            s_pLogInstance = new Log(filename);
            CauldronAssert(ASSERT_ERROR, s_pLogInstance != nullptr, L"Unable to create the Logger");
            if (s_pLogInstance != nullptr)
                return 0;
        }

        // Something went wrong
        return -1;
    }

    int Log::TerminateLogSystem()
    {
        // Uninitialize anything we need to here
        delete s_pLogInstance;
        s_pLogInstance = nullptr;   // In case there's some things stuck in the pipes (should never be the case but ...)

        return 0;
    }

    Log::Log(const wchar_t* filename)
        : m_messageBuffer()
        , m_output(filename, std::ofstream::out)
        , m_thread(&Log::Worker, this)
        , m_messagesLock()
        , m_messageStartIndex(0)
        , m_messageCount(0)
        , m_messagesRingBuffer()
    {
    }

    Log::~Log()
    {
        m_messageBuffer.Close();
        m_thread.join();
        m_output.close();
    }


    void Log::Write(LogLevel level, const wchar_t* text, ...)
    {
        if (s_pLogInstance != nullptr)
        {
            va_list args;
            va_start(args, text);
            s_pLogInstance->QueueMessage(level, nullptr, 0, text, args);
            va_end(args);
        }
    }

    void Log::WriteDetailed(LogLevel level, const wchar_t* filename, int line, const wchar_t* text, ...)
    {
        if (s_pLogInstance != nullptr)
        {
            va_list args;
            va_start(args, text);
            s_pLogInstance->QueueMessage(level, filename, line, text, args);
            va_end(args);
        }
    }

    void Log::QueueMessage(LogLevel level, const wchar_t* filename, int line, const wchar_t* text, va_list args)
    {
        time_t now = time(0);
        
        int body_len = _vscwprintf(text, args);

        const wchar_t* filename_format = L" (%ls: %d)";
        int filename_len = 0;
        if (filename != nullptr)
            filename_len = GetFormattedLength(filename_format, filename, line);

        int total_len = body_len + filename_len + 1;
        MessageBuffer msg(total_len, level, now);

        int offset = 0;

        offset += vswprintf_s(msg.Data() + offset, (size_t)total_len - (size_t)offset, text, args);

        if (filename != nullptr)
        {
            offset += PrintInBuffer(msg.Data() + offset, total_len - offset, filename_format, filename, line);
            msg.Data()[total_len - 1] = L'\0';
        }

        m_messageBuffer.PushBack(std::move(msg));
    }

    void Log::Worker() {
        std::cout << "Log Worker thread started" << std::endl;
        MessageBuffer msg;
        while (m_messageBuffer.Pop(msg))
        {
            // write in the file
            PrintMessage(m_output, msg);
            
            // output to debugger console
            OutputToDebugger(msg);

            // save in the buffer of last messages
            std::lock_guard<std::mutex> lk(m_messagesLock);
            int index = (m_messageStartIndex + m_messageCount) % s_MAX_SAVED_MESSAGES;
            if (m_messageCount == s_MAX_SAVED_MESSAGES)
                ++m_messageStartIndex;
            else
                ++m_messageCount;
            m_messagesRingBuffer[index] = std::move(msg);
        }

        std::cout << "Log Worker thread ended" << std::endl;
    }

    void Log::OutputToDebugger(const MessageBuffer& msg)
    {
#ifdef WIN32
        if (IsDebuggerPresent())
        {
            // Although this creates a stream and a string, it's preferable to use the same code path to format the message
            // this is only used when the debugger is attached so we might not worry too much about allocations
            std::wstringstream ss;
            PrintMessage(ss, msg);
            OutputDebugStringW(ss.str().c_str());
        }
#endif
    }

    std::wstring Log::GetMessages(int32_t flags/*=0xffff*/)
    {
        if (s_pLogInstance != nullptr)
            return s_pLogInstance->FilterMessages(flags);

        return L"";
    }

    void Log::GetMessages(std::vector<LogMessageEntry>& messages, int32_t flags/*=0xffff*/)
    {
        if (s_pLogInstance != nullptr)
            s_pLogInstance->GetAllMessageBuffers(messages, flags);
    }

    void Log::QueryMessageCounts(std::array<uint32_t, LOGLEVEL_COUNT>& countAray)
    {
        if (s_pLogInstance != nullptr)
            s_pLogInstance->QueryMessageBufferCounts(countAray);
    }

    std::wstring Log::FilterMessages(int32_t flags)
    {
        std::lock_guard<std::mutex> lk(m_messagesLock);
        std::wstringstream ss;
        for (size_t i = 0; i < m_messageCount; ++i)
        {
            size_t index = (m_messageStartIndex + i) % s_MAX_SAVED_MESSAGES;
            if (((int32_t)m_messagesRingBuffer[index].Level() & flags) != 0)
                PrintMessage(ss, m_messagesRingBuffer[index]);
        }

        return ss.str();
    }

    void Log::GetAllMessageBuffers(std::vector<LogMessageEntry>& messages, int32_t flags)
    {
        std::lock_guard<std::mutex> lk(m_messagesLock);
        for (size_t i = 0; i < m_messageCount; ++i)
        {
            size_t index = (m_messageStartIndex + i) % s_MAX_SAVED_MESSAGES;
            if (((int32_t)m_messagesRingBuffer[index].Level() & flags) != 0)
                messages.push_back(LogMessageEntry(m_messagesRingBuffer[index].Level(), m_messagesRingBuffer[index].Time(), m_messagesRingBuffer[index].Data()));
        }
    }

    void Log::QueryMessageBufferCounts(std::array<uint32_t, LOGLEVEL_COUNT>& countAray)
    {
        countAray.fill(0);
        std::lock_guard<std::mutex> lk(m_messagesLock);
        for (size_t i = 0; i < m_messageCount; ++i)
        {
            size_t  index    = (m_messageStartIndex + i) % s_MAX_SAVED_MESSAGES;
            int32_t logLevel = static_cast<int32_t>(m_messagesRingBuffer[index].Level());
            int32_t bitIndex = static_cast<int32_t>(log2(logLevel & -logLevel));
            ++countAray[bitIndex];
        }
    }

} //  namespace cauldron
