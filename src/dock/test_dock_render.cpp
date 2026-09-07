#include "dock/DockWidget.hpp"
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <functional>
#include <iostream>

using namespace txui;

static void render_dock_frame(const std::string& name,
                              const std::function<void(DockWidget&)>& setup,
                              const std::string& filename) {
    std::cout << "[Visual Test] Rendering Dock: " << name << " -> " << filename << std::endl;
    const uint32_t W = 960, H = 200;
    Canvas canvas(W, H);
    canvas.clear(Color(10, 12, 18, 255));
    PixmanBackend backend;

    auto dock = make_ref<DockWidget>();
    setup(*dock);

    Constraints constraints(0, W, 0, H);
    dock->measure(constraints);
    dock->layout(Rect(0, 0, W, H));

    CommandBuffer cmds;
    Painter painter(cmds);
    painter.begin_frame();
    dock->paint(painter);
    painter.end_frame();
    backend.execute(cmds, canvas);

    if (ImageWriter::save_png(canvas, filename)) {
        std::cout << "  [SUCCESS] Saved " << filename << std::endl;
    } else {
        std::cerr << "  [FAILURE] Could not save " << filename << std::endl;
    }
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << " TxUI Dock Visual Verification Harness            " << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Idle Dock State (Running indicators active on Terminal and Files)
    render_dock_frame("Dock Idle State", [](DockWidget& d) {
        d.update_icon_state("tinexus-terminal", DockIconAppState::RunningFocused);
        d.update_icon_state("tinexus-files",    DockIconAppState::RunningBg);
        d.update_icon_state("tinexus-settings", DockIconAppState::NotRunning);
        d.update_icon_state("tinexus-monitor",  DockIconAppState::NotRunning);
        d.update_icon_state("tinexus-pkg",      DockIconAppState::NotRunning);
    }, "dock_idle_state.png");

    // 2. Hover Magnify State (Hover over Files icon: parabolic magnification + tooltip)
    render_dock_frame("Dock Hover Magnify (Files)", [](DockWidget& d) {
        d.update_icon_state("tinexus-terminal", DockIconAppState::RunningFocused);
        d.update_icon_state("tinexus-files",    DockIconAppState::RunningBg);

        // Pre-measure, layout, and warmup paint to compute icon center coordinates
        Constraints constraints(0, 960, 0, 200);
        d.measure(constraints);
        d.layout(Rect(0, 0, 960, 200));

        CommandBuffer warmup_cmds;
        Painter warmup_painter(warmup_cmds);
        warmup_painter.begin_frame();
        d.paint(warmup_painter);
        warmup_painter.end_frame();

        // Center of icon 1 ("Files")
        const auto& icons = d.icons();
        int files_x = static_cast<int>(icons[1].center_x);

        Event ev_move;
        ev_move.type = EventType::PointerMove;
        ev_move.pointer.x = files_x;
        ev_move.pointer.y = 150;
        d.handle_event(ev_move);

        // Advance physics springs to reach target scale
        for (int i = 0; i < 60; ++i) {
            d.tick_animations(0.016);
        }
        for (const auto& ic : d.icons()) {
            std::cout << "  [DEBUG] " << ic.label << ": scale=" << ic.scale_spring.value
                      << " target=" << ic.scale_spring.target << " center_x=" << ic.center_x << std::endl;
        }
    }, "dock_hover_magnify.png");

    return 0;
}
