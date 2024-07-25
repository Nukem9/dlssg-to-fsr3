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

#include "native_backend_shader_reloader.h"

#include "backend_shader_reloader_common.h"

#include "command_execution.h"

#ifdef _DX12
#include "core/backend_implementations/backend_interface_dx12.h"
#else
#include "core/backend_implementations/backend_interface_vk.h"
#endif

#include "misc/log.h"

#include <windows.h> 

static HMODULE hBackendLib = nullptr;

void backend_shader_reloader::LoadNativeBackend()
{
    CAULDRON_ASSERT(hBackendLib == nullptr);

    std::string dllPath = FFX_SDK_ROOT;
    dllPath += "/sdk/bin/ffx_sdk/";

    std::string dllName = "ffx_backend_";

#ifdef _DX12
    dllName += "dx12_";
#else
    dllName += "vk_";
#endif

    dllName += FFX_PLATFORM_NAME;
#ifdef _DEBUG
    dllName += "d";
#endif
#ifdef _RELEASEWDEBUG
    dllName += "drel";
#endif

    if ((hBackendLib = LoadBackendDll(dllPath, dllName)) != nullptr)
    {
    #ifdef _DX12
        GetScratchMemorySizeFunc pFfxGetScratchMemorySizeFunc = (GetScratchMemorySizeFunc)GetProcAddress(hBackendLib, "ffxGetScratchMemorySizeDX12");
        GetInterfaceFunc         pFfxGetInterfaceFunc         = (GetInterfaceFunc)GetProcAddress(hBackendLib, "ffxGetInterfaceDX12");
        GetDeviceDX12Func        pFfxGetDeviceFunc            = (GetDeviceDX12Func)GetProcAddress(hBackendLib, "ffxGetDeviceDX12");
        GetCommandListFunc       pFfxGetCommandListFunc       = (GetCommandListFunc)GetProcAddress(hBackendLib, "ffxGetCommandListDX12");
        GetPipelineFunc          pFfxGetPipelineFunc          = (GetPipelineFunc)GetProcAddress(hBackendLib, "ffxGetPipelineDX12");
        GetResourceFunc          pFfxGetResourceFunc          = (GetResourceFunc)GetProcAddress(hBackendLib, "ffxGetResourceDX12");
        // These functions were added for FSR 3.
        ReplaceSwapchainForFrameinterpolationFunc pFfxReplaceSwapchainForFrameinterpolationFunc = (ReplaceSwapchainForFrameinterpolationFunc)GetProcAddress(hBackendLib,
                                                                                                                                                            "ffxReplaceSwapchainForFrameinterpolationDX12");
        RegisterFrameinterpolationUiResourceFunc  pFfxRegisterFrameinterpolationUiResourceFunc  = (RegisterFrameinterpolationUiResourceFunc)GetProcAddress(hBackendLib,
                                                                                                                                                           "ffxRegisterFrameinterpolationUiResourceDX12");
        GetInterpolationCommandlistFunc           pFfxGetInterpolationCommandlistFunc           = (GetInterpolationCommandlistFunc)GetProcAddress(hBackendLib,
                                                                                                                                                  "ffxGetFrameinterpolationCommandlistDX12");
        GetSwapchainFunc                          pFfxGetSwapchainFunc                          = (GetSwapchainFunc)GetProcAddress(hBackendLib, "ffxGetSwapchainDX12");
        GetCommandQueueFunc                       pFfxGetCommandQueueFunc                       = (GetCommandQueueFunc)GetProcAddress(hBackendLib, "ffxGetCommandQueueDX12");
        GetResourceDescriptionFunc                pFfxGetResourceDescriptionFunc                = (GetResourceDescriptionFunc)GetProcAddress(hBackendLib, "ffxGetResourceDescriptionDX12");
        GetFrameinterpolationTextureFunc          pFfxGetFrameinterpolationTextureFunc          = (GetFrameinterpolationTextureFunc)GetProcAddress(hBackendLib, "ffxGetFrameinterpolationTextureDX12");
        LoadPixDllFunc                            pFfxLoadPixDllFunc                            = (LoadPixDllFunc)GetProcAddress(hBackendLib, "ffxLoadPixDll");
        GetDX12SwapchainPtrFunc                   pFfxGetDX12SwapchainPtrFunc                   = (GetDX12SwapchainPtrFunc)GetProcAddress(hBackendLib, "ffxGetDX12SwapchainPtr");

        InitDX12BackendInterface(
            pFfxGetScratchMemorySizeFunc, pFfxGetInterfaceFunc, pFfxGetDeviceFunc, pFfxGetCommandListFunc, pFfxGetPipelineFunc, pFfxGetResourceFunc,
            pFfxReplaceSwapchainForFrameinterpolationFunc, pFfxRegisterFrameinterpolationUiResourceFunc, pFfxGetInterpolationCommandlistFunc,
            pFfxGetSwapchainFunc, pFfxGetCommandQueueFunc, pFfxGetResourceDescriptionFunc, pFfxGetFrameinterpolationTextureFunc,
            pFfxLoadPixDllFunc, pFfxGetDX12SwapchainPtrFunc);
    #else
        GetScratchMemorySizeFunc pFfxGetScratchMemorySizeFunc = (GetScratchMemorySizeFunc)GetProcAddress(hBackendLib, "ffxGetScratchMemorySizeVK");
        GetInterfaceFunc         pFfxGetInterfaceFunc         = (GetInterfaceFunc)GetProcAddress(hBackendLib, "ffxGetInterfaceVK");
        GetDeviceVKFunc          pFfxGetDeviceFunc            = (GetDeviceVKFunc)GetProcAddress(hBackendLib, "ffxGetDeviceVK");
        GetCommandListFunc       pFfxGetCommandListFunc       = (GetCommandListFunc)GetProcAddress(hBackendLib, "ffxGetCommandListVK");
        GetPipelineFunc          pFfxGetPipelineFunc          = (GetPipelineFunc)GetProcAddress(hBackendLib, "ffxGetPipelineVK");
        GetResourceFunc          pFfxGetResourceFunc          = (GetResourceFunc)GetProcAddress(hBackendLib, "ffxGetResourceVK");

        // These functions were added for FSR 3.
        ReplaceSwapchainForFrameinterpolationFunc pFfxReplaceSwapchainForFrameinterpolationFunc = (ReplaceSwapchainForFrameinterpolationFunc)GetProcAddress(hBackendLib, "ffxReplaceSwapchainForFrameinterpolationVK");
        RegisterFrameinterpolationUiResourceFunc  pFfxRegisterFrameinterpolationUiResourceFunc  = (RegisterFrameinterpolationUiResourceFunc)GetProcAddress(hBackendLib, "ffxRegisterFrameinterpolationUiResourceVK");
        GetInterpolationCommandlistFunc           pFfxGetInterpolationCommandlistFunc           = (GetInterpolationCommandlistFunc)GetProcAddress(hBackendLib, "ffxGetFrameinterpolationCommandlistVK");
        GetSwapchainFunc                          pFfxGetSwapchainFunc                          = (GetSwapchainFunc)GetProcAddress(hBackendLib, "ffxGetSwapchainVK");
        GetCommandQueueFunc                       pFfxGetCommandQueueFunc                       = (GetCommandQueueFunc)GetProcAddress(hBackendLib, "ffxGetCommandQueueVK");
        GetImageResourceDescriptionFunc           pFfxGetImageResourceDescriptionFunc           = (GetImageResourceDescriptionFunc)GetProcAddress(hBackendLib, "ffxGetImageResourceDescriptionVK");
        GetBufferResourceDescriptionFunc          pFfxGetBufferResourceDescriptionFunc          = (GetBufferResourceDescriptionFunc)GetProcAddress(hBackendLib, "ffxGetBufferResourceDescriptionVK");
        GetFrameinterpolationTextureFunc          pFfxGetFrameinterpolationTextureFunc          = (GetFrameinterpolationTextureFunc)GetProcAddress(hBackendLib, "ffxGetFrameinterpolationTextureVK");
        GetVKSwapchainFunc                        pFfxGetVKSwapchainFunc                        = (GetVKSwapchainFunc)GetProcAddress(hBackendLib, "ffxGetVKSwapchain");
        GetSwapchainReplacementFunctionsFunc      pFfxGetSwapchainReplacementFunctionsFunc      = (GetSwapchainReplacementFunctionsFunc)GetProcAddress(hBackendLib, "ffxGetSwapchainReplacementFunctionsVK");

        InitVKBackendInterface(pFfxGetScratchMemorySizeFunc,
                               pFfxGetInterfaceFunc,
                               pFfxGetDeviceFunc,
                               pFfxGetCommandListFunc,
                               pFfxGetPipelineFunc,
                               pFfxGetResourceFunc,
                               pFfxReplaceSwapchainForFrameinterpolationFunc,
                               pFfxRegisterFrameinterpolationUiResourceFunc,
                               pFfxGetInterpolationCommandlistFunc,
                               pFfxGetSwapchainFunc,
                               pFfxGetCommandQueueFunc,
                               pFfxGetImageResourceDescriptionFunc,
                               pFfxGetBufferResourceDescriptionFunc,
                               pFfxGetFrameinterpolationTextureFunc,
                               pFfxGetVKSwapchainFunc,
                               pFfxGetSwapchainReplacementFunctionsFunc);
    #endif
    }
    else
    {
        std::string dllFullPath = dllPath + dllName + ".dll";
        cauldron::Log::Write(cauldron::LOGLEVEL_TRACE, L"backend_shader_reloader: LoadLibrary(%ls) failed!", StringToWString(dllFullPath).c_str());

        std::string errorMsg = "Failed to load: ";
        errorMsg += dllName;
        throw std::runtime_error(errorMsg);
    }
}

void backend_shader_reloader::ShutdownNativeBackend()
{
    if (hBackendLib != nullptr)
    {
        cauldron::Log::Write(cauldron::LOGLEVEL_TRACE, L"backend_shader_reloader: shutting down backend dll");

        FreeLibrary(hBackendLib);
        hBackendLib = nullptr;
    }
}

void backend_shader_reloader::RebuildNativeBackend()
{
    // Path to the directory that contains the vcproj files used to build the backend and shaders.
    std::string backendProjectDir = FFX_SDK_ROOT;
    backendProjectDir += "/sdk/build/src/backends/";

    // Name of the backend shader builder vcproj file.
    std::string shaderBuildProject = "ffx_backend_";
#ifdef _DX12
    backendProjectDir += "dx12/";
    shaderBuildProject += "dx12_shaders_";
 #else
    backendProjectDir += "vk/";
    shaderBuildProject += "vk_shaders_";
#endif
    shaderBuildProject += FFX_PLATFORM_NAME;

    // Name of the backend vcproj file.
    std::string backendBuildProject = "ffx_backend_";
#ifdef _DX12
    backendBuildProject += "dx12_";
 #else
    backendBuildProject += "vk_";
#endif
    backendBuildProject += FFX_PLATFORM_NAME;

    // Name of the type of build to run.
    std::string buildConfig;
#ifdef _DEBUG
    buildConfig += "Debug";
#else
#ifdef _RELEASEWDEBUG
    buildConfig += "RelWithDebInfo";
#else
    buildConfig += "Release";
#endif
#endif

    RebuildBackendShaders(
        backendProjectDir,
        shaderBuildProject,
        backendBuildProject,
        buildConfig);
}

#endif // SUPPORT_RUNTIME_SHADER_RECOMPILE
