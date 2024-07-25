# This file is part of the FidelityFX SDK.
# 
# Copyright (C) 2024 Advanced Micro Devices, Inc.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files(the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions :
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

cmake_minimum_required(VERSION 3.23)

if(POLICY CMP0147)
	# parallel custom command builds (used for shader compilation)
	cmake_policy(SET CMP0147 NEW)
endif()

# Clean a bit before
unset(FFX_GDK CACHE)

message(STATUS "FFX_API_BACKEND is ${FFX_API_BACKEND}")

if(FFX_API_BACKEND STREQUAL VK_X64 OR 
   FFX_API_BACKEND STREQUAL DX12_X64 OR 
   FFX_API_BACKEND STREQUAL GDK_DESKTOP_X64 OR 
   FFX_API_BACKEND STREQUAL GDK_SCARLETT_X64)
	set(FFX_CUSTOM_API OFF)
else()
	set(FFX_CUSTOM_API ON)
endif()

# Set configuration types based on what we are building for
if (NOT FFX_CUSTOM_API)

	# Are we buiding the GDK?
	if (FFX_API_BACKEND STREQUAL GDK_DESKTOP_X64 OR FFX_API_BACKEND STREQUAL GDK_SCARLETT_X64)
		set(FFX_GDK ON)
	endif()

	# Pull in tool chain definitions
	include(toolchain.cmake)
	
	# If we are NOT building the GDK, queue up DX12/VK solution for building
	if (NOT FFX_GDK)
		if (FFX_API_BACKEND STREQUAL DX12_X64)
			message(STATUS "Building FidelityFX SDK with DX12 backend")
		elseif(FFX_API_BACKEND STREQUAL VK_X64)
			message(STATUS "Building FidelityFX SDK with VK backend")
		endif()
	else()
		# List the API we are building for
		if(FFX_API_BACKEND STREQUAL GDK_DESKTOP_X64)
			message(STATUS "Building FidelityFX SDK with GDK Gaming.Desktop.x64 backend")
		elseif(FFX_API_BACKEND STREQUAL GDK_SCARLETT_X64)
			message(STATUS "Building FidelityFX SDK with GDK Gaming.Scarlett.x64 backend")
		else()
			message(FATAL_ERROR "Requesting unsupported GDK build of FidelityFX SDK!")
		endif()
	endif()
endif()

# Set platform
if(CMAKE_GENERATOR_PLATFORM STREQUAL "Gaming.Desktop.x64")
	set(FFX_PLATFORM_NAME gdk_desktop_x64)
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "Gaming.Xbox.Scarlett.x64")
	set(FFX_PLATFORM_NAME gdk_scarlett_x64)
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "x64" OR CMAKE_EXE_LINKER_FLAGS STREQUAL "/machine:x64")
    set(FFX_PLATFORM_NAME x64)
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "Win32" OR CMAKE_EXE_LINKER_FLAGS STREQUAL "/machine:X86")
    set(FFX_PLATFORM_NAME x86)
else()
    message(FATAL_ERROR "Unsupported target platform \"${CMAKE_GENERATOR_PLATFORM}\" - only supporting gdk, x64, and Win32 currently")
endif()

# Pre-compile shaders
set(FFX_AUTO_COMPILE_SHADERS ON CACHE BOOL "Compile shaders automatically as a prebuild step.")

if(CMAKE_GENERATOR STREQUAL "Ninja")
    set(USE_DEPFILE TRUE)
else()
    set(USE_DEPFILE FALSE)
endif()

# Set Visual Studio version
if(CMAKE_GENERATOR STREQUAL "Visual Studio 14 2015")
    set(FFX_VS_VERSION 2015)
elseif(CMAKE_GENERATOR STREQUAL "Visual Studio 15 2017")
	set(FFX_VS_VERSION 2017)
elseif(CMAKE_GENERATOR STREQUAL "Visual Studio 16 2019")
    set(FFX_VS_VERSION 2019)
elseif(CMAKE_GENERATOR STREQUAL "Visual Studio 17 2022")
	set(FFX_VS_VERSION 2022)
endif()

# Set configuration types based on what we are building for
if (NOT FFX_CUSTOM_API)

	# Embed PDBs in the debug versions of the libs
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Z7 /Od")

	# Write both debug and release versions of the static libs to the /bin folder as they are uniquely named
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_HOME_DIRECTORY}/bin/ffx_sdk/)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_HOME_DIRECTORY}/bin/ffx_sdk/)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_HOME_DIRECTORY}/bin/ffx_sdk/)

	if (FFX_BUILD_AS_DLL)
		# Write both debug and release versions of the dlls to the /bin folder as they are uniquely named
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_HOME_DIRECTORY}/bin/ffx_sdk/)
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_HOME_DIRECTORY}/bin/ffx_sdk/)
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_HOME_DIRECTORY}/bin/ffx_sdk/)
        set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS FFX_BUILD_AS_DLL)
	endif()
else()

	# Ouput the static libs to the /bin folder as they are uniquely named
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BIN_OUTPUT}/ffx_sdk)
	foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
		string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
		set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${BIN_OUTPUT}/ffx_sdk/ )
		if (FFX_BUILD_AS_DLL)
			# Write both debug and release versions of the dlls to the /bin folder as they are uniquely named
			set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_HOME_DIRECTORY}/bin/ffx_sdk/)
			set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_HOME_DIRECTORY}/bin/ffx_sdk/)
			set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_HOME_DIRECTORY}/bin/ffx_sdk/)
		endif()
	endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

endif()

if (FFX_FSR3)
	option(FFX_FSR3UPSCALER "Build FFX FSR3 Upscaler API" ON)
	option(FFX_FI "Build FFX Frame Interpolation API" ON)
	option(FFX_OF "Build FFX Optical Flow API" ON)
endif()

# Add requred compile definitions
add_compile_definitions(_UNICODE)
add_compile_definitions(UNICODE)

if(FFX_VS_VERSION STREQUAL 2015 OR FFX_VS_VERSION STREQUAL 2017)
    message(NOTICE "Forcing the SDK path for VS 2015 and VS 2017")
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0.18362.0")
endif()

# Setup common variables
set(FFX_SC_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/tools/binary_store/FidelityFX_SC.exe)
set(FFX_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(FFX_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libs)
set(FFX_BIN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin/ffx_sdk)
set(FFX_SHARED_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/shared)
set(FFX_HOST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include/FidelityFX/host)
set(FFX_GPU_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include/FidelityFX/gpu)
set(FFX_COMPONENTS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/components)
set(FFX_HOST_BACKENDS_PATH ${FFX_HOST_PATH}/backends)
set(FFX_SRC_BACKENDS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/backends)

# ------------------------------------------------------
# Define common source groupings used by all effects

# Set shared and global public sources
file(GLOB FFX_SHARED_SOURCES
    "${FFX_SHARED_PATH}/*.cpp"
    "${FFX_SHARED_PATH}/*.h")

set(FFX_PUBLIC_SOURCES
    "${FFX_HOST_PATH}/ffx_assert.h"
    "${FFX_HOST_PATH}/ffx_error.h"
	"${FFX_HOST_PATH}/ffx_fx.h"
	"${FFX_HOST_PATH}/ffx_interface.h"
    "${FFX_HOST_PATH}/ffx_types.h"
    "${FFX_HOST_PATH}/ffx_util.h")

# Init shader sources (for easy viewing)
set(FFX_PUBLIC_SHADER_SOURCES
    "${FFX_GPU_PATH}/ffx_common_types.h"
    "${FFX_GPU_PATH}/ffx_core.h"
    "${FFX_GPU_PATH}/ffx_core_cpu.h"
    "${FFX_GPU_PATH}/ffx_core_glsl.h"
    "${FFX_GPU_PATH}/ffx_core_gpu_common.h"
    "${FFX_GPU_PATH}/ffx_core_gpu_common_half.h"
    "${FFX_GPU_PATH}/ffx_core_hlsl.h"
    "${FFX_GPU_PATH}/ffx_core_portability.h")

if (FFX_API_BACKEND STREQUAL DX12_X64)
	message(STATUS "Creating project FidelityFX-SDK_DX12_x64")
	project (FidelityFX-SDK_DX12_x64)
elseif (FFX_API_BACKEND STREQUAL VK_X64)
	message(STATUS "Creating project FidelityFX-SDK_VK_x64")
	project (FidelityFX-SDK_VK_x64)
elseif(FFX_API_BACKEND STREQUAL GDK_DESKTOP_X64)
	message(STATUS "Creating project FidelityFX-SDK_GDK_Desktop_x64")
	project (FidelityFX-SDK_GDK_Desktop_x64)
elseif(FFX_API_BACKEND STREQUAL GDK_SCARLETT_X64)
	message(STATUS "Creating project FidelityFX-SDK_GDK_Scarlett_x64")
	project (FidelityFX-SDK_GDK_Scarlett_x64)
else()
	# This is likely a custom include
	project (FidelityFX-SDK)
endif()

# Components
add_subdirectory(${FFX_COMPONENTS_PATH}/opticalflow)
add_subdirectory(${FFX_COMPONENTS_PATH}/frameinterpolation)
add_subdirectory(${FFX_COMPONENTS_PATH}/fsr3)
add_subdirectory(${FFX_COMPONENTS_PATH}/fsr3upscaler)
add_subdirectory(${FFX_COMPONENTS_PATH}/fsr2)
add_subdirectory(${FFX_COMPONENTS_PATH}/fsr1)
add_subdirectory(${FFX_COMPONENTS_PATH}/spd)
add_subdirectory(${FFX_COMPONENTS_PATH}/cacao)
add_subdirectory(${FFX_COMPONENTS_PATH}/lpm)
add_subdirectory(${FFX_COMPONENTS_PATH}/blur)
add_subdirectory(${FFX_COMPONENTS_PATH}/vrs)
add_subdirectory(${FFX_COMPONENTS_PATH}/cas)
add_subdirectory(${FFX_COMPONENTS_PATH}/dof)
add_subdirectory(${FFX_COMPONENTS_PATH}/lens)
add_subdirectory(${FFX_COMPONENTS_PATH}/parallelsort)
add_subdirectory(${FFX_COMPONENTS_PATH}/denoiser)
add_subdirectory(${FFX_COMPONENTS_PATH}/sssr)
add_subdirectory(${FFX_COMPONENTS_PATH}/brixelizer)
add_subdirectory(${FFX_COMPONENTS_PATH}/brixelizergi)
add_subdirectory(${FFX_COMPONENTS_PATH}/classifier)
add_subdirectory(${FFX_COMPONENTS_PATH}/breadcrumbs)

# Add appropriate graphics backend if requested
if(FFX_API_BACKEND STREQUAL DX12_X64)
	add_subdirectory(${FFX_LIB_PATH}/pix)
    add_subdirectory(${FFX_SRC_BACKENDS_PATH}/dx12)
endif()

if (FFX_API_BACKEND STREQUAL VK_X64)
	add_subdirectory(${FFX_SRC_BACKENDS_PATH}/vk)
endif()

if(FFX_API_BACKEND STREQUAL GDK_DESKTOP_X64 OR FFX_API_BACKEND STREQUAL GDK_SCARLETT_X64)
	# TODO when we have the right lib to include
	#add_subdirectory(${FFX_LIB_PATH}/pix)
	add_subdirectory(${FFX_SRC_BACKENDS_PATH}/gdk)
endif()
