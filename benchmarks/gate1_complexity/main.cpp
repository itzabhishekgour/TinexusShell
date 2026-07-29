#include <txui/widgets/Widget.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/render/Painter.hpp>
#include <iostream>
#include <chrono>
#include <vector>

using namespace txui;

double measure_time(int num_widgets) {
    auto root = make_ref<Column>();
    
    // Create deep/wide tree
    for (int i = 0; i < num_widgets; ++i) {
        root->add_child(make_ref<SolidColorWidget>(Color(255, 255, 255, 255)));
    }
    
    CommandBuffer buffer;
    Painter painter(buffer);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    root->measure(Constraints::tight(1920, 1080));
    root->layout(Rect(0, 0, 1920, 1080));
    root->paint(painter);
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> diff = end - start;
    return diff.count();
}

int main() {
    std::cout << "Running Gate 1: Performance Complexity Gate...\n";

    double time_100 = measure_time(100);
    double time_1000 = measure_time(1000);
    double time_5000 = measure_time(5000);

    std::cout << "100 widgets:   " << time_100 << " us\n";
    std::cout << "1000 widgets:  " << time_1000 << " us\n";
    std::cout << "5000 widgets:  " << time_5000 << " us\n";

    // O(n) check: 1000 should be roughly 10x 100.
    // Allow up to 3x variance for cache effects/warmup (e.g. 30x instead of 10x).
    // If it was O(n^2), 1000 would be 100x slower.
    
    double ratio_10x = time_1000 / time_100;
    double ratio_50x = time_5000 / time_100;
    
    std::cout << "Ratio 1000/100: " << ratio_10x << "x (Expected ~10x, Limit < 40x)\n";
    std::cout << "Ratio 5000/100: " << ratio_50x << "x (Expected ~50x, Limit < 200x)\n";
    
    if (ratio_10x > 40.0 || ratio_50x > 200.0) {
        std::cerr << "FAIL: O(n) scaling violated! Potential O(n^2) regression detected.\n";
        return 1;
    }
    
    std::cout << "PASS: Layout pipeline scales linearly O(n).\n";
    return 0;
}
