cmake_minimum_required(VERSION 3.28)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    Qml
    WaylandClient
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

add_executable(tinexus-files
    main.cpp
    FilesBridge.cpp
    FilesBridge.hpp
)

target_include_directories(tinexus-files PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(tinexus-files PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::WaylandClient
)

set_target_properties(tinexus-files PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS tinexus-files RUNTIME DESTINATION bin)

add_executable(test-files-live
    test_files_live.cpp
    FilesBridge.cpp
    FilesBridge.hpp
)
target_include_directories(test-files-live PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-files-live PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
)
set_target_properties(test-files-live PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

