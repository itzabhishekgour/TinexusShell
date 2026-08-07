cmake_minimum_required(VERSION 3.28)

# Read VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TINEXUS_VERSION_RAW)
string(STRIP "${TINEXUS_VERSION_RAW}" TINEXUS_VERSION)

project(TinexusPlatform
    VERSION ${TINEXUS_VERSION}
    DESCRIPTION "Tinexus Wayland Desktop Platform"
    HOMEPAGE_URL "https://github.com/tinexus/tinexus"
    LANGUAGES C CXX
)

# Mandatory C++20 enforcement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Unified output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Prevent in-source builds
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are strictly forbidden. "
        "Please create a build directory (e.g. build/debug) and run CMake from there."
    )
endif()

# Global Options
option(TINEXUS_ENABLE_SANITIZERS "Enable ASan/UBSan sanitizers in debug builds" OFF)
option(TINEXUS_WARNINGS_AS_ERRORS "Treat all compiler warnings as errors" OFF)
option(TINEXUS_ENABLE_LTO "Enable Link-Time Optimization in release builds" OFF)
option(BUILD_TESTING "Build unit and integration test suites" ON)

# Interface target for warning flags
add_library(tinexus_warnings INTERFACE)
target_compile_options(tinexus_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wcast-align
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
    -Wshadow
    -Wno-unused-parameter
    $<$<BOOL:${TINEXUS_WARNINGS_AS_ERRORS}>:-Werror>
)

# Interface target for sanitizers
add_library(tinexus_sanitizers INTERFACE)
if(TINEXUS_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tinexus_sanitizers INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(tinexus_sanitizers INTERFACE
        -fsanitize=address,undefined
    )
endif()

# Subdirectories according to monorepo structure in 04_FOLDER_STRUCTURE.md
add_subdirectory(protocols)
add_subdirectory(src)
add_subdirectory(sdk)
add_subdirectory(tools)
add_subdirectory(examples)
add_subdirectory(benchmarks)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
