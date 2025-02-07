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

#include "misc/corecounts.h"
#include "misc/helpers.h"

#if _WINDOWS
    // ACS only works on windows
    #include "acs/amd_acs.h"
    #include <intrin.h>
#else
    #include <thread>
#endif // _WINDOWS

#include <array>
#include <vector>

namespace cauldron
{
    uint32_t GetRecommendedThreadCount()
    {
#if defined(_WINDOWS)
        // Print processor info to the degub output
        acsPrintProcessorInfo();

        // Use the AMD core count library to get the optimal number of threads to use
        // (we use the GameInit config because we are mostly doing loading and compiling in the background with this)
        return acsGetRecommendedThreadCountForGameInit();
#else
        // Use std::thread as a backup and just return the number of logical threads we can support
        Log::Write(LOGLEVEL_INFO, L"This platform does not define an optimal core count detection algorithm. Falling back on std::thread::hardware_concurrency");
        uint32_t numLogicalThreads = std::thread::hardware_concurrency();
        return numLogicalThreads;
#endif
    }

    void GetCPUDescription(std::wstring& cpuName)
    {
#if defined(_WINDOWS) && (defined(_M_X64) || defined(_M_IX86))
        int32_t nIDs = 0;
        int32_t nExIDs = 0;

        char strCPUName[0x40] = { };

        std::array<int, 4> cpuInfo;
        std::vector<std::array<int, 4>> extData;

        __cpuid(cpuInfo.data(), 0);

        // Calling __cpuid with 0x80000000 as the function_id argument
        // gets the number of the highest valid extended ID.
        __cpuid(cpuInfo.data(), 0x80000000);

        nExIDs = cpuInfo[0];
        for (int i = 0x80000000; i <= nExIDs; ++i)
        {
            __cpuidex(cpuInfo.data(), i, 0);
            extData.push_back(cpuInfo);
        }

        // Interpret CPU strCPUName string if reported
        if (nExIDs >= 0x80000004)
        {
            memcpy(strCPUName, extData[2].data(), sizeof(cpuInfo));
            memcpy(strCPUName + 16, extData[3].data(), sizeof(cpuInfo));
            memcpy(strCPUName + 32, extData[4].data(), sizeof(cpuInfo));
        }

        cpuName = StringToWString(strCPUName);
#else
    #pragma message("Please add code to fetch CPU name for this platform")
    cpuName = L"Unavailable";
#endif // _WINDOWS
    }

} // namespace cauldron
