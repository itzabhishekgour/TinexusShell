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
