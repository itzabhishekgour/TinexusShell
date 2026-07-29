add_executable(tinexus-launcher
    main.cpp
)

target_include_directories(tinexus-launcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-launcher PRIVATE
    tinexus_common
    txui
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-launcher RUNTIME DESTINATION bin)
