find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

# Check for PAM (optional — degrades gracefully if not present)
find_library(PAM_LIBRARY pam)
find_path(PAM_INCLUDE_DIR security/pam_appl.h
    PATHS /usr/include /usr/local/include
    PATH_SUFFIXES security
)

add_executable(tinexus-lock 
    main.cpp
    LockWidget.cpp
)

target_include_directories(tinexus-lock PRIVATE include)

target_link_libraries(tinexus-lock PRIVATE
    tinexus_common
    tinexus_warnings
    txui
    PkgConfig::WAYLAND_CLIENT
)

if(PAM_LIBRARY AND PAM_INCLUDE_DIR)
    message(STATUS "PAM found: ${PAM_LIBRARY} — tinexus-lock will use real PAM authentication")
    target_link_libraries(tinexus-lock PRIVATE ${PAM_LIBRARY})
    target_compile_definitions(tinexus-lock PRIVATE TINEXUS_LOCK_HAS_PAM=1)
else()
    message(WARNING "libpam-dev not found — tinexus-lock will use stub authentication (install libpam0g-dev)")
    target_compile_definitions(tinexus-lock PRIVATE TINEXUS_LOCK_HAS_PAM=0)
endif()

set_target_properties(tinexus-lock PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS tinexus-lock RUNTIME DESTINATION bin)

add_executable(test-lock-render
    test_lock_render.cpp
    LockWidget.cpp
)
target_include_directories(test-lock-render PRIVATE include)
target_link_libraries(test-lock-render PRIVATE
    tinexus_common
    tinexus_warnings
    txui
)
set_target_properties(test-lock-render PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

