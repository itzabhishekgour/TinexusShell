cmake_minimum_required(VERSION 3.28)
project(tinexus-shell VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    Qml
    DBus
    WaylandClient
    Svg
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(LayerShellQt QUIET)

# ── Main Shell Binary ─────────────────────────────────────────────────────────
add_executable(tinexus-shell
    main.cpp
    ShellBridge.cpp
    ShellBridge.hpp
)

target_include_directories(tinexus-shell PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(tinexus-shell PRIVATE
    tinexus_common
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::DBus
    Qt6::WaylandClient
    Qt6::Svg
    tinexus_warnings
)

if(LayerShellQt_FOUND)
    target_link_libraries(tinexus-shell PRIVATE LayerShellQt::Interface)
    target_compile_definitions(tinexus-shell PRIVATE HAVE_LAYERSHELL=1)
    message(STATUS "tinexus-shell: LayerShellQt linked")
else()
    message(STATUS "tinexus-shell: LayerShellQt not found — running in fallback QWindow mode")
endif()

install(TARGETS tinexus-shell RUNTIME DESTINATION bin)

# ── Offscreen Visual Render Test ──────────────────────────────────────────────
add_executable(test-shell-render
    test_shell_render.cpp
    ShellBridge.cpp
    ShellBridge.hpp
)

target_include_directories(test-shell-render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(test-shell-render PRIVATE
    tinexus_common
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::DBus
    Qt6::Svg
    tinexus_warnings
)

set_target_properties(test-shell-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Live Functional Verification Test ─────────────────────────────────────────
add_executable(test-shell-live
    test_shell_live.cpp
    ShellBridge.cpp
    ShellBridge.hpp
)
target_include_directories(test-shell-live PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-shell-live PRIVATE
    tinexus_common
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::DBus
    Qt6::Svg
    tinexus_warnings
)
set_target_properties(test-shell-live PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

