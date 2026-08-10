cmake_minimum_required(VERSION 3.28)
project(tinexus-dock VERSION 1.0.0 LANGUAGES CXX)

add_executable(tinexus-dock
    main.cpp
    ui/DockWidget.cpp
)

target_include_directories(tinexus-dock PRIVATE 
    ${CMAKE_SOURCE_DIR}/include 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(tinexus-dock PRIVATE
    tinexus_common
    txui
)

install(TARGETS tinexus-dock DESTINATION bin)
