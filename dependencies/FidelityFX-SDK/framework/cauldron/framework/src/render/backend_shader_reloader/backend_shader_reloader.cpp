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

#include "backend_shader_reloader.h"

#include <FidelityFX/host/ffx_interface.h>
#include <FidelityFX/host/ffx_error.h>
#include <FidelityFX/host/ffx_types.h>

#include "native_backend_shader_reloader.h"

#include "command_execution.h"

#if SUPPORT_RUNTIME_SHADER_RECOMPILE

void backend_shader_reloader::Init()
{
    LoadNativeBackend();
}

void backend_shader_reloader::Shutdown()
{
    ShutdownNativeBackend();
}

void backend_shader_reloader::RebuildShaders()
{
    class AutoReloader
    {
    public:
        AutoReloader()
        {
            Shutdown();
        }
        ~AutoReloader()
        {
            Init();
        }
    };
 
    // This makes sure that Init is called prior to exiting this function, even if
    // the Rebuild throws an exception, so that even if the build fails, we return the
    // backend to a working state.
    AutoReloader autoReloader;

    RebuildNativeBackend();
}

#else // SUPPORT_RUNTIME_SHADER_RECOMPILE
// This library doesn't do anything if runtime shader reload is not configured.
void backend_shader_reloader::Init() {  }
void backend_shader_reloader::Shutdown() {}
void backend_shader_reloader::RebuildShaders() {}
#endif // SUPPORT_RUNTIME_SHADER_RECOMPILE
