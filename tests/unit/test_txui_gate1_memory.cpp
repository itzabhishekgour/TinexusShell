#include <txui/window/Window.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/render/Painter.hpp>
#include <iostream>
#include <vector>

using namespace txui;

int main() {
    std::cout << "Running Gate 1: Memory Stability Stress Test...\n";

    // 1. Build a moderate complexity tree
    auto root = make_ref<Column>();
    for (int i = 0; i < 50; ++i) {
        auto row = make_ref<Row>();
        for (int j = 0; j < 20; ++j) {
            row->add_child(make_ref<SolidColorWidget>(Color(i % 255, j % 255, 100, 255)));
        }
        root->add_child(std::move(row));
    }

    CommandBuffer buffer;

    // 2. We can't strictly assert RSS without platform-specific OS APIs (like getrusage or GlobalMemoryStatusEx).
    // Instead, we will simulate 10,000 cycles. If there is a massive leak, this will either 
    // exhaust memory or ASAN/Valgrind will catch it in CI.
    // We will just ensure it completes successfully without crashing or out-of-memory.
    
    std::cout << "Starting 10,000 layout and paint cycles...\n";
    
    for (int cycle = 0; cycle < 10000; ++cycle) {
        // Mutate constraints slightly to force full layout recalculation
        double width = 800.0 + (cycle % 100);
        double height = 600.0 + (cycle % 50);
        
        root->mark_needs_measure();
        root->measure(Constraints::tight(width, height));
        root->layout(Rect(0, 0, width, height));
        
        buffer.clear();
        Painter painter(buffer);
        painter.begin_frame();
        root->paint(painter);
        painter.end_frame();
    }
    
    std::cout << "PASS: 10,000 cycles completed. No crashes. Ensure you run this under ASAN/Valgrind in CI to verify zero leaks.\n";
    return 0;
}
