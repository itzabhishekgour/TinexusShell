add_executable(tinexus-displayd
    main.cpp
    vt_manager.cpp
    seat_manager.cpp
    login_monitor.cpp
    session_launcher.cpp
    systemd_interface.cpp
    display_daemon.cpp
)

target_include_directories(tinexus-displayd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-displayd PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-displayd RUNTIME DESTINATION bin)
