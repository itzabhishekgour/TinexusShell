add_executable(tinexus-clipboard
    main.cpp
    clipboard_item.cpp
    privacy_filter.cpp
    clipboard_manager.cpp
)

target_include_directories(tinexus-clipboard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-clipboard PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-clipboard RUNTIME DESTINATION bin)
