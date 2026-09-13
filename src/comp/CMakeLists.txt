set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.19)
pkg_check_modules(DRM REQUIRED IMPORTED_TARGET libdrm)
pkg_check_modules(VULKAN REQUIRED IMPORTED_TARGET vulkan)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)

add_executable(tinexus-comp
    main.cpp
    server/server.cpp
    server/global_registry.cpp
    server/surface_tree.cpp
    backend/headless_backend.cpp
    backend/wlroots_backend.cpp
    backend/drm_backend.cpp
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
    window/decoration_manager.cpp
    window/window_manager.cpp
    window/MinimizeAnimation.cpp
    window/RestoreAnimation.cpp
    input/keymap_engine.cpp
    input/interaction_controller.cpp
    input/seat_manager.cpp
    input/udev_monitor.cpp
    output/output.cpp
    output/output_manager.cpp
    cursor/cursor_manager.cpp
    window/window_rules.cpp
    focus/focus_manager.cpp
    workspace/workspace_manager.cpp
    shell/shell_state.cpp
    render/frame_scheduler.cpp
    animation/animation.cpp
    animation/animation_manager.cpp
    input/shortcut_engine.cpp
    surface/surface_manager.cpp
    surface/layer_shell_manager.cpp
    surface/exclusive_zone_calculator.cpp
    server/protocol_dispatcher.cpp
    window/layer_manager.cpp
    render/damage_tracker.cpp
    output/output_layout.cpp
    renderer/renderer_factory.cpp
    renderer/render_target.cpp
    renderer/gpu_resource_manager.cpp
    renderer/vulkan_renderer.cpp
    renderer/opengl_renderer.cpp
    renderer/blur_pass.cpp
    renderer/shadow_generator.cpp
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
    ../ipcd/include
)

add_compile_definitions(WLR_USE_UNSTABLE=1)

target_link_libraries(tinexus-comp PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    tinexus_protocols
    PkgConfig::WAYLAND_SERVER
    PkgConfig::WLROOTS
    PkgConfig::DRM
    PkgConfig::VULKAN
    PkgConfig::XKBCOMMON
    PkgConfig::PIXMAN
)

install(TARGETS tinexus-comp RUNTIME DESTINATION bin)
