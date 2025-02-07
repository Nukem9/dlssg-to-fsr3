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

# Enables multithreading compilation
add_compile_options(/MP)

# General language options (require language standards specified)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get warnings for everything
if (CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall")
endif()
if (MSVC)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /W3")
endif()

# Define various roots
set(SAMPLE_ROOT ${CMAKE_HOME_DIRECTORY}/samples)
set(SDK_ROOT ${CMAKE_HOME_DIRECTORY}/sdk)
set(API_ROOT ${CMAKE_HOME_DIRECTORY}/ffx-api)
set(PREBUILT_SIGNED_DLL_ROOT ${CMAKE_HOME_DIRECTORY}/PrebuiltSignedDLL)
set(FRAMEWORK_ROOT ${CMAKE_HOME_DIRECTORY}/framework)
set(BIN_OUTPUT ${CMAKE_HOME_DIRECTORY}/bin)
set(CAULDRON_ROOT ${FRAMEWORK_ROOT}/cauldron)
set(RENDERMODULE_ROOT ${FRAMEWORK_ROOT}/rendermodules)
set(FFX_API_CAULDRON_ROOT ${SAMPLE_ROOT}/ffx_cauldron)
set(SHADER_OUTPUT ${BIN_OUTPUT}/shaders)
set(CONFIG_OUTPUT ${BIN_OUTPUT}/configs)
set(RENDERMODULE_MEDIA_OUTPUT ${BIN_OUTPUT}/media/rendermodule)
set(SUPPORT_RUNTIME_SHADER_RECOMPILE 0)

# Define common sdk variables
set(FFX_SC_EXECUTABLE ${SDK_ROOT}/tools/binary_store/FidelityFX_SC.exe)
set(FFX_INCLUDE_PATH ${SDK_ROOT}/include)
set(FFX_LIB_PATH ${SDK_ROOT}/libs)
set(FFX_BIN_PATH ${SDK_ROOT}/bin/ffx_sdk)
set(FFX_SHARED_PATH ${SDK_ROOT}/src/shared)
set(FFX_HOST_PATH ${SDK_ROOT}/include/FidelityFX/host)
set(FFX_GPU_PATH ${SDK_ROOT}/include/FidelityFX/gpu)
set(FFX_COMPONENTS_PATH ${SDK_ROOT}/src/components)
set(FFX_HOST_BACKENDS_PATH ${FFX_HOST_PATH}/backends)
set(FFX_SRC_BACKENDS_PATH ${SDK_ROOT}/src/backends)

#Setup pathing to FFX_API objects
if (FFX_DEV_API)
	set(FFX_API_ROOTPATH ${API_ROOT}/bin)
	set(FFX_API_PATHNAME_DX12D 		${FFX_API_ROOTPATH}/amd_fidelityfx_dx12d)
	set(FFX_API_PATHNAME_DX12DREL 	${FFX_API_ROOTPATH}/amd_fidelityfx_dx12drel)
	set(FFX_API_PATHNAME_DX12 		${FFX_API_ROOTPATH}/amd_fidelityfx_dx12)
	set(FFX_API_PATHNAME_VKD 		${FFX_API_ROOTPATH}/amd_fidelityfx_vkd)
	set(FFX_API_PATHNAME_VKDREL 	${FFX_API_ROOTPATH}/amd_fidelityfx_vkdrel)
	set(FFX_API_PATHNAME_VK 		${FFX_API_ROOTPATH}/amd_fidelityfx_vk)
else()
	set(FFX_API_ROOTPATH ${PREBUILT_SIGNED_DLL_ROOT})
	set(FFX_API_PATHNAME_DX12D 		${FFX_API_ROOTPATH}/amd_fidelityfx_dx12)
	set(FFX_API_PATHNAME_DX12DREL 	${FFX_API_ROOTPATH}/amd_fidelityfx_dx12)
	set(FFX_API_PATHNAME_DX12 		${FFX_API_ROOTPATH}/amd_fidelityfx_dx12)
	set(FFX_API_PATHNAME_VKD 		${FFX_API_ROOTPATH}/amd_fidelityfx_vk)
	set(FFX_API_PATHNAME_VKDREL 	${FFX_API_ROOTPATH}/amd_fidelityfx_vk)
	set(FFX_API_PATHNAME_VK 		${FFX_API_ROOTPATH}/amd_fidelityfx_vk)
endif()

# Set top level includes (all projects will need these to some extent)
include_directories(${CAULDRON_ROOT}/framework/inc)
include_directories(${CAULDRON_ROOT}/framework/libs)
include_directories(${CAULDRON_ROOT}/framework/src)
include_directories(${CAULDRON_ROOT}/framework/src/render/rendermodules)
include_directories(${RENDERMODULE_ROOT})
include_directories(${SAMPLE_ROOT})
include_directories(${SDK_ROOT}/include)


if (CMAKE_GENERATOR_PLATFORM STREQUAL x64 OR
    CMAKE_GENERATOR_PLATFORM STREQUAL ARM64 OR
    CMAKE_GENERATOR_PLATFORM STREQUAL ARM64EC)
	#Set so taken by default when creating custom configs
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MD")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<OR:$<CONFIG:DebugDX12>,$<CONFIG:DebugVK>>:Debug>DLL")
endif()

# ------------------------------------------
# Cauldron helper function definitions
# ------------------------------------------

# Creates a configuration from a base config (for flag inclusion)
# In the future, we will redefine custom compile/linker options
function(createConfig CONFIGNAME CONFIGSTRING BASECONFIG)
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set(CMAKE_CXX_FLAGS_${BASECONFIG}${CONFIGNAME} "${CMAKE_CXX_FLAGS_${BASECONFIG}}"
        CACHE STRING "Flags used by the C++ compiler during ${CONFIGSTRING} builds.")
    set(CMAKE_C_FLAGS_${BASECONFIG}${CONFIGNAME} "${CMAKE_C_FLAGS_${BASECONFIG}}"
        CACHE STRING "Flags used by the C compiler during ${CONFIGSTRING} builds.")
    set(CMAKE_EXE_LINKER_FLAGS_${BASECONFIG}${CONFIGNAME} "${CMAKE_EXE_LINKER_FLAGS_${BASECONFIG}}"
        CACHE STRING "Flags used for linking binaries during ${CONFIGSTRING} builds.")
    set(CMAKE_SHARED_LINKER_FLAGS_${BASECONFIG}${CONFIGNAME} "${CMAKE_SHARED_LINKER_FLAGS_${BASECONFIG}}"
        CACHE STRING "Flags used by the shared libraries linker during ${CONFIGSTRING} builds.")

	MARK_AS_ADVANCED(CMAKE_CXX_FLAGS_${BASECONFIG}${CONFIGNAME}
        CMAKE_C_FLAGS_${BASECONFIG}${CONFIGNAME}
        CMAKE_EXE_LINKER_FLAGS_${BASECONFIG}${CONFIGNAME}
        CMAKE_SHARED_LINKER_FLAGS_${BASECONFIG}${CONFIGNAME} )
endfunction()

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
