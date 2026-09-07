#include "DesktopShellWidget.hpp"
#include "../about/AboutWidget.hpp"
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <common/logger.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <common/stb_image.h>
#pragma GCC diagnostic pop

using namespace tinexus;
using namespace tinexus::shell;

namespace {

std::shared_ptr<std::vector<uint32_t>> load_wallpaper(uint32_t& out_w, uint32_t& out_h) {
    int w = 0, h = 0, comp = 0;
    const std::vector<std::string> paths = {
        "assets/wallpaper/tinexus-default.jpg",
        "../assets/wallpaper/tinexus-default.jpg",
        "../../assets/wallpaper/tinexus-default.jpg",
        "/usr/share/backgrounds/tinexus-default.jpg"
    };

    uint8_t* raw = nullptr;
    for (const auto& p : paths) {
        std::ifstream f(p, std::ios::binary);
        if (f.good()) {
            f.close();
            raw = stbi_load(p.c_str(), &w, &h, &comp, 4);
            if (raw) {
                std::cout << "[Full Desktop] Loaded wallpaper from " << p << " (" << w << "x" << h << ")" << std::endl;
                break;
            }
        }
    }

    if (!raw) return nullptr;

    out_w = static_cast<uint32_t>(w);
    out_h = static_cast<uint32_t>(h);
    auto pixels = std::make_shared<std::vector<uint32_t>>(static_cast<size_t>(w) * static_cast<size_t>(h));

    for (size_t i = 0; i < pixels->size(); ++i) {
        uint32_t a = raw[i * 4 + 3];
        uint32_t r = (static_cast<uint32_t>(raw[i * 4 + 0]) * a) / 255U;
        uint32_t g = (static_cast<uint32_t>(raw[i * 4 + 1]) * a) / 255U;
        uint32_t b = (static_cast<uint32_t>(raw[i * 4 + 2]) * a) / 255U;
        (*pixels)[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    stbi_image_free(raw);
    return pixels;
}

void render_base_wallpaper(txui::Painter& painter,
                           const std::shared_ptr<std::vector<uint32_t>>& wp_pixels,
                           uint32_t wp_w, uint32_t wp_h,
                           double W, double H) {
    if (wp_pixels && wp_w > 0 && wp_h > 0) {
        painter.draw_image(txui::Rect(0, 0, W, H), wp_pixels, wp_w, wp_h);
    } else {
        painter.fill_gradient_rect(txui::Rect(0, 0, W, H),
                                   txui::Color(14, 18, 36, 255),
                                   txui::Color(4, 6, 14, 255));
    }
}

// Compositor blending: simulates Wayland SHM surface composite onto the desktop canvas
void blend_surface(txui::Canvas& dst_canvas, const txui::Canvas& src_canvas, int dst_x, int dst_y) {
    uint32_t dw = dst_canvas.width();
    uint32_t dh = dst_canvas.height();
    uint32_t sw = src_canvas.width();
    uint32_t sh = src_canvas.height();

    uint32_t* dst_buf = dst_canvas.data();
    const uint32_t* src_buf = src_canvas.data();

    for (uint32_t sy = 0; sy < sh; ++sy) {
        int dy = dst_y + static_cast<int>(sy);
        if (dy < 0 || dy >= static_cast<int>(dh)) continue;

        for (uint32_t sx = 0; sx < sw; ++sx) {
            int dx = dst_x + static_cast<int>(sx);
            if (dx < 0 || dx >= static_cast<int>(dw)) continue;

            uint32_t src = src_buf[sy * sw + sx];
            uint32_t sa = (src >> 24) & 0xFFU;
            if (sa == 0) continue;

            uint32_t di = static_cast<uint32_t>(dy) * dw + static_cast<uint32_t>(dx);
            uint32_t dst = dst_buf[di];

            if (sa == 255U) {
                dst_buf[di] = src;
            } else {
                uint32_t inv_a = 255U - sa;
                uint32_t sr = (src >> 16) & 0xFFU;
                uint32_t sg = (src >> 8)  & 0xFFU;
                uint32_t sb =  src        & 0xFFU;

                uint32_t dr = (dst >> 16) & 0xFFU;
                uint32_t dg = (dst >> 8)  & 0xFFU;
                uint32_t db =  dst        & 0xFFU;

                uint32_t out_r = sr + ((dr * inv_a) / 255U);
                uint32_t out_g = sg + ((dg * inv_a) / 255U);
                uint32_t out_b = sb + ((db * inv_a) / 255U);
                dst_buf[di] = (0xFFU << 24) | (out_r << 16) | (out_g << 8) | out_b;
            }
        }
    }
}

} // namespace

int main() {
    std::cout << "[Full Desktop Test] Rendering complete 1920x1080 OS desktop..." << std::endl;
    const uint32_t W = 1920;
    const uint32_t H = 1080;
    txui::PixmanBackend backend;

    uint32_t wp_w = 0, wp_h = 0;
    auto wp_pixels = load_wallpaper(wp_w, wp_h);

    auto render_wallpaper_canvas = [&]() -> txui::Canvas {
        txui::Canvas bg(W, H);
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        render_base_wallpaper(painter, wp_pixels, wp_w, wp_h, W, H);
        painter.end_frame();
        backend.execute(cmds, bg);
        return bg;
    };

    // ── 1. Full Desktop Home / Idle (Wallpaper + Top Bar + Protruding Aura) ──
    {
        auto desktop = render_wallpaper_canvas();

        // Render Shell surface (1920 x 46)
        const uint32_t shell_h = 46;
        auto shell_widget = txui::make_ref<DesktopShellWidget>();
        shell_widget->measure(txui::Constraints(0, W, 0, shell_h));
        shell_widget->layout(txui::Rect(0, 0, W, shell_h));

        txui::Canvas shell_canvas(W, shell_h);
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        shell_widget->paint(painter);
        painter.end_frame();
        backend.execute(cmds, shell_canvas);

        blend_surface(desktop, shell_canvas, 0, 0);

        txui::ImageWriter::save_png(desktop, "full_desktop_home.png");
        std::cout << "[Full Desktop Test] Saved full_desktop_home.png" << std::endl;
    }

    // ── 2. Full Desktop with About Tinexus Window Floating Over Wallpaper ────
    {
        auto desktop = render_wallpaper_canvas();

        // 1. Render About Window surface (740 x 450)
        const uint32_t win_w = 740;
        const uint32_t win_h = 450;
        const int win_x = static_cast<int>((W - win_w) / 2);
        const int win_y = static_cast<int>((H - win_h) / 2 - 30);

        auto about_widget = txui::make_ref<about::AboutWidget>();
        about_widget->measure(txui::Constraints(0, win_w, 0, win_h));
        about_widget->layout(txui::Rect(0, 0, win_w, win_h));

        // Draw shadow on desktop first
        {
            txui::CommandBuffer shadow_cmds;
            txui::Painter painter(shadow_cmds);
            painter.begin_frame();
            painter.fill_rounded_rect(txui::Rect(win_x - 12.0, win_y + 14.0, win_w + 24.0, win_h + 16.0),
                                      20.0, txui::Color(0, 0, 0, 120));
            painter.end_frame();
            backend.execute(shadow_cmds, desktop);
        }

        txui::Canvas about_canvas(win_w, win_h);
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        about_widget->paint(painter);
        painter.end_frame();
        backend.execute(cmds, about_canvas);

        blend_surface(desktop, about_canvas, win_x, win_y);

        // 2. Render Shell surface on top (1920 x 46)
        const uint32_t shell_h = 46;
        auto shell_widget = txui::make_ref<DesktopShellWidget>();
        shell_widget->measure(txui::Constraints(0, W, 0, shell_h));
        shell_widget->layout(txui::Rect(0, 0, W, shell_h));

        txui::Canvas shell_canvas(W, shell_h);
        txui::CommandBuffer shell_cmds;
        txui::Painter shell_painter(shell_cmds);
        shell_painter.begin_frame();
        shell_widget->paint(shell_painter);
        shell_painter.end_frame();
        backend.execute(shell_cmds, shell_canvas);

        blend_surface(desktop, shell_canvas, 0, 0);

        txui::ImageWriter::save_png(desktop, "full_desktop_about_window.png");
        std::cout << "[Full Desktop Test] Saved full_desktop_about_window.png" << std::endl;

        // Render Displays Tab
        {
            auto d_desk = render_wallpaper_canvas();
            about_widget->switch_tab(about::AboutTab::Displays);
            about_canvas.clear(txui::Color(0,0,0,0));
            txui::CommandBuffer d_cmds;
            txui::Painter d_painter(d_cmds);
            d_painter.begin_frame();
            about_widget->paint(d_painter);
            d_painter.end_frame();
            backend.execute(d_cmds, about_canvas);
            blend_surface(d_desk, about_canvas, win_x, win_y);
            blend_surface(d_desk, shell_canvas, 0, 0);
            txui::ImageWriter::save_png(d_desk, "full_desktop_about_displays.png");
        }

        // Render Storage Tab
        {
            auto s_desk = render_wallpaper_canvas();
            about_widget->switch_tab(about::AboutTab::Storage);
            about_canvas.clear(txui::Color(0,0,0,0));
            txui::CommandBuffer s_cmds;
            txui::Painter s_painter(s_cmds);
            s_painter.begin_frame();
            about_widget->paint(s_painter);
            s_painter.end_frame();
            backend.execute(s_cmds, about_canvas);
            blend_surface(s_desk, about_canvas, win_x, win_y);
            blend_surface(s_desk, shell_canvas, 0, 0);
            txui::ImageWriter::save_png(s_desk, "full_desktop_about_storage.png");
        }

        // Render Service Tab
        {
            auto v_desk = render_wallpaper_canvas();
            about_widget->switch_tab(about::AboutTab::Service);
            about_canvas.clear(txui::Color(0,0,0,0));
            txui::CommandBuffer v_cmds;
            txui::Painter v_painter(v_cmds);
            v_painter.begin_frame();
            about_widget->paint(v_painter);
            v_painter.end_frame();
            backend.execute(v_cmds, about_canvas);
            blend_surface(v_desk, about_canvas, win_x, win_y);
            blend_surface(v_desk, shell_canvas, 0, 0);
            txui::ImageWriter::save_png(v_desk, "full_desktop_about_service.png");
        }
    }

    // ── 3. Full Desktop with Calendar Flyout Open ───────────────────────────
    {
        auto desktop = render_wallpaper_canvas();

        const uint32_t shell_h = 360;
        auto shell_widget = txui::make_ref<DesktopShellWidget>();
        shell_widget->calendar_open = true;
        shell_widget->measure(txui::Constraints(0, W, 0, shell_h));
        shell_widget->layout(txui::Rect(0, 0, W, shell_h));

        txui::Canvas shell_canvas(W, shell_h);
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        shell_widget->paint(painter);
        painter.end_frame();
        backend.execute(cmds, shell_canvas);

        blend_surface(desktop, shell_canvas, 0, 0);

        txui::ImageWriter::save_png(desktop, "full_desktop_calendar.png");
        std::cout << "[Full Desktop Test] Saved full_desktop_calendar.png" << std::endl;
    }

    // ── 4. Full Desktop with Ctrl+K Pulse Command Palette Active ────────────
    {
        auto desktop = render_wallpaper_canvas();

        const uint32_t shell_h = 580;
        auto shell_widget = txui::make_ref<DesktopShellWidget>();
        shell_widget->pulse_active = true;
        shell_widget->pulse_results = {
            {"Terminal", "foot", "System Terminal (C++20)", true, "", ResultKind::App},
            {"Files", "tinexus-files", "Miller Column File Browser", false, "", ResultKind::App},
            {"Settings", "tinexus-settings-ui", "Tinexus System Settings", false, "", ResultKind::App},
            {"About Tinexus", "tinexus-about", "System Profiler & Specs", false, "", ResultKind::App},
            {"Calculator", "", "42 * 1024 = 43008", false, "", ResultKind::Calculator},
            {"Restart...", "", "Restart the computer", false, "", ResultKind::System}
        };
        shell_widget->measure(txui::Constraints(0, W, 0, shell_h));
        shell_widget->layout(txui::Rect(0, 0, W, shell_h));

        txui::Canvas shell_canvas(W, shell_h);
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        shell_widget->paint(painter);
        painter.end_frame();
        backend.execute(cmds, shell_canvas);

        blend_surface(desktop, shell_canvas, 0, 0);

        txui::ImageWriter::save_png(desktop, "full_desktop_pulse_ctrl_k.png");
        std::cout << "[Full Desktop Test] Saved full_desktop_pulse_ctrl_k.png" << std::endl;
    }

    // ── 5. Full Desktop with Tinexus Logo Menu Open ─────────────────────────
    {
        auto desktop = render_wallpaper_canvas();

        const uint32_t shell_h = 320;
        auto shell_widget = txui::make_ref<DesktopShellWidget>();
        shell_widget->logo_menu_open = true;
        shell_widget->logo_menu_hover = 0; // Hover on "About Tinexus"
        shell_widget->measure(txui::Constraints(0, W, 0, shell_h));
        shell_widget->layout(txui::Rect(0, 0, W, shell_h));

        txui::Canvas shell_canvas(W, shell_h);
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        shell_widget->paint(painter);
        painter.end_frame();
        backend.execute(cmds, shell_canvas);

        blend_surface(desktop, shell_canvas, 0, 0);

        txui::ImageWriter::save_png(desktop, "full_desktop_logo_menu.png");
        std::cout << "[Full Desktop Test] Saved full_desktop_logo_menu.png" << std::endl;
    }

    // ── 6. Full Desktop with Notifications Flyout Open ───────────────────────
    {
        auto desktop = render_wallpaper_canvas();

        const uint32_t shell_h = 360;
        auto shell_widget = txui::make_ref<DesktopShellWidget>();
        shell_widget->notifications_open = true;
        shell_widget->measure(txui::Constraints(0, W, 0, shell_h));
        shell_widget->layout(txui::Rect(0, 0, W, shell_h));

        txui::Canvas shell_canvas(W, shell_h);
        txui::CommandBuffer cmds;
        txui::Painter painter(cmds);
        painter.begin_frame();
        shell_widget->paint(painter);
        painter.end_frame();
        backend.execute(cmds, shell_canvas);

        blend_surface(desktop, shell_canvas, 0, 0);

        txui::ImageWriter::save_png(desktop, "desktop_notifications_open.png");
        std::cout << "[Full Desktop Test] Saved desktop_notifications_open.png" << std::endl;
    }

    return 0;
}
