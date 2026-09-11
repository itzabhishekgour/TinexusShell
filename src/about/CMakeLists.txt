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

add_executable(tinexus-about
    main.cpp
    AboutBridge.cpp
    AboutBridge.hpp
)

target_include_directories(tinexus-about PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(tinexus-about PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::WaylandClient
)

set_target_properties(tinexus-about PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

install(TARGETS tinexus-about RUNTIME DESTINATION bin)

add_executable(test-tinexus-about-live
    test_about_live.cpp
    AboutBridge.cpp
    AboutBridge.hpp
)

target_include_directories(test-tinexus-about-live PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(test-tinexus-about-live PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
)

set_target_properties(test-tinexus-about-live PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
