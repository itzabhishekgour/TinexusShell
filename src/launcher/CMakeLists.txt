add_executable(tinexus-launcher
    main.cpp
    theme_manager.cpp
    search_model.cpp
    ipc_client.cpp
    launcher_controller.cpp
)

target_include_directories(tinexus-launcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-launcher PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-launcher RUNTIME DESTINATION bin)
