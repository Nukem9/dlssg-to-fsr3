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

if (FFX_FSR3UPSCALER OR FFX_ALL)

	# Shared sources
	set(SHARED_SOURCES ${FFX_SHARED_SOURCES})
	
	# Private sources
	file(GLOB PRIVATE_SOURCES
		# NOTE that ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp(h)" can also be used instead
		# simply including effect name in the path for emphasis
		"${FFX_COMPONENTS_PATH}/fsr3upscaler/*.cpp"
        "${FFX_COMPONENTS_PATH}/fsr3upscaler/*.h")

	# Public source
	file(GLOB PUBLIC_SOURCES
		"${FFX_HOST_PATH}/ffx_fsr3upscaler.h")
	list(APPEND PUBLIC_SOURCES ${FFX_PUBLIC_SOURCES})

	if (FFX_BUILD_AS_DLL)
		add_library(ffx_fsr3upscaler_${FFX_PLATFORM_NAME} SHARED ${SHARED_SOURCES} ${PRIVATE_SOURCES} ${PUBLIC_SOURCES})
	else()
		add_library(ffx_fsr3upscaler_${FFX_PLATFORM_NAME} STATIC ${SHARED_SOURCES} ${PRIVATE_SOURCES} ${PUBLIC_SOURCES})
	endif()

	# API
	source_group("shared_source"  FILES ${SHARED_SOURCES})
	source_group("private_source" FILES ${PRIVATE_SOURCES})
	source_group("public_source"  FILES ${PUBLIC_SOURCES})

	target_include_directories(ffx_fsr3upscaler_${FFX_PLATFORM_NAME} PUBLIC ${FFX_INCLUDE_PATH})
	target_include_directories(ffx_fsr3upscaler_${FFX_PLATFORM_NAME} PUBLIC ${FFX_SHARED_PATH})

	set_source_files_properties(${SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
	set_target_properties(ffx_fsr3upscaler_${FFX_PLATFORM_NAME} PROPERTIES FOLDER Components)
	
endif()
