add_executable(tinexus-wallpaper
    main.cpp
    wallpaper_provider.cpp
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

install(TARGETS tinexus-wallpaper RUNTIME DESTINATION bin)
