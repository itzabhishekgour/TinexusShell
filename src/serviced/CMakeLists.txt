add_executable(tinexus-serviced
    daemon_spec.cpp
    dep_graph.cpp
    event_journal.cpp
    heartbeat_watchdog.cpp
    process_manager.cpp
    runtime_socket.cpp
    launch_authority.cpp
    main.cpp
)

target_include_directories(tinexus-serviced PRIVATE
    include
)

target_link_libraries(tinexus-serviced PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-serviced
    RUNTIME DESTINATION bin
)
