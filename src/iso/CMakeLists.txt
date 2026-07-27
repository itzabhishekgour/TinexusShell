add_executable(tinexus-iso
    main.cpp
    rootfs_stager.cpp
    initramfs_builder.cpp
    squashfs_builder.cpp
    boot_config.cpp
    iso_builder.cpp
)

target_include_directories(tinexus-iso PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-iso PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-iso RUNTIME DESTINATION bin)
