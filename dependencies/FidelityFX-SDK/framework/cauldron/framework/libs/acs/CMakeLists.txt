add_library(amd_acs SHARED IMPORTED GLOBAL)
set_property(TARGET amd_acs PROPERTY IMPORTED_IMPLIB ${CMAKE_CURRENT_SOURCE_DIR}/amd_acs_${CMAKE_GENERATOR_PLATFORM}.lib)

copyTargetCommand(${CMAKE_CURRENT_SOURCE_DIR}/amd_acs_${CMAKE_GENERATOR_PLATFORM}.dll ${BIN_OUTPUT} copied_amd_acs_bin)
add_dependencies(amd_acs copied_amd_acs_bin)
