#
# Set up the source files and output library
#
set(CURRENT_PROJECT dlssg_output_dll)

set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(FIDELITYFX_SDK_DIR "${PROJECT_DEPENDENCIES_PATH}/FidelityFX-SDK/sdk")

file(
	GLOB HEADER_FILES
	LIST_DIRECTORIES FALSE
	CONFIGURE_DEPENDS
	"${SOURCE_DIR}/NGX/*.h"
	"${SOURCE_DIR}/*.h"
)

file(
	GLOB SOURCE_FILES
	LIST_DIRECTORIES FALSE
	CONFIGURE_DEPENDS
	"${SOURCE_DIR}/NGX/*.cpp"
	"${SOURCE_DIR}/*.cpp"
)

source_group(
	TREE "${SOURCE_DIR}/.."
	FILES
		${HEADER_FILES}
		${SOURCE_FILES}
)

add_library(
	${CURRENT_PROJECT}
	SHARED
		${HEADER_FILES}
		${SOURCE_FILES}
)

target_precompile_headers(
	${CURRENT_PROJECT}
    PRIVATE
        pch.h
)

target_include_directories(
	${CURRENT_PROJECT}
	PRIVATE
		"${SOURCE_DIR}"
)

set_target_properties(
	${CURRENT_PROJECT}
	PROPERTIES
		OUTPUT_NAME "dlssg_to_fsr3_amd_is_better"
		MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase"
)

#
# Compiler-specific options
#
target_compile_features(
	${CURRENT_PROJECT}
	PRIVATE
		cxx_std_23
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(
		${CURRENT_PROJECT}
		PRIVATE
			"/utf-8"
			"/sdl"
			"/permissive-"
			"/Zc:preprocessor"
			"/Zc:inline"
			"/EHsc"

			"/W4"
			"/wd4100"	# '': unreferenced formal parameter
			"/wd4200"	# nonstandard extension used: zero-sized array in struct/union
			"/wd4201"	# nonstandard extension used: nameless struct/union
			"/wd4324"	# '': structure was padded due to alignment specifier
	)

	target_link_options(
		${CURRENT_PROJECT}
		PRIVATE
			# Force generate release PDBs: https://learn.microsoft.com/en-us/cpp/build/reference/debug-generate-debug-info
			"$<$<CONFIG:RELEASE>:/DEBUG:FULL;/OPT:REF;/OPT:ICF>"
			# Anonymize RSDS PDB paths
			"$<$<CONFIG:RELEASE>:/PDBALTPATH:$<TARGET_FILE_PREFIX:${CURRENT_PROJECT}>$<TARGET_FILE_BASE_NAME:${CURRENT_PROJECT}>.pdb>"
	)
endif()

target_compile_definitions(
	${CURRENT_PROJECT}
	PRIVATE
		NOMINMAX
		VC_EXTRALEAN
		WIN32_LEAN_AND_MEAN

		BUILD_GIT_COMMIT_HASH="${BUILD_GIT_COMMIT_HASH}"
		BUILD_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
		BUILD_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
)

#
# Dependencies
#
# Windows SDK
target_link_libraries(
	${CURRENT_PROJECT}
	PRIVATE
		delayimp
		dxgi
)

target_link_options(
	${CURRENT_PROJECT}
	PRIVATE
		"/DELAYLOAD:gdi32.dll"
		"/DELAYLOAD:dxgi.dll"
)

# Spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${CURRENT_PROJECT} PRIVATE spdlog::spdlog)

# Vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${CURRENT_PROJECT} PRIVATE "$ENV{VULKAN_SDK}/include")
target_link_libraries(${CURRENT_PROJECT} PRIVATE Vulkan::Vulkan)
target_link_options(${CURRENT_PROJECT} PRIVATE "/DELAYLOAD:vulkan-1.dll")

# FidelityFX
target_include_directories(
	${CURRENT_PROJECT}
	PRIVATE
		"${FIDELITYFX_SDK_DIR}/include"
		"${FIDELITYFX_SDK_DIR}/src/components"
)

target_link_libraries(
	${CURRENT_PROJECT}
	PRIVATE
		"${FIDELITYFX_SDK_DIR}/bin/ffx_sdk/ffx_backend_dx12_x64$<$<CONFIG:Debug>:d>.lib"
		"${FIDELITYFX_SDK_DIR}/bin/ffx_sdk/ffx_backend_vk_x64$<$<CONFIG:Debug>:d>.lib"
		"${FIDELITYFX_SDK_DIR}/bin/ffx_sdk/ffx_frameinterpolation_x64$<$<CONFIG:Debug>:d>.lib"
		"${FIDELITYFX_SDK_DIR}/bin/ffx_sdk/ffx_opticalflow_x64$<$<CONFIG:Debug>:d>.lib"
)

#
# Commands
#
if(NOT $ENV{GAME_ROOT_DIRECTORY} STREQUAL "")
	add_custom_command(
		TARGET ${CURRENT_PROJECT}
		POST_BUILD
		COMMAND
			"${CMAKE_COMMAND}" -E copy_if_different
			"$<TARGET_FILE:${CURRENT_PROJECT}>"
			"$ENV{GAME_ROOT_DIRECTORY}/$<TARGET_FILE_NAME:${CURRENT_PROJECT}>"
	)
endif()
