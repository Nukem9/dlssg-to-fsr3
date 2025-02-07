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

mark_as_advanced(CMAKE_TOOLCHAIN_FILE)
message(STATUS "PULLING IN TOOLCHAIN_FILE")

if(_TOOLCHAIN_)
  return()
endif()

# Set configuration options
set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo)
set(CMAKE_DEBUG_POSTFIX d)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Get warnings for everything
if (CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

if (MSVC)
	# Recommended compiler switches:
	# /fp:fast (Fast floating-point optimizations)
	# /GR (RTTI): if not using typeid or dynamic_cast, we can disable RTTI to save binary size using /GR-
	# /GS (buffer security check)
	# /Gy (enable function-level linking)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /GR- /fp:fast /GS /Gy")
endif()

if (FFX_API_BACKEND STREQUAL DX12_X64 OR
	FFX_API_BACKEND STREQUAL VK_X64)

	set(CMAKE_RELWITHDEBINFO_POSTFIX drel)
	set(CMAKE_SYSTEM_NAME WINDOWS)
	set(CMAKE_SYSTEM_VERSION 10.0)

	set(CMAKE_GENERATOR_PLATFORM "x64" CACHE STRING "" FORCE)
	set(CMAKE_VS_PLATFORM_NAME "x64" CACHE STRING "" FORCE)

	# Ensure our platform toolset is x64
	set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "x64" CACHE STRING "" FORCE)
	
elseif(FFX_API_BACKEND STREQUAL DX12_ARM64)

	set(CMAKE_RELWITHDEBINFO_POSTFIX drel)
	set(CMAKE_SYSTEM_NAME WINDOWS)
	set(CMAKE_SYSTEM_VERSION 10.0)

	set(CMAKE_GENERATOR_PLATFORM "arm64" CACHE STRING "" FORCE)
	set(CMAKE_VS_PLATFORM_NAME "arm64" CACHE STRING "" FORCE)

	# Ensure our platform toolset is arm64
	set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "arm64" CACHE STRING "" FORCE)

elseif(FFX_API_BACKEND STREQUAL DX12_ARM64EC)

	set(CMAKE_RELWITHDEBINFO_POSTFIX drel)
	set(CMAKE_SYSTEM_NAME WINDOWS)
	set(CMAKE_SYSTEM_VERSION 10.0)

	set(CMAKE_GENERATOR_PLATFORM "arm64ec" CACHE STRING "" FORCE)
	set(CMAKE_VS_PLATFORM_NAME "arm64ec" CACHE STRING "" FORCE)

	# Ensure our platform toolset is arm64
	set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "arm64ec" CACHE STRING "" FORCE)
    
elseif(FFX_API_BACKEND STREQUAL GDK_SCARLETT_X64)

	# On GDK, RELWITHDEBINFO is Profile
	set(CMAKE_RELWITHDEBINFO_POSTFIX _profile)

	# Make sure GDK is installed
	if(NOT DEFINED ENV{GXDKLatest})
		message(FATAL_ERROR "Could not detect GameDK environment variable. Please install the latest Microsoft GDK.")
	endif()
	
	set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES XdkEditionTarget)

	set(CMAKE_SYSTEM_NAME WINDOWS)
	set(CMAKE_SYSTEM_VERSION 10.0)

	# Parse the GDK edition to use from the environment variable
	# We will always use the latest version found
	file(TO_CMAKE_PATH "$ENV{GameDKLatest}" GDKLatestPath)
	cmake_path(GET GDKLatestPath FILENAME XDKEDITION)
	message(STATUS "Setting XdkEditionTarget to latest version found: ${XDKEDITION}")

	# Set our GDK edition
	set(XdkEditionTarget ${XDKEDITION} CACHE STRING "Microsoft GDK Edition")

	set(CMAKE_GENERATOR_PLATFORM "Gaming.Xbox.Scarlett.x64" CACHE STRING "" FORCE)
	set(CMAKE_VS_PLATFORM_NAME "Gaming.Xbox.Scarlett.x64" CACHE STRING "" FORCE)

	# Ensure our platform toolset is x64
	set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "x64" CACHE STRING "" FORCE)

	# Let the GDK MSBuild rules decide the WindowsTargetPlatformVersion
	set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "" CACHE STRING "" FORCE)

	set(CMAKE_CXX_FLAGS_INIT "$ENV{CFLAGS} ${CMAKE_CXX_FLAGS_INIT} -D_GAMING_XBOX -D_GAMING_XBOX_SCARLETT -DWINAPI_FAMILY=WINAPI_FAMILY_GAMES -D_ATL_NO_DEFAULT_LIBS -D__WRL_NO_DEFAULT_LIB__ -D_CRT_USE_WINAPI_PARTITION_APP -D_UITHREADCTXT_SUPPORT=0 -D__WRL_CLASSIC_COM_STRICT__ /arch:AVX2 /favor:AMD64" CACHE STRING "" FORCE)
	
	# Set platform libraries
	set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "xgameplatform.lib" CACHE STRING "" FORCE)
	set(CMAKE_CXX_STANDARD_LIBRARIES ${CMAKE_CXX_STANDARD_LIBRARIES_INIT} CACHE STRING "" FORCE)

elseif(FFX_API_BACKEND STREQUAL GDK_XBOXONE_X64)

	# On GDK, RELWITHDEBINFO is Profile
	set(CMAKE_RELWITHDEBINFO_POSTFIX _profile)

	# Make sure GDK is installed
	if(NOT DEFINED ENV{GXDKLatest})
		message(FATAL_ERROR "Could not detect GameDK environment variable. Please install the latest Microsoft GDK.")
	endif()

	set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES XdkEditionTarget)

	set(CMAKE_SYSTEM_NAME WINDOWS)
	set(CMAKE_SYSTEM_VERSION 10.0)

	# Parse the GDK edition to use from the environment variable
	# We will always use the latest version found
	file(TO_CMAKE_PATH "$ENV{GameDKLatest}" GDKLatestPath)
	cmake_path(GET GDKLatestPath FILENAME XDKEDITION)
	message(STATUS "Setting XdkEditionTarget to latest version found: ${XDKEDITION}")

	# Set our GDK edition
	set(XdkEditionTarget ${XDKEDITION} CACHE STRING "Microsoft GDK Edition")

	set(CMAKE_GENERATOR_PLATFORM "Gaming.Xbox.XboxOne.x64" CACHE STRING "" FORCE)
	set(CMAKE_VS_PLATFORM_NAME "Gaming.Xbox.XboxOne.x64" CACHE STRING "" FORCE)

	# Ensure our platform toolset is x64
	set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "x64" CACHE STRING "" FORCE)

	# Let the GDK MSBuild rules decide the WindowsTargetPlatformVersion
	set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "" CACHE STRING "" FORCE)

	set(CMAKE_CXX_FLAGS_INIT "$ENV{CFLAGS} ${CMAKE_CXX_FLAGS_INIT} -D_GAMING_XBOX -D_GAMING_XBOX_XBOX_ONE -DWINAPI_FAMILY=WINAPI_FAMILY_GAMES -D_ATL_NO_DEFAULT_LIBS -D__WRL_NO_DEFAULT_LIB__ -D_CRT_USE_WINAPI_PARTITION_APP -D_UITHREADCTXT_SUPPORT=0 -D__WRL_CLASSIC_COM_STRICT__ /arch:AVX2 /favor:AMD64" CACHE STRING "" FORCE)
	
	# Set platform libraries
	set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "xgameplatform.lib" CACHE STRING "" FORCE)
	set(CMAKE_CXX_STANDARD_LIBRARIES ${CMAKE_CXX_STANDARD_LIBRARIES_INIT} CACHE STRING "" FORCE)

else()
	message(FATAL_ERROR "Backend toolchain \"${FFX_API_BACKEND}\" not yet implemented!")
endif()

set(_TOOLCHAIN_ ON)
