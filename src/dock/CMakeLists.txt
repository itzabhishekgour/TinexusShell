cmake_minimum_required(VERSION 3.28)
project(tinexus-dock VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    Qml
    DBus
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
    DockModel.cpp
    include/dock/DockModel.hpp
    include/dock/DockBridge.hpp
    ToplevelTracker.cpp
    include/dock/ToplevelTracker.hpp
    DockMenuPopup.cpp
    include/dock/DockMenuPopup.hpp
    DockWindow.cpp
    include/dock/DockWindow.hpp
    DnDHandler.cpp
    include/dock/DnDHandler.hpp
    StacksModel.cpp
    include/dock/StacksModel.hpp
    StacksPopup.cpp
    include/dock/StacksPopup.hpp
    DockIpcClient.cpp
    include/dock/DockIpcClient.hpp
    wayland/protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.c
    wayland/protocol/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h
)

target_include_directories(tinexus-dock PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/wayland/protocol
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_BINARY_DIR}/protocols
)

target_link_libraries(tinexus-dock PRIVATE
    tinexus_common
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::DBus
    Qt6::WaylandClient
    tinexus_warnings
)

if(LayerShellQt_FOUND)
    target_link_libraries(tinexus-dock PRIVATE LayerShellQt::Interface)
    target_compile_definitions(tinexus-dock PRIVATE HAVE_LAYERSHELL=1)
    message(STATUS "tinexus-dock: LayerShellQt linked (Layer=Top, Scope=dock)")
else()
    message(STATUS "tinexus-dock: LayerShellQt not found — running in fallback QWindow mode")
endif()

set_target_properties(tinexus-dock PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Offscreen Visual Render Test ──────────────────────────────────────────────
add_executable(test-dock-render
    test_dock_render.cpp
    DockBridge.cpp
    DockBridge.hpp
    DockModel.cpp
    include/dock/DockModel.hpp
    include/dock/DockBridge.hpp
    ToplevelTracker.cpp
    include/dock/ToplevelTracker.hpp
    DockMenuPopup.cpp
    include/dock/DockMenuPopup.hpp
    DockWindow.cpp
    include/dock/DockWindow.hpp
    DnDHandler.cpp
    include/dock/DnDHandler.hpp
    StacksModel.cpp
    include/dock/StacksModel.hpp
    StacksPopup.cpp
    include/dock/StacksPopup.hpp
    DockIpcClient.cpp
    include/dock/DockIpcClient.hpp
    wayland/protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.c
    wayland/protocol/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h
)

target_include_directories(test-dock-render PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/wayland/protocol
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_BINARY_DIR}/protocols
)

target_link_libraries(test-dock-render PRIVATE
    tinexus_common
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::DBus
    Qt6::WaylandClient
    tinexus_warnings
)

set_target_properties(test-dock-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

install(TARGETS tinexus-dock DESTINATION bin)
