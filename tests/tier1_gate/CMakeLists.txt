# ==============================================================================
# tests/tier1_gate/CMakeLists.txt
#
# test-tier1-gate — Tier 1 Qt6 Migration Gate
# Validates all blockers for: tinexus-shell, tinexus-dock, tinexus-launcher
#
# GATES:
#   1. zwlr_layer_shell_v1 integration (layer-shell-qt probe)
#   2. QSocketNotifier IPC pattern (replaces txui::WaylandEventLoop::add_fd)
#   3. SpringState portable port golden-value verification
#
# Build options:
#   -DTIER1_GATE_LAYERSHELL=ON   Build WITH layer-shell-qt for live surface test
#                                (requires layer-shell-qt installed on build host)
#
# Usage:
#   cmake --build . --target test-tier1-gate
#   ./bin/test-tier1-gate                 # full test
#   ./bin/test-tier1-gate --headless      # skip layer-shell (CI safe)
#   ./bin/test-tier1-gate --ipc-messages 5000  # load test with 5000 messages
# ==============================================================================

cmake_minimum_required(VERSION 3.28)

# ── Qt6 discovery ─────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    Qml
    WaylandClient
)

# Enable Qt6 automatic tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# ── Gate binary ───────────────────────────────────────────────────────────────
add_executable(test-tier1-gate
    main.cpp
    IpcBridge.cpp      # MOC-only stub — generates QObject vtable for IpcBridge
    SpringBridge.cpp   # MOC-only stub — generates QObject vtable for SpringBridge
)

target_include_directories(test-tier1-gate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/ipcd/include   # for protocol/header.hpp, dock_protocol.hpp
    ${CMAKE_SOURCE_DIR}/include            # for txui/animation/SpringState.hpp (golden ref)
)

target_link_libraries(test-tier1-gate PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::WaylandClient
    tinexus_warnings
)

target_compile_features(test-tier1-gate PRIVATE cxx_std_20)

# ── Optional: layer-shell-qt live surface test ────────────────────────────────
option(TIER1_GATE_LAYERSHELL
    "Build test-tier1-gate with layer-shell-qt for live zwlr_layer_shell_v1 surface test"
    OFF)

if(TIER1_GATE_LAYERSHELL)
    # Try both KDE and standalone layer-shell-qt package names
    find_package(LayerShellQt QUIET)
    if(NOT LayerShellQt_FOUND)
        # Ubuntu/Debian: libqt6-layer-shell-dev or manually installed
        find_library(LAYERSHELLQT_LIB NAMES LayerShellQt LayerShellQtInterface
            PATHS /usr/lib /usr/local/lib)
        find_path(LAYERSHELLQT_INCLUDE NAMES LayerShellQt/Window
            PATHS /usr/include /usr/local/include)
        if(LAYERSHELLQT_LIB AND LAYERSHELLQT_INCLUDE)
            message(STATUS "tier1-gate: Found layer-shell-qt manually at ${LAYERSHELLQT_LIB}")
            target_include_directories(test-tier1-gate PRIVATE ${LAYERSHELLQT_INCLUDE})
            target_link_libraries(test-tier1-gate PRIVATE ${LAYERSHELLQT_LIB})
            target_compile_definitions(test-tier1-gate PRIVATE TIER1_GATE_HAS_LAYERSHELL=1)
        else()
            message(WARNING
                "tier1-gate: TIER1_GATE_LAYERSHELL=ON but layer-shell-qt not found.\n"
                "  Install: pacman -S layer-shell-qt6  OR  apt install qt6-layer-shell\n"
                "  Then re-run cmake. Building without live layer-shell test.")
        endif()
    else()
        message(STATUS "tier1-gate: Using LayerShellQt::Interface from find_package()")
        target_link_libraries(test-tier1-gate PRIVATE LayerShellQt::Interface)
        target_compile_definitions(test-tier1-gate PRIVATE TIER1_GATE_HAS_LAYERSHELL=1)
    endif()
else()
    message(STATUS "tier1-gate: TIER1_GATE_LAYERSHELL=OFF — skipping live layer-shell "
        "surface creation (add -DTIER1_GATE_LAYERSHELL=ON to enable)")
endif()

# ── Output directory ──────────────────────────────────────────────────────────
set_target_properties(test-tier1-gate PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── CTest integration ─────────────────────────────────────────────────────────
add_test(NAME tier1-gate-headless
    COMMAND test-tier1-gate --headless --ipc-messages 1000)

add_test(NAME tier1-spring-golden
    COMMAND test-tier1-gate --headless --ipc-messages 0)

add_test(NAME tier1-dock-render
    COMMAND test-dock-render)

add_test(NAME tier1-shell-render
    COMMAND test-shell-render)

add_test(NAME tier1-launcher-render
    COMMAND test-launcher-render)

set_tests_properties(tier1-gate-headless tier1-spring-golden tier1-dock-render tier1-shell-render tier1-launcher-render PROPERTIES
    LABELS "migration;tier1;gate;render"
    TIMEOUT 60)

message(STATUS "tier1-gate: CMakeLists.txt configured.")
message(STATUS "  Build: cmake --build . --target test-tier1-gate")
message(STATUS "  Run:   ./bin/test-tier1-gate [--headless] [--ipc-messages N]")
