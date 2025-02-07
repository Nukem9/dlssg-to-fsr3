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


copyTargetCommand("${winpixevent_binary}" ${BIN_OUTPUT} copied_winpixevent_bin)
copyTargetCommand("${winpixgpucapturer_binary}" ${BIN_OUTPUT} winpixgpucapturer_binary_bin)

add_dependencies(pixlib copied_winpixevent_bin winpixgpucapturer_binary_bin)
