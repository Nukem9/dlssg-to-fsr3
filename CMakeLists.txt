cmake_minimum_required(VERSION 3.26)

project(
    dlssg-to-fsr3
    VERSION 0.70
    LANGUAGES CXX
)

set(PROJECT_DEPENDENCIES_PATH "${CMAKE_CURRENT_LIST_DIR}/dependencies")
set(PROJECT_RESOURCES_PATH "${CMAKE_CURRENT_LIST_DIR}/resources")
set(PROJECT_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/source")

#
# Store the current git commit hash for later use
#
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE BUILD_GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

#
# Set up the actual library
#
add_subdirectory("${PROJECT_SOURCE_PATH}/maindll")

#
# And finally produce build artifacts
#
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}")

if(BUILD_OUTPUT_WRAPPER STREQUAL "DLSSTWEAKS")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME} (DLSSTweaks Edition)")

    add_subdirectory("${PROJECT_SOURCE_PATH}/wrapper_dt")
    install(TARGETS dlsstweaks_wrapper_output_dll RUNTIME DESTINATION "/")

    install(
        FILES
            "${PROJECT_RESOURCES_PATH}/READ ME (DLSSTweaks).txt"
        DESTINATION
            "/"
)
elseif(BUILD_OUTPUT_WRAPPER STREQUAL "NVNGX")

    add_subdirectory("${PROJECT_SOURCE_PATH}/wrapper_nvngx")
    install(TARGETS nvngx_wrapper_output_dll RUNTIME DESTINATION "/")

    install(
        FILES
            "${PROJECT_RESOURCES_PATH}/READ ME (Nvngx).txt"
            "${PROJECT_RESOURCES_PATH}/DisableNvidiaSignatureChecks.reg"
            "${PROJECT_RESOURCES_PATH}/RestoreNvidiaSignatureChecks.reg"
        DESTINATION
            "/"
)
endif()

install(
    FILES
        "${PROJECT_RESOURCES_PATH}/binary_dist_license.txt"
    DESTINATION
        "/"
    RENAME
        "LICENSE.txt"
)

install(
    TARGETS
        dlssg_output_dll
    RUNTIME
        DESTINATION "/"
)

set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
