add_executable(tinexus-ipcd
    main.cpp
)

target_include_directories(tinexus-ipcd PRIVATE include)

target_link_libraries(tinexus-ipcd PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-ipcd
    RUNTIME DESTINATION bin
)
