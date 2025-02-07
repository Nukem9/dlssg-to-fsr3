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

file(GLOB LOADER_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/backend_shader_reloader.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/backend_shader_reloader.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/backend_shader_reloader_common.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/backend_shader_reloader_common.h"
    	"${CMAKE_CURRENT_SOURCE_DIR}/command_execution.cpp"
    	"${CMAKE_CURRENT_SOURCE_DIR}/command_execution.h")

file(GLOB BACKEND_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/native_backend_shader_reloader.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/native_backend_shader_reloader.h")

add_library(backend_shader_reloader_impl STATIC ${LOADER_SOURCES} ${COMPONENT_SOURCES} ${BACKEND_SOURCES})
add_library(backend_shader_reloader_stub STATIC ${LOADER_SOURCES} ${COMPONENT_SOURCES} ${BACKEND_SOURCES})

target_compile_definitions(backend_shader_reloader_impl PRIVATE FFX_SDK_ROOT=\"${CMAKE_SOURCE_DIR}\" FFX_SDK_BUILD_ROOT=\"${CMAKE_BINARY_DIR}\")
target_compile_definitions(backend_shader_reloader_stub PRIVATE FFX_SDK_ROOT=\"${CMAKE_SOURCE_DIR}\" FFX_SDK_BUILD_ROOT=\"${CMAKE_BINARY_DIR}\")

if(CMAKE_GENERATOR_PLATFORM STREQUAL "x64" OR CMAKE_EXE_LINKER_FLAGS STREQUAL "/machine:x64")
    set(FFX_PLATFORM_NAME x64)
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "Win32" OR CMAKE_EXE_LINKER_FLAGS STREQUAL "/machine:X86")
    set(FFX_PLATFORM_NAME x86)
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64" OR CMAKE_EXE_LINKER_FLAGS STREQUAL "/machine:ARM64")
    set(FFX_PLATFORM_NAME ARM64)
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64EC" OR CMAKE_EXE_LINKER_FLAGS STREQUAL "/machine:ARM64EC")
    set(FFX_PLATFORM_NAME ARM64EC)
else()
    message(FATAL_ERROR "Unsupported target platform \"${CMAKE_GENERATOR_PLATFORM}\"")
endif()

target_compile_definitions(backend_shader_reloader_impl PRIVATE FFX_PLATFORM_NAME=\"${FFX_PLATFORM_NAME}\")
target_compile_definitions(backend_shader_reloader_stub PRIVATE FFX_PLATFORM_NAME=\"${FFX_PLATFORM_NAME}\")

target_compile_definitions(backend_shader_reloader_impl PRIVATE SUPPORT_RUNTIME_SHADER_RECOMPILE=1)
target_compile_definitions(backend_shader_reloader_stub PRIVATE SUPPORT_RUNTIME_SHADER_RECOMPILE=0)

target_include_directories(backend_shader_reloader_impl PUBLIC ${FFX_INCLUDE_PATH} ${FFX_SHARED_PATH} ${SDK_ROOT}/include/FidelityFX/host/backends ${FFX_API_CAULDRON_ROOT} ${Vulkan_INCLUDE_DIRS})
target_include_directories(backend_shader_reloader_stub PUBLIC ${FFX_INCLUDE_PATH} ${FFX_SHARED_PATH} ${SDK_ROOT}/include/FidelityFX/host/backends ${FFX_API_CAULDRON_ROOT} ${Vulkan_INCLUDE_DIRS})
