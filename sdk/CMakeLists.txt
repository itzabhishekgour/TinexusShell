add_library(tinexus-sdk SHARED
    src/client.cpp
)

target_include_directories(tinexus-sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-sdk PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_executable(hello_sdk
    examples/hello_sdk.cpp
)

target_link_libraries(hello_sdk PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-sdk LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/tinexus DESTINATION include)
