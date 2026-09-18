find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_SERVER wayland-server)
pkg_check_modules(WLROOTS IMPORTED_TARGET wlroots-0.19)
pkg_check_modules(XKBCOMMON IMPORTED_TARGET xkbcommon)
pkg_check_modules(PIXMAN IMPORTED_TARGET pixman-1)

add_executable(unit_test_common
    unit/test_common.cpp
)

target_link_libraries(unit_test_common PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME unit_test_common COMMAND unit_test_common)

add_executable(unit_test_graphics_probe
    unit/test_graphics_probe.cpp
)

target_link_libraries(unit_test_graphics_probe PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME unit_test_graphics_probe COMMAND unit_test_graphics_probe)

if(ENABLE_LEGACY_TXUI)
    if(NOT TARGET unit_test_txui_clipboard)
    add_executable(unit_test_txui_clipboard
        unit/test_txui_clipboard.cpp
    )

    target_link_libraries(unit_test_txui_clipboard PRIVATE
        txui
        tinexus_common
        tinexus_warnings
        tinexus_sanitizers
    )

    add_test(NAME unit_test_txui_clipboard COMMAND unit_test_txui_clipboard)
    endif()
endif()

add_executable(unit_test_wifi_contracts
    unit/test_wifi_backend_contracts.cpp
    ${CMAKE_SOURCE_DIR}/src/settings-ui/WifiManager.cpp
)

target_include_directories(unit_test_wifi_contracts PRIVATE
    ${CMAKE_SOURCE_DIR}/src/settings-ui/include
)

target_link_libraries(unit_test_wifi_contracts PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME unit_test_wifi_contracts COMMAND unit_test_wifi_contracts)

add_executable(unit_test_searchd
    unit/test_searchd.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/desktop_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/ram_snapshot.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/normalizer.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/intent_detector.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/cache.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/ranking_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/provider_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/providers/calculator_provider.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/providers/system_provider.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/session.cpp
)

target_include_directories(unit_test_searchd PRIVATE
    ${CMAKE_SOURCE_DIR}/src/indexer/include
    ${CMAKE_SOURCE_DIR}/src/searchd/include
)

target_link_libraries(unit_test_searchd PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    SQLite::SQLite3
)

add_test(NAME unit_test_searchd COMMAND unit_test_searchd)

add_executable(unit_test_serviced
    unit/test_serviced.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/daemon_spec.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/dep_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/event_journal.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/heartbeat_watchdog.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/process_manager.cpp
)

target_include_directories(unit_test_serviced PRIVATE
    ${CMAKE_SOURCE_DIR}/src/serviced/include
)

target_link_libraries(unit_test_serviced PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME unit_test_serviced COMMAND unit_test_serviced)

if(FALSE)
add_executable(unit_test_comp
    unit/test_comp.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/global_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/surface_tree.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/backend.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/pixman_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/render/damage_tracker.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/xdg_toplevel_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/keymap_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/interaction_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/seat_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/output/output_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/cursor/cursor_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_rules.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/focus/focus_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/workspace/workspace_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/shell/shell_state.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/render/frame_scheduler.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/animation/animation.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/shortcut_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_manager.cpp
)

target_include_directories(unit_test_comp PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/server/include
    ${CMAKE_SOURCE_DIR}/src/comp/backend/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
    ${CMAKE_SOURCE_DIR}/src/comp/output/include
    ${CMAKE_SOURCE_DIR}/src/comp/cursor/include
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    /src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/focus/include
    ${CMAKE_SOURCE_DIR}/src/comp/workspace/include
    ${CMAKE_SOURCE_DIR}/src/comp/shell/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
)

target_compile_definitions(unit_test_comp PRIVATE WLR_USE_UNSTABLE=1)

target_link_libraries(unit_test_comp PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    tinexus_protocols
    PkgConfig::WAYLAND_SERVER
    PkgConfig::WLROOTS
    PkgConfig::XKBCOMMON
    PkgConfig::PIXMAN
)

add_test(NAME unit_test_comp COMMAND unit_test_comp)

add_executable(unit_test_launcher
    unit/test_launcher.cpp
    ${CMAKE_SOURCE_DIR}/src/launcher/theme_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/launcher/search_model.cpp
    ${CMAKE_SOURCE_DIR}/src/launcher/ipc_client.cpp
    ${CMAKE_SOURCE_DIR}/src/launcher/launcher_controller.cpp
)

target_include_directories(unit_test_launcher PRIVATE
    ${CMAKE_SOURCE_DIR}/src/launcher/include
)

target_link_libraries(unit_test_launcher PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME unit_test_launcher COMMAND unit_test_launcher)
endif()

add_executable(unit_test_sdk
    unit/test_sdk.cpp
)

target_link_libraries(unit_test_sdk PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME unit_test_sdk COMMAND unit_test_sdk)

add_executable(integration_test_runtime
    integration/test_runtime_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/daemon_spec.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/dep_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/event_journal.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/heartbeat_watchdog.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/process_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/serviced/launch_authority.cpp
)

target_include_directories(integration_test_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/src/serviced/include
    ${CMAKE_SOURCE_DIR}/src/guard/include
)

target_link_libraries(integration_test_runtime PRIVATE
    tinexus-guard
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_runtime COMMAND integration_test_runtime)

add_executable(integration_test_wayland
    integration/test_wayland_server.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_manager.cpp
)

target_include_directories(integration_test_wayland PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
)

target_link_libraries(integration_test_wayland PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_wayland COMMAND integration_test_wayland)

add_executable(integration_test_alpha_stress
    integration/test_alpha_stress.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_rules.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/focus/focus_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_manager.cpp
)

target_include_directories(integration_test_alpha_stress PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/focus/include
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
    ${WLROOTS_INCLUDE_DIRS}
)

target_compile_definitions(integration_test_alpha_stress PRIVATE WLR_USE_UNSTABLE=1)

target_link_libraries(integration_test_alpha_stress PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    PkgConfig::WLROOTS
    PkgConfig::XKBCOMMON
    PkgConfig::PIXMAN
)

add_test(NAME integration_test_alpha_stress COMMAND integration_test_alpha_stress)

add_executable(integration_test_session
    integration/test_session_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/session/env_bootstrap.cpp
    ${CMAKE_SOURCE_DIR}/src/session/autostart_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/session/session_manager.cpp
)

target_include_directories(integration_test_session PRIVATE
    ${CMAKE_SOURCE_DIR}/src/session/include
)

target_link_libraries(integration_test_session PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_session COMMAND integration_test_session)

add_executable(integration_test_files
    integration/test_tinexus_files.cpp
    ${CMAKE_SOURCE_DIR}/src/files/file_model.cpp
    ${CMAKE_SOURCE_DIR}/src/files/column_view_model.cpp
    ${CMAKE_SOURCE_DIR}/src/files/trash_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/files/file_operations.cpp
)

target_include_directories(integration_test_files PRIVATE
    ${CMAKE_SOURCE_DIR}/src/files/include
)

target_link_libraries(integration_test_files PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_files COMMAND integration_test_files)

add_executable(integration_test_settings
    integration/test_settings_daemon.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/config_store.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/schema_validator.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/settings_daemon.cpp
)

target_include_directories(integration_test_settings PRIVATE
    ${CMAKE_SOURCE_DIR}/src/settings/include
)

target_link_libraries(integration_test_settings PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_settings COMMAND integration_test_settings)

add_executable(integration_test_notifications
    integration/test_notifications.cpp
    ${CMAKE_SOURCE_DIR}/src/notifications/notification_item.cpp
    ${CMAKE_SOURCE_DIR}/src/notifications/dnd_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/notifications/notification_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/notifications/notification_server.cpp
)

target_include_directories(integration_test_notifications PRIVATE
    ${CMAKE_SOURCE_DIR}/src/notifications/include
)

target_link_libraries(integration_test_notifications PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_notifications COMMAND integration_test_notifications)

add_executable(integration_test_clipboard
    integration/test_clipboard.cpp
    ${CMAKE_SOURCE_DIR}/src/clipboard/clipboard_item.cpp
    ${CMAKE_SOURCE_DIR}/src/clipboard/privacy_filter.cpp
    ${CMAKE_SOURCE_DIR}/src/clipboard/clipboard_manager.cpp
)

target_include_directories(integration_test_clipboard PRIVATE
    ${CMAKE_SOURCE_DIR}/src/clipboard/include
)

target_link_libraries(integration_test_clipboard PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_clipboard COMMAND integration_test_clipboard)

add_executable(integration_test_terminal
    integration/test_terminal.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/pty_process.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/ansi_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminal_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminal_renderer.cpp
)

target_include_directories(integration_test_terminal PRIVATE
    ${CMAKE_SOURCE_DIR}/src/terminal/include
)

target_link_libraries(integration_test_terminal PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_terminal COMMAND integration_test_terminal)

add_executable(integration_test_monitor
    integration/test_monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/metrics_snapshot.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/cpu_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/memory_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/disk_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/network_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/process_tree.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/process_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/resource_monitor.cpp
)

target_include_directories(integration_test_monitor PRIVATE
    ${CMAKE_SOURCE_DIR}/src/monitor/include
)

target_link_libraries(integration_test_monitor PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_monitor COMMAND integration_test_monitor)

add_executable(integration_test_login
    integration/test_login.cpp
    ${CMAKE_SOURCE_DIR}/src/login/pam_authenticator.cpp
    ${CMAKE_SOURCE_DIR}/src/login/session_launcher.cpp
    ${CMAKE_SOURCE_DIR}/src/login/login_model.cpp
    ${CMAKE_SOURCE_DIR}/src/login/login_view.cpp
    ${CMAKE_SOURCE_DIR}/src/login/login_controller.cpp
)

target_include_directories(integration_test_login PRIVATE
    ${CMAKE_SOURCE_DIR}/src/login/include
)

target_link_libraries(integration_test_login PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_login COMMAND integration_test_login)

add_executable(integration_test_pkg
    integration/test_pkg.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/package_manifest.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/crypto_verifier.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/dependency_solver.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/package_database.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/transaction_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/package_manager.cpp
)

target_include_directories(integration_test_pkg PRIVATE
    ${CMAKE_SOURCE_DIR}/src/pkg/include
)

target_link_libraries(integration_test_pkg PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_pkg COMMAND integration_test_pkg)

add_executable(integration_test_displayd
    integration/test_displayd.cpp
    ${CMAKE_SOURCE_DIR}/src/displayd/vt_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/displayd/seat_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/displayd/login_monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/displayd/session_launcher.cpp
    ${CMAKE_SOURCE_DIR}/src/displayd/systemd_interface.cpp
    ${CMAKE_SOURCE_DIR}/src/displayd/display_daemon.cpp
)

target_include_directories(integration_test_displayd PRIVATE
    ${CMAKE_SOURCE_DIR}/src/displayd/include
)

target_link_libraries(integration_test_displayd PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_displayd COMMAND integration_test_displayd)


add_executable(integration_test_iso
    integration/test_iso.cpp
    ${CMAKE_SOURCE_DIR}/src/iso/rootfs_stager.cpp
    ${CMAKE_SOURCE_DIR}/src/iso/initramfs_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/iso/squashfs_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/iso/boot_config.cpp
    ${CMAKE_SOURCE_DIR}/src/iso/iso_builder.cpp
)

target_include_directories(integration_test_iso PRIVATE
    ${CMAKE_SOURCE_DIR}/src/iso/include
)

target_link_libraries(integration_test_iso PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_iso COMMAND integration_test_iso)

add_executable(integration_test_liveusb
    integration/test_liveusb.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/device_detector.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/iso_verifier.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/raw_writer.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/verification.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/progress_tracker.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/persistence_mgr.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/liveusb_controller.cpp
)

target_include_directories(integration_test_liveusb PRIVATE
    ${CMAKE_SOURCE_DIR}/src/liveusb/include
)

target_link_libraries(integration_test_liveusb PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_liveusb COMMAND integration_test_liveusb)

add_executable(integration_test_release
    integration/test_release.cpp
    ${CMAKE_SOURCE_DIR}/src/release/checksum_generator.cpp
    ${CMAKE_SOURCE_DIR}/src/release/signature_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/release/release_notes_gen.cpp
    ${CMAKE_SOURCE_DIR}/src/release/release_manifest.cpp
    ${CMAKE_SOURCE_DIR}/src/release/artifact_packager.cpp
    ${CMAKE_SOURCE_DIR}/src/release/git_release.cpp
    ${CMAKE_SOURCE_DIR}/src/release/release_pipeline.cpp
)

target_include_directories(integration_test_release PRIVATE
    ${CMAKE_SOURCE_DIR}/src/release/include
)

target_link_libraries(integration_test_release PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_release COMMAND integration_test_release)

# NOTE: integration_test_wayland_session requires the full compositor stack including
# generated Wayland protocol headers (${CMAKE_BINARY_DIR}/protocols) which are only
# available as part of the tinexus-comp CMake target. tinexus-comp itself validates
# the compositor builds. Re-enabling requires linking against the compositor object library.
if(FALSE)
add_executable(integration_test_wayland_session
    integration/test_wayland_session.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/server.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/global_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/surface_tree.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/backend.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/wlroots_backend.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/pixman_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/render/damage_tracker.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/xdg_toplevel_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/layer_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/MinimizeAnimation.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/RestoreAnimation.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/keymap_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/interaction_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/seat_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/output/output_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/cursor/cursor_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_rules.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/focus/focus_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/workspace/workspace_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/shell/shell_state.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/render/frame_scheduler.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/animation/animation.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/shortcut_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_manager.cpp
)

target_include_directories(integration_test_wayland_session PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/server/include
    ${CMAKE_SOURCE_DIR}/src/comp/backend/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/output/include
    ${CMAKE_SOURCE_DIR}/src/comp/cursor/include
    ${CMAKE_SOURCE_DIR}/src/comp/focus/include
    ${CMAKE_SOURCE_DIR}/src/comp/workspace/include
    ${CMAKE_SOURCE_DIR}/src/comp/shell/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${CMAKE_BINARY_DIR}/protocols
    ${WAYLAND_SERVER_INCLUDE_DIRS}
    ${WLROOTS_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
)

target_compile_definitions(integration_test_wayland_session PRIVATE WLR_USE_UNSTABLE=1)

target_link_libraries(integration_test_wayland_session PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    ${WAYLAND_SERVER_LIBRARIES}
    PkgConfig::WLROOTS
    PkgConfig::XKBCOMMON
    PkgConfig::PIXMAN
)

add_test(NAME integration_test_wayland_session COMMAND integration_test_wayland_session)
endif() # full compositor stack — requires ${CMAKE_BINARY_DIR}/protocols generated headers

# Milestone v0.2.1 Surface Protocol Validation Test Suites
add_executable(integration_test_surface_lifecycle
    integration/test_surface_lifecycle.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_state.cpp
)

target_include_directories(integration_test_surface_lifecycle PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
)

target_link_libraries(integration_test_surface_lifecycle PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_surface_lifecycle COMMAND integration_test_surface_lifecycle)

add_executable(integration_test_configure_serial
    integration/test_configure_serial.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/configure_serial.cpp
)

target_include_directories(integration_test_configure_serial PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
)

target_link_libraries(integration_test_configure_serial PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_configure_serial COMMAND integration_test_configure_serial)

add_executable(integration_test_buffer_commit
    integration/test_buffer_commit.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_state.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/buffer_manager.cpp
)

target_include_directories(integration_test_buffer_commit PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
)

target_link_libraries(integration_test_buffer_commit PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_buffer_commit COMMAND integration_test_buffer_commit)

add_executable(integration_test_frame_callback
    integration/test_frame_callback.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/frame_callback.cpp
)

target_include_directories(integration_test_frame_callback PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
)

target_link_libraries(integration_test_frame_callback PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_frame_callback COMMAND integration_test_frame_callback)

add_executable(integration_test_resource_cleanup
    integration/test_resource_cleanup.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_state.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/resource_cleanup.cpp
)

target_include_directories(integration_test_resource_cleanup PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
)

target_link_libraries(integration_test_resource_cleanup PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_resource_cleanup COMMAND integration_test_resource_cleanup)

# NOTE: integration_test_first_client requires the full compositor stack. See wayland_session note.
if(FALSE)
add_executable(integration_test_first_client
    integration/test_first_client.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/server.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/global_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/surface_tree.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/backend.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/wlroots_backend.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/pixman_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/render/damage_tracker.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/xdg_toplevel_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/layer_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/MinimizeAnimation.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/RestoreAnimation.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/keymap_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/interaction_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/seat_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_state.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/configure_serial.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/buffer_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/frame_callback.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/resource_cleanup.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/xdg_shell_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/output/output_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/cursor/cursor_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_rules.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/focus/focus_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/workspace/workspace_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/shell/shell_state.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/render/frame_scheduler.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/animation/animation.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/shortcut_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_manager.cpp
)

target_include_directories(integration_test_first_client PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/server/include
    ${CMAKE_SOURCE_DIR}/src/comp/backend/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
    ${CMAKE_SOURCE_DIR}/src/comp/output/include
    ${CMAKE_SOURCE_DIR}/src/comp/cursor/include
    ${CMAKE_SOURCE_DIR}/src/comp/focus/include
    ${CMAKE_SOURCE_DIR}/src/comp/workspace/include
    ${CMAKE_SOURCE_DIR}/src/comp/shell/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${WAYLAND_SERVER_INCLUDE_DIRS}
    ${WLROOTS_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
)

target_compile_definitions(integration_test_first_client PRIVATE WLR_USE_UNSTABLE=1)

target_link_libraries(integration_test_first_client PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    ${WAYLAND_SERVER_LIBRARIES}
    PkgConfig::WLROOTS
    PkgConfig::XKBCOMMON
    PkgConfig::PIXMAN
)

add_test(NAME integration_test_first_client COMMAND integration_test_first_client)
endif() # full compositor stack

# Milestone v0.3.0 Input Subsystem & Live Window Interactivity Test Suites
# NOTE: integration_test_input_seat and integration_test_keyboard_focus require a live
# wlr_seat* (SeatManager is now a singleton with private ctor; bind_seat takes wlr_seat*).
# These tests are integration tests that require the full compositor to be running.
if(FALSE)
add_executable(integration_test_input_seat
    integration/test_input_seat.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/keymap_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/interaction_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/seat_manager.cpp
)

target_include_directories(integration_test_input_seat PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
    ${WLROOTS_INCLUDE_DIRS}
)

target_compile_definitions(integration_test_input_seat PRIVATE WLR_USE_UNSTABLE=1)

target_link_libraries(integration_test_input_seat PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    PkgConfig::WLROOTS
    PkgConfig::XKBCOMMON
)

add_test(NAME integration_test_input_seat COMMAND integration_test_input_seat)

add_executable(integration_test_keyboard_focus
    integration/test_keyboard_focus.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/keymap_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/interaction_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/seat_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/focus/focus_manager.cpp
)

target_include_directories(integration_test_keyboard_focus PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
    ${CMAKE_SOURCE_DIR}/src/comp/focus/include
    ${WLROOTS_INCLUDE_DIRS}
)

target_compile_definitions(integration_test_keyboard_focus PRIVATE WLR_USE_UNSTABLE=1)

target_link_libraries(integration_test_keyboard_focus PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    PkgConfig::WLROOTS
    PkgConfig::XKBCOMMON
)

add_test(NAME integration_test_keyboard_focus COMMAND integration_test_keyboard_focus)
endif() # requires live wlr_seat*

# NOTE: pointer_focus and interactive_window also require live compositor state.
if(FALSE)
add_executable(integration_test_pointer_focus
    integration/test_pointer_focus.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/keymap_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/interaction_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/seat_manager.cpp
)

target_include_directories(integration_test_pointer_focus PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
)

target_link_libraries(integration_test_pointer_focus PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_pointer_focus COMMAND integration_test_pointer_focus)

add_executable(integration_test_interactive_window
    integration/test_interactive_window.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/interaction_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/xdg_toplevel_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
)

target_include_directories(integration_test_interactive_window PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    /src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
)

target_link_libraries(integration_test_interactive_window PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_interactive_window COMMAND integration_test_interactive_window)
endif()

# Milestone v0.4.0 Phase A Smoke Test Suites
add_executable(smoke_protocol_dispatch
    smoke/protocol/test_smoke_protocol_dispatch.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/protocol_dispatcher.cpp
)

target_include_directories(smoke_protocol_dispatch PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/server/include
    ${WAYLAND_SERVER_INCLUDE_DIRS}
)

target_link_libraries(smoke_protocol_dispatch PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    ${WAYLAND_SERVER_LIBRARIES}
)

add_test(NAME smoke_protocol_dispatch COMMAND smoke_protocol_dispatch)

add_executable(smoke_surface_lifecycle
    smoke/lifecycle/test_smoke_surface_lifecycle.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/layer_shell_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/exclusive_zone_calculator.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/layer_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/xdg_toplevel_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/animation/animation.cpp
)

target_include_directories(smoke_surface_lifecycle PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
    ${CMAKE_SOURCE_DIR}/src/comp/output/include
)

target_link_libraries(smoke_surface_lifecycle PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_surface_lifecycle COMMAND smoke_surface_lifecycle)

# Milestone v0.4.0 Phase B Smoke Test Suites
add_executable(smoke_damage_tracker
    smoke/render/test_smoke_damage_tracker.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/render/damage_tracker.cpp
)

target_include_directories(smoke_damage_tracker PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
)

target_link_libraries(smoke_damage_tracker PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_damage_tracker COMMAND smoke_damage_tracker)

add_executable(smoke_output_layout
    smoke/output/test_smoke_output_layout.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/output/output_layout.cpp
)

target_include_directories(smoke_output_layout PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/output/include
)

target_link_libraries(smoke_output_layout PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_output_layout COMMAND smoke_output_layout)

# Milestone v0.4.0 Phase C/D Smoke Test Suites
if(FALSE)
add_executable(smoke_multi_output
    smoke/output/test_smoke_multi_output.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/output/output_layout.cpp
    ${CMAKE_SOURCE_DIR}/src/wallpaper/wallpaper_provider.cpp
    ${CMAKE_SOURCE_DIR}/src/panel/panel_bar.cpp
)

target_include_directories(smoke_multi_output PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/output/include
    ${CMAKE_SOURCE_DIR}/src/wallpaper/include
    ${CMAKE_SOURCE_DIR}/src/panel/include
)

target_link_libraries(smoke_multi_output PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_multi_output COMMAND smoke_multi_output)

add_executable(smoke_panel_exclusive
    smoke/surface/test_smoke_panel_exclusive.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/exclusive_zone_calculator.cpp
    ${CMAKE_SOURCE_DIR}/src/panel/panel_bar.cpp
)

target_include_directories(smoke_panel_exclusive PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
    ${CMAKE_SOURCE_DIR}/src/panel/include
)

target_link_libraries(smoke_panel_exclusive PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_panel_exclusive COMMAND smoke_panel_exclusive)
endif()

add_executable(smoke_notification_overlay
    smoke/lifecycle/test_smoke_notification_overlay.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/layer_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/xdg_toplevel_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/animation/animation.cpp
)

target_include_directories(smoke_notification_overlay PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
)

target_link_libraries(smoke_notification_overlay PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_notification_overlay COMMAND smoke_notification_overlay)

# Milestone v0.5.0 Smoke Test Suites
add_executable(smoke_pam_auth
    smoke/security/test_smoke_pam_auth.cpp
    ${CMAKE_SOURCE_DIR}/src/login/pam_authenticator.cpp
)

target_include_directories(smoke_pam_auth PRIVATE
    ${CMAKE_SOURCE_DIR}/src/login/include
)

target_link_libraries(smoke_pam_auth PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_pam_auth COMMAND smoke_pam_auth)

add_executable(smoke_failed_login
    smoke/security/test_smoke_failed_login.cpp
    ${CMAKE_SOURCE_DIR}/src/login/pam_authenticator.cpp
)

target_include_directories(smoke_failed_login PRIVATE
    ${CMAKE_SOURCE_DIR}/src/login/include
)

target_link_libraries(smoke_failed_login PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_failed_login COMMAND smoke_failed_login)

add_executable(smoke_display_vt
    smoke/session/test_smoke_display_vt.cpp
    ${CMAKE_SOURCE_DIR}/src/displayd/vt_manager.cpp
)

target_include_directories(smoke_display_vt PRIVATE
    ${CMAKE_SOURCE_DIR}/src/displayd/include
)

target_link_libraries(smoke_display_vt PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_display_vt COMMAND smoke_display_vt)

add_executable(smoke_session_bootstrap
    smoke/session/test_smoke_session_bootstrap.cpp
    ${CMAKE_SOURCE_DIR}/src/session/session_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/session/env_bootstrap.cpp
)

target_include_directories(smoke_session_bootstrap PRIVATE
    ${CMAKE_SOURCE_DIR}/src/session/include
)

target_link_libraries(smoke_session_bootstrap PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_session_bootstrap COMMAND smoke_session_bootstrap)

add_executable(smoke_multiple_sessions
    smoke/session/test_smoke_multiple_sessions.cpp
    ${CMAKE_SOURCE_DIR}/src/session/session_manager.cpp
)

target_include_directories(smoke_multiple_sessions PRIVATE
    ${CMAKE_SOURCE_DIR}/src/session/include
)

target_link_libraries(smoke_multiple_sessions PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_multiple_sessions COMMAND smoke_multiple_sessions)

add_executable(smoke_daemon_restart
    smoke/session/test_smoke_daemon_restart.cpp
    ${CMAKE_SOURCE_DIR}/src/session/session_manager.cpp
)

target_include_directories(smoke_daemon_restart PRIVATE
    ${CMAKE_SOURCE_DIR}/src/session/include
)

target_link_libraries(smoke_daemon_restart PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_daemon_restart COMMAND smoke_daemon_restart)

add_executable(smoke_autostart_order
    smoke/session/test_smoke_autostart_order.cpp
    ${CMAKE_SOURCE_DIR}/src/session/autostart_parser.cpp
)

target_include_directories(smoke_autostart_order PRIVATE
    ${CMAKE_SOURCE_DIR}/src/session/include
)

target_link_libraries(smoke_autostart_order PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_autostart_order COMMAND smoke_autostart_order)

add_executable(smoke_output_persistence
    smoke/output/test_smoke_output_persistence.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/output/output_layout.cpp
)

target_include_directories(smoke_output_persistence PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/output/include
)

target_link_libraries(smoke_output_persistence PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_output_persistence COMMAND smoke_output_persistence)

# Milestone v0.6A Native Graphics Smoke Test Suite
add_executable(smoke_drm_kms
    smoke/backend/test_smoke_drm_kms.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/drm_backend.cpp
)

target_include_directories(smoke_drm_kms PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/backend/include
)

target_link_libraries(smoke_drm_kms PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_drm_kms COMMAND smoke_drm_kms)

# Milestone v0.6CD Hardware Discovery & Distribution Stack Smoke Test Suites
add_executable(smoke_udev_hotplug
    smoke/input/test_smoke_udev_hotplug.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/udev_monitor.cpp
)
target_include_directories(smoke_udev_hotplug PRIVATE ${CMAKE_SOURCE_DIR}/src/comp/input/include)
target_link_libraries(smoke_udev_hotplug PRIVATE tinexus_common tinexus_warnings tinexus_sanitizers)
add_test(NAME smoke_udev_hotplug COMMAND smoke_udev_hotplug)

add_executable(smoke_pkg_manager
    smoke/pkg/test_smoke_pkg_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/package_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/package_database.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/transaction_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/dependency_solver.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/crypto_verifier.cpp
    ${CMAKE_SOURCE_DIR}/src/pkg/package_manifest.cpp
)
target_include_directories(smoke_pkg_manager PRIVATE ${CMAKE_SOURCE_DIR}/src/pkg/include)
target_link_libraries(smoke_pkg_manager PRIVATE tinexus_common tinexus_warnings tinexus_sanitizers)
add_test(NAME smoke_pkg_manager COMMAND smoke_pkg_manager)


add_executable(smoke_iso_builder
    smoke/iso/test_smoke_iso_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/iso/iso_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/iso/rootfs_stager.cpp
    ${CMAKE_SOURCE_DIR}/src/iso/initramfs_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/iso/squashfs_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/iso/boot_config.cpp
)
target_include_directories(smoke_iso_builder PRIVATE ${CMAKE_SOURCE_DIR}/src/iso/include)
target_link_libraries(smoke_iso_builder PRIVATE tinexus_common tinexus_warnings tinexus_sanitizers)
add_test(NAME smoke_iso_builder COMMAND smoke_iso_builder)

add_executable(smoke_liveusb_writer
    smoke/liveusb/test_smoke_liveusb_writer.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/liveusb_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/device_detector.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/iso_verifier.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/raw_writer.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/verification.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/progress_tracker.cpp
    ${CMAKE_SOURCE_DIR}/src/liveusb/persistence_mgr.cpp
)
target_include_directories(smoke_liveusb_writer PRIVATE ${CMAKE_SOURCE_DIR}/src/liveusb/include)
target_link_libraries(smoke_liveusb_writer PRIVATE tinexus_common tinexus_warnings tinexus_sanitizers)
add_test(NAME smoke_liveusb_writer COMMAND smoke_liveusb_writer)

# Milestone v1.0-RC Release Candidate Smoke Test Suite
add_executable(smoke_release_candidate
    smoke/release/test_smoke_release_candidate.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/drm_backend.cpp
    ${CMAKE_SOURCE_DIR}/src/session/session_manager.cpp
)
target_include_directories(smoke_release_candidate PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/backend/include
    ${CMAKE_SOURCE_DIR}/src/session/include
)
target_link_libraries(smoke_release_candidate PRIVATE tinexus_common tinexus_warnings tinexus_sanitizers)
add_test(NAME smoke_release_candidate COMMAND smoke_release_candidate)

if(ENABLE_LEGACY_TXUI)
# Phase 4.1 Step 3 libtxui Core & Math Unit Tests
add_executable(unit_test_txui_core unit/test_txui_core.cpp)
target_link_libraries(unit_test_txui_core PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_core COMMAND unit_test_txui_core)

add_executable(unit_test_txui_math unit/test_txui_math.cpp)
target_link_libraries(unit_test_txui_math PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_math COMMAND unit_test_txui_math)

add_executable(unit_test_txui_render unit/test_txui_render.cpp)
target_link_libraries(unit_test_txui_render PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_render COMMAND unit_test_txui_render)

add_executable(unit_test_txui_gate0 unit/test_txui_gate0.cpp)
target_link_libraries(unit_test_txui_gate0 PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_gate0 COMMAND unit_test_txui_gate0)

add_executable(unit_test_txui_golden_png unit/test_txui_golden_png.cpp)
target_link_libraries(unit_test_txui_golden_png PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_golden_png COMMAND unit_test_txui_golden_png)

add_executable(unit_test_txui_render_rounded unit/test_txui_render_rounded.cpp)
target_link_libraries(unit_test_txui_render_rounded PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_render_rounded COMMAND unit_test_txui_render_rounded)

add_executable(unit_test_txui_wayland unit/test_txui_wayland.cpp)
target_link_libraries(unit_test_txui_wayland PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_wayland COMMAND unit_test_txui_wayland)

add_executable(unit_test_txui_window_event unit/test_txui_window_event.cpp)
target_link_libraries(unit_test_txui_window_event PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_window_event COMMAND unit_test_txui_window_event)

add_executable(unit_test_txui_layout unit/test_txui_layout.cpp)
target_link_libraries(unit_test_txui_layout PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_layout COMMAND unit_test_txui_layout)

add_executable(unit_test_txui_gate1_tree unit/test_txui_gate1_tree.cpp)
target_link_libraries(unit_test_txui_gate1_tree PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_gate1_tree COMMAND unit_test_txui_gate1_tree)

add_executable(unit_test_txui_gate1_layout unit/test_txui_gate1_layout.cpp)
target_link_libraries(unit_test_txui_gate1_layout PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_gate1_layout COMMAND unit_test_txui_gate1_layout)

add_executable(unit_test_txui_gate1_paint unit/test_txui_gate1_paint.cpp)
target_link_libraries(unit_test_txui_gate1_paint PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_gate1_paint COMMAND unit_test_txui_gate1_paint)

add_executable(unit_test_txui_gate1_events unit/test_txui_gate1_events.cpp)
target_link_libraries(unit_test_txui_gate1_events PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_gate1_events COMMAND unit_test_txui_gate1_events)

add_executable(unit_test_txui_gate1_resize unit/test_txui_gate1_resize.cpp)
target_link_libraries(unit_test_txui_gate1_resize PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_gate1_resize COMMAND unit_test_txui_gate1_resize)

add_executable(unit_test_txui_gate1_memory unit/test_txui_gate1_memory.cpp)
target_link_libraries(unit_test_txui_gate1_memory PRIVATE txui tinexus_warnings)
add_test(NAME unit_test_txui_gate1_memory COMMAND unit_test_txui_gate1_memory)

add_executable(unit_test_store_render
    unit/test_store_render.cpp
    ${CMAKE_SOURCE_DIR}/src/app-installer/StoreWidget.cpp
)
target_include_directories(unit_test_store_render PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app-installer
)
target_link_libraries(unit_test_store_render PRIVATE txui tinexus_common tinexus_warnings)
add_test(NAME unit_test_store_render COMMAND unit_test_store_render)

add_executable(unit_test_single_instance unit/test_single_instance.cpp)
target_link_libraries(unit_test_single_instance PRIVATE txui tinexus_common tinexus_warnings)
add_test(NAME unit_test_single_instance COMMAND unit_test_single_instance)

add_executable(unit_test_app_id_isolation unit/test_app_id_isolation.cpp)
target_link_libraries(unit_test_app_id_isolation PRIVATE txui tinexus_common tinexus_warnings)
add_test(NAME unit_test_app_id_isolation COMMAND unit_test_app_id_isolation)
endif()

add_executable(test_window_state_machine unit/test_window_state_machine.cpp)
target_include_directories(test_window_state_machine PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test_window_state_machine PRIVATE tinexus_common tinexus_warnings)
add_test(NAME test_window_state_machine COMMAND test_window_state_machine)

add_executable(test_memory_safety_during_anim unit/test_memory_safety_during_anim.cpp)
target_include_directories(test_memory_safety_during_anim PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test_memory_safety_during_anim PRIVATE tinexus_common tinexus_warnings)
add_test(NAME test_memory_safety_during_anim COMMAND test_memory_safety_during_anim)

if(ENABLE_LEGACY_TXUI)
add_executable(unit_test_topbar_geometry unit/test_topbar_geometry.cpp)
target_link_libraries(unit_test_topbar_geometry PRIVATE txui tinexus_common tinexus_warnings)
add_test(NAME unit_test_topbar_geometry COMMAND unit_test_topbar_geometry)

add_executable(unit_test_input_region unit/test_input_region.cpp)
target_link_libraries(unit_test_input_region PRIVATE txui tinexus_common tinexus_warnings)
add_test(NAME unit_test_input_region COMMAND unit_test_input_region)
endif()

add_executable(unit_test_animation_manager
    unit/test_animation_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/animation/animation_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/animation/animation.cpp
)
target_include_directories(unit_test_animation_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(unit_test_animation_manager PRIVATE tinexus_common tinexus_warnings)
add_test(NAME unit_test_animation_manager COMMAND unit_test_animation_manager)

if(ENABLE_LEGACY_TXUI)
add_executable(unit_test_lifecycle_dock
    unit/test_lifecycle_dock.cpp
    ${CMAKE_SOURCE_DIR}/src/dock/ui/DockWidget.cpp
)
target_include_directories(unit_test_lifecycle_dock PRIVATE
    ${CMAKE_SOURCE_DIR}/src/dock/include
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(unit_test_lifecycle_dock PRIVATE txui tinexus_common tinexus_warnings)
add_test(NAME unit_test_lifecycle_dock COMMAND unit_test_lifecycle_dock)
endif()

if(TINEXUS_BUILD_QT_APPS)
    message(STATUS "Qt6 Tier 2 apps/tests enabled (TINEXUS_BUILD_QT_APPS=ON)")
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2)
    add_subdirectory(qt6_popup_test)
endif()

# ── Tier 1 Migration Gate ──────────────────────────────────────────────────────
# test-tier1-gate: validates all blockers for tinexus-shell, tinexus-dock,
# tinexus-launcher Qt6 migration in one combined proof-of-concept harness.
# Does NOT require TINEXUS_BUILD_QT_APPS — runs standalone with Qt6 Core/Gui/Quick.
# Requires Qt6 to be present on the build host.
find_package(Qt6 QUIET COMPONENTS Core Gui Quick Qml WaylandClient)
if(Qt6_FOUND AND Qt6Gui_FOUND AND Qt6Quick_FOUND)
    add_subdirectory(tier1_gate)
    message(STATUS "Tier 1 Gate: test-tier1-gate enabled")
    message(STATUS "  Run: cmake --build . --target test-tier1-gate")
    message(STATUS "  For live layer-shell test: cmake -DTIER1_GATE_LAYERSHELL=ON .")
else()
    message(STATUS "Tier 1 Gate: Qt6 not found — test-tier1-gate SKIPPED")
    message(STATUS "  Install Qt6 and re-run cmake to enable the migration gate.")
endif()
