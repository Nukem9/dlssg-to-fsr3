cmake_minimum_required(VERSION 3.26)

project(
    dlssg-to-fsr3
    VERSION 1.0
    LANGUAGES CXX)

set(PROJECT_DEPENDENCIES_PATH "${CMAKE_CURRENT_LIST_DIR}/dependencies")
set(PROJECT_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/source")

#
# Set up the actual library
#
add_subdirectory("${PROJECT_SOURCE_PATH}/maindll")
#add_subdirectory("${PROJECT_SOURCE_PATH}/sigbreaker")

#
# And finally produce build artifacts
#
install(
    TARGETS
        dlssg_output_dll
#	sigbreaker_output_dll
    RUNTIME
        DESTINATION "/"
)

set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
