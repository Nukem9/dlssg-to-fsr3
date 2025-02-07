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

if (CAULDRON_VK)
	find_package(Vulkan REQUIRED)
	CheckVulkanSDKVersion()
endif()

file(GLOB files_dx12
    ${CMAKE_CURRENT_SOURCE_DIR}/D3D12MemAlloc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/D3D12MemAlloc.cpp)

file(GLOB files_vk
    ${CMAKE_CURRENT_SOURCE_DIR}/memoryallocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memoryallocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vk_mem_alloc.h)

if (SUPPORT_RUNTIME_SHADER_RECOMPILE EQUAL 1)
	
	if (CAULDRON_VK)
		add_library(memoryallocator_vk SHARED ${files_vk})
		target_compile_definitions(memoryallocator_vk PRIVATE VMA_EXPORTS)
	endif()
	
	if (CAULDRON_DX12)
		add_library(memoryallocator_dx12 SHARED ${files_dx12})
		target_compile_definitions(memoryallocator_dx12 PRIVATE D3D12MA_EXPORTS)
	endif()
    
else()

	if (CAULDRON_VK)
		add_library(memoryallocator_vk STATIC ${files_vk})
	endif()

	if (CAULDRON_DX12)
		add_library(memoryallocator_dx12 STATIC ${files_dx12})
	endif()
endif()

if (CAULDRON_VK)
	set_target_properties(memoryallocator_vk PROPERTIES LINKER_LANGUAGE CXX)
	target_link_libraries (memoryallocator_vk "Vulkan::Vulkan")
endif()
