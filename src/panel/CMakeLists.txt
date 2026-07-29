add_executable(tinexus-panel
    main.cpp
    layer_shell_window.cpp
)

target_include_directories(tinexus-panel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-panel PRIVATE
    tinexus_common
    txui
    tinexus_protocols_client
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-panel RUNTIME DESTINATION bin)
