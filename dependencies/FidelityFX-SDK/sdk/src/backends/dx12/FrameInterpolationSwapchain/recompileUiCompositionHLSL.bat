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

:: determine architecture
echo.
echo architecture %PROCESSOR_ARCHITECTURE% detected
echo.
if /i "%PROCESSOR_ARCHITECTURE%" == "ARM64" (
    set arch=arm64
) else (
    set arch=x64
)

:: regenerates the header files containing the shaderblobs for the ui composition blit
:: only needs to be run when the shader is changed
..\..\..\..\tools\ffx_shader_compiler\libs\dxc\bin\%arch%\dxc.exe -Fh FrameInterpolationSwapchainUiCompositionVS.h -DFFX_UI_PREMUL=0 -T vs_6_0 -E mainVS FrameInterpolationSwapchainUiComposition.hlsl
..\..\..\..\tools\ffx_shader_compiler\libs\dxc\bin\%arch%\dxc.exe -Fh FrameInterpolationSwapchainUiCompositionPS.h -DFFX_UI_PREMUL=0 -T ps_6_0 -E mainPS FrameInterpolationSwapchainUiComposition.hlsl
..\..\..\..\tools\ffx_shader_compiler\libs\dxc\bin\%arch%\dxc.exe -Fh FrameInterpolationSwapchainUiCompositionPremulVS.h -DFFX_UI_PREMUL=1 -T vs_6_0 -E mainVS FrameInterpolationSwapchainUiComposition.hlsl
..\..\..\..\tools\ffx_shader_compiler\libs\dxc\bin\%arch%\dxc.exe -Fh FrameInterpolationSwapchainUiCompositionPremulPS.h -DFFX_UI_PREMUL=1 -T ps_6_0 -E mainPS FrameInterpolationSwapchainUiComposition.hlsl
..\..\..\..\tools\ffx_shader_compiler\libs\dxc\bin\%arch%\dxc.exe -Fh FrameInterpolationSwapchainDebugPacingVS.h -T vs_6_0 -E mainVS FrameInterpolationSwapchainDebugPacing.hlsl
..\..\..\..\tools\ffx_shader_compiler\libs\dxc\bin\%arch%\dxc.exe -Fh FrameInterpolationSwapchainDebugPacingPS.h -T ps_6_0 -E mainPS FrameInterpolationSwapchainDebugPacing.hlsl
