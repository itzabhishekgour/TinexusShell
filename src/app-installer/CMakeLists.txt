find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
find_package(OpenSSL REQUIRED)

add_executable(tinexus-store
    main.cpp
    StoreWidget.cpp
)

target_include_directories(tinexus-store PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
)

target_link_libraries(tinexus-store PRIVATE
    tinexus_common
    tinexus_warnings
    txui
    PkgConfig::WAYLAND_CLIENT
    OpenSSL::Crypto
)

set_target_properties(tinexus-store PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS tinexus-store RUNTIME DESTINATION bin)

# Maintain tinexus-app-installer backward-compatibility target
add_custom_target(tinexus-app-installer ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/bin/tinexus-store"
        "${CMAKE_BINARY_DIR}/bin/tinexus-app-installer"
    DEPENDS tinexus-store
)

add_executable(test-store-render
    ${CMAKE_SOURCE_DIR}/tests/unit/test_store_render.cpp
    StoreWidget.cpp
)

target_include_directories(test-store-render PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${CMAKE_SOURCE_DIR}/src/app-installer
)

target_link_libraries(test-store-render PRIVATE
    tinexus_common
    tinexus_warnings
    txui
    PkgConfig::WAYLAND_CLIENT
    OpenSSL::Crypto
)

add_executable(test-ksnip-pipeline
    ${CMAKE_SOURCE_DIR}/tests/unit/test_ksnip_live_pipeline.cpp
)

target_link_libraries(test-ksnip-pipeline PRIVATE
    tinexus_common
    tinexus_warnings
    OpenSSL::Crypto
)

add_executable(test-desktop-store-render
    ${CMAKE_SOURCE_DIR}/tests/unit/test_desktop_store_render.cpp
    StoreWidget.cpp
)

target_include_directories(test-desktop-store-render PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${CMAKE_SOURCE_DIR}/src/app-installer
)

target_link_libraries(test-desktop-store-render PRIVATE
    tinexus_common
    tinexus_warnings
    txui
    PkgConfig::WAYLAND_CLIENT
    OpenSSL::Crypto
)

set_target_properties(test-desktop-store-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")


