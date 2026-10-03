cmake_minimum_required(VERSION 3.28)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    Qml
    WaylandClient
    DBus
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(LayerShellQt QUIET)

# Check for PAM
find_library(PAM_LIBRARY pam)
find_path(PAM_INCLUDE_DIR security/pam_appl.h
    PATHS /usr/include /usr/local/include
    PATH_SUFFIXES security
)

add_executable(tinexus-lock
    main.cpp
    LockBridge.cpp
    LockBridge.hpp
)

target_include_directories(tinexus-lock PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(tinexus-lock PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::WaylandClient
    Qt6::DBus
)

if(LayerShellQt_FOUND)
    target_link_libraries(tinexus-lock PRIVATE LayerShellQt::Interface)
    target_compile_definitions(tinexus-lock PRIVATE HAVE_LAYERSHELL=1)
    message(STATUS "tinexus-lock: LayerShellQt linked")
endif()

if(PAM_LIBRARY AND PAM_INCLUDE_DIR)
    message(STATUS "PAM found: ${PAM_LIBRARY} — tinexus-lock will use real PAM authentication")
    target_link_libraries(tinexus-lock PRIVATE ${PAM_LIBRARY})
    target_compile_definitions(tinexus-lock PRIVATE TINEXUS_LOCK_HAS_PAM=1)
else()
    message(WARNING "libpam-dev not found — tinexus-lock will use stub authentication")
    target_compile_definitions(tinexus-lock PRIVATE TINEXUS_LOCK_HAS_PAM=0)
endif()

add_executable(test-lock-live
    test_lock_live.cpp
    LockBridge.cpp
    LockBridge.hpp
)
target_include_directories(test-lock-live PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-lock-live PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
)
if(PAM_LIBRARY AND PAM_INCLUDE_DIR)
    target_link_libraries(test-lock-live PRIVATE ${PAM_LIBRARY})
    target_compile_definitions(test-lock-live PRIVATE TINEXUS_LOCK_HAS_PAM=1)
else()
    target_compile_definitions(test-lock-live PRIVATE TINEXUS_LOCK_HAS_PAM=0)
endif()
set_target_properties(test-lock-live PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")


set_target_properties(tinexus-lock PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS tinexus-lock RUNTIME DESTINATION bin)
