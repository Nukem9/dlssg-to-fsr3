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

if(NOT ${FFX_API_DX12})
	return()
endif()

#include CMake custom command generator functions for compiling shaders.
include("${FFX_GPU_PATH}/CMakeCompileShaders.txt")

set(PUBLIC_SHADERS "")
set(PRIVATE_SHADERS "")

file(GLOB PRIVATE_SOURCE
    "${FFX_SHARED_PATH}/ffx_assert.cpp"
    "${FFX_SHARED_PATH}/ffx_breadcrumbs_list.h"
    "${FFX_SHARED_PATH}/ffx_breadcrumbs_list.cpp"
	"${FFX_SRC_BACKENDS_PATH}/shared/*.h"
	"${FFX_SRC_BACKENDS_PATH}/shared/*.cpp"
	"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/"
	"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/FrameInterpolationSwapchain/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/FrameInterpolationSwapchain/*.cpp"
)

if (FFX_FSR1 OR FFX_ALL)
	set(FFX_FSR1_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_fsr1_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_fsr1_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_FSR1_PRIVATE_SOURCE})
	
	# Add shaders for FSR1
	
	file(GLOB FFX_FSR1_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr1/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_FSR1_PRIVATE_SHADERS})
	
	file(GLOB FFX_FSR1_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/fsr1/*.h"
		"${FFX_GPU_PATH}/fsr1/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_FSR1_PUBLIC_SHADERS})
endif()

if (FFX_FSR2 OR FFX_ALL)
	set(FFX_FSR2_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_fsr2_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_fsr2_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_FSR2_PRIVATE_SOURCE})
	
	# Add shaders for FSR2
	
	file(GLOB FFX_FSR2_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr2/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_FSR2_PRIVATE_SHADERS})
	
	file(GLOB FFX_FSR2_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/fsr2/*.h"
		"${FFX_GPU_PATH}/fsr2/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_FSR2_PUBLIC_SHADERS})
endif()

if (FFX_FSR3UPSCALER OR FFX_ALL)
	set(FFX_FSR3UPSCALER_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_fsr3upscaler_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_fsr3upscaler_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_FSR3UPSCALER_PRIVATE_SOURCE})
	
	# Add shaders for FSR3UPSCALER
	
	file(GLOB FFX_FSR3UPSCALER_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr3upscaler/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_FSR3UPSCALER_PRIVATE_SHADERS})
	
	file(GLOB FFX_FSR3UPSCALER_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/fsr3upscaler/*.h"
		"${FFX_GPU_PATH}/fsr3upscaler/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_FSR3UPSCALER_PUBLIC_SHADERS})
endif()

if (FFX_FI OR FFX_ALL)
	set(FFX_FI_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_frameinterpolation_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_frameinterpolation_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_FI_PRIVATE_SOURCE})
	
	# Add shaders for Frame Interpolation
	
	file(GLOB FFX_FI_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/frameinterpolation/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_FI_PRIVATE_SHADERS})
	
	file(GLOB FFX_FI_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/frameinterpolation/*.h"
		"${FFX_GPU_PATH}/frameinterpolation/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_FI_PUBLIC_SHADERS})
endif()

if (FFX_OF OR FFX_ALL)
	set(FFX_OF_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_opticalflow_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_opticalflow_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_OF_PRIVATE_SOURCE})
	
	# Add shaders for Optical Flow
	
	file(GLOB FFX_OF_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/opticalflow/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_OF_PRIVATE_SHADERS})
	
	file(GLOB FFX_OF_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/opticalflow/*.h"
		"${FFX_GPU_PATH}/opticalflow/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_OF_PUBLIC_SHADERS})
endif()

if (FFX_SPD OR FFX_ALL)
	set(FFX_SPD_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_spd_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_spd_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_SPD_PRIVATE_SOURCE})
	
	# Add shaders for SPD
	
	file(GLOB FFX_SPD_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/spd/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_SPD_PRIVATE_SHADERS})
	
	file(GLOB FFX_SPD_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/spd/*.h"
		"${FFX_GPU_PATH}/spd/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_SPD_PUBLIC_SHADERS})
endif()

if (FFX_CACAO OR FFX_ALL)
	set(FFX_CACAO_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_cacao_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_cacao_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_CACAO_PRIVATE_SOURCE})
	
	# Add shaders for CACAO
	
	file(GLOB FFX_CACAO_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/cacao/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_CACAO_PRIVATE_SHADERS})
	
	file(GLOB FFX_CACAO_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/cacao/*.h"
		"${FFX_GPU_PATH}/cacao/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_CACAO_PUBLIC_SHADERS})
endif()

if (FFX_LPM OR FFX_ALL)
	set(FFX_LPM_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_lpm_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_lpm_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_LPM_PRIVATE_SOURCE})
	
	# Add shaders for LPM
	
	file(GLOB FFX_LPM_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/lpm/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_LPM_PRIVATE_SHADERS})
	
	file(GLOB FFX_LPM_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/lpm/*.h"
		"${FFX_GPU_PATH}/lpm/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_LPM_PUBLIC_SHADERS})
endif()

if (FFX_BLUR OR FFX_ALL)
	set(FFX_BLUR_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_blur_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_blur_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_BLUR_PRIVATE_SOURCE})
	
	# Add shaders for BLUR
	
	file(GLOB FFX_BLUR_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/blur/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_BLUR_PRIVATE_SHADERS})
	
	file(GLOB FFX_BLUR_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/blur/*.h"
		"${FFX_GPU_PATH}/blur/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_BLUR_PUBLIC_SHADERS})
endif()

if (FFX_VRS OR FFX_ALL)
	set(FFX_VRS_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_vrs_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_vrs_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_VRS_PRIVATE_SOURCE})
	
	# Add shaders for VRS
	
	file(GLOB FFX_VRS_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/vrs/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_VRS_PRIVATE_SHADERS})
	
	file(GLOB FFX_VRS_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/vrs/*.h"
		"${FFX_GPU_PATH}/vrs/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_VRS_PUBLIC_SHADERS})
endif()

if (FFX_CAS OR FFX_ALL)
	set(FFX_CAS_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_cas_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_cas_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_CAS_PRIVATE_SOURCE})
	
	# Add shaders for CAS
	
	file(GLOB FFX_CAS_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/cas/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_CAS_PRIVATE_SHADERS})
	
	file(GLOB FFX_CAS_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/cas/*.h"
		"${FFX_GPU_PATH}/cas/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_CAS_PUBLIC_SHADERS})
endif()

if (FFX_DOF OR FFX_ALL)
	set(FFX_DOF_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_dof_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_dof_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_DOF_PRIVATE_SOURCE})
	
	# Add shaders for DOF
	
	file(GLOB FFX_DOF_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/dof/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_DOF_PRIVATE_SHADERS})
	
	file(GLOB FFX_DOF_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/dof/*.h"
		"${FFX_GPU_PATH}/dof/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_DOF_PUBLIC_SHADERS})
endif()

if (FFX_LENS OR FFX_ALL)
	set(FFX_LENS_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_lens_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_lens_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_LENS_PRIVATE_SOURCE})
	
	# Add shaders for LENS
	
	file(GLOB FFX_LENS_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/lens/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_LENS_PRIVATE_SHADERS})
	
	file(GLOB FFX_LENS_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/lens/*.h"
		"${FFX_GPU_PATH}/lens/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_LENS_PUBLIC_SHADERS})
endif()

if (FFX_PARALLEL_SORT OR FFX_ALL)
	set(FFX_PARALLEL_SORT_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_parallelsort_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_parallelsort_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_PARALLEL_SORT_PRIVATE_SOURCE})
	
	# Add shaders for PARALLEL_SORT
	
	file(GLOB FFX_PARALLEL_SORT_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/parallelsort/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_PARALLEL_SORT_PRIVATE_SHADERS})
	
	file(GLOB FFX_PARALLEL_SORT_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/parallelsort/*.h"
		"${FFX_GPU_PATH}/parallelsort/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_PARALLEL_SORT_PUBLIC_SHADERS})
endif()

if (FFX_DENOISER OR FFX_ALL)
	set(FFX_DENOISER_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_denoiser_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_denoiser_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_DENOISER_PRIVATE_SOURCE})
	
	# Add shaders for DENOISER
	
	file(GLOB FFX_DENOISER_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/denoiser/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_DENOISER_PRIVATE_SHADERS})
	
	file(GLOB FFX_DENOISER_SORT_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/denoiser/*.h"
		"${FFX_GPU_PATH}/denoiser/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_DENOISER_SORT_PUBLIC_SHADERS})
endif()

if (FFX_CLASSIFIER OR FFX_ALL)
	set(FFX_CLASSIFIER_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_classifier_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_classifier_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_CLASSIFIER_PRIVATE_SOURCE})
	
	# Add shaders for CLASSIFIER
	
	file(GLOB FFX_CLASSIFIER_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/classifier/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_CLASSIFIER_PRIVATE_SHADERS})
	
	file(GLOB FFX_CLASSIFIER_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/classifier/*.h"
		"${FFX_GPU_PATH}/classifier/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_CLASSIFIER_PUBLIC_SHADERS})
endif()

if (FFX_SSSR OR FFX_ALL)
	set(FFX_SSSR_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_sssr_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_sssr_shaderblobs.cpp"
	)
	list(APPEND PRIVATE_SOURCE ${FFX_SSSR_PRIVATE_SOURCE})
	
	# Add shaders for SSSR
	
	file(GLOB FFX_SSSR_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/sssr/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_SSSR_PRIVATE_SHADERS})
	
	file(GLOB FFX_SSSR_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/sssr/*.h"
		"${FFX_GPU_PATH}/sssr/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_SSSR_PUBLIC_SHADERS})
endif()

if (FFX_BRIXELIZER OR FFX_ALL)
	set(FFX_BRIXELIZER_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_brixelizer_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_brixelizer_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_BRIXELIZER_PRIVATE_SOURCE})
	
	# Add shaders for Brixelizer

	file(GLOB FFX_BRIXELIZER_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/brixelizer/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_BRIXELIZER_PRIVATE_SHADERS})

	file(GLOB FFX_BRIXELIZER_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/brixelizer/*.h"
		"${FFX_GPU_PATH}/brixelizer/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_BRIXELIZER_PUBLIC_SHADERS})
endif()

if (FFX_BRIXELIZER_GI OR FFX_ALL)
	set(FFX_BRIXELIZER_GI_PRIVATE_SOURCE
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_brixelizergi_shaderblobs.h"
		"${FFX_SRC_BACKENDS_PATH}/shared/blob_accessors/ffx_brixelizergi_shaderblobs.cpp")
	list(APPEND PRIVATE_SOURCE ${FFX_BRIXELIZER_GI_PRIVATE_SOURCE})

	# Add shaders for Brixelizer GI

	file(GLOB FFX_BRIXELIZER_GI_PRIVATE_SHADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders/brixelizergi/*.hlsl")
	list(APPEND PRIVATE_SHADERS ${FFX_BRIXELIZER_GI_PRIVATE_SHADERS})

	file(GLOB FFX_BRIXELIZER_GI_PUBLIC_SHADERS
		"${FFX_GPU_PATH}/brixelizergi/*.h"
		"${FFX_GPU_PATH}/brixelizergi/*.hlsl")
	list(APPEND PUBLIC_SHADERS ${FFX_BRIXELIZER_GI_PUBLIC_SHADERS})
endif()

# Does this need to recurse??
file(GLOB_RECURSE PUBLIC_SOURCE
    "${FFX_HOST_BACKENDS_PATH}/dx12/*.h")

if (FFX_BUILD_AS_DLL)
    add_library(ffx_backend_dx12_${FFX_PLATFORM_NAME} SHARED ${PRIVATE_SOURCE} ${PUBLIC_SOURCE} ${PRIVATE_SHADERS} ${PUBLIC_SHADERS})
else()
    add_library(ffx_backend_dx12_${FFX_PLATFORM_NAME} STATIC ${PRIVATE_SOURCE} ${PUBLIC_SOURCE} ${PRIVATE_SHADERS} ${PUBLIC_SHADERS})
endif()

# dx12 backend source
source_group("private_source"  FILES ${PRIVATE_SOURCE})
source_group("public_source"   FILES ${PUBLIC_SOURCE})
source_group("private_shaders" FILES ${PRIVATE_SHADERS})
source_group("public_shaders"  FILES ${PUBLIC_SHADERS})

get_filename_component(FFX_PASS_SHADER_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/../shaders/dx12 ABSOLUTE)

target_include_directories(ffx_backend_dx12_${FFX_PLATFORM_NAME} PUBLIC ${FFX_INCLUDE_PATH})
target_include_directories(ffx_backend_dx12_${FFX_PLATFORM_NAME} PUBLIC ${FFX_LIB_PATH})
target_include_directories(ffx_backend_dx12_${FFX_PLATFORM_NAME} PUBLIC ${FFX_PASS_SHADER_OUTPUT_PATH})
target_include_directories(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE ${FFX_COMPONENTS_PATH})
target_include_directories(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE ${FFX_SHARED_PATH})
target_include_directories(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE "${FFX_SRC_BACKENDS_PATH}/shared")
target_include_directories(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE ${FFX_PASS_SHADER_OUTPUT_PATH})

target_link_libraries(ffx_backend_dx12_${FFX_PLATFORM_NAME} pixlib dxgi)

set_source_files_properties(${PRIVATE_SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
set_source_files_properties(${PUBLIC_SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

file(MAKE_DIRECTORY ${FFX_PASS_SHADER_OUTPUT_PATH})

if (FFX_AUTO_COMPILE_SHADERS)
	set(FFX_SC_DEPENDENT_TARGET ffx_backend_dx12_${FFX_PLATFORM_NAME})
else()
    set(FFX_SC_DEPENDENT_TARGET ffx_backend_dx12_shaders_${FFX_PLATFORM_NAME})
    add_custom_target(${FFX_SC_DEPENDENT_TARGET})
    #add_dependencies(ffx_backend_dx12_${FFX_PLATFORM_NAME} ${FFX_SC_DEPENDENT_TARGET})
endif()

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.20.0")
	cmake_policy(SET CMP0116 OLD)
endif()

target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE
	NOMINMAX
	WIN32_LEAN_AND_MEAN)

if (MSVC)
	target_compile_options(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE
	"/W4" # warning level 4
	"/WX" # warnings as errors
	# disable MSVC warnings that are too strict
	"/wd4201" # nonstandard extension used: nameless struct/union
	"/wd4324" # structure was padded due to alignment specifier
	"/wd4408" # anonymous struct did not declare any data members
	"/wd4505" # unreferenced function with internal linkage has been removed
	)
endif()

# add pass shaders for all the components
if (FFX_FSR1 OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_FSR1)
    include (CMakeShadersFSR1.txt)
endif()

if (FFX_FSR2 OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_FSR2)
	include (CMakeShadersFSR2.txt)
endif()

if (FFX_FSR3 OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_FSR3)
endif()

if (FFX_FSR3UPSCALER OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_FSR3UPSCALER)
    include (CMakeShadersFSR3Upscaler.txt)
endif()

if (FFX_FI OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_FI)
	include (CMakeShadersFrameinterpolation.txt)
endif()

if (FFX_OF OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_OF)
	include (CMakeShadersOpticalflow.txt)
endif()

if (FFX_SPD OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_SPD)
	include (CMakeShadersSPD.txt)
endif()

if (FFX_CACAO OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_CACAO)
	include (CMakeShadersCACAO.txt)
endif()

if (FFX_LPM OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_LPM)
	include (CMakeShadersLPM.txt)
endif()

if (FFX_BLUR OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_BLUR)
	include (CMakeShadersBlur.txt)
endif()

if (FFX_VRS OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_VRS)
	include (CMakeShadersVRS.txt)
endif()

if (FFX_CAS OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_CAS)
	include (CMakeShadersCAS.txt)
endif()

if (FFX_DOF OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_DOF)
	include (CMakeShadersDOF.txt)
endif()

if (FFX_LENS OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_LENS)
	include (CMakeShadersLENS.txt)
endif()

if (FFX_PARALLEL_SORT OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_PARALLEL_SORT)
	include (CMakeShadersParallelSort.txt)
endif()

if (FFX_DENOISER OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_DENOISER)
	include (CMakeShadersDenoiser.txt)
endif()

if (FFX_CLASSIFIER OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_CLASSIFIER)
	include (CMakeShadersClassifier.txt)
endif()

if (FFX_SSSR OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_SSSR)
	include (CMakeShadersSSSR.txt)
endif()

if (FFX_BREADCRUMBS OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_BREADCRUMBS)
endif()

if (FFX_BRIXELIZER OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_BRIXELIZER)
	include (CMakeShadersBrixelizer.txt)
endif()

if (FFX_BRIXELIZER_GI OR FFX_ALL)
	target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_BRIXELIZER_GI)
	include (CMakeShadersBrixelizerGI.txt)
endif()

if (FFX_CLASSIFIER OR FFX_ALL)
    target_compile_definitions(ffx_backend_dx12_${FFX_PLATFORM_NAME} PRIVATE FFX_CLASSIFIER)
    include (CMakeShadersClassifier.txt)
endif()

add_custom_target(ffx_shader_permutations_dx12 DEPENDS ${FFX_SC_PERMUTATION_OUTPUTS})
add_dependencies(${FFX_SC_DEPENDENT_TARGET} ffx_shader_permutations_dx12)

# Make sure shader builds are a dependency of the backend
add_dependencies(ffx_backend_dx12_${FFX_PLATFORM_NAME} ffx_shader_permutations_dx12)

# Add to solution folder.
set_target_properties(ffx_backend_dx12_${FFX_PLATFORM_NAME} PROPERTIES FOLDER Backends)
set_target_properties(ffx_shader_permutations_dx12 PROPERTIES FOLDER Backends)
