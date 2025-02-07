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

# Declare project
project(FFX_LENS)

# ---------------------------------------------
# Fidelity FX render module
# ---------------------------------------------

# Import SDK libs
add_library(ffx_lens_native STATIC IMPORTED)
set_target_properties(ffx_lens_native PROPERTIES
					  IMPORTED_CONFIGURATIONS "DebugDX12;DebugVK;RelWithDebInfoDX12;RelWithDebInfoVK;ReleaseDX12;ReleaseVK"
					  IMPORTED_LOCATION_DEBUGDX12 "${SDK_ROOT}/bin/ffx_sdk/ffx_lens_${CMAKE_GENERATOR_PLATFORM}d.lib"
					  IMPORTED_LOCATION_DEBUGVK "${SDK_ROOT}/bin/ffx_sdk/ffx_lens_${CMAKE_GENERATOR_PLATFORM}d.lib"
					  IMPORTED_LOCATION_RELEASEDX12 "${SDK_ROOT}/bin/ffx_sdk/ffx_lens_${CMAKE_GENERATOR_PLATFORM}.lib"
					  IMPORTED_LOCATION_RELEASEVK "${SDK_ROOT}/bin/ffx_sdk/ffx_lens_${CMAKE_GENERATOR_PLATFORM}.lib"
					  IMPORTED_LOCATION_RELWITHDEBINFODX12 "${SDK_ROOT}/bin/ffx_sdk/ffx_lens_${CMAKE_GENERATOR_PLATFORM}drel.lib"
					  IMPORTED_LOCATION_RELWITHDEBINFOVK "${SDK_ROOT}/bin/ffx_sdk/ffx_lens_${CMAKE_GENERATOR_PLATFORM}drel.lib")

# Lens
file(GLOB lens_src
	${CMAKE_CURRENT_SOURCE_DIR}/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# Add config file
set(config_file ${CMAKE_CURRENT_SOURCE_DIR}/config/lensconfig.json)
copyCommand("${config_file}" ${CONFIG_OUTPUT})

# Add the sample to the solution
add_executable(${PROJECT_NAME} WIN32 ${default_icon_src} ${default_entry_point} ${default_sample_files} ${config_file} ${lens_src})

# Setup the correct exe based on backend name
set(EXE_OUT_NAME ${PROJECT_NAME}_)

# Link everything (including the compiler for now)
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC Framework RenderModules d3dcompiler ffx_lens_native backend_interface)
set_target_properties(${PROJECT_NAME} PROPERTIES
					OUTPUT_NAME_DEBUGDX12 "${EXE_OUT_NAME}DX12D"
					OUTPUT_NAME_DEBUGVK "${EXE_OUT_NAME}VKD"
					OUTPUT_NAME_RELEASEDX12 "${EXE_OUT_NAME}DX12"
					OUTPUT_NAME_RELEASEVK "${EXE_OUT_NAME}VK"
                    OUTPUT_NAME_RELWITHDEBINFODX12 "${EXE_OUT_NAME}drelDX12"
					OUTPUT_NAME_RELWITHDEBINFOVK "${EXE_OUT_NAME}drelVK"
					VS_DEBUGGER_WORKING_DIRECTORY "${BIN_OUTPUT}")

# Add compile definitions to identify the sample
target_compile_definitions(${PROJECT_NAME} PRIVATE SampleName=L"FidelityFX Lens v1.1"
											ConfigFileName=L"configs/lensconfig.json"
											ConfigSampleName="FidelityFX Lens"
#Add RenderModuleClass definition only if this sample will use a custom rendermodule
#This will translate to <RenderModuleName>RenderModule as a class name / include file name
											RenderModuleName=Lens
#Add RenderModuleConfig (path to config file) only if this sample uses a custom rendermodule requiring a config file
											#RenderModuleConfig=L"${CMAKE_CURRENT_SOURCE_DIR}/config/config.json"
											)

# Add manifest info
addManifest(${PROJECT_NAME})

# And solution layout definitions
source_group(""							FILES ${default_entry_point})
source_group("Sample" 					FILES ${default_sample_files})
source_group("Icon"    					FILES ${default_icon_src})
source_group("Config" 					FILES ${config_file})
source_group("RenderModule"				FILES ${lens_src})
