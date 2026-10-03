cmake_minimum_required(VERSION 3.28)

set(CMAKE_AUTOMOC ON)

add_executable(tinexus-qt6-popup-test
    main.cpp
)

target_link_libraries(tinexus-qt6-popup-test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)
