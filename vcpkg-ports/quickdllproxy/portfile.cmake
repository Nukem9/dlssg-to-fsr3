vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Nukem9/QuickDllProxy
    REF 9607eea30e43fef62e21e5842e89ffe1b1dba25c
    SHA512 5e1af5c62c19a0f67ec40f872b675b5b5da77ca2fee3fe9f39b23679aef8f6e8f31d292d97d952b8b60a17dfddef90b15bd6b438922135014b37bffe2e219d0f
)

file(INSTALL "${SOURCE_PATH}/include" DESTINATION "${CURRENT_PACKAGES_DIR}")

# Handle copyright
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")