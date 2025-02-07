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
project(FFX_API_FSR)

# ---------------------------------------------
# Fidelity FX Api FSR Sample
# ---------------------------------------------

# Import SDK libs
add_library(amd_fidelityfx SHARED IMPORTED)
set_target_properties(amd_fidelityfx PROPERTIES
					  INTERFACE_INCLUDE_DIRECTORIES 		"${API_ROOT}/include"
					  IMPORTED_LOCATION_RELEASEDX12 		"${FFX_API_PATHNAME_DX12}.dll"
					  IMPORTED_LOCATION_RELWITHDEBINFODX12 	"${FFX_API_PATHNAME_DX12DREL}.dll"
					  IMPORTED_LOCATION_DEBUGDX12 			"${FFX_API_PATHNAME_DX12D}.dll"
					  IMPORTED_LOCATION_RELEASEVK 			"${FFX_API_PATHNAME_VK}.dll"
					  IMPORTED_LOCATION_RELWITHDEBINFOVK 	"${FFX_API_PATHNAME_VKDREL}.dll"
					  IMPORTED_LOCATION_DEBUGVK 			"${FFX_API_PATHNAME_VKD}.dll"
					  IMPORTED_IMPLIB_RELEASEDX12 			"${FFX_API_PATHNAME_DX12}.lib"
					  IMPORTED_IMPLIB_RELWITHDEBINFODX12 	"${FFX_API_PATHNAME_DX12DREL}.lib"
					  IMPORTED_IMPLIB_DEBUGDX12 			"${FFX_API_PATHNAME_DX12D}.lib"
					  IMPORTED_IMPLIB_RELEASEVK 			"${FFX_API_PATHNAME_VK}.lib"
					  IMPORTED_IMPLIB_RELWITHDEBINFOVK 		"${FFX_API_PATHNAME_VKDREL}.lib"
					  IMPORTED_IMPLIB_DEBUGVK 				"${FFX_API_PATHNAME_VKD}.lib")

# FSR render module files
set(fsr_api_src ${CMAKE_CURRENT_SOURCE_DIR}/fsrapirendermodule.h ${CMAKE_CURRENT_SOURCE_DIR}/fsrapirendermodule.cpp)

# Add config file
set(config_file ${CMAKE_CURRENT_SOURCE_DIR}/config/fsrapiconfig.json)
copyCommand("${config_file}" ${CONFIG_OUTPUT})

# Add the sample to the solution
add_executable(${PROJECT_NAME} WIN32 ${default_icon_src} ${default_entry_point} ${default_sample_files} ${config_file} ${fsr_api_src} ${fsr_shaders})

# Setup the correct exe based on backend name
set(EXE_OUT_NAME ${PROJECT_NAME}_)

# Link everything
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC Framework RenderModules d3dcompiler amd_fidelityfx backend_interface_stub)
set_target_properties(${PROJECT_NAME} PROPERTIES
						OUTPUT_NAME_DEBUGDX12 "${EXE_OUT_NAME}DX12D"
						OUTPUT_NAME_DEBUGVK "${EXE_OUT_NAME}VKD"
						OUTPUT_NAME_RELEASEDX12 "${EXE_OUT_NAME}DX12"
						OUTPUT_NAME_RELEASEVK "${EXE_OUT_NAME}VK"
						OUTPUT_NAME_RELWITHDEBINFODX12 "${EXE_OUT_NAME}drelDX12"
						OUTPUT_NAME_RELWITHDEBINFOVK "${EXE_OUT_NAME}drelVK"
						VS_DEBUGGER_WORKING_DIRECTORY "${BIN_OUTPUT}")

file(GLOB api_dlls ${FFX_API_ROOTPATH}/*.dll)

# Copy amd_fidelityfx dll to output. Relies on it being built before this project is configured!
# TODO: remove tech debt from build system....
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	${api_dlls} ${BIN_OUTPUT})

# Add compile definitions to identify the sample
target_compile_definitions(${PROJECT_NAME} PRIVATE SampleName=L"FidelityFX FSR FFXAPI v1.3"
										   ConfigFileName=L"configs/fsrapiconfig.json"
										   ConfigSampleName="FidelityFX FSR FFXAPI"
										   RenderModuleName=FSRApi)

# Add manifest info
addManifest(${PROJECT_NAME})

# And solution layout definitions
source_group(""							FILES ${default_entry_point})
source_group("Sample" 					FILES ${fsr_sample_files})
source_group("Icon"    					FILES ${default_icon_src})
source_group("Config" 					FILES ${config_file})
source_group("RenderModule"				FILES ${fsr_api_src})
