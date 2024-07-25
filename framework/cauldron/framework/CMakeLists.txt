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

if(CAULDRON_VK)
function (CheckVulkanSDKVersion)
    # set the desired version
    set(vulkan_min_version "1.3.250")

    if (${Vulkan_VERSION} VERSION_LESS ${vulkan_min_version})
        message(FATAL_ERROR "Vulkan SDK ${vulkan_min_version} or above is required. Found Vulkan SDK ${Vulkan_VERSION}")
    endif()
endfunction()
endif()

# Reference libs used
add_subdirectory(libs/agilitysdk)
add_subdirectory(libs/acs)
add_subdirectory(libs/ags)
add_subdirectory(libs/dxc)
add_subdirectory(libs/dxheaders)
add_subdirectory(libs/imgui)
add_subdirectory(libs/json)
add_subdirectory(libs/memoryallocator)
add_subdirectory(libs/renderdoc)
add_subdirectory(libs/stb)
add_subdirectory(libs/vectormath)
add_subdirectory(libs/pix)

if(CAULDRON_VK)
#Add sources
find_package(Vulkan REQUIRED)
CheckVulkanSDKVersion()
endif()

add_subdirectory(src)
