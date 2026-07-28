#include <txui/render/CanvasRenderTarget.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/core/Time.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>

int main() {
    constexpr txui::uint32 WIDTH = 1920;
    constexpr txui::uint32 HEIGHT = 1080;
    constexpr int NUM_COMMANDS = 10000;
    constexpr int WARMUP_ITERATIONS = 5;
    constexpr int MEASURED_ITERATIONS = 20;

    std::cout << "=== libtxui Benchmark: Paint Rectangles ===" << std::endl;

    txui::CanvasRenderTarget canvas(WIDTH, HEIGHT);
    txui::CommandBuffer buffer;
    txui::Painter painter(buffer);

    painter.begin_frame();
    for (int i = 0; i < NUM_COMMANDS; ++i) {
        auto idx = static_cast<txui::uint32>(i);
        double x = static_cast<double>((idx * 17U) % (WIDTH - 100U));
        double y = static_cast<double>((idx * 31U) % (HEIGHT - 100U));
        painter.fill_rect(
            txui::Rect(x, y, 80.0, 60.0),
            txui::Color(static_cast<txui::uint8>((i * 3) % 255),
                        static_cast<txui::uint8>((i * 7) % 255),
                        static_cast<txui::uint8>((i * 11) % 255),
                        180)
        );
    }
    painter.end_frame();

    txui::PixmanBackend backend;

    // Warm-up
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        backend.execute(buffer, canvas);
    }

    // Measured iterations
    std::vector<double> times_ms;
    times_ms.reserve(MEASURED_ITERATIONS);

    for (int i = 0; i < MEASURED_ITERATIONS; ++i) {
        auto start = txui::Time::now();
        backend.execute(buffer, canvas);
        auto end = txui::Time::now();
        double ms = txui::Time::to_milliseconds(end - start);
        times_ms.push_back(ms);
    }

    double sum_ms = 0.0;
    double min_ms = 1e9;
    double max_ms = 0.0;
    for (double ms : times_ms) {
        sum_ms += ms;
        min_ms = std::min(min_ms, ms);
        max_ms = std::max(max_ms, ms);
    }
    double avg_ms = sum_ms / MEASURED_ITERATIONS;

    // Throughput calculations
    // Total pixels rendered per frame ~ NUM_COMMANDS * (80 * 60)
    constexpr double PIXELS_PER_FRAME = NUM_COMMANDS * (80.0 * 60.0);
    double m_pixels_per_ms = (PIXELS_PER_FRAME / avg_ms) / 1e6;
    double rects_per_sec = (static_cast<double>(NUM_COMMANDS) / (avg_ms / 1000.0)) / 1e6;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\nCanvas Size:         " << WIDTH << "x" << HEIGHT << "\n"
              << "Commands:            " << NUM_COMMANDS << "\n"
              << "Average:             " << avg_ms << " ms\n"
              << "Worst:               " << max_ms << " ms\n"
              << "Best:                " << min_ms << " ms\n"
              << "Throughput:          " << m_pixels_per_ms << " million pixels/ms\n"
              << "Rectangles/sec:      " << rects_per_sec << " million\n"
              << std::endl;

    return 0;
}
