add_executable(tinexus-monitor
    main.cpp
    MonitorWidget.cpp
    metrics_snapshot.cpp
    cpu_parser.cpp
    memory_parser.cpp
    disk_parser.cpp
    network_parser.cpp
    power_parser.cpp
    gpu_parser.cpp
    process_tree.cpp
    process_controller.cpp
    resource_monitor.cpp
)

target_include_directories(tinexus-monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(tinexus-monitor PRIVATE
    txui
    tinexus_common
    tinexus-sdk
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-monitor RUNTIME DESTINATION bin)

add_executable(test-monitor-render
    test_monitor_render.cpp
    MonitorWidget.cpp
    metrics_snapshot.cpp
    cpu_parser.cpp
    memory_parser.cpp
    disk_parser.cpp
    network_parser.cpp
    power_parser.cpp
    gpu_parser.cpp
    process_tree.cpp
    process_controller.cpp
    resource_monitor.cpp
)

target_include_directories(test-monitor-render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(test-monitor-render PRIVATE
    txui
    tinexus_common
    tinexus-sdk
    tinexus_warnings
    tinexus_sanitizers
)

set_target_properties(test-monitor-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(test-monitor-live
    test_monitor_live.cpp
    MonitorWidget.cpp
    metrics_snapshot.cpp
    cpu_parser.cpp
    memory_parser.cpp
    disk_parser.cpp
    network_parser.cpp
    power_parser.cpp
    gpu_parser.cpp
    process_tree.cpp
    process_controller.cpp
    resource_monitor.cpp
)

target_include_directories(test-monitor-live PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(test-monitor-live PRIVATE
    txui
    tinexus_common
    tinexus-sdk
    tinexus_warnings
    tinexus_sanitizers
)

set_target_properties(test-monitor-live PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
