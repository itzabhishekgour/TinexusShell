cmake_minimum_required(VERSION 3.28)
project(tinexus-dock VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    Qml
    WaylandClient
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(LayerShellQt QUIET)

# ── Main Dock Binary ──────────────────────────────────────────────────────────
add_executable(tinexus-dock
    main.cpp
    DockBridge.cpp
    DockBridge.hpp
)

target_include_directories(tinexus-dock PRIVATE 
    ${CMAKE_SOURCE_DIR}/include 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(tinexus-dock PRIVATE
    tinexus_common
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::WaylandClient
    tinexus_warnings
)

if(LayerShellQt_FOUND)
    target_link_libraries(tinexus-dock PRIVATE LayerShellQt::Interface)
    target_compile_definitions(tinexus-dock PRIVATE HAVE_LAYERSHELL=1)
    message(STATUS "tinexus-dock: LayerShellQt linked")
else()
    message(STATUS "tinexus-dock: LayerShellQt not found — running in fallback QWindow mode")
endif()

# ── Offscreen Visual Render Test ──────────────────────────────────────────────
add_executable(test-dock-render
    test_dock_render.cpp
    DockBridge.cpp
    DockBridge.hpp
)

target_include_directories(test-dock-render PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(test-dock-render PRIVATE
    tinexus_common
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    tinexus_warnings
)

set_target_properties(test-dock-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

install(TARGETS tinexus-dock DESTINATION bin)
