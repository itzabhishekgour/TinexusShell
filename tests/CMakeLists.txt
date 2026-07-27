find_package(SQLite3 REQUIRED)

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
    ${CMAKE_SOURCE_DIR}/src/comp/output/output_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/cursor/cursor_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/window/window_manager.cpp
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
