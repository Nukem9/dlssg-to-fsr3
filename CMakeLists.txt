cmake_minimum_required(VERSION 3.26)

project(
    dlssg-to-fsr3
    VERSION 0.81
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
# Always build the main library
#
add_subdirectory("${PROJECT_SOURCE_PATH}/maindll")

#
# Then set up proxies/wrappers and install everything
#
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}")

function(install_universal_variant PROXYDLL IMPLDLL ROOTDIR READMEDIR DLLNAME)

    if (NOT DLLNAME STREQUAL "")
        install(FILES "$<TARGET_FILE:${PROXYDLL}>" DESTINATION "${ROOTDIR}/" RENAME "${DLLNAME}")
    else()
        install(TARGETS ${PROXYDLL} RUNTIME DESTINATION "${ROOTDIR}/")
    endif()
    install(TARGETS ${IMPLDLL} RUNTIME DESTINATION "${ROOTDIR}/")
    install(FILES "${PROJECT_RESOURCES_PATH}/${READMEDIR}" DESTINATION "${ROOTDIR}/" RENAME "READ ME.txt")

endfunction()

if(BUILD_OUTPUT_WRAPPER STREQUAL "UNIVERSAL")

    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME} (Universal)")

    add_subdirectory("${PROJECT_SOURCE_PATH}/wrapper_generic")
    install_universal_variant(generic_wrapper_output_dll dlssg_output_dll "plugin_red4ext" "read_me_generic_plugin.txt" "dlssg_to_fsr3.dll")
    install_universal_variant(generic_wrapper_output_dll dlssg_output_dll "plugin_asi_loader" "read_me_generic_plugin.txt" "dlssg_to_fsr3.asi")
    install_universal_variant(generic_wrapper_output_dll dlssg_output_dll "dll_version" "read_me_generic_dll.txt" "version.dll")
    install_universal_variant(generic_wrapper_output_dll dlssg_output_dll "dll_winhttp" "read_me_generic_dll.txt" "winhttp.dll")
    install_universal_variant(generic_wrapper_output_dll dlssg_output_dll "dll_dbghelp" "read_me_generic_dll.txt" "dbghelp.dll")

elseif(BUILD_OUTPUT_WRAPPER STREQUAL "DLSSTWEAKS")

    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME} (DLSSTweaks Edition)")

    add_subdirectory("${PROJECT_SOURCE_PATH}/wrapper_dt")
    install_universal_variant(dlsstweaks_wrapper_output_dll dlssg_output_dll "" "read_me_dlsstweaks.txt" "")

elseif(BUILD_OUTPUT_WRAPPER STREQUAL "NVNGX")

    add_subdirectory("${PROJECT_SOURCE_PATH}/wrapper_generic")
    install_universal_variant(generic_wrapper_output_dll dlssg_output_dll "" "read_me_nvngx.txt" "")

    install(
        FILES
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

set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
