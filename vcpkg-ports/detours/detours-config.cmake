if(TARGET detours::detours)
    return()
endif()

get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH) # detours
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH) # share
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH) # package root
add_library(detours::detours STATIC IMPORTED)
set_target_properties(detours::detours PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include")
set(detours_FOUND 1)

get_filename_component(_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
file(GLOB CONFIG_FILES "${_DIR}/detours-targets-*.cmake")
foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()
