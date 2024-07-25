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

#if SUPPORT_RUNTIME_SHADER_RECOMPILE

#include "backend_shader_reloader_common.h"

#include "command_execution.h"

#include "misc/helpers.h"
#include "misc/log.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

HMODULE backend_shader_reloader::LoadBackendDll(
    const std::string& dllPath,
    const std::string& dllNameNoExt)
{
    // Use filesystem::path to normalize the path because the move command requires backslash '\',
    // but FFX_SDK_BUILD_ROOT and FFX_SDK_ROOT use fowardslash '/' because cmake does it that way.
    std::experimental::filesystem::path pdbPath = dllPath + dllNameNoExt + ".pdb";
    std::experimental::filesystem::path movePdbPath = dllPath + dllNameNoExt + ".pdb.bak";
    try
    {
        // Move the debug symbols because if Visual Studio IDE loads them then when FreeLibrary is called it
        // doesn't unload the symbols, so then the rebuild fails because it can't write to the pdb file.
        ExecuteSystemCommand("move " + pdbPath.u8string() + " " + movePdbPath.u8string());
    }
    catch (std::runtime_error e)
    {
        // If moving the pdb fails then its likely because it doesn't exist.
        // So ignore this error.
    }

    std::string dllName = dllNameNoExt;
    dllName += ".dll";

    cauldron::Log::Write(cauldron::LOGLEVEL_TRACE, L"backend_shader_reloader: LoadLibrary(%ls)", StringToWString(dllName).c_str());

    std::string dllFullPath = dllPath + dllName;
    return LoadLibrary(TEXT(dllFullPath.c_str()));
}

void backend_shader_reloader::RebuildBackendShaders(
    const std::string& backendProjectDir,
    const std::string& shaderBuildProject,
    const std::string& backendBuildProject,
    const std::string& buildConfig)
{
    // Rebuild shaders project.
    ExecuteBuildCommand(backendProjectDir, shaderBuildProject, buildConfig);
    
    // Rebuild backend dll.
    ExecuteBuildCommand(backendProjectDir, backendBuildProject, buildConfig);
}

#endif // SUPPORT_RUNTIME_SHADER_RECOMPILE
