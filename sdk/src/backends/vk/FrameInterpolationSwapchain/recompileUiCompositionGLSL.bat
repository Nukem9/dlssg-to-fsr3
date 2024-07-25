:: This file is part of the FidelityFX SDK.
::
:: Copyright (C) 2024 Advanced Micro Devices, Inc.
:: 
:: Permission is hereby granted, free of charge, to any person obtaining a copy
:: of this software and associated documentation files(the "Software"), to deal
:: in the Software without restriction, including without limitation the rights
:: to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
:: copies of the Software, and to permit persons to whom the Software is
:: furnished to do so, subject to the following conditions :
::
:: The above copyright notice and this permission notice shall be included in
:: all copies or substantial portions of the Software.
::
:: THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
:: IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
:: FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
:: AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
:: LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
:: OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
:: THE SOFTWARE.

@REM regenerates the header files containing the shaderblobs for the ui composition blit
@REM only needs to be run when the shader is changed
..\..\..\..\tools\ffx_shader_compiler\libs\glslangValidator\bin\x64\glslangValidator.exe -o FrameInterpolationSwapchainUiCompositionVS.h -DFFX_UI_PREMUL=0 -Os --vn g_mainVS -S vert -e main --target-env vulkan1.2 FrameInterpolationSwapchainUiCompositionVS.glsl 
..\..\..\..\tools\ffx_shader_compiler\libs\glslangValidator\bin\x64\glslangValidator.exe -o FrameInterpolationSwapchainUiCompositionPS.h -DFFX_UI_PREMUL=0 -Os --vn g_mainPS -S frag -e main --target-env vulkan1.2 FrameInterpolationSwapchainUiCompositionPS.glsl 
..\..\..\..\tools\ffx_shader_compiler\libs\glslangValidator\bin\x64\glslangValidator.exe -o FrameInterpolationSwapchainUiCompositionPremulVS.h -DFFX_UI_PREMUL=1 -Os --vn g_mainPremulVS -S vert -e main --target-env vulkan1.2 FrameInterpolationSwapchainUiCompositionVS.glsl 
..\..\..\..\tools\ffx_shader_compiler\libs\glslangValidator\bin\x64\glslangValidator.exe -o FrameInterpolationSwapchainUiCompositionPremulPS.h -DFFX_UI_PREMUL=1 -Os --vn g_mainPremulPS -S frag -e main --target-env vulkan1.2 FrameInterpolationSwapchainUiCompositionPS.glsl 