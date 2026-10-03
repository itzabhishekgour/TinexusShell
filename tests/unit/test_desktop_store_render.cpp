#include <txui/render/Canvas.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/ImageWriter.hpp>
#include "../../src/app-installer/StoreWidget.hpp"
#include <iostream>

using namespace txui;
using namespace tinexus::store;

int main() {
    const uint32_t W = 1280;
    const uint32_t H = 800;

    Canvas canvas(W, H);
    // 1. Dark Desktop Background with subtle gradient feel
    canvas.clear(Color(10, 12, 16, 255));
    PixmanBackend backend;

    CommandBuffer cmds;
    Painter p(cmds);
    p.begin_frame();

    // Subtle desktop wallpaper gradient block
    p.fill_rect(Rect(0, 0, W, H), Color(13, 15, 20, 255));
    p.fill_rounded_rect(Rect(80, 60, W - 160, H - 120), 40.0, Color(18, 22, 30, 255));

    // 2. Top Bar (32px height)
    p.fill_rect(Rect(0, 0, W, 32), Color(20, 22, 28, 230));
    p.draw_text(Point(16, 21), "Tinexus OS", Color(255, 255, 255, 255), 13.0, true);
    p.draw_text(Point(110, 21), "Activities", Color(180, 185, 200, 255), 13.0);
    p.draw_text(Point(W / 2.0 - 50.0, 21), "Tue Sep 8  15:10", Color(220, 225, 235, 255), 13.0);
    p.draw_text(Point(W - 140.0, 21), "100%  [WiFi]  [Vol]", Color(180, 185, 200, 255), 13.0);

    // 3. Render Store Window at (160, 80) with 960x600 size
    const double win_x = 160.0;
    const double win_y = 80.0;
    const double win_w = 960.0;
    const double win_h = 600.0;

    // Window shadow & frame
    p.fill_rounded_rect(Rect(win_x - 4, win_y - 4, win_w + 8, win_h + 8), 16.0, Color(0, 0, 0, 120));
    p.fill_rounded_rect(Rect(win_x, win_y, win_w, win_h), 12.0, Color(13, 14, 18, 255));

    // Window Titlebar
    p.fill_rounded_rect(Rect(win_x, win_y, win_w, 36), 12.0, Color(22, 24, 30, 255));
    p.fill_rect(Rect(win_x, win_y + 24, win_w, 12), Color(22, 24, 30, 255)); // square bottom corners
    // Window control buttons
    p.fill_circle(Point(win_x + 18, win_y + 18), 6.0, Color(237, 106, 94, 255));
    p.fill_circle(Point(win_x + 38, win_y + 18), 6.0, Color(245, 190, 79, 255));
    p.fill_circle(Point(win_x + 58, win_y + 18), 6.0, Color(98, 197, 84, 255));
    p.draw_text(Point(win_x + win_w / 2.0 - 60.0, win_y + 23), "App Store — Tinexus", Color(200, 205, 215, 255), 13.0);

    p.end_frame();
    backend.execute(cmds, canvas);

    // Paint StoreWidget inside the window frame
    auto store = make_ref<StoreWidget>();
    store->set_category("Discover");
    Constraints store_constraints(0, win_w, 0, win_h - 36);
    store->measure(store_constraints);
    store->layout(Rect(win_x, win_y + 36, win_w, win_h - 36));

    CommandBuffer store_cmds;
    Painter store_p(store_cmds);
    store_p.begin_frame();
    store->paint(store_p);
    store_p.end_frame();
    backend.execute(store_cmds, canvas);

    // 4. Desktop Dock at Bottom
    CommandBuffer dock_cmds;
    Painter dock_p(dock_cmds);
    dock_p.begin_frame();
    const double dock_w = 340.0;
    const double dock_h = 56.0;
    const double dock_x = (W - dock_w) / 2.0;
    const double dock_y = H - dock_h - 16.0;

    dock_p.fill_rounded_rect(Rect(dock_x, dock_y, dock_w, dock_h), 18.0, Color(24, 26, 34, 220));

    // Dock App Icons
    const double icon_y = dock_y + 12.0;
    // Terminal
    dock_p.fill_rounded_rect(Rect(dock_x + 20, icon_y, 32, 32), 8.0, Color(40, 44, 56, 255));
    dock_p.draw_text(Point(dock_x + 28, icon_y + 22), ">_", Color(255, 255, 255, 255), 14.0);
    // Files
    dock_p.fill_rounded_rect(Rect(dock_x + 72, icon_y, 32, 32), 8.0, Color(66, 133, 244, 255));
    dock_p.draw_text(Point(dock_x + 83, icon_y + 22), "F", Color(255, 255, 255, 255), 14.0);
    // Monitor
    dock_p.fill_rounded_rect(Rect(dock_x + 124, icon_y, 32, 32), 8.0, Color(52, 168, 83, 255));
    dock_p.draw_text(Point(dock_x + 133, icon_y + 22), "M", Color(255, 255, 255, 255), 14.0);
    // Store (Active Indicator dot below)
    dock_p.fill_rounded_rect(Rect(dock_x + 176, icon_y, 32, 32), 8.0, Color(168, 85, 247, 255));
    dock_p.draw_text(Point(dock_x + 187, icon_y + 22), "S", Color(255, 255, 255, 255), 14.0);
    dock_p.fill_circle(Point(dock_x + 192, dock_y + dock_h - 5), 2.0, Color(255, 255, 255, 255));
    // Settings
    dock_p.fill_rounded_rect(Rect(dock_x + 228, icon_y, 32, 32), 8.0, Color(75, 85, 99, 255));
    dock_p.draw_text(Point(dock_x + 239, icon_y + 24), "*", Color(255, 255, 255, 255), 16.0);
    // Ksnip
    dock_p.fill_rounded_rect(Rect(dock_x + 280, icon_y, 32, 32), 8.0, Color(236, 72, 153, 255));
    dock_p.draw_text(Point(dock_x + 290, icon_y + 22), "K", Color(255, 255, 255, 255), 14.0);

    dock_p.end_frame();
    backend.execute(dock_cmds, canvas);

    bool saved = ImageWriter::save_png(canvas, "build/tinexus_desktop_store_ksnip.png");
    if (saved) {
        std::cout << "[PASS] Successfully rendered full desktop screenshot to build/tinexus_desktop_store_ksnip.png" << std::endl;
        return 0;
    }
    return 1;
}
