add_library(tinexus_common SHARED
    logger.cpp
    string_interner.cpp
    peer_credentials.cpp
    dbus_power.cpp
    NetUtils.cpp
    TinexusLogo.cpp
    HardwareConfig.cpp
    BacklightUtils.cpp
    AudioUtils.cpp
    DisplayUtils.cpp
    PlatformServices.cpp
)

target_include_directories(tinexus_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus_common PRIVATE
    tinexus_warnings
    tinexus_sanitizers
)
target_link_libraries(tinexus_common PUBLIC systemd)

set_target_properties(tinexus_common PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS tinexus_common
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

add_executable(test-hardware-utils
    ${CMAKE_SOURCE_DIR}/tests/unit/test_hardware_utils.cpp
)
target_link_libraries(test-hardware-utils PRIVATE tinexus_common)
set_target_properties(test-hardware-utils PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
