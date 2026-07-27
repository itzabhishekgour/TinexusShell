find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_SERVER wayland-server)

add_executable(tinexus-comp
    main.cpp
    server/server.cpp
    server/global_registry.cpp
    server/surface_tree.cpp
    backend/backend.cpp
    renderer/renderer.cpp
    renderer/render_surface.cpp
    renderer/pixman_renderer.cpp
    surface/surface_state.cpp
    surface/configure_serial.cpp
    surface/buffer_manager.cpp
    surface/frame_callback.cpp
    surface/resource_cleanup.cpp
    surface/xdg_shell_manager.cpp
    window/scene_node.cpp
    window/window_node.cpp
    window/xdg_toplevel_node.cpp
    window/scene_graph.cpp
    window/window_manager.cpp
    input/keymap_engine.cpp
    input/interaction_controller.cpp
    input/seat_manager.cpp
    output/output_manager.cpp
    cursor/cursor_manager.cpp
    window/window_rules.cpp
    focus/focus_manager.cpp
    workspace/workspace_manager.cpp
    shell/shell_state.cpp
    render/frame_scheduler.cpp
    animation/animation.cpp
    input/shortcut_engine.cpp
    surface/surface_manager.cpp
    surface/layer_shell_manager.cpp
    surface/exclusive_zone_calculator.cpp
    server/protocol_dispatcher.cpp
    window/layer_manager.cpp
)

target_include_directories(tinexus-comp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server/include
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/include
    ${CMAKE_CURRENT_SOURCE_DIR}/output/include
    ${CMAKE_CURRENT_SOURCE_DIR}/cursor/include
    ${CMAKE_CURRENT_SOURCE_DIR}/window/include
    ${CMAKE_CURRENT_SOURCE_DIR}/focus/include
    ${CMAKE_CURRENT_SOURCE_DIR}/workspace/include
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/include
    ${CMAKE_CURRENT_SOURCE_DIR}/render/include
    ${CMAKE_CURRENT_SOURCE_DIR}/animation/include
    ${CMAKE_CURRENT_SOURCE_DIR}/input/include
    ${CMAKE_CURRENT_SOURCE_DIR}/surface/include
    ${WAYLAND_SERVER_INCLUDE_DIRS}
)

target_link_libraries(tinexus-comp PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    ${WAYLAND_SERVER_LIBRARIES}
)

install(TARGETS tinexus-comp RUNTIME DESTINATION bin)
