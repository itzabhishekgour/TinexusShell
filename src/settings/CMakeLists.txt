add_executable(tinexus-settings
    main.cpp
    config_store.cpp
    schema_validator.cpp
    settings_daemon.cpp
)

target_include_directories(tinexus-settings PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    # Include ipcd headers for WallpaperChangedPayload and MessageType definitions.
    # This is a header-only dependency — we do NOT link against the ipcd binary.
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
)

target_link_libraries(tinexus-settings PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-settings RUNTIME DESTINATION bin)
