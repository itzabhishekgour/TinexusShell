add_executable(tinexus-comp
    main.cpp
    server/server.cpp
    output/output_manager.cpp
    cursor/cursor_manager.cpp
    window/window_manager.cpp
    workspace/workspace_manager.cpp
    shell/shell_state.cpp
    render/frame_scheduler.cpp
    animation/animation.cpp
    input/shortcut_engine.cpp
    surface/surface_manager.cpp
)

target_include_directories(tinexus-comp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server/include
    ${CMAKE_CURRENT_SOURCE_DIR}/output/include
    ${CMAKE_CURRENT_SOURCE_DIR}/cursor/include
    ${CMAKE_CURRENT_SOURCE_DIR}/window/include
    ${CMAKE_CURRENT_SOURCE_DIR}/workspace/include
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/include
    ${CMAKE_CURRENT_SOURCE_DIR}/render/include
    ${CMAKE_CURRENT_SOURCE_DIR}/animation/include
    ${CMAKE_CURRENT_SOURCE_DIR}/input/include
    ${CMAKE_CURRENT_SOURCE_DIR}/surface/include
)

target_link_libraries(tinexus-comp PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-comp RUNTIME DESTINATION bin)
