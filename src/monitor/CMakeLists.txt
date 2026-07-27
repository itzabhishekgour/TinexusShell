add_executable(tinexus-monitor
    main.cpp
    metrics_snapshot.cpp
    cpu_parser.cpp
    memory_parser.cpp
    disk_parser.cpp
    network_parser.cpp
    process_tree.cpp
    process_controller.cpp
    resource_monitor.cpp
)

target_include_directories(tinexus-monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-monitor PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-monitor RUNTIME DESTINATION bin)
