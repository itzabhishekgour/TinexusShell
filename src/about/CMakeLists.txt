find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

add_executable(tinexus-about
    main.cpp
    AboutWidget.cpp
)

target_include_directories(tinexus-about PRIVATE include .)

target_link_libraries(tinexus-about PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus-guard
    txui
    PkgConfig::WAYLAND_CLIENT
)

set_target_properties(tinexus-about PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

install(TARGETS tinexus-about RUNTIME DESTINATION bin)

add_executable(test-about-render
    test_about_render.cpp
    AboutWidget.cpp
)
target_include_directories(test-about-render PRIVATE include .)
target_link_libraries(test-about-render PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus-guard
    txui
    PkgConfig::WAYLAND_CLIENT
)
set_target_properties(test-about-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
