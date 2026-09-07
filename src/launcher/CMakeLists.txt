cmake_minimum_required(VERSION 3.28)
project(tinexus-launcher VERSION 1.0.0 LANGUAGES CXX)

add_executable(tinexus-launcher
    main.cpp
    LauncherWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/desktop_parser.cpp
)

target_include_directories(tinexus-launcher PRIVATE 
    ${CMAKE_SOURCE_DIR}/include 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
)

target_link_libraries(tinexus-launcher PRIVATE
    tinexus_common
    txui
)

add_executable(test-launcher-render
    test_launcher_render.cpp
    LauncherWidget.cpp
)

target_include_directories(test-launcher-render PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
)

target_link_libraries(test-launcher-render PRIVATE
    tinexus_common
    txui
)

install(TARGETS tinexus-launcher DESTINATION bin)
