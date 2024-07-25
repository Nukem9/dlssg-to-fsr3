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
project(Cauldron)

#Add sources

# Add Config file
set(config_file ${CMAKE_CURRENT_SOURCE_DIR}/config/cauldronsampleconfig.json)
copyCommand("${config_file}" ${CONFIG_OUTPUT})

# Add the sample to the solution
add_executable(Cauldron WIN32 ${default_icon_src} ${default_entry_point} ${default_sample_files} ${config_file} )

# Link everything (including the compiler for now)
target_link_libraries(Cauldron LINK_PUBLIC Framework RenderModules d3dcompiler backend_interface_stub)
set_target_properties(Cauldron PROPERTIES
					OUTPUT_NAME_DEBUGDX12 "${PROJECT_NAME}DX12D"
					OUTPUT_NAME_DEBUGVK "${PROJECT_NAME}VKD"
					OUTPUT_NAME_RELEASEDX12 "${PROJECT_NAME}DX12"
					OUTPUT_NAME_RELEASEVK "${PROJECT_NAME}VK"
					OUTPUT_NAME_RELWITHDEBINFODX12 "${PROJECT_NAME}drelDX12"
					OUTPUT_NAME_RELWITHDEBINFOVK "${PROJECT_NAME}drelVK"
					VS_DEBUGGER_WORKING_DIRECTORY "${BIN_OUTPUT}")

# Add compile definitions to identify the sample
target_compile_definitions(Cauldron PRIVATE SampleName=L"Cauldron"
											ConfigFileName=L"configs/cauldronsampleconfig.json"
											ConfigSampleName="CauldronSample")

# Add manifest info
addManifest(Cauldron)

# And solution layout definitions
source_group(""			FILES ${default_entry_point})
source_group("Sample" 	FILES ${default_sample_files})
source_group("Icon"    	FILES ${default_icon_src})
source_group("Config" 	FILES ${config_file})
