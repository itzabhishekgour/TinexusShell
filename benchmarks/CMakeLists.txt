# txui benchmarks build configuration

add_executable(txui-bench-rectangles paint_rectangles/main.cpp)
target_link_libraries(txui-bench-rectangles PRIVATE txui tinexus_warnings)

add_executable(txui-bench-rounded-rects paint_rounded_rects/main.cpp)
target_link_libraries(txui-bench-rounded-rects PRIVATE txui tinexus_warnings)

add_executable(txui-bench-gate1-complexity gate1_complexity/main.cpp)
target_link_libraries(txui-bench-gate1-complexity PRIVATE txui tinexus_warnings)
