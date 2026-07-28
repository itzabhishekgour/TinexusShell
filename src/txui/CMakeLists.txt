find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

add_library(txui STATIC
    core/Version.cpp
    render/pixman/PixmanBackend.cpp
    render/ImageWriter.cpp
    render/ImageReader.cpp
    render/WaylandRenderTarget.cpp
    wayland/WaylandConnection.cpp
    wayland/WaylandBuffer.cpp
    wayland/WaylandSurface.cpp
)

target_compile_features(txui PUBLIC cxx_std_20)

target_include_directories(txui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(txui
    PUBLIC
        PkgConfig::WAYLAND_CLIENT
    PRIVATE
        tinexus_warnings
)
