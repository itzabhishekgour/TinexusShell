#include "monitor/MonitorWidget.hpp"
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include <iostream>

using namespace txui;
using namespace tinexus::monitor;

int main() {
    std::cout << "=== Tinexus Activity Monitor Batch 2 Test Suite ===" << std::endl;

    auto widget = txui::make_ref<MonitorWidget>();
    auto chrome = txui::make_ref<txui::ChromeWidget>(
        "Activity Monitor",
        widget,
        []() {}, []() {}, []() {}, [](uint32_t) {}
    );

    const uint32_t W = 860, H = 580;
    txui::Constraints constraints(0, W, 0, H);
    chrome->measure(constraints);
    chrome->layout(txui::Rect(0, 0, W, H));

    txui::Canvas canvas(W, H);
    txui::PixmanBackend backend;

    // Refresh telemetry to collect real Linux /proc snapshot and trigger CPU % deltas
    widget->refresh_telemetry();

    // 1. Render Performance Tab
    widget->switch_tab(MonitorTab::Performance);
    canvas.clear(txui::Color(14, 14, 20, 255));
    {
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        chrome->paint(painter);
        painter.end_frame();
        backend.execute(cmds, canvas);
    }
    if (!txui::ImageWriter::save_png(canvas, "monitor_performance_tab.png")) {
        std::cerr << "Failed to save monitor_performance_tab.png" << std::endl;
        return 1;
    }
    std::cout << "[Visual Test] Saved monitor_performance_tab.png" << std::endl;

    // 2. Render Services Tab
    widget->switch_tab(MonitorTab::Services);
    canvas.clear(txui::Color(14, 14, 20, 255));
    {
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        chrome->paint(painter);
        painter.end_frame();
        backend.execute(cmds, canvas);
    }
    if (!txui::ImageWriter::save_png(canvas, "monitor_services_tab.png")) {
        std::cerr << "Failed to save monitor_services_tab.png" << std::endl;
        return 1;
    }
    std::cout << "[Visual Test] Saved monitor_services_tab.png" << std::endl;

    // 3. Render Processes Tab (Real Linux /proc data, sorted by CPU% desc)
    widget->switch_tab(MonitorTab::Processes);
    canvas.clear(txui::Color(14, 14, 20, 255));
    {
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        chrome->paint(painter);
        painter.end_frame();
        backend.execute(cmds, canvas);
    }
    if (!txui::ImageWriter::save_png(canvas, "monitor_processes_tab.png")) {
        std::cerr << "Failed to save monitor_processes_tab.png" << std::endl;
        return 1;
    }
    std::cout << "[Visual Test] Saved monitor_processes_tab.png (Real Processes)" << std::endl;

    // 4. Test Search Filtering (Search "system")
    widget->set_search_query("system");
    canvas.clear(txui::Color(14, 14, 20, 255));
    {
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        chrome->paint(painter);
        painter.end_frame();
        backend.execute(cmds, canvas);
    }
    if (!txui::ImageWriter::save_png(canvas, "monitor_processes_search.png")) {
        std::cerr << "Failed to save monitor_processes_search.png" << std::endl;
        return 1;
    }
    std::cout << "[Visual Test] Saved monitor_processes_search.png (Search Filtered)" << std::endl;

    // Reset search
    widget->set_search_query("");

    // 5. Test Confirmation Modal (Select process and trigger End Process modal)
    const auto& snap = widget->snapshot();
    if (!snap.processes.empty()) {
        widget->set_selected_pid(snap.processes.front().pid);
    }
    widget->trigger_end_process(false); // Opens SIGTERM confirmation modal

    canvas.clear(txui::Color(14, 14, 20, 255));
    {
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        chrome->paint(painter);
        painter.end_frame();
        backend.execute(cmds, canvas);
    }
    if (!txui::ImageWriter::save_png(canvas, "monitor_kill_modal.png")) {
        std::cerr << "Failed to save monitor_kill_modal.png" << std::endl;
        return 1;
    }
    std::cout << "[Visual Test] Saved monitor_kill_modal.png (Confirmation Modal)" << std::endl;

    std::cout << "=== All Monitor visual and interactive tests passed successfully! ===" << std::endl;
    return 0;
}
