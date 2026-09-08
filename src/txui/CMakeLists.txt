find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

add_library(txui STATIC
    core/Version.cpp
    render/pixman/PixmanBackend.cpp
    render/FontMetrics.cpp
    render/ImageWriter.cpp
    render/ImageReader.cpp
    render/WaylandRenderTarget.cpp
    wayland/WaylandConnection.cpp
    wayland/WaylandClipboard.cpp
    wayland/WaylandBuffer.cpp
    wayland/WaylandSurface.cpp
    wayland/WaylandInput.cpp
    wayland/WaylandEventLoop.cpp
    window/Window.cpp
    widgets/Widget.cpp
    widgets/TitleBarWidget.cpp
    widgets/ChromeWidget.cpp
    widgets/ListView.cpp
    widgets/Label.cpp
    widgets/Icon.cpp
    widgets/Button.cpp
    widgets/TextInput.cpp
    widgets/SegmentedControl.cpp
    widgets/ToggleSwitch.cpp
    widgets/Slider.cpp
    widgets/NavItem.cpp
    widgets/ColorPicker.cpp
    widgets/Badge.cpp
    widgets/LineGraphWidget.cpp
    layout/FlexLayout.cpp
    layout/ScrollArea.cpp
    animation/KineticScroller.cpp
    animation/CursorAnimator.cpp
)

target_compile_features(txui PUBLIC cxx_std_20)

target_include_directories(txui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(txui
    PUBLIC
        PkgConfig::WAYLAND_CLIENT
        PkgConfig::FREETYPE
        tinexus_protocols_client
    PRIVATE
        tinexus_warnings
)

add_executable(test-lru-stress ../../tests/unit/test_lru_stress.cpp)
target_link_libraries(test-lru-stress PRIVATE txui tinexus_common)

add_executable(unit_test_txui_clipboard ../../tests/unit/test_txui_clipboard.cpp)
target_link_libraries(unit_test_txui_clipboard PRIVATE txui tinexus_common)

add_executable(test-txui-widgets-render ../../tests/unit/test_txui_widgets_render.cpp)
target_link_libraries(test-txui-widgets-render PRIVATE txui tinexus_common)
set_target_properties(test-txui-widgets-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
