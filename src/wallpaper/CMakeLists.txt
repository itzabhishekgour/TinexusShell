add_executable(tinexus-wallpaper
    main.cpp
    wallpaper_provider.cpp
    solar_schedule.cpp
)

target_include_directories(tinexus-wallpaper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/protocols
    # Include ipcd headers for WallpaperChangedPayload and MessageType.
    # Header-only dependency — we do NOT link against the ipcd binary.
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
)

target_link_libraries(tinexus-wallpaper PRIVATE
    tinexus_common
    txui
    tinexus_warnings
    tinexus_sanitizers
)

# Enable SIMD autovectorization for crossfade_lerp hot path.
# -O2 is the minimum; release builds add -O3 or -march=native on top.
target_compile_options(tinexus-wallpaper PRIVATE
    -O2
    -ftree-vectorize
)


install(TARGETS tinexus-wallpaper RUNTIME DESTINATION bin)
