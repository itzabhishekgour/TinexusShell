find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

find_package(Qt6 COMPONENTS Core Gui Quick Qml QuickControls2 DBus QUIET)

if(Qt6_FOUND)
    # Qt6 QML Settings Application
    add_executable(tinexus-settings-ui
        qt/main_qt.cpp
        qt/SettingsBridge.cpp
        qt/SettingsAdaptor.cpp
        qt/SettingsAdaptor.hpp
        include/settings-ui/SettingsAdaptor.hpp
        WifiManager.cpp
    )

    set_target_properties(tinexus-settings-ui PROPERTIES
        AUTOMOC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    target_include_directories(tinexus-settings-ui PRIVATE include qt ${CMAKE_SOURCE_DIR}/include)

    target_link_libraries(tinexus-settings-ui PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::DBus
        tinexus_common
        tinexus_warnings
        tinexus-guard
    )
    install(TARGETS tinexus-settings-ui RUNTIME DESTINATION bin)

    # Qt6 Visual Test Renderers
    add_executable(test-settings-render
        test_settings_render.cpp
        qt/SettingsBridge.cpp
        qt/SettingsAdaptor.cpp
        qt/SettingsAdaptor.hpp
        include/settings-ui/SettingsAdaptor.hpp
        WifiManager.cpp
    )
    set_target_properties(test-settings-render PROPERTIES
        AUTOMOC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_include_directories(test-settings-render PRIVATE include qt ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(test-settings-render PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::DBus
        tinexus_common
        tinexus_warnings
        tinexus-guard
    )

    add_executable(test-settings-wifi-render
        test_settings_wifi_render.cpp
        qt/SettingsBridge.cpp
        qt/SettingsAdaptor.cpp
        qt/SettingsAdaptor.hpp
        include/settings-ui/SettingsAdaptor.hpp
        WifiManager.cpp
    )
    set_target_properties(test-settings-wifi-render PROPERTIES
        AUTOMOC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_include_directories(test-settings-wifi-render PRIVATE include qt ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(test-settings-wifi-render PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::DBus
        tinexus_common
        tinexus_warnings
        tinexus-guard
    )
    add_executable(test-about-live
        qt/test_about_live.cpp
        qt/SettingsBridge.cpp
        qt/SettingsAdaptor.cpp
        qt/SettingsAdaptor.hpp
        include/settings-ui/SettingsAdaptor.hpp
        WifiManager.cpp
    )
    set_target_properties(test-about-live PROPERTIES
        AUTOMOC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_include_directories(test-about-live PRIVATE include qt ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(test-about-live PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::DBus
        tinexus_common
        tinexus_warnings
        tinexus-guard
    )
endif()

# Wi-Fi backend contract tests (pure C++20 network logic)
add_executable(test-wifi-backend-contracts
    ${CMAKE_SOURCE_DIR}/tests/unit/test_wifi_backend_contracts.cpp
    WifiManager.cpp
)
target_include_directories(test-wifi-backend-contracts PRIVATE include)
target_link_libraries(test-wifi-backend-contracts PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus-guard
)
set_target_properties(test-wifi-backend-contracts PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

