add_executable(tinexus-shell
    main.cpp
    DesktopShellWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/desktop_parser.cpp
)

target_include_directories(tinexus-shell PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
)

target_link_libraries(tinexus-shell PRIVATE
    tinexus_common
    txui
    tinexus_protocols_client
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-shell RUNTIME DESTINATION bin)

add_executable(test-shell-render
    test_shell_render.cpp
    DesktopShellWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/desktop_parser.cpp
)

target_include_directories(test-shell-render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
)

target_link_libraries(test-shell-render PRIVATE
    tinexus_common
    txui
    tinexus_protocols_client
    tinexus_warnings
    tinexus_sanitizers
)

set_target_properties(test-shell-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(test-full-desktop
    test_full_desktop.cpp
    DesktopShellWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/desktop_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/about/AboutWidget.cpp
)

target_include_directories(test-full-desktop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${CMAKE_SOURCE_DIR}/src/about
)

target_link_libraries(test-full-desktop PRIVATE
    tinexus_common
    txui
    tinexus_protocols_client
    tinexus_warnings
    tinexus_sanitizers
)

set_target_properties(test-full-desktop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

