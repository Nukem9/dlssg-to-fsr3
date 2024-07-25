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

#include "command_execution.h"

#include "misc/helpers.h"
#include "misc/log.h"

#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <string>

void backend_shader_reloader::ExecuteSystemCommand(const std::string& cmd)
{
    cauldron::Log::Write(cauldron::LOGLEVEL_TRACE, StringToWString(cmd).c_str());

    char        buffer[128];
    std::string result = "";
    FILE*       pipe   = _popen(cmd.c_str(), "r");
    if (!pipe)
        throw std::runtime_error(" error: popen() failed!");
    try
    {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL)
        {
            result += buffer;
        }
    }
    catch (...)
    {
        _pclose(pipe);
        throw std::runtime_error(result);
    }

    int retValue = _pclose(pipe);
    if (retValue != 0)
    {
        std::string errorMsg = " command failed: " + std::string(cmd);
        errorMsg += "\nOutput:\n";
        errorMsg += result;
        throw std::runtime_error(errorMsg);
    }
    else if (result.length() > 0)
        cauldron::Log::Write(cauldron::LOGLEVEL_TRACE, StringToWString(result).c_str());
}

void backend_shader_reloader::ExecuteBuildCommand(const std::string& projectDir, const std::string& projectName, const std::string& buildConfig)
{
    std::string rebuildCommand = "cmake --build ";
    rebuildCommand += projectDir;
    rebuildCommand += " --target ";
    rebuildCommand += projectName;
    rebuildCommand += " --config ";
    rebuildCommand += buildConfig;

    ExecuteSystemCommand(rebuildCommand);
}
