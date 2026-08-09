find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

add_executable(tinexus-app-installer
    main.cpp
    InstallerWidget.cpp
)

target_include_directories(tinexus-app-installer PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
)

target_link_libraries(tinexus-app-installer PRIVATE
    tinexus_common
    tinexus_warnings
    txui
    PkgConfig::WAYLAND_CLIENT
)

set_target_properties(tinexus-app-installer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS tinexus-app-installer RUNTIME DESTINATION bin)
