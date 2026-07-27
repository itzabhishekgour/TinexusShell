add_executable(tinexus-terminal
    main.cpp
    pty_process.cpp
    ansi_parser.cpp
    terminal_buffer.cpp
    terminal_renderer.cpp
)

target_include_directories(tinexus-terminal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-terminal PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-terminal RUNTIME DESTINATION bin)
