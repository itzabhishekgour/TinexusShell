find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

add_executable(tinexus-settings-ui
    main.cpp
    SettingsWidget.cpp
)

target_include_directories(tinexus-settings-ui PRIVATE include)

target_link_libraries(tinexus-settings-ui PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus-guard
    txui
    PkgConfig::WAYLAND_CLIENT
)

set_target_properties(tinexus-settings-ui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS tinexus-settings-ui RUNTIME DESTINATION bin)
