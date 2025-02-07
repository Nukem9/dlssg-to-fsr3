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

echo ===============================================================
echo.
echo  FidelityFX Samples Build System
echo.
echo ===============================================================
echo.

set samples_build_options=-DRUNTIME_SHADER_RECOMPILE=0
set sdk_build_options=-DFFX_AUTO_COMPILE_SHADERS=1

set build_as_dll=
set /P build_as_dll=Build the SDK as DLL [y/n]?

if /i "%build_as_dll%" == "Y" (
    set sdk_build_options=-DFFX_AUTO_COMPILE_SHADERS=1 -DFFX_BUILD_AS_DLL=1
    set samples_build_options=-DFFX_BUILD_AS_DLL=1
)

:select_component

ECHO 1. ALL
ECHO 2. BLUR
ECHO 3. BREADCRUMBS
ECHO 4. BRIXELIZER GI
ECHO 5. CACAO
ECHO 6. CAS
ECHO 7. DOF
ECHO 8. FSR
ECHO 9. HYBRID REFLECTIONS
ECHO 10. HYBRID SHADOWS
ECHO 11. LENS
ECHO 12. LPM
ECHO 13. PARALLEL SORT
ECHO 14. SPD
ECHO 15. SSSR
ECHO 16. VRS
ECHO.

set /P samples=Enter numbers of which samples to build [space delimitted]:
:loop
for /f "tokens=1*" %%a in ("%samples%") do (
   if %%a == 1 (
    set samples_build_options=-DFFX_ALL=ON %samples_build_options%
    set sdk_build_options=-DFFX_ALL=ON %sdk_build_options%
   )
   if %%a == 2 (
    set samples_build_options=-DFFX_BLUR=ON %samples_build_options%
    set sdk_build_options=-DFFX_BLUR=ON %sdk_build_options%
   )
   if %%a == 3 (
    set samples_build_options=-DFFX_BREADCRUMBS=ON %samples_build_options%
    set sdk_build_options=-DFFX_BREADCRUMBS=ON %sdk_build_options%
   )
   if %%a == 4 (
    set samples_build_options=-DFFX_BRIXELIZER_GI=ON %samples_build_options%
    set sdk_build_options=-DFFX_BRIXELIZER=ON -DFFX_BRIXELIZER_GI=ON %sdk_build_options%
   )
   if %%a == 5 (
    set samples_build_options=-DFFX_CACAO=ON %samples_build_options%
    set sdk_build_options=-DFFX_CACAO=ON %sdk_build_options%
   )
   if %%a == 6 (
    set samples_build_options=-DFFX_CAS=ON %samples_build_options%
    set sdk_build_options=-DFFX_CAS=ON %sdk_build_options%
   )
   if %%a == 7 (
    set samples_build_options=-DFFX_DOF=ON %samples_build_options%
    set sdk_build_options=-DFFX_DOF=ON %sdk_build_options%
   )
   if %%a == 8 (
    set samples_build_options=-DFFX_FSR=ON %samples_build_options%
    set sdk_build_options=-DFFX_FSR1=ON %sdk_build_options%
   )
   if %%a == 9 (
    set samples_build_options=-DFFX_HYBRIDREFLECTIONS=ON %samples_build_options%
    set sdk_build_options=-DFFX_CLASSIFIER=ON -DFFX_DENOISER=ON -DFFX_SPD=ON %sdk_build_options%
   )
   if %%a == 10 (
    set samples_build_options=-DFFX_HYBRIDSHADOWS=ON %samples_build_options%
    set sdk_build_options=-DFFX_CLASSIFIER=ON -DFFX_DENOISER=ON %sdk_build_options%
   )
   if %%a == 11 (
    set samples_build_options=-DFFX_LENS=ON %samples_build_options%
    set sdk_build_options=-DFFX_LENS=ON %sdk_build_options%
   )
   if %%a == 12 (
    set samples_build_options=-DFFX_LPM=ON %samples_build_options%
    set sdk_build_options=-DFFX_LPM=ON %sdk_build_options%
   )
   if %%a == 13 (
    set samples_build_options=-DFFX_PARALLEL_SORT=ON %samples_build_options%
    set sdk_build_options=-DFFX_PARALLEL_SORT=ON %sdk_build_options%
   )
   if %%a == 14 (
    set samples_build_options=-DFFX_SPD=ON %samples_build_options%
    set sdk_build_options=-DFFX_SPD=ON %sdk_build_options%
   )
   if %%a == 15 (
    set samples_build_options=-DFFX_SSSR=ON %samples_build_options%
    set sdk_build_options=-DFFX_SSSR=ON -DFFX_DENOISER=ON %sdk_build_options%
   )
   if %%a == 16 (
    set samples_build_options=-DFFX_VRS=ON %samples_build_options%
    set sdk_build_options=-DFFX_VRS=ON %sdk_build_options%
   )
   set samples=%%b
)
if defined samples goto :loop

:: determine architecture
if /i "%PROCESSOR_ARCHITECTURE%" == "ARM64" (
    set arch=ARM64
    set samples_build_options=-AARM64 -DCMAKE_GENERATOR_PLATFORM=ARM64 %samples_build_options%
    set sdk_build_options=-AARM64 -DCMAKE_GENERATOR_PLATFORM=ARM64 %sdk_build_options%
) else (
    set arch=X64
    set samples_build_options=-Ax64 -DCMAKE_GENERATOR_PLATFORM=x64 %samples_build_options%
    set sdk_build_options=-Ax64 -DCMAKE_GENERATOR_PLATFORM=x64 %sdk_build_options%
)
echo architecture %arch% detected
echo.

set build_type=-DBUILD_TYPE=SAMPLES_DX12
set sdk_build_options=-DFFX_API_BACKEND=DX12_%arch% %sdk_build_options%

:: Start by building the backend SDK
echo Building native %arch% backends: %sdk_build_options%
cd sdk
call BuildFidelityFXSDK.bat %sdk_build_options%
cd ..

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

echo.
echo Building SDK sample solutions %samples_build_options%
echo.

cmake .. %build_type% %samples_build_options%

:: Come back to root level
cd ..
pause
