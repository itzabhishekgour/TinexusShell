# txui examples build configuration

add_executable(txui-demo-01-core 01_core/main.cpp)
target_link_libraries(txui-demo-01-core PRIVATE txui)

add_executable(txui-demo-02-math 02_math/main.cpp)
target_link_libraries(txui-demo-02-math PRIVATE txui)

add_executable(txui-demo-01-canvas 01_canvas/main.cpp)
target_link_libraries(txui-demo-01-canvas PRIVATE txui)

add_executable(txui-demo-02-rounded-rects 02_rounded_rects/main.cpp)
target_link_libraries(txui-demo-02-rounded-rects PRIVATE txui)

add_executable(txui-demo-03-wayland 03_wayland/main.cpp)
target_link_libraries(txui-demo-03-wayland PRIVATE txui)

add_executable(txui-demo-04-window 04_window/main.cpp)
target_link_libraries(txui-demo-04-window PRIVATE txui)

add_executable(txui-demo-05-layout 05_layout/main.cpp)
target_link_libraries(txui-demo-05-layout PRIVATE txui)

add_executable(txui-demo-06-resize-stress 06_resize_stress/main.cpp)
target_link_libraries(txui-demo-06-resize-stress PRIVATE txui)
