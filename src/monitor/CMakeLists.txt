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

set(MONITOR_COMMON_SRCS
    metrics_snapshot.cpp
    cpu_parser.cpp
    memory_parser.cpp
    disk_parser.cpp
    network_parser.cpp
    power_parser.cpp
    gpu_parser.cpp
    process_tree.cpp
    process_controller.cpp
    resource_monitor.cpp
)

add_executable(tinexus-monitor
    main.cpp
    MonitorBridge.cpp
    MonitorBridge.hpp
    ${MONITOR_COMMON_SRCS}
)

target_include_directories(tinexus-monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(tinexus-monitor PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::WaylandClient
)

set_target_properties(tinexus-monitor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS tinexus-monitor RUNTIME DESTINATION bin)

add_executable(test-monitor-live
    test_monitor_live.cpp
    MonitorBridge.cpp
    MonitorBridge.hpp
    ${MONITOR_COMMON_SRCS}
)

target_include_directories(test-monitor-live PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(test-monitor-live PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::WaylandClient
)

set_target_properties(test-monitor-live PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
