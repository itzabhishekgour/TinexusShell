add_executable(tinexus-installer
    main.cpp
    disk_inspector.cpp
    partition_engine.cpp
    squashfs_extractor.cpp
    bootloader_config.cpp
    install_controller.cpp
    ui/InstallerWindow.cpp
)

target_include_directories(tinexus-installer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-installer PRIVATE
    tinexus-sdk
    tinexus_common
    txui
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-installer RUNTIME DESTINATION bin)
