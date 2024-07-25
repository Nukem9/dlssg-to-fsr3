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

#include "helpers.h"
#include "log.h"

#include <string>

#if defined(_WINDOWS)
    #include <comdef.h>
#endif // #if defined(_WINDOWS)

/// @defgroup CauldronAssert Assert
/// <c><i>FidelityFX Cauldron Framework</i></c>'s assert support.
///
/// @ingroup CauldronMisc

/// \def Stringize(L)
/// Converts \L into a string in the context of a define (macro).
/// 
/// @ingroup CauldronAssert
#define Stringize(L)     #L

/// \def MakeString(M, L)
/// Calls passed in macro \M with parameter \L (used to make a string via <c><i>Stringize</i></c>.
/// 
/// @ingroup CauldronAssert
#define MakeString(M, L) M(L)

/// \def $Line
/// Macro to create a string representation out of the __LINE__ preprocessor macro.
/// 
/// @ingroup CauldronAssert
#define $Line            MakeString(Stringize, __LINE__)

/// \def Reminder
/// Macros for #pragma message() statements, links message to corresponding line of code.
/// Usage: #pragma message(Reminder "message goes here")
/// 
/// @ingroup CauldronAssert
#define Reminder         __FILE__ "(" $Line ") : Reminder: "

namespace cauldron
{
#if defined(_WINDOWS)

    /// Throw an error if we encounter program crashing error in Windows.
    /// Will display an error message box and trow the calling process.
    ///
    /// @param [in] format  The formatted string to parse.
    ///
    /// @returns            None.
    ///
    /// @ingroup CauldronAssert
    inline void CauldronCritical(const wchar_t* format, ...)
    {
        // Format the message string
        wchar_t buffer[512];

        va_list args;
        va_start(args, format);
        vswprintf(buffer, 512, format, args);
        va_end(args);

        Log::Write(LOGLEVEL_FATAL, buffer);
        MessageBoxW(NULL, buffer, L"Critical Error", MB_OK);
        throw 1;
    }

    /// Calls <c><i>CauldronCritical</i></c> if the passed in Windows HRESULT code is a failure code.
    ///
    /// @param [in] hrCode  The Windows HRESULT to process.
    ///
    /// @returns            None.
    ///
    /// @ingroup CauldronAssert
    inline void CauldronThrowOnFail(HRESULT hrCode)
    {
        if (FAILED(hrCode))
        {
            std::string stringError = _com_error(hrCode).ErrorMessage();
            CauldronCritical(StringToWString(stringError).c_str());
        }
    }

    /// Display an error if expected result failed, but from witch the system can recover (will not throw).
    /// Will display an error message box on Windows.
    ///
    /// @param [in] format  The formatted string to parse.
    ///
    /// @returns            None.
    ///
    /// @ingroup CauldronAssert
    inline void CauldronError(const wchar_t* format, ...)
    {
        // Format the message string
        wchar_t buffer[512];

        va_list args;
        va_start(args, format);
        vswprintf(buffer, 512, format, args);
        va_end(args);

        Log::Write(LOGLEVEL_ERROR, buffer);
#if defined(_DEBUG)
        MessageBoxW(NULL, buffer, L"Error", MB_OK);
#endif // #if defined(_DEBUG)
    }

    /// Calls <c><i>CauldronError</i></c> if the passed in Windows HRESULT code is a failure code.
    ///
    /// @param [in] hrCode  The Windows HRESULT to process.
    ///
    /// @returns            None.
    ///
    /// @ingroup CauldronAssert
    inline void CauldronErrorOnFail(HRESULT hrCode)
    {
        if (FAILED(hrCode))
        {
            std::string stringError = _com_error(hrCode).ErrorMessage();
            CauldronError(StringToWString(stringError).c_str());
        }
    }

    /// Issue a warning.
    ///
    /// @param [in] format  The formatted string to parse.
    ///
    /// @returns            None.
    ///
    /// @ingroup CauldronAssert
    inline void CauldronWarning(const wchar_t* format, ...)
    {
        // Format the message string
        wchar_t buffer[1024];
        
        va_list args;
        va_start(args, format);
        vswprintf(buffer, 1024, format, args);
        va_end(args);

        Log::Write(LOGLEVEL_WARNING, buffer);
    }

    /// Calls <c><i>CauldronWarning</i></c> if the passed in Windows HRESULT code is a failure code.
    ///
    /// @param [in] hrCode  The Windows HRESULT to process.
    ///
    /// @returns            None.
    ///
    /// @ingroup CauldronAssert
    inline void CauldronWarnOnFail(HRESULT hrCode)
    {
        if (FAILED(hrCode))
        {
            std::string stringError = _com_error(hrCode).ErrorMessage();
            CauldronWarning(StringToWString(stringError).c_str());
        }
    }
#else
    #error CauldronCritical needs to be defined for this platform
    #error CauldronThrowOnFail needs to be defined for this platform
    #error CauldronError needs to be defined for this platform
    #error CauldronWarning needs to be defined for this platform
#endif // defined(_WINDOWS)

    /// An enumeration of <c><i>Cauldron</i></c>'s assertion levels.
    ///
    /// @ingroup CauldronAssert
    enum AssertLevel
    {
        ASSERT_WARNING = 0,     ///< Warning.
        ASSERT_ERROR,           ///< Error. (Displays error UI if supported on Platform)
        ASSERT_CRITICAL,        ///< Critical. Throws calling application.
    };

    /// Calls the proper assertion function according to the <c><i>AssertLevel</i></c> passed in.
    ///
    /// @param [in] severity                The <c><i>AssertLevel</i></c> to process.
    /// @param [in] format                  The formatted string to parse and pass to the assert function.
    ///
    /// @returns                            None.
    ///
    /// @ingroup CauldronAssert
    inline void CauldronAssert(AssertLevel severity, bool condition, const wchar_t* format, ...)
    {
        if (!condition)
        {
            wchar_t buffer[512];

            va_list args;
            va_start(args, format);
            vswprintf(buffer, 512, format, args);
            va_end(args);

            // Most common
            if (severity == ASSERT_CRITICAL)
                CauldronCritical(buffer);

            else if (severity == ASSERT_ERROR)
                CauldronError(buffer);

            else
                CauldronWarning(buffer);
        }
    }

#define CAULDRON_ASSERT(condition)  cauldron::CauldronAssert(cauldron::ASSERT_CRITICAL, condition, L"Assertion Failed %ls - line %d", WFILE, __LINE__)

} // namespace cauldron
