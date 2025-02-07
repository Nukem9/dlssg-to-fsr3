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

add_library(pixlib SHARED IMPORTED GLOBAL)
set_property(TARGET pixlib PROPERTY IMPORTED_IMPLIB ${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_GENERATOR_PLATFORM}/WinPixEventRuntime.lib)

set(winpixevent_binary
    ${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_GENERATOR_PLATFORM}/WinPixEventRuntime.dll)

set(winpixgpucapturer_binary
    ${CMAKE_CURRENT_SOURCE_DIR}/bin/WinPixGpuCapturer.dll)

# Copies a set of files to a directory (only if they already don't exist or are different)
#
# Usage example:
#     add_library(my_lib ...)
#     set(media_src ${CMAKE_CURRENT_SOURCE_DIR}/../../media/brdfLut.dds )
#     copyTargetCommand("${media_src}" ${CMAKE_HOME_DIRECTORY}/bin copied_media_src)
#     add_dependencies(my_lib copied_media_src)
#
function(copyTargetCommand list dest returned_target_name)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    foreach(fullFileName ${list})
        get_filename_component(file ${fullFileName} NAME)
        message("Generating custom command for ${fullFileName}")
        add_custom_command(
            OUTPUT   ${dest}/${file}
            PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${dest}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${fullFileName} ${dest}
            MAIN_DEPENDENCY  ${fullFileName}
            COMMENT "Updating ${file} into ${dest}"
        )
        list(APPEND dest_list ${dest}/${file})
    endforeach()

    add_custom_target(${returned_target_name} DEPENDS "${dest_list}")
    set_target_properties(${returned_target_name} PROPERTIES FOLDER CopyTargets)
endfunction()

copyTargetCommand("${winpixevent_binary}" ${FFX_BIN_PATH} copied_winpixevent_bin)

add_dependencies(pixlib copied_winpixevent_bin)
