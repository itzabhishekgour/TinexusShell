find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

add_executable(tinexus-settings-ui
    main.cpp
    SettingsWidget.cpp
    WifiManager.cpp
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

add_executable(test-settings-wifi-render
    test_settings_wifi_render.cpp
    SettingsWidget.cpp
    WifiManager.cpp
)
target_include_directories(test-settings-wifi-render PRIVATE include)
target_link_libraries(test-settings-wifi-render PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus-guard
    txui
    PkgConfig::WAYLAND_CLIENT
)
set_target_properties(test-settings-wifi-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(test-wifi-backend-contracts
    ${CMAKE_SOURCE_DIR}/tests/unit/test_wifi_backend_contracts.cpp
    WifiManager.cpp
)
target_include_directories(test-wifi-backend-contracts PRIVATE include)
target_link_libraries(test-wifi-backend-contracts PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus-guard
    txui
)
set_target_properties(test-wifi-backend-contracts PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

