add_executable(tinexus-wallpaper
    main.cpp
    wallpaper_provider.cpp
    solar_schedule.cpp
    wallpaper_dbus.cpp
    ${CMAKE_SOURCE_DIR}/build/protocols/wp-protocols-glue.c
)

target_include_directories(tinexus-wallpaper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/build/protocols
    ${CMAKE_BINARY_DIR}/protocols
)

target_link_libraries(tinexus-wallpaper PRIVATE
    tinexus_common
    tinexus_protocols_client
    wayland-client
    tinexus_warnings
    tinexus_sanitizers
    systemd
)

# Enable SIMD autovectorization for crossfade_lerp hot path.
# -O2 is the minimum; release builds add -O3 or -march=native on top.
target_compile_options(tinexus-wallpaper PRIVATE
    -O2
    -ftree-vectorize
)


install(TARGETS tinexus-wallpaper RUNTIME DESTINATION bin)
