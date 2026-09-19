cmake_minimum_required(VERSION 3.28)
project(tinexus-launcher VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    Qml
    DBus
    Network
    WaylandClient
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(LayerShellQt QUIET)

# ── Main Launcher Binary ──────────────────────────────────────────────────────
add_executable(tinexus-launcher
    main.cpp
    LauncherBridge.cpp
    LauncherBridge.hpp
)

target_include_directories(tinexus-launcher PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
)

target_link_libraries(tinexus-launcher PRIVATE
    tinexus_common
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::DBus
    Qt6::Network
    Qt6::WaylandClient
    tinexus_warnings
)

if(LayerShellQt_FOUND)
    target_link_libraries(tinexus-launcher PRIVATE LayerShellQt::Interface)
    target_compile_definitions(tinexus-launcher PRIVATE HAVE_LAYERSHELL=1)
    message(STATUS "tinexus-launcher: LayerShellQt linked")
else()
    message(STATUS "tinexus-launcher: LayerShellQt not found — running in fallback QWindow mode")
endif()

install(TARGETS tinexus-launcher DESTINATION bin)

# ── Offscreen Visual Render Test ──────────────────────────────────────────────
add_executable(test-launcher-render
    test_launcher_render.cpp
    LauncherBridge.cpp
    LauncherBridge.hpp
)

target_include_directories(test-launcher-render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
)

target_link_libraries(test-launcher-render PRIVATE
    tinexus_common
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::DBus
    tinexus_warnings
)

set_target_properties(test-launcher-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Live Functional Verification Test ─────────────────────────────────────────
add_executable(test-launcher-live
    test_launcher_live.cpp
    LauncherBridge.cpp
    LauncherBridge.hpp
)
target_include_directories(test-launcher-live PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
)
target_link_libraries(test-launcher-live PRIVATE
    tinexus_common
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::Network
    Qt6::DBus
    tinexus_warnings
)
set_target_properties(test-launcher-live PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
