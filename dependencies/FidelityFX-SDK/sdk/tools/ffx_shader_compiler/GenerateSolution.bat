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

echo Checking pre-requisites...

:: Check if CMake is installed
cmake --version > nul 2>&1
if %errorlevel% NEQ 0 (
    echo Cannot find path to cmake. Is CMake installed? Exiting...
    exit /b -1
) else (
    echo    CMake      - Ready.
)

echo.
echo Building FidelityFX Shader Compiler Tool
echo.

:: Check directories exist and create if not
if not exist build\ (
	mkdir build
)

:: determine architecture
echo.
echo architecture %PROCESSOR_ARCHITECTURE% detected
echo.
if /i "%PROCESSOR_ARCHITECTURE%" == "ARM64" (
    set arch=ARM64
) else (
    set arch=X64
)

cd build
cmake -A %arch% ..
cd..

:: Pause so the user can acknowledge any errors or other outputs from the build process
pause
