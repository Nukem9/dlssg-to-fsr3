cmake_minimum_required(VERSION 3.26)

project(
    dlssg-to-fsr3
    VERSION 0.2
    LANGUAGES CXX)

set(PROJECT_DEPENDENCIES_PATH "${CMAKE_CURRENT_LIST_DIR}/dependencies")
set(PROJECT_RESOURCES_PATH "${CMAKE_CURRENT_LIST_DIR}/resources")
set(PROJECT_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/source")

#
# Set up the actual library
#
add_subdirectory("${PROJECT_SOURCE_PATH}/maindll")

#
# And finally produce build artifacts
#
if(BUILD_FOR_DLSSTWEAKS)
    add_subdirectory("${PROJECT_SOURCE_PATH}/dtwrapper")

    set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR} (DLSSTweaks Edition)")

    install(
        TARGETS
            dlsstweaks_wrapper_output_dll
        RUNTIME
            DESTINATION "/"
    )

    install(
        FILES
            "${PROJECT_RESOURCES_PATH}/READ ME (DLSSTweaks).txt"
        DESTINATION "/"
    )
else()
    add_subdirectory("${PROJECT_SOURCE_PATH}/sigbreaker")

    set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}")

    install(
        TARGETS
            sigbreaker_output_dll
        RUNTIME
            DESTINATION "/"
    )

    install(
        FILES
            "${PROJECT_RESOURCES_PATH}/READ ME.txt"
        DESTINATION "/"
    )
endif()

install(
    TARGETS
        dlssg_output_dll
    RUNTIME
        DESTINATION "/"
)

set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
