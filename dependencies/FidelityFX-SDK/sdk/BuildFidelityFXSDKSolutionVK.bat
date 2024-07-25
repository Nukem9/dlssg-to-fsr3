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

@echo off
setlocal enabledelayedexpansion

echo ===============================================================
echo.
echo  FidelityFX Build System
echo.
echo ===============================================================
echo.

set build_as_dll=
set /P build_as_dll=Build the SDK as DLL [y/n]? 

set sdk_build_options=
if /i "%build_as_dll%" == "Y" (
    set sdk_build_options=-DFFX_BUILD_AS_DLL=1
)

set auto_compile_shaders=
set /P auto_compile_shaders=Auto-compile shaders [y/n]? 
if /i "%auto_compile_shaders%" == "N" (
    set auto_compile_shaders=-DFFX_AUTO_COMPILE_SHADERS=0
) else (
    set auto_compile_shaders=-DFFX_AUTO_COMPILE_SHADERS=1
)

ECHO 1. ALL
ECHO 2. BLUR
ECHO 3. BREADCRUMBS
ECHO 4. BRIXELIZER
ECHO 5. CACAO
ECHO 6. CAS
ECHO 7. CLASSIFIER
ECHO 8. DENOISER
ECHO 9. DOF
ECHO 10. FSR 1
ECHO 11. FSR 2
ECHO 12. FSR 3
ECHO 13. LENS
ECHO 14. LPM
ECHO 15. PARALLEL SORT
ECHO 16. SPD
ECHO 17. SSSR
ECHO 18. VRS
ECHO.

set /P samples=Enter numbers of which effects to build [space delimitted]: 
:loop
for /f "tokens=1*" %%a in ("%samples%") do (
   if %%a == 1 set sdk_build_options=-DFFX_ALL=ON %sdk_build_options%
   if %%a == 2 set sdk_build_options=-DFFX_BLUR=ON %sdk_build_options%
   if %%a == 3 set sdk_build_options=-DFFX_BREADCRUMBS=ON %sdk_build_options%
   if %%a == 4 set sdk_build_options=-DFFX_BRIXELIZER=ON %sdk_build_options%
   if %%a == 5 set sdk_build_options=-DFFX_CACAO=ON %sdk_build_options%
   if %%a == 6 set sdk_build_options=-DFFX_CAS=ON %sdk_build_options%
   if %%a == 7 set sdk_build_options=-DFFX_CLASSIFIER=ON %sdk_build_options%
   if %%a == 8 set sdk_build_options=-DFFX_DENOISER=ON %sdk_build_options%
   if %%a == 9 set sdk_build_options=-DFFX_DOF=ON %sdk_build_options%
   if %%a == 10 set sdk_build_options=-DFFX_FSR1=ON %sdk_build_options%
   if %%a == 11 set sdk_build_options=-DFFX_FSR2=ON %sdk_build_options%
   if %%a == 12 set sdk_build_options=-DFFX_FSR3=ON %sdk_build_options%
   if %%a == 13 set sdk_build_options=-DFFX_LENS=ON %sdk_build_options%
   if %%a == 14 set sdk_build_options=-DFFX_LPM=ON %sdk_build_options%
   if %%a == 15 set sdk_build_options=-DFFX_PARALLEL_SORT=ON %sdk_build_options%
   if %%a == 16 set sdk_build_options=-DFFX_SPD=ON %sdk_build_options%
   if %%a == 17 set sdk_build_options=-DFFX_SSSR=ON -DFFX_DENOISER=ON %sdk_build_options%
   if %%a == 18 set sdk_build_options=-DFFX_VRS=ON %sdk_build_options%
   set samples=%%b
)
if defined samples goto :loop

echo Checking pre-requisites... 

:: Check if cmake is installed
cmake --version > nul 2>&1
if %errorlevel% NEQ 0 (
    echo Cannot find path to CMake. Is CMake installed? Exiting...
    exit /b -1
) else (
    echo CMake Ready.
)

:: Check if VULKAN_SDK is installed but don't bail out
if "%VULKAN_SDK%"=="" (
    echo Vulkan SDK is not installed -Environment variable VULKAN_SDK is not defined- : Please install the latest Vulkan SDK from LunarG.
) else (
    echo    Vulkan SDK - Ready : %VULKAN_SDK%
)

echo.
echo Building FidelityFX SDK VK (Standalone) %sdk_build_options% %auto_compile_shaders%
echo.

:: Check directories exist and create if not
if not exist build\ (
	mkdir build 
)

cd build

:: Clear out CMakeCache
if exist CMakeFiles\ (
    rmdir /S /Q CMakeFiles
)
if exist CMakeCache.txt (
    del /S /Q CMakeCache.txt
)

cmake .. -DFFX_API_BACKEND=VK_X64 %sdk_build_options% %auto_compile_shaders%

cd..

:: Pause so the user can acknowledge any errors or other outputs from the build process
pause
