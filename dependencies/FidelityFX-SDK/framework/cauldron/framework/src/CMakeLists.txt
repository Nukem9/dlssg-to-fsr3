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

project (Framework)

file(GLOB imguifiles
     ${CMAKE_CURRENT_SOURCE_DIR}/../libs/imgui/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/../libs/imgui/*.cpp
	 ${CMAKE_CURRENT_SOURCE_DIR}/../libs/imgui/backends/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/../libs/imgui/backends/*.cpp)

file(GLOB miscfiles
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/misc/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/*.cpp)

file(GLOB miscfiles_dx12
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/dx12/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/dx12/*.cpp)

file(GLOB miscfiles_vk
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/vk/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/vk/*.cpp)

file(GLOB corefiles
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/core/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/core/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp)

# Set compile definitions for by Runtime Shader Recompiler
if(CAULDRON_DX12)
	set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS FFX_API_DX12)
endif()
if(CAULDRON_VK)
    set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS FFX_API_VK)
endif()

# add API and SDK backends include paths for vulkan only
if(CAULDRON_VK)
    include_directories(${API_ROOT}/include ${FFX_HOST_BACKENDS_PATH})
endif()

file (GLOB corefiles_components
	${CMAKE_CURRENT_SOURCE_DIR}/../inc/core/components/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/core/components/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/core/components/*.cpp)

file (GLOB corefiles_loaders
	${CMAKE_CURRENT_SOURCE_DIR}/../inc/core/loaders/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/core/loaders/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/core/loaders/*.cpp)

file(GLOB corefiles_windows
    ${CMAKE_CURRENT_SOURCE_DIR}/core/win/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/win/*.cpp)

file(GLOB renderfiles
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/render/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/*.cpp)

file(GLOB renderfiles_dx12
    ${CMAKE_CURRENT_SOURCE_DIR}/render/dx12/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/dx12/*.cpp)

file(GLOB renderfiles_vk
    ${CMAKE_CURRENT_SOURCE_DIR}/render/vk/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/vk/*.cpp)

file(GLOB renderfiles_win
    ${CMAKE_CURRENT_SOURCE_DIR}/render/win/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/win/*.cpp)

file(GLOB rendermodules_swapchain
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/swapchain/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/swapchain/*.cpp)

file(GLOB rendermodules_ui
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/ui/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/ui/*.cpp)

file(GLOB rendermodules_skinning
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/skinning/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/skinning/*.cpp)

file(GLOB rendermodules_raytracing
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/raytracing/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/raytracing/*.cpp)

file(GLOB rendermodules_tonemapping
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/tonemapping/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/tonemapping/*.cpp)

file(GLOB rendermodules_fpslimiter
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/fpslimiter/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/fpslimiter/*.cpp)

file(GLOB rendermodules_runtimeshaderrecompiler
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/rsr/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/rendermodules/rsr/*.cpp)

file(GLOB shaderfiles
	${CMAKE_CURRENT_SOURCE_DIR}/../inc/shaders/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/../inc/shaders/*.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/shaders/tonemapping/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/../inc/shaders/tonemapping/*.hlsl
	${CMAKE_CURRENT_SOURCE_DIR}/../inc/shaders/fpslimiter/*.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.hlsl)

# If cauldron shaders have any fidelityfx dependencies, add them here.
file(GLOB fidelityfx_common_shaderfiles
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../sdk/include/FidelityFX/gpu/*.h)

file(GLOB fidelityfx_spd_shaderfiles
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../sdk/include/FidelityFX/gpu/spd/ffx_spd.h)
file(GLOB fidelityfx_parallelsort_shaderfiles
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../sdk/include/FidelityFX/gpu/parallelsort/ffx_parallelsort.h
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../sdk/include/FidelityFX/gpu/parallelsort/ffx_parallelsort_common.h)	

set_source_files_properties(${shaderfiles} PROPERTIES VS_TOOL_OVERRIDE "Text")
# Copy shaders to bin directory
copyCommand("${shaderfiles}" ${SHADER_OUTPUT})

set_source_files_properties(${fidelityfx_common_shaderfiles} PROPERTIES VS_TOOL_OVERRIDE "Text")
# Copy shaders to bin/shaders/fidelityfx directory
copyCommand("${fidelityfx_common_shaderfiles}" ${SHADER_OUTPUT}/fidelityfx)

set_source_files_properties(${fidelityfx_spd_shaderfiles} PROPERTIES VS_TOOL_OVERRIDE "Text")
# Copy shaders to bin/shaders/fidelityfx/spd directory
copyCommand("${fidelityfx_spd_shaderfiles}" ${SHADER_OUTPUT}/fidelityfx/spd)

set_source_files_properties(${fidelityfx_parallelsort_shaderfiles} PROPERTIES VS_TOOL_OVERRIDE "Text")
# Copy shaders to bin/shaders/fidelityfx/parallelsort directory
copyCommand("${fidelityfx_parallelsort_shaderfiles}" ${SHADER_OUTPUT}/fidelityfx/parallelsort)

set(cauldronconfigfiles ${CMAKE_CURRENT_SOURCE_DIR}/../config/cauldronconfig.json)
copyCommand("${cauldronconfigfiles}" ${CONFIG_OUTPUT})

add_library(Framework STATIC
    ${imguifiles} ${miscfiles} ${corefiles} ${cauldronconfigfiles} ${corefiles_components} ${corefiles_loaders}
    ${corefiles_windows} ${renderfiles} ${renderfiles_win} ${rendermodules_swapchain} ${rendermodules_ui}
    ${rendermodules_skinning} ${rendermodules_raytracing} ${rendermodules_tonemapping} ${rendermodules_fpslimiter}
    ${rendermodules_runtimeshaderrecompiler} ${shaderfiles} ${fidelityfx_common_shaderfiles} ${fidelityfx_spd_shaderfiles}
    ${fidelityfx_parallelsort_shaderfiles}
    $<$<OR:$<CONFIG:DebugVK>,$<CONFIG:ReleaseVK>,$<CONFIG:RelWithDebInfoVK>>: ${miscfiles_vk} ${renderfiles_vk}>
    $<$<OR:$<CONFIG:DebugDX12>,$<CONFIG:ReleaseDX12>,$<CONFIG:RelWithDebInfoDX12>>: ${miscfiles_dx12} ${renderfiles_dx12}>)

target_compile_definitions(Framework PRIVATE RenderModuleRoot="${RENDERMODULE_ROOT}")

target_compile_definitions(Framework PRIVATE SUPPORT_RUNTIME_SHADER_RECOMPILE=${SUPPORT_RUNTIME_SHADER_RECOMPILE})

if (FFX_BUILD_AS_DLL)
    target_compile_definitions(Framework PRIVATE FFX_BUILD_AS_DLL=1)
else()
    target_compile_definitions(Framework PRIVATE FFX_BUILD_AS_DLL=0)
endif()

# Backend interface and shader recompiler

set(corefiles_backend_dx12
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/core/backend_implementations/backend_interface_dx12.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/backend_implementations/backend_interface_dx12.cpp)

set(corefiles_backend_vk
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/core/backend_implementations/backend_interface_vk.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/backend_implementations/backend_interface_vk.cpp)

add_library(backend_interface_static
    $<$<OR:$<CONFIG:DebugVK>,$<CONFIG:ReleaseVK>,$<CONFIG:RelWithDebInfoVK>>: ${corefiles_backend_vk}>
    $<$<OR:$<CONFIG:DebugDX12>,$<CONFIG:ReleaseDX12>,$<CONFIG:RelWithDebInfoDX12>>: ${corefiles_backend_dx12}>)

target_include_directories(backend_interface_static PUBLIC ${FFX_HOST_BACKENDS_PATH} ${FFX_INCLUDE_PATH})
target_compile_definitions(backend_interface_static PRIVATE SUPPORT_RUNTIME_SHADER_RECOMPILE=0)

add_library(backend_interface_dynamic
    $<$<OR:$<CONFIG:DebugVK>,$<CONFIG:ReleaseVK>,$<CONFIG:RelWithDebInfoVK>>: ${corefiles_backend_vk}>
    $<$<OR:$<CONFIG:DebugDX12>,$<CONFIG:ReleaseDX12>,$<CONFIG:RelWithDebInfoDX12>>: ${corefiles_backend_dx12}>)

target_include_directories(backend_interface_dynamic PUBLIC ${FFX_HOST_BACKENDS_PATH} ${FFX_INCLUDE_PATH})
target_compile_definitions(backend_interface_dynamic PRIVATE SUPPORT_RUNTIME_SHADER_RECOMPILE=1)

add_library(backend_interface_stub INTERFACE)

# Runtime shader recompiler render module uses the backend_shader_reloader library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/render/backend_shader_reloader)

target_link_libraries(backend_interface_static PUBLIC backend_shader_reloader_stub ffx_backend_native)
target_link_libraries(backend_interface_dynamic PUBLIC backend_shader_reloader_impl)
target_link_libraries(backend_interface_stub INTERFACE backend_shader_reloader_stub)

if (${SUPPORT_RUNTIME_SHADER_RECOMPILE})
    add_library(backend_interface ALIAS backend_interface_dynamic)
else()
    add_library(backend_interface ALIAS backend_interface_static)
endif()

if(CAULDRON_DX12)
	add_dependencies(Framework memoryallocator_dx12)
endif()
if(CAULDRON_VK)
	add_dependencies(Framework memoryallocator_vk)
endif()

target_link_libraries (Framework stb imgui amd_acs dxc Xinput Shcore)

if(CAULDRON_DX12)
	target_link_libraries (Framework DXGI amd_ags memoryallocator_dx12 D3D12 pixlib agilitysdk dxheaders)
endif()
if(CAULDRON_VK)
	target_link_libraries (Framework "Vulkan::Vulkan" memoryallocator_vk)
endif()

source_group("Config"							             FILES ${cauldronconfigfiles})
source_group("Core"                 			             FILES ${corefiles})
if(CAULDRON_DX12)
	source_group("Core\\DX12" 								 FILES ${corefiles_backend_dx12})
endif()
if(CAULDRON_VK)
	source_group("Core\\VK"            			             FILES ${corefiles_backend_vk})
endif()
source_group("Core\\Components"    				             FILES ${corefiles_components})
source_group("Core\\Loaders"					             FILES ${corefiles_loaders})
source_group("Core\\Win"            			             FILES ${corefiles_windows})
source_group("Misc"                 			             FILES ${miscfiles})
if(CAULDRON_DX12)
	source_group("Misc\\DX12"          			             FILES ${miscfiles_dx12})
endif()
if(CAULDRON_VK)
	source_group("Misc\\VK"                		             FILES ${miscfiles_vk})
endif()
source_group("Render"               			             FILES ${renderfiles})
source_group("Render\\Rendermodules")
source_group("Render\\Rendermodules\\Swapchain"	             FILES ${rendermodules_swapchain})
source_group("Render\\Rendermodules\\UI"		             FILES ${rendermodules_ui})
source_group("Render\\Rendermodules\\skinning"		         FILES ${rendermodules_skinning})
source_group("Render\\Rendermodules\\raytracing"             FILES ${rendermodules_raytracing})
source_group("Render\\Rendermodules\\Tonemapping"            FILES ${rendermodules_tonemapping})
source_group("Render\\Rendermodules\\FPSLimiter"             FILES ${rendermodules_fpslimiter})
source_group("Render\\Rendermodules\\RuntimeShaderRecompiler" FILES ${rendermodules_runtimeshaderrecompiler})
if(CAULDRON_DX12)
	source_group("Render\\DX12"         		             FILES ${renderfiles_dx12})
endif()
if(CAULDRON_VK)
	source_group("Render\\VK"           			         FILES ${renderfiles_vk})
endif()
source_group("Render\\Win"          			             FILES ${renderfiles_win})
source_group("Shaders"              			             FILES ${shaderfiles})
source_group("Shaders\\FidelityFX"                           FILES ${fidelityfx_common_shaderfiles})
source_group("Shaders\\FidelityFX\\spd"                      FILES ${fidelityfx_spd_shaderfiles})
source_group("Shaders\\FidelityFX\\parallelsort"             FILES ${fidelityfx_parallelsort_shaderfiles})
source_group("ThirdParty")
source_group("ThirdParty\\ImGui"        		             FILES ${imguifiles})
