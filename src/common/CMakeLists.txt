add_library(tinexus_common SHARED
    logger.cpp
    string_interner.cpp
    peer_credentials.cpp
)

target_include_directories(tinexus_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus_common PRIVATE
    tinexus_warnings
    tinexus_sanitizers
)

set_target_properties(tinexus_common PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS tinexus_common
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
