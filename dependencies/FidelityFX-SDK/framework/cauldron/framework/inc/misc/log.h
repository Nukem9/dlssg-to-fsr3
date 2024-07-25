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

#include "misc/threadsafe_ringbuffer.h"

#include <fstream>
#include <mutex>
#include <stdint.h>
#include <thread>
#include <time.h>
#include <vector>
#include <array>

namespace cauldron
{
    /// An enumeration of message log types.
    ///
    /// @ingroup CauldronMisc
    enum LogLevel : int32_t
    {
        LOGLEVEL_TRACE   = 0x1 << 0,    ///< Trace message.
        LOGLEVEL_DEBUG   = 0x1 << 1,    ///< Debug message (only shows in debug builds).
        LOGLEVEL_INFO    = 0x1 << 2,    ///< Information message.
        LOGLEVEL_WARNING = 0x1 << 3,    ///< Warning message.
        LOGLEVEL_ERROR   = 0x1 << 4,    ///< Error message. Will show an error dialog on Windows.
        LOGLEVEL_FATAL   = 0x1 << 5,    ///< Fatal message. This will throw a critical assert and halt execution.

        LOGLEVEL_COUNT   = 6            ///< Message type count.
    };

    /**
     * @class MessageBuffer
     *
     * Represents a message entry in our list of log messages.
     *
     * @ingroup CauldronMisc
     */
    class MessageBuffer
    {
    public:

        /**
         * @brief   MessageBuffer initialization.
         */
        MessageBuffer();

        /**
         * @brief   MessageBuffer initialization. Creates with a specified length, level, and time-stamp
         */
        MessageBuffer(size_t length, LogLevel level, time_t t);

        /**
         * @brief   MessageBuffer copy constructor.
         */
        MessageBuffer(MessageBuffer& other);

        MessageBuffer(MessageBuffer&& other) = delete;

        /**
         * @brief   MessageBuffer destruction.
         */
        ~MessageBuffer();

        MessageBuffer& operator=(MessageBuffer& other) = delete;

        /**
         * @brief   MessageBuffer move operator.
         */
        MessageBuffer& operator=(MessageBuffer&& other) noexcept;

        /**
         * @brief   Returns the <c><i>LogLevel</i></c> of the message buffer.
         */
        LogLevel Level() const { return m_level; }

        /**
         * @brief   Returns the time stamp of the message buffer.
         */
        time_t Time() const { return m_time; }

        /**
         * @brief   Returns the message data of the message buffer.
         */
        wchar_t* Data();
        const wchar_t* Data() const;

    private:
        static constexpr size_t s_STATIC_BUFFER_SIZE = 256;
        size_t               m_length;
        LogLevel             m_level;
        time_t               m_time;
        wchar_t              m_static[s_STATIC_BUFFER_SIZE];
        std::vector<wchar_t> m_dynamic;
    };


    // Bunch of defines to have a wchar_t version of __FILE__
    #define WIDE2(x) L##x
    #define WIDE1(x) WIDE2(x)
    #define WFILE WIDE1(__FILE__)

    #define CAUDRON_LOG_TRACE(text, ...)   Log::WriteDetailed(LOGLEVEL_TRACE,   WFILE, __LINE__, text, __VA_ARGS__)
    #define CAUDRON_LOG_DEBUG(text, ...)   Log::WriteDetailed(LOGLEVEL_DEBUG,   WFILE, __LINE__, text, __VA_ARGS__)
    #define CAUDRON_LOG_INFO(text, ...)    Log::WriteDetailed(LOGLEVEL_INFO,    WFILE, __LINE__, text, __VA_ARGS__)
    #define CAUDRON_LOG_WARNING(text, ...) Log::WriteDetailed(LOGLEVEL_WARNING, WFILE, __LINE__, text, __VA_ARGS__)
    #define CAUDRON_LOG_FATAL(text, ...)   Log::WriteDetailed(LOGLEVEL_FATAL,   WFILE, __LINE__, text, __VA_ARGS__)
    #define CAUDRON_LOG_ERROR(text, ...)   Log::WriteDetailed(LOGLEVEL_ERROR,   WFILE, __LINE__, text, __VA_ARGS__)

    /**
     * @struct LogMessageEntry
     *
     * Structure used to fetch filtered messages based on a message type.
     *
     * @ingroup CauldronMisc
     */
    struct LogMessageEntry
    {
        LogLevel        LogPriority;        ///< The priority of the message being logged
        std::wstring    LogMessage;         ///< The message to log

        LogMessageEntry(LogLevel level, time_t time, wchar_t* msg) :
            LogPriority(level) 
        {
            tm ts;
            localtime_s(&ts, &time);
            wchar_t time_buf[16];
            wcsftime(time_buf, 16, L"[%H:%M:%S]", &ts);
            LogMessage = time_buf;

            switch (level)
            {
            case LogLevel::LOGLEVEL_TRACE:
                LogMessage += L"[Trace]   ";
                break;
            case LogLevel::LOGLEVEL_DEBUG:
                LogMessage += L"[Debug]   ";
                break;
            case LogLevel::LOGLEVEL_INFO:
                LogMessage += L"[Info]    ";
                break;
            case LogLevel::LOGLEVEL_WARNING:
                LogMessage += L"[Warning] ";
                break;
            case LogLevel::LOGLEVEL_ERROR:
                LogMessage += L"[Error]   ";
                break;
            case LogLevel::LOGLEVEL_FATAL:
                LogMessage += L"[Fatal]   ";
                break;
            default:
                LogMessage += L"[Unknown] ";
                break;
            }

            LogMessage += msg;
        }
    };

    /**
     * @class Log
     *
     * This is the logger for FidelityFX Cauldron Framework. It provides a static interface for message logging.
     *
     * @ingroup CauldronMisc
     */
    class Log
    {
    public:

        /**
         * @brief   Log construction with filename to write results to.
         */
        Log(const wchar_t* filename);

        /**
         * @brief   Log destruction.
         */
        ~Log();

        /**
         * @brief   Log system initialization. Called as part of <c><i>Framework</i></c> initialization.
         */
        static int InitLogSystem(const wchar_t* filename);

        /**
         * @brief   Log system termination. Called as part of <c><i>Framework</i></c> shutdown.
         */
        static int TerminateLogSystem();

        /**
         * @brief   Writes a log message.
         */
        static void Write(LogLevel level, const wchar_t* text, ...);

        /**
         * @brief   Writes a detailed log message. Detailed message contain file and line identifiers.
         */
        static void WriteDetailed(LogLevel level, const wchar_t* filename, int line, const wchar_t* text, ...);

        /**
         * @brief   Gets all the messages with the requested levels. It returns a single string with all the messages.
         */
        static std::wstring GetMessages(int32_t flags = 0xffff);

        /**
         * @brief   Gets all the messages with the requested levels. It returns a vector with all of the individual messages.
         */
        static void GetMessages(std::vector<LogMessageEntry>& messages, int32_t flags = 0xffff);
        
        /**
         * @brief   Gets the number of messages for each message type.
         */
        static void QueryMessageCounts(std::array<uint32_t, LOGLEVEL_COUNT>& countAray);

    private:
        static_assert(LOGLEVEL_COUNT == 6, L"Number of log levels has changed. Please fix up impacted code.");
        static Log* s_pLogInstance;

        void QueueMessage(LogLevel level, const wchar_t* filename, int line, const wchar_t* text, va_list args);
        void Worker();
        void OutputToDebugger(const MessageBuffer& msg);
        std::wstring FilterMessages(int32_t flags);
        void GetAllMessageBuffers(std::vector<LogMessageEntry>& messages, int32_t flags);
        void QueryMessageBufferCounts(std::array<uint32_t, LOGLEVEL_COUNT>& countAray);

    private:
        static constexpr size_t s_MESSAGE_BUFFER_SIZE = 16;
        ThreadSafeRingBuffer<MessageBuffer, s_MESSAGE_BUFFER_SIZE> m_messageBuffer; // a buffer storing the message before they are put in the output file
        std::thread m_thread;

        std::wofstream m_output; // the file output
        
        std::mutex m_messagesLock;
        static constexpr size_t s_MAX_SAVED_MESSAGES = 1024;
        size_t m_messageStartIndex;
        size_t m_messageCount;
        MessageBuffer m_messagesRingBuffer[s_MAX_SAVED_MESSAGES]; // to only save the last messages
    };
} // namespace cauldron
