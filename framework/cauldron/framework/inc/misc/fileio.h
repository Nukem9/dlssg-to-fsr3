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

#include "json/json.h"
using json = nlohmann::ordered_json;

#include <cstdint>

/// @defgroup CauldronFileIO FileIO
/// File read/write support for FidelityFX Cauldron Framework.
///
/// @ingroup CauldronMisc

namespace cauldron
{
    /// Performs a full file read
    ///
    /// @param [in] fileName    The file to read.
    /// @param [in] buffer      The buffer into which to copy read file data.
    /// @param [in] bufferLen   The amount of data to read.
    ///
    /// @returns                The number of bytes read.
    ///
    /// @ingroup CauldronFileIO
    int64_t ReadFileAll(const wchar_t* fileName, void* buffer, size_t bufferLen);

    /// Performs a partial file read
    ///
    /// @param [in] fileName    The file to read.
    /// @param [in] buffer      The buffer into which to copy read file data.
    /// @param [in] bufferLen   The amount of data to read.
    /// @param [in] readOffset  Offset in the file where to start reading.
    ///
    /// @returns                The number of bytes read.
    ///
    /// @ingroup CauldronFileIO
    int64_t ReadFilePartial(const wchar_t* fileName, void* buffer, size_t bufferLen, int64_t readOffset = 0);

    /// Fetches the size (in bytes) of a file
    ///
    /// @param [in] fileName    The file whose size we are querying.
    ///
    /// @returns                The number of bytes contained in the file.
    ///
    /// @ingroup CauldronFileIO
    int64_t GetFileSize(const wchar_t* fileName);

    /// Helper to read and parse json files
    ///
    /// @param [in]  fileName   The json file to read and parse.
    /// @param [out] jsonOut    The json data read from file.
    ///
    /// @returns                True if operation succeeded, false otherwise.
    ///
    /// @ingroup CauldronFileIO
    bool ParseJsonFile(const wchar_t* fileName, json& jsonOut);

} // namespace cauldron
