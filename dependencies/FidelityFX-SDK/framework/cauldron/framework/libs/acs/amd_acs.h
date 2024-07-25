//
// Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#ifndef AMD_ACS_H
#define AMD_ACS_H

#define AMD_ACS_VERSION_MAJOR 0             ///< CoreCount major version
#define AMD_ACS_VERSION_MINOR 1             ///< CoreCount minor version
#define AMD_ACS_VERSION_PATCH 0             ///< CoreCount patch version

// For std type includes
#include <cstdint>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \defgroup Defines AGS defines
/// @{
#define AMD_ACS_API __declspec(dllexport)   ///< CoreCount exported functions

#define CORECOUNT_MAKE_VERSION( major, minor, patch ) ( ( major << 22 ) | ( minor << 12 ) | patch ) ///< Macro to create the lib version
#define CORECOUNT_UNSPECIFIED_VERSION 0xFFFFAD00                                                    ///< Use this to specify no version
/// @}

///
/// Function used to get a recommended number of hardware threads to use for running your game, 
/// taking into account processor family and configuration
/// For Ryzen processors with a number of physical cores below the configured threshold, logical 
/// processor cores are added to the recommended thread count
/// 
/// This advice is specific only to AMD processors and is NOT general guidance for all processor manufacturers
///									Remember to profile!
//
	AMD_ACS_API uint32_t acsGetRecommendedThreadCountForGameplay(bool forceSingleNumaNode = false, bool forceSMT = false, uint32_t maxThreadPoolSize = MAXUINT32, uint32_t forceThreadPoolSize = 0);


///
/// Function used to get a recommended number of hardware threads to use for initialising your game, 
/// taking into account processor family and configuration
/// For Ryzen processors with a number of physical cores below the configured threshold, logical 
/// processor cores are added to the recommended thread count
/// 
/// This advice is specific only to AMD processors and is NOT general guidance for all processor manufacturers
///									Remember to profile!
//
	AMD_ACS_API uint32_t acsGetRecommendedThreadCountForGameInit(bool forceSingleNumaNode = false, bool forceSMT = false, uint32_t maxThreadPoolSize = MAXUINT32, uint32_t forceThreadPoolSize = 0);

///
/// Function to print all of the processor information to debug output
/// 
	AMD_ACS_API void acsPrintProcessorInfo();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // AMD_ACS_H