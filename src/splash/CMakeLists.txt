add_executable(tinexus-splash
    main.cpp
)

target_include_directories(tinexus-splash PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wallpaper/include
)

target_link_libraries(tinexus-splash PRIVATE
    tinexus_warnings
    tinexus_sanitizers
)

set_target_properties(tinexus-splash PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS tinexus-splash RUNTIME DESTINATION bin)
