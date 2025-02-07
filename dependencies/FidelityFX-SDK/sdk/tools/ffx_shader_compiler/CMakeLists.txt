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

cmake_minimum_required(VERSION 3.17)


project(FidelityFX_SC)

# Enable multi-threaded compilation
add_compile_options(/MP)

# General language options (require language standards specified)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get warnings for everything
if (CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall")
endif()
if (MSVC)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /W3")
endif()

if (CMAKE_GENERATOR_PLATFORM STREQUAL x64 OR
    CMAKE_GENERATOR_PLATFORM STREQUAL ARM64 OR
    CMAKE_GENERATOR_PLATFORM STREQUAL ARM64EC)
	#Set so taken by default when creating custom configs
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
endif()

# Check for Visual Studio 2019's build tooling
if(MSVC_TOOLSET_VERSION VERSION_LESS 142)
    message(FATAL_ERROR "Cannot find MSVC toolset version 142 or greater. Please make sure Visual Studio 2019 or newer installed")
endif()

# Generate the output binary in the /bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)

# Copies a set of files to a directory (only if they already don't exist or are different)
# Usage example:
#     set(media_src ${CMAKE_CURRENT_SOURCE_DIR}/../../media/brdfLut.dds )
#     copyCommand("${media_src}" ${CMAKE_HOME_DIRECTORY}/bin)
function(copyCommand list dest)
    foreach(fullFileName ${list})
        get_filename_component(file ${fullFileName} NAME)
        message("Generating custom command for ${fullFileName}")
        add_custom_command(
            OUTPUT   ${dest}/${file}
            PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${dest}
            COMMAND ${CMAKE_COMMAND} -E copy ${fullFileName}  ${dest}
            MAIN_DEPENDENCY  ${fullFileName}
            COMMENT "Updating ${file} into ${dest}"
        )
    endforeach()
endfunction()

#
# Same as copyCommand() but you can give a target name
# This custom target will depend on all those copied files
# Then the target can be properly set as a dependency of other executable or library
# Usage example:
#     add_library(my_lib ...)
#     set(media_src ${CMAKE_CURRENT_SOURCE_DIR}/../../media/brdfLut.dds )
#     copyTargetCommand("${media_src}" ${CMAKE_HOME_DIRECTORY}/bin copied_media_src)
#     add_dependencies(my_lib copied_media_src)
#
function(copyTargetCommand list dest returned_target_name)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    foreach(fullFileName ${list})
        get_filename_component(file ${fullFileName} NAME)
        message("Generating custom command for ${fullFileName}")
        add_custom_command(
            OUTPUT   ${dest}/${file}
            PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${dest}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${fullFileName} ${dest}
            MAIN_DEPENDENCY  ${fullFileName}
            COMMENT "Updating ${file} into ${dest}"
        )
        list(APPEND dest_list ${dest}/${file})
    endforeach()

    add_custom_target(${returned_target_name} DEPENDS "${dest_list}")
    set_target_properties(${returned_target_name} PROPERTIES FOLDER CopyTargets)
endfunction()

# Set sources
file(GLOB sources
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")

# Append lib sources
list(APPEND sources
	${CMAKE_CURRENT_SOURCE_DIR}/libs/MD5/md5.h
	${CMAKE_CURRENT_SOURCE_DIR}/libs/MD5/md5.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/libs/SPIRV-Reflect/spirv_reflect.h
	${CMAKE_CURRENT_SOURCE_DIR}/libs/SPIRV-Reflect/spirv_reflect.c)

# Setup target binary
add_executable(${PROJECT_NAME} ${sources})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Add external libs
add_subdirectory(libs/agilitysdk)
add_subdirectory(libs/dxc)
add_subdirectory(libs/glslangValidator)
add_subdirectory(libs/tiny-process-library)

# Add link time dependencies
target_link_libraries (${PROJECT_NAME} dxguid agilitysdk dxc glslangValidator tiny-process-library)
target_include_directories (${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libs/MD5
                                                   ${CMAKE_CURRENT_SOURCE_DIR}/libs/SPIRV-Reflect
                                                   ${CMAKE_CURRENT_SOURCE_DIR}/libs/tiny-process-library)
