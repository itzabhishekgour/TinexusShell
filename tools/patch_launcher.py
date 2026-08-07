#!/usr/bin/env python3
"""Patch launcher main.cpp: replace main() and everything after build_results() with premium UI."""
import sys

path = "/mnt/e/Tinu's Technology/Tinexus Manager/src/launcher/main.cpp"

with open(path, 'r', errors='replace') as f:
    content = f.read()

# Normalise line endings
content = content.replace('\r\n', '\n')

# Find where build_results ends (the closing brace of the function)
# Strategy: locate "int main(" and truncate everything from there
marker = '\nint main('
idx = content.find(marker)
if idx == -1:
    print("ERROR: 'int main(' not found in file", file=sys.stderr)
    sys.exit(1)

header = content[:idx]  # everything before main

NEW_MAIN = r'''

// ─────────────────────────────────────────────────────────────────────────────
// LauncherWidget — full custom-painted spotlight palette
// ─────────────────────────────────────────────────────────────────────────────
#include <txui/widgets/Widget.hpp>

namespace launcher_ui {
    constexpr txui::Color BG_PANEL_T {18, 18,  30, 252};
    constexpr txui::Color BG_PANEL_B {12, 12,  22, 252};
    constexpr txui::Color ACCENT     {107,140, 239, 255};
    constexpr txui::Color SEL_APP    { 59,130, 246, 200};
    constexpr txui::Color SEL_SYS    {239, 68,  68, 190};
    constexpr txui::Color SEL_CALC   { 16,185, 129, 200};
    constexpr txui::Color TXT_PRI    {240,240, 248, 255};
    constexpr txui::Color TXT_SEC    {180,180, 210, 200};
    constexpr txui::Color TXT_DIM    {120,120, 150, 150};
    constexpr double PANEL_W   = 680.0;
    constexpr double PANEL_RAD = 20.0;
    constexpr double ROW_H     = 46.0;
    constexpr double ROW_GAP   =  2.0;
}

class LauncherWidget : public txui::Widget {
public:
    std::string          query;
    std::vector<AppItem> results;
    size_t               selected_index{0};

    txui::Size measure_override(const txui::Constraints& c) noexcept override {
        return txui::Size(c.max_width, c.max_height);
    }

    void paint_override(txui::Painter& painter) const noexcept override {
        using namespace launcher_ui;
        const double W  = frame().width();
        const double H  = frame().height();
        const double cx = W / 2.0;
        const double cy = H / 2.0;

        // 1. Full-screen dimmed backdrop
        painter.fill_gradient_rect(frame(),
            txui::Color( 2,  2,  8, 242),
            txui::Color( 5,  3, 15, 242));

        // 2. Aurora atmosphere
        painter.fill_circle(txui::Point(cx, cy - 50),       250.0, txui::Color(70, 50, 170, 18));
        painter.fill_circle(txui::Point(cx + 140, cy + 80), 190.0, txui::Color(40, 90, 200, 12));

        // 3. Panel geometry
        const size_t n_rows   = std::min(results.size(), size_t{7});
        const double rows_h   = n_rows > 0 ? (n_rows * ROW_H + (n_rows - 1) * ROW_GAP + 14.0) : 0.0;
        const double SEARCH_H = 58.0;
        const double HEADER_H = 32.0;
        const double panel_h  = HEADER_H + SEARCH_H + rows_h;
        const double panel_x  = cx - PANEL_W * 0.5;
        const double panel_y  = cy - panel_h * 0.5 - 24.0;

        // 4. Panel card with glow + border + glass gradient
        painter.fill_circle(txui::Point(cx, panel_y + panel_h * 0.5),
            PANEL_W * 0.55, txui::Color(80, 60, 180, 8));
        painter.fill_gradient_rounded_rect(
            txui::Rect(panel_x, panel_y, PANEL_W, panel_h),
            PANEL_RAD, BG_PANEL_T, BG_PANEL_B);
        painter.fill_rounded_rect(
            txui::Rect(panel_x + 3, panel_y + 1, PANEL_W - 6, 1),
            0.5, txui::Color(255, 255, 255, 20));
        painter.fill_rounded_rect(
            txui::Rect(panel_x - 1, panel_y - 1, PANEL_W + 2, panel_h + 2),
            PANEL_RAD + 1, txui::Color(130, 130, 220, 30));
        painter.fill_gradient_rounded_rect(
            txui::Rect(panel_x, panel_y, PANEL_W, panel_h),
            PANEL_RAD, BG_PANEL_T, BG_PANEL_B);

        // 5. Header bar
        painter.fill_gradient_rounded_rect(
            txui::Rect(panel_x, panel_y, PANEL_W, HEADER_H),
            PANEL_RAD,
            txui::Color(22, 22, 40, 255),
            txui::Color(16, 16, 30, 255));
        painter.fill_circle(txui::Point(panel_x + 22, panel_y + 16), 5.0, ACCENT);
        painter.draw_glow(txui::Point(panel_x + 22, panel_y + 16), 5.0, 16.0, ACCENT);
        painter.draw_text(txui::Point(panel_x + 36, panel_y + 10),
            "TINEXUS  Command Palette", TXT_DIM, 1.0);
        painter.fill_gradient_rounded_rect(
            txui::Rect(panel_x + PANEL_W - 46, panel_y + 8, 36, 17), 4.0,
            txui::Color(50, 50, 70, 200), txui::Color(38, 38, 56, 200));
        painter.draw_text(txui::Point(panel_x + PANEL_W - 38, panel_y + 11), "Esc", TXT_DIM, 1.0);

        // 6. Search row
        const double srch_y = panel_y + HEADER_H;
        painter.fill_gradient_rect(
            txui::Rect(panel_x, srch_y, PANEL_W, SEARCH_H),
            txui::Color(28, 28, 48, 255),
            txui::Color(22, 22, 38, 255));
        painter.draw_circle(txui::Point(panel_x + 34, srch_y + SEARCH_H * 0.5),
            10.0, 1.5, ACCENT);
        painter.fill_gradient_rounded_rect(
            txui::Rect(panel_x + 40, srch_y + SEARCH_H * 0.5 + 5.0, 9.0, 2.0), 1.0,
            ACCENT, txui::Color(80, 110, 200, 160));
        const std::string display = query.empty()
            ? "Search apps, run commands..." : (query + "_");
        painter.draw_text(
            txui::Point(panel_x + 58, srch_y + (SEARCH_H - 16.0) * 0.5),
            display, query.empty() ? TXT_DIM : TXT_PRI, 1.0);
        painter.fill_gradient_rect(
            txui::Rect(panel_x + 14, srch_y + SEARCH_H - 1, PANEL_W - 28, 1),
            txui::Color(107, 140, 239, 50),
            txui::Color(107, 140, 239, 0), true);

        // 7. Result rows
        if (!results.empty()) {
            const double rows_y = srch_y + SEARCH_H + 8.0;
            for (size_t i = 0; i < n_rows; ++i) {
                const auto&  item = results[i];
                const bool   sel  = (i == selected_index);
                const double ry   = rows_y + static_cast<double>(i) * (ROW_H + ROW_GAP);
                txui::Color  cat  =
                    item.kind == ResultKind::Calculator ? SEL_CALC :
                    item.kind == ResultKind::System     ? SEL_SYS  : SEL_APP;

                if (sel) {
                    painter.fill_gradient_rounded_rect(
                        txui::Rect(panel_x + 8, ry + 1, PANEL_W - 16, ROW_H - 2), 10.0,
                        txui::Color(cat.r(), cat.g(), cat.b(), 52),
                        txui::Color(cat.r(), cat.g(), cat.b(), 22));
                    painter.fill_gradient_rounded_rect(
                        txui::Rect(panel_x + 10, ry + 8, 3, ROW_H - 16), 1.5,
                        cat, txui::Color(cat.r(), cat.g(), cat.b(), 110));
                }

                const double icx = panel_x + 34.0;
                const double icy = ry + ROW_H * 0.5;
                if (sel) {
                    painter.fill_circle(txui::Point(icx, icy), 13.0,
                        txui::Color(cat.r(), cat.g(), cat.b(), 45));
                    painter.draw_circle(txui::Point(icx, icy), 13.0, 1.0,
                        txui::Color(cat.r(), cat.g(), cat.b(), 150));
                } else {
                    painter.draw_circle(txui::Point(icx, icy), 11.0, 1.0,
                        txui::Color(80, 80, 130, 70));
                }
                painter.fill_circle(txui::Point(icx, icy), 3.5,
                    sel ? cat : txui::Color(110, 110, 160, 130));

                painter.draw_text(
                    txui::Point(panel_x + 56, ry + (ROW_H - 16.0) * 0.5),
                    item.name, sel ? TXT_PRI : TXT_SEC, 1.0);

                if (!item.description.empty()) {
                    const size_t max_d = 30;
                    std::string  desc  = item.description.size() > max_d
                        ? item.description.substr(0, max_d) + "..." : item.description;
                    const double dx = panel_x + PANEL_W
                        - static_cast<double>(desc.size()) * 8.0 - 34.0;
                    if (dx > panel_x + PANEL_W * 0.55) {
                        painter.draw_text(txui::Point(dx, ry + (ROW_H - 16.0) * 0.5),
                            desc, TXT_DIM, 1.0);
                    }
                }
                if (i < 9) {
                    painter.draw_text(
                        txui::Point(panel_x + PANEL_W - 22, ry + (ROW_H - 16.0) * 0.5),
                        std::to_string(i + 1), TXT_DIM, 1.0);
                }
            }
        }

        // 8. Bottom tip row
        const double tip_y = panel_y + panel_h + 14.0;
        const char* tips[] = {"Up/Down: navigate", "Enter: launch", "Tab: cycle", nullptr};
        double tip_x = cx - 200.0;
        for (int ti = 0; tips[ti]; ++ti) {
            std::string s(tips[ti]);
            painter.draw_text(txui::Point(tip_x, tip_y), s,
                txui::Color(120, 120, 155, 100), 1.0);
            tip_x += static_cast<double>(s.size()) * 8.0 + 20.0;
        }
    }
};

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    log::set_component_name("launcher");
    log::info("[Launcher] Tinexus Command Palette v2 starting...");
    signal(SIGCHLD, SIG_IGN);

    auto window = txui::Window::create(900, 700, "Tinexus Launcher");
    if (!window || !window->is_wayland_connected()) {
        log::error("[Launcher] Failed to connect to Wayland display! Exiting.");
        return 1;
    }

    std::vector<AppItem> all_apps = load_system_apps();
    std::string          query;
    size_t               selected_index = 0;
    std::vector<AppItem> current_results;

    auto launcher_widget = txui::make_ref<LauncherWidget>();
    window->set_root_widget(launcher_widget);

    auto sync = [&]() {
        current_results = build_results(query, all_apps);
        if (current_results.empty())
            selected_index = 0;
        else if (selected_index >= current_results.size())
            selected_index = current_results.size() - 1;
        launcher_widget->query          = query;
        launcher_widget->selected_index = selected_index;
        launcher_widget->results        = current_results;
        launcher_widget->mark_needs_paint();
    };
    sync();

    bool running = true;
    while (running && !window->should_close()) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            } else if (event.type == txui::EventType::KeyDown) {
                const bool shift = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Shift);
                const bool ctrl  = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Ctrl);
                const bool alt   = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Alt);

                if (event.keyboard.key == txui::Key::Escape) {
                    running = false;
                } else if (event.keyboard.key == txui::Key::Enter) {
                    if (alt) {
                        if (!current_results.empty() && selected_index < current_results.size()) {
                            std::string cmd = "echo -n '" + current_results[selected_index].exec + "' | wl-copy";
                            ::system(cmd.c_str()); // NOLINT
                            running = false;
                        }
                    } else if (!current_results.empty() && selected_index < current_results.size()) {
                        spawn_app(current_results[selected_index]); running = false;
                    } else if (!query.empty()) {
                        AppItem ci; ci.name = query; ci.exec = query; ci.kind = ResultKind::App;
                        spawn_app(ci); running = false;
                    }
                } else if (event.keyboard.key == txui::Key::Up) {
                    if (selected_index > 0) { selected_index--; sync(); }
                } else if (event.keyboard.key == txui::Key::Down) {
                    selected_index++; sync();
                } else if (event.keyboard.key == txui::Key::Tab) {
                    selected_index = (selected_index + 1) % std::max(current_results.size(), size_t{1});
                    sync();
                } else if (event.keyboard.key == txui::Key::Backspace) {
                    if (!query.empty()) { query.pop_back(); selected_index = 0; sync(); }
                } else if (ctrl && event.keyboard.key >= txui::Key::N1 &&
                           event.keyboard.key <= txui::Key::N9) {
                    size_t j = static_cast<size_t>(
                        static_cast<int>(event.keyboard.key) - static_cast<int>(txui::Key::N1));
                    if (j < current_results.size()) { spawn_app(current_results[j]); running = false; }
                } else {
                    char ch = key_to_char(event.keyboard.key, shift);
                    if (ch != '\0') { query += ch; selected_index = 0; sync(); }
                }
            }
        }
        window->present();
        usleep(16666);
    }
    log::info("[Launcher] Exiting cleanly.");
    return 0;
}
'''

result = header + NEW_MAIN
with open(path, 'w', newline='\n') as f:
    f.write(result)

print("SUCCESS: launcher patched, total lines =", result.count('\n'))
