#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/wayland/WaylandConnection.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/core/Version.hpp>
#include <iostream>

int main() {
    std::cout << "=== txui-demo-03-wayland (Phase 4.2.3 Vertical Slice) ===" << std::endl;
    std::cout << "libtxui Version: " << txui::TXUI_VERSION_STRING << std::endl;

    auto conn_opt = txui::wayland::WaylandConnection::connect();
    if (!conn_opt.has_value()) {
        std::cout << "NOTE: No active Wayland compositor socket found. "
                  << "To view live Wayland window, execute inside tinexus-comp or a Wayland desktop session."
                  << std::endl;
        return 0;
    }

    txui::wayland::WaylandConnection& conn = conn_opt.value();
    auto target_opt = txui::WaylandRenderTarget::create(conn, 480, 320);
    if (!target_opt.has_value() || !target_opt->is_valid()) {
        std::cerr << "FAIL: Could not create double-buffered WaylandRenderTarget" << std::endl;
        return 1;
    }

    txui::WaylandRenderTarget& target = target_opt.value();

    txui::CommandBuffer buffer;
    txui::Painter painter(buffer);

    painter.begin_frame();
    // Clear background to dark charcoal
    painter.fill_rect(txui::Rect(0.0, 0.0, 480.0, 320.0), txui::Color(30, 30, 35, 255));

    // Red rounded rectangle as requested in acceptance criteria
    painter.fill_rounded_rect(
        txui::Rect(40.0, 40.0, 400.0, 240.0),
        24.0,
        txui::Color(235, 50, 50, 250)
    );
    painter.end_frame();

    txui::PixmanBackend backend;
    backend.execute(buffer, target);

    target.present();

    std::cout << "SUCCESS: Displayed red rounded rectangle on real Wayland surface "
              << "(double buffer swapped: Front=" << target.front_index()
              << ", Back=" << target.back_index() << ")" << std::endl;

    return 0;
}
