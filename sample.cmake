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

#Contains everything necessary to bundle together a sample

# Configure icons for use when building apps
set(default_icon_src 
    ${CAULDRON_ROOT}/application/icon/GPUOpenChip.ico
    ${CAULDRON_ROOT}/application/icon/resource.h
    ${CAULDRON_ROOT}/application/icon/cauldron.rc)
	
set_source_files_properties(${CAULDRON_ROOT}/application/icon/cauldron.rc PROPERTIES VS_TOOL_OVERRIDE "Resource compiler")
set_source_files_properties(${CAULDRON_ROOT}/application/icon/GPUOpenChip.ico  PROPERTIES VS_TOOL_OVERRIDE "Image")

# Configure default sample files to pull in to avoid having every sample need to duplicate the code
set(default_sample_files
	${CAULDRON_ROOT}/application/sample/sample.h
    ${CAULDRON_ROOT}/application/sample/sample.cpp)

# Default entry point to use for samples
set(default_entry_point ${CAULDRON_ROOT}/application/main.cpp)

# Add manifest so the app uses the right DPI settings
function(addManifest PROJECT_NAME)
    IF (MSVC)
        IF (CMAKE_MAJOR_VERSION LESS 3)
            MESSAGE(WARNING "CMake version 3.0 or newer is required use build variable TARGET_FILE")
        ELSE()
            ADD_CUSTOM_COMMAND(
                TARGET ${PROJECT_NAME}
                POST_BUILD
                COMMAND "mt.exe" -manifest \"${CAULDRON_ROOT}/application/dpiawarescaling.manifest\" 
				-inputresource:\"$<TARGET_FILE:${PROJECT_NAME}>\"\;\#1 -outputresource:\"$<TARGET_FILE:${PROJECT_NAME}>\"\;\#1
                COMMENT "Adding display aware manifest...")
        ENDIF()
    ENDIF(MSVC)
endfunction()
