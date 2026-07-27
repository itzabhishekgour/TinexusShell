find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_SERVER wayland-server)

add_executable(unit_test_common
    unit/test_common.cpp
)

target_link_libraries(unit_test_common PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME unit_test_common COMMAND unit_test_common)

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

add_executable(unit_test_comp
    unit/test_comp.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/global_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/surface_tree.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/backend.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/pixman_renderer.cpp
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
    ${CMAKE_SOURCE_DIR}/src/comp/focus/include
    ${CMAKE_SOURCE_DIR}/src/comp/workspace/include
    ${CMAKE_SOURCE_DIR}/src/comp/shell/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
)

target_link_libraries(unit_test_comp PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
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
)

target_link_libraries(integration_test_runtime PRIVATE
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
    ${CMAKE_SOURCE_DIR}/src/comp/focus/include
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
)

target_link_libraries(integration_test_alpha_stress PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
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

add_executable(integration_test_installer
    integration/test_installer.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/disk_inspector.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/partition_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/squashfs_extractor.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/bootloader_config.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/install_controller.cpp
)

target_include_directories(integration_test_installer PRIVATE
    ${CMAKE_SOURCE_DIR}/src/installer/include
)

target_link_libraries(integration_test_installer PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_installer COMMAND integration_test_installer)

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

add_executable(integration_test_wayland_session
    integration/test_wayland_session.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/server.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/global_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/surface_tree.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/backend.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/pixman_renderer.cpp
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

target_include_directories(integration_test_wayland_session PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/server/include
    ${CMAKE_SOURCE_DIR}/src/comp/backend/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/comp/output/include
    ${CMAKE_SOURCE_DIR}/src/comp/cursor/include
    ${CMAKE_SOURCE_DIR}/src/comp/focus/include
    ${CMAKE_SOURCE_DIR}/src/comp/workspace/include
    ${CMAKE_SOURCE_DIR}/src/comp/shell/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
    ${WAYLAND_SERVER_INCLUDE_DIRS}
)

target_link_libraries(integration_test_wayland_session PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    ${WAYLAND_SERVER_LIBRARIES}
)

add_test(NAME integration_test_wayland_session COMMAND integration_test_wayland_session)

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

add_executable(integration_test_first_client
    integration/test_first_client.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/server.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/global_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/surface_tree.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/backend.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/pixman_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/xdg_toplevel_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_manager.cpp
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
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
    ${CMAKE_SOURCE_DIR}/src/comp/output/include
    ${CMAKE_SOURCE_DIR}/src/comp/cursor/include
    ${CMAKE_SOURCE_DIR}/src/comp/focus/include
    ${CMAKE_SOURCE_DIR}/src/comp/workspace/include
    ${CMAKE_SOURCE_DIR}/src/comp/shell/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
    ${WAYLAND_SERVER_INCLUDE_DIRS}
)

target_link_libraries(integration_test_first_client PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    ${WAYLAND_SERVER_LIBRARIES}
)

add_test(NAME integration_test_first_client COMMAND integration_test_first_client)

# Milestone v0.2.2 Software Rendering Pipeline Test Suite
add_executable(integration_test_pixman_renderer
    integration/test_pixman_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/pixman_renderer.cpp
)

target_include_directories(integration_test_pixman_renderer PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
)

target_link_libraries(integration_test_pixman_renderer PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_pixman_renderer COMMAND integration_test_pixman_renderer)

# Milestone v0.2.3 Window Management & Scene Graph Test Suite
add_executable(integration_test_scene_graph
    integration/test_scene_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/xdg_toplevel_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/pixman_renderer.cpp
)

target_include_directories(integration_test_scene_graph PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
)

target_link_libraries(integration_test_scene_graph PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_scene_graph COMMAND integration_test_scene_graph)

# Milestone v0.2.4 First Real Wayland Application Test Suites
add_executable(integration_test_xdg_shell
    integration/test_xdg_shell.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/configure_serial.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/surface_state.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/surface/xdg_shell_manager.cpp
)

target_include_directories(integration_test_xdg_shell PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
)

target_link_libraries(integration_test_xdg_shell PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_xdg_shell COMMAND integration_test_xdg_shell)

add_executable(integration_test_real_client
    integration/test_real_client.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/server.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/global_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/server/surface_tree.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/backend.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/pixman_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/xdg_toplevel_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_manager.cpp
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

target_include_directories(integration_test_real_client PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/server/include
    ${CMAKE_SOURCE_DIR}/src/comp/backend/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
    ${CMAKE_SOURCE_DIR}/src/comp/output/include
    ${CMAKE_SOURCE_DIR}/src/comp/cursor/include
    ${CMAKE_SOURCE_DIR}/src/comp/focus/include
    ${CMAKE_SOURCE_DIR}/src/comp/workspace/include
    ${CMAKE_SOURCE_DIR}/src/comp/shell/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
    ${CMAKE_SOURCE_DIR}/src/comp/animation/include
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
    ${WAYLAND_SERVER_INCLUDE_DIRS}
)

target_link_libraries(integration_test_real_client PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    ${WAYLAND_SERVER_LIBRARIES}
)

add_test(NAME integration_test_real_client COMMAND integration_test_real_client)

# Milestone v0.3.0 Input Subsystem & Live Window Interactivity Test Suites
add_executable(integration_test_input_seat
    integration/test_input_seat.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/keymap_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/interaction_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/input/seat_manager.cpp
)

target_include_directories(integration_test_input_seat PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/input/include
)

target_link_libraries(integration_test_input_seat PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
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
)

target_link_libraries(integration_test_keyboard_focus PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_keyboard_focus COMMAND integration_test_keyboard_focus)

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
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
)

target_link_libraries(integration_test_interactive_window PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME integration_test_interactive_window COMMAND integration_test_interactive_window)

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
)

target_include_directories(smoke_surface_lifecycle PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/surface/include
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
)

target_link_libraries(smoke_surface_lifecycle PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_surface_lifecycle COMMAND smoke_surface_lifecycle)
