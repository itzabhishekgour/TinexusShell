add_executable(tinexus-session
    main.cpp
    env_bootstrap.cpp
    autostart_parser.cpp
    session_manager.cpp
)

target_include_directories(tinexus-session PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-session PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-session RUNTIME DESTINATION bin)
