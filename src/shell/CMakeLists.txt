set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

add_executable(tinexus-shell
    main.cpp
    client/client_state.cpp
    client/layer_surface.cpp
    render/shm_surface_buffer.cpp
    ui/painter.cpp
    ui/panel_widget.cpp
)

target_include_directories(tinexus-shell PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-shell PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    tinexus_protocols_client
    PkgConfig::WAYLAND_CLIENT
)

install(TARGETS tinexus-shell RUNTIME DESTINATION bin)
