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
    ${CMAKE_SOURCE_DIR}/src/comp/render/damage_tracker.cpp
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
    ${CMAKE_SOURCE_DIR}/src/comp/render/damage_tracker.cpp
)

target_include_directories(integration_test_pixman_renderer PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
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
    ${CMAKE_SOURCE_DIR}/src/comp/render/damage_tracker.cpp
)

target_include_directories(integration_test_scene_graph PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
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
    ${CMAKE_SOURCE_DIR}/src/comp/render/damage_tracker.cpp
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

add_executable(smoke_notification_overlay
    smoke/lifecycle/test_smoke_notification_overlay.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/layer_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/scene_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/xdg_toplevel_node.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
)

target_include_directories(smoke_notification_overlay PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
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

# Milestone v0.6B Hardware Acceleration Smoke Test Suites
add_executable(smoke_render_target
    smoke/render/test_smoke_render_target.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_target.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/gpu_resource_manager.cpp
)

target_include_directories(smoke_render_target PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
)

target_link_libraries(smoke_render_target PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_render_target COMMAND smoke_render_target)

add_executable(smoke_renderer_fallback
    smoke/render/test_smoke_renderer_fallback.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/renderer_factory.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_target.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/gpu_resource_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/vulkan_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/opengl_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/pixman_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/render_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/render/damage_tracker.cpp
)

target_include_directories(smoke_renderer_fallback PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/renderer/include
    ${CMAKE_SOURCE_DIR}/src/comp/render/include
)

target_link_libraries(smoke_renderer_fallback PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_test(NAME smoke_renderer_fallback COMMAND smoke_renderer_fallback)

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

add_executable(smoke_installer_partition
    smoke/installer/test_smoke_installer_partition.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/install_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/disk_inspector.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/partition_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/squashfs_extractor.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/bootloader_config.cpp
)
target_include_directories(smoke_installer_partition PRIVATE ${CMAKE_SOURCE_DIR}/src/installer/include)
target_link_libraries(smoke_installer_partition PRIVATE tinexus_common tinexus_warnings tinexus_sanitizers)
add_test(NAME smoke_installer_partition COMMAND smoke_installer_partition)

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
