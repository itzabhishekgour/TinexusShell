#include "lock/LockWidget.hpp"
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <iostream>
#include <memory>

using namespace tinexus::lock;

void render_lock_state(const std::string& name,
                       const std::function<void(LockWidget&)>& setup,
                       const std::string& out_filename) {
    std::cout << "[Visual Test] Rendering Lock Screen: " << name << " -> " << out_filename << std::endl;

    const uint32_t W = 960, H = 540;
    txui::Canvas canvas(W, H);
    canvas.clear(txui::Color(2, 3, 10, 255));
    txui::PixmanBackend backend;

    auto widget = txui::make_ref<LockWidget>();
    setup(*widget);

    txui::Constraints constraints(0, W, 0, H);
    widget->measure(constraints);
    widget->layout(txui::Rect(0, 0, W, H));

    txui::CommandBuffer cmds;
    txui::Painter painter(cmds);
    painter.begin_frame();
    widget->paint(painter);
    painter.end_frame();
    backend.execute(cmds, canvas);

    if (txui::ImageWriter::save_png(canvas, out_filename)) {
        std::cout << "  [SUCCESS] Saved " << out_filename << std::endl;
    } else {
        std::cerr << "  [FAILURE] Could not save " << out_filename << std::endl;
    }
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << " TxUI Lock Screen Visual Verification Harness     " << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Default Locked Screen (Placeholder hint)
    render_lock_state("Default Locked", [](LockWidget&) {
        // No password typed
    }, "lock_screen_default.png");

    // 2. Active Password Entry (Masked bullets)
    render_lock_state("Password Entered (8 chars)", [](LockWidget& w) {
        for (char c : std::string("mypass12")) {
            w.add_password_char(c);
        }
    }, "lock_screen_password.png");

    // 3. Failed Attempt Shake & Error Border (Test-only direct trigger)
    render_lock_state("Auth Error / Shaking State", [](LockWidget& w) {
        w.trigger_shake_animation();
        // Advance 2 frames to capture active displacement (e.g. +10 or -10px) and error border
        w.advance_shake();
        w.advance_shake();
    }, "lock_screen_shake.png");

    // 4. Lockout State (5 failed attempts -> 30s countdown)
    render_lock_state("Locked Out State (30s)", [](LockWidget& w) {
        w.set_lockout(30);
    }, "lock_screen_lockout.png");

    return 0;
}
