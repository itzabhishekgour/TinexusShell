add_executable(tinexus-wallpaper
    main.cpp
    wallpaper_provider.cpp
)

target_include_directories(tinexus-wallpaper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-wallpaper PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-wallpaper RUNTIME DESTINATION bin)
