#include "DesktopShellWidget.hpp"
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <iostream>

using namespace tinexus;
using namespace tinexus::shell;

int main() {
    std::cout << "[Visual Test] Rendering Desktop Top Bar and Flyouts..." << std::endl;
    auto widget = txui::make_ref<DesktopShellWidget>();

    const uint32_t W = 1920;
    txui::PixmanBackend backend;

    // ── 1. Render Idle Top Bar (1920 x 46) ──────────────────────────────────
    {
        const uint32_t H = 46;
        txui::Constraints constraints(0, W, 0, H);
        widget->measure(constraints);
        widget->layout(txui::Rect(0, 0, W, H));

        txui::Canvas canvas(W, H);
        canvas.clear(txui::Color(0, 0, 0, 0));

        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer_cmds, canvas);

        if (!txui::ImageWriter::save_png(canvas, "desktop_topbar_idle.png")) {
            std::cerr << "FAIL: Failed to save desktop_topbar_idle.png" << std::endl;
            return 1;
        }
        std::cout << "[Visual Test] Successfully saved desktop_topbar_idle.png" << std::endl;
    }

    // ── 2. Render Logo Menu Open (1920 x 290) ───────────────────────────────
    {
        const uint32_t H = 290;
        widget->logo_menu_open = true;
        widget->logo_menu_hover = 0; // Hover on "About Tinexus"

        txui::Constraints constraints(0, W, 0, H);
        widget->measure(constraints);
        widget->layout(txui::Rect(0, 0, W, H));

        txui::Canvas canvas(W, H);
        canvas.clear(txui::Color(0, 0, 0, 0));

        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer_cmds, canvas);

        if (!txui::ImageWriter::save_png(canvas, "desktop_logo_menu_open.png")) {
            std::cerr << "FAIL: Failed to save desktop_logo_menu_open.png" << std::endl;
            return 1;
        }
        std::cout << "[Visual Test] Successfully saved desktop_logo_menu_open.png" << std::endl;
        widget->logo_menu_open = false;
    }

    // ── 3. Render Calendar Flyout Open (1920 x 330) ─────────────────────────
    {
        const uint32_t H = 330;
        widget->calendar_open = true;

        txui::Constraints constraints(0, W, 0, H);
        widget->measure(constraints);
        widget->layout(txui::Rect(0, 0, W, H));

        txui::Canvas canvas(W, H);
        canvas.clear(txui::Color(0, 0, 0, 0));

        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer_cmds, canvas);

        if (!txui::ImageWriter::save_png(canvas, "desktop_calendar_open.png")) {
            std::cerr << "FAIL: Failed to save desktop_calendar_open.png" << std::endl;
            return 1;
        }
        std::cout << "[Visual Test] Successfully saved desktop_calendar_open.png" << std::endl;
        widget->calendar_open = false;
    }

    // ── 4. Render Notification Banners Stack (1920 x 350) ───────────────────
    {
        const uint32_t H = 350;
        widget->notifications_open = true;

        txui::Constraints constraints(0, W, 0, H);
        widget->measure(constraints);
        widget->layout(txui::Rect(0, 0, W, H));

        txui::Canvas canvas(W, H);
        canvas.clear(txui::Color(0, 0, 0, 0));

        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer_cmds, canvas);

        if (!txui::ImageWriter::save_png(canvas, "desktop_notifications_open.png")) {
            std::cerr << "FAIL: Failed to save desktop_notifications_open.png" << std::endl;
            return 1;
        }
        std::cout << "[Visual Test] Successfully saved desktop_notifications_open.png" << std::endl;
        widget->notifications_open = false;
    }

    return 0;
}
