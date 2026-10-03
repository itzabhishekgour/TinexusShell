#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <txui/render/FontMetrics.hpp>
#include <txui/widgets/Button.hpp>
#include <txui/widgets/TextInput.hpp>
#include <txui/widgets/SegmentedControl.hpp>
#include <txui/widgets/ToggleSwitch.hpp>
#include <txui/widgets/Slider.hpp>
#include <txui/widgets/Icon.hpp>
#include <iostream>
#include <vector>
#include <string>

using namespace txui;

void render_widgets_showcase() {
    std::cout << "[Visual Test] Rendering TxUI Widgets Showcase..." << std::endl;

    const uint32_t W = 840, H = 640;
    Canvas canvas(W, H);
    canvas.clear(Color(16, 18, 24, 255));
    PixmanBackend backend;

    CommandBuffer cmds;
    Painter painter(cmds);
    painter.begin_frame();

    // Background subtle gradient overlay
    painter.fill_gradient_rect(Rect(0, 0, W, H), Color(25, 28, 38, 255), Color(14, 15, 20, 255), false);

    // Header Title
    painter.draw_text(Point(40.0, 36.0), "TxUI Core Widgets Showcase", Color(255, 255, 255, 255), 20.0, true);
    painter.draw_text(Point(40.0, 68.0), "Verified Measure -> Layout -> Paint rendering with FreeType font metrics", Color(140, 145, 160, 220), 13.0);

    // Section 1: Buttons
    painter.draw_text(Point(40.0, 108.0), "1. BUTTON WIDGETS", Color(0, 195, 255, 240), 12.0, true);
    {
        Button btn_std("Cancel");
        btn_std.set_style(Button::Style::Standard);
        btn_std.layout(Rect(40.0, 130.0, 100.0, 36.0));
        btn_std.paint(painter);

        Button btn_pri("Save Changes");
        btn_pri.set_style(Button::Style::Primary);
        btn_pri.layout(Rect(155.0, 130.0, 130.0, 36.0));
        btn_pri.paint(painter);

        Button btn_danger("Delete");
        btn_danger.set_style(Button::Style::Danger);
        btn_danger.layout(Rect(300.0, 130.0, 100.0, 36.0));
        btn_danger.paint(painter);

        Button btn_icon("Browse...", IconType::Folder);
        btn_icon.set_style(Button::Style::Standard);
        btn_icon.layout(Rect(415.0, 130.0, 135.0, 36.0));
        btn_icon.paint(painter);

        Button btn_term("Terminal", IconType::Terminal);
        btn_term.set_style(Button::Style::Standard);
        btn_term.layout(Rect(565.0, 130.0, 135.0, 36.0));
        btn_term.paint(painter);

        Button btn_ghost("Skip");
        btn_ghost.set_style(Button::Style::Ghost);
        btn_ghost.layout(Rect(715.0, 130.0, 80.0, 36.0));
        btn_ghost.paint(painter);
    }

    // Section 2: TextInput
    painter.draw_text(Point(40.0, 190.0), "2. TEXT INPUT (FontMetrics Caret & Selection)", Color(0, 195, 255, 240), 12.0, true);
    {
        // Unfocused input with placeholder
        TextInput input_empty("Search apps or settings...");
        input_empty.layout(Rect(40.0, 212.0, 360.0, 40.0));
        input_empty.paint(painter);

        // Focused input with text and glowing cyan caret
        TextInput input_focused("Username");
        input_focused.set_text("abhishek@tinexus.org");
        input_focused.set_focused(true);
        input_focused.set_caret_position(8); // Caret right after "abhishek"
        input_focused.layout(Rect(430.0, 212.0, 365.0, 40.0));
        input_focused.paint(painter);
    }

    // Section 3: SegmentedControl
    painter.draw_text(Point(40.0, 280.0), "3. SEGMENTED CONTROL (Pill-shaped switcher)", Color(0, 195, 255, 240), 12.0, true);
    {
        SegmentedControl seg_tabs({"Overview", "Displays", "Storage", "Network", "Services"}, 1);
        seg_tabs.layout(Rect(40.0, 302.0, 520.0, 38.0));
        seg_tabs.paint(painter);
    }

    // Section 4: ToggleSwitch & Sliders
    painter.draw_text(Point(40.0, 365.0), "4. TOGGLE SWITCHES", Color(0, 195, 255, 240), 12.0, true);
    {
        // Switch 1: Night Light (Checked)
        ToggleSwitch sw1(true);
        sw1.layout(Rect(40.0, 390.0, 46.0, 26.0));
        sw1.paint(painter);
        painter.draw_text(Point(96.0, 395.0), "Night Light (On)", Color(230, 235, 245, 255), 13.0);

        // Switch 2: Adaptive Sync (Unchecked)
        ToggleSwitch sw2(false);
        sw2.layout(Rect(260.0, 390.0, 46.0, 26.0));
        sw2.paint(painter);
        painter.draw_text(Point(316.0, 395.0), "Adaptive Sync (Off)", Color(150, 155, 170, 220), 13.0);

        // Switch 3: Bluetooth (Checked)
        ToggleSwitch sw3(true);
        sw3.layout(Rect(510.0, 390.0, 46.0, 26.0));
        sw3.paint(painter);
        painter.draw_text(Point(566.0, 395.0), "Bluetooth (Connected)", Color(230, 235, 245, 255), 13.0);
    }

    // Section 5: Sliders
    painter.draw_text(Point(40.0, 445.0), "5. SLIDERS (Horizontal Range Controls)", Color(0, 195, 255, 240), 12.0, true);
    {
        // Brightness: 75%
        painter.draw_text(Point(40.0, 470.0), "Display Brightness: 75%", Color(210, 215, 225, 220), 13.0);
        Slider s1(0.0, 100.0, 75.0);
        s1.layout(Rect(40.0, 492.0, 350.0, 28.0));
        s1.paint(painter);

        // Output Volume: 40%
        painter.draw_text(Point(440.0, 470.0), "Master Volume: 40%", Color(210, 215, 225, 220), 13.0);
        Slider s2(0.0, 100.0, 40.0);
        s2.layout(Rect(440.0, 492.0, 350.0, 28.0));
        s2.paint(painter);

        // Accent Hue: 90%
        painter.draw_text(Point(40.0, 535.0), "UI Scaling: 125%", Color(210, 215, 225, 220), 13.0);
        Slider s3(100.0, 200.0, 125.0);
        s3.layout(Rect(40.0, 557.0, 350.0, 28.0));
        s3.paint(painter);
    }

    painter.end_frame();
    backend.execute(cmds, canvas);

    if (ImageWriter::save_png(canvas, "txui_widgets_showcase.png")) {
        std::cout << "[Visual Test] Successfully generated txui_widgets_showcase.png" << std::endl;
    } else {
        std::cerr << "[Visual Test] FAILED to save txui_widgets_showcase.png" << std::endl;
    }
}

void render_icons_grid() {
    std::cout << "[Visual Test] Rendering TxUI Vector Icons Grid..." << std::endl;

    const uint32_t W = 880, H = 380;
    Canvas canvas(W, H);
    canvas.clear(Color(18, 20, 26, 255));
    PixmanBackend backend;

    CommandBuffer cmds;
    Painter painter(cmds);
    painter.begin_frame();

    painter.fill_gradient_rect(Rect(0, 0, W, H), Color(24, 26, 36, 255), Color(15, 16, 22, 255), false);

    painter.draw_text(Point(40.0, 36.0), "TxUI Vector Icons Pipeline (Phase 3)", Color(255, 255, 255, 255), 20.0, true);
    painter.draw_text(Point(40.0, 66.0), "High-fidelity vector geometry using native Painter primitives", Color(140, 145, 160, 220), 13.0);

    struct IconItem {
        IconType type;
        const char* name;
    };

    const std::vector<IconItem> items = {
        {IconType::Folder,     "Folder"},
        {IconType::File,       "Document"},
        {IconType::Terminal,   "Terminal"},
        {IconType::Settings,   "Settings"},
        {IconType::Downloads,  "Downloads"},
        {IconType::Trash,      "Trash"},
        {IconType::Archive,    "Archive"},
        {IconType::Image,      "Image"},
        {IconType::Apps,       "AppGrid"},
        {IconType::Home,       "Home"},
        {IconType::Executable, "Executable"}
    };

    const double icon_size = 48.0;
    const double col_w = 70.0;
    const double start_x = 40.0;
    const double start_y = 120.0;

    for (size_t i = 0; i < items.size(); ++i) {
        const double ix = start_x + static_cast<double>(i) * col_w;
        const double iy = start_y;

        // Card background tile for icon
        painter.fill_rounded_rect(Rect(ix - 5.0, iy - 8.0, 58.0, 58.0), 10.0, Color(255, 255, 255, 10));
        painter.fill_rounded_rect(Rect(ix - 5.0, iy - 8.0, 58.0, 1.0), 1.0, Color(255, 255, 255, 25));

        // Render Icon
        Icon icon(items[i].type, icon_size);
        icon.layout(Rect(ix, iy - 3.0, icon_size, icon_size));
        icon.paint(painter);

        // Render Centered Label below icon
        const auto extents = FontMetrics::measure(items[i].name, 11.0, false, FontFamily::UI);
        const double lx = ix + (icon_size - extents.width) * 0.5;
        painter.draw_text(Point(lx, iy + 62.0), items[i].name, Color(210, 215, 225, 230), 11.0);
    }

    // Secondary row showing various sizes (16, 24, 32, 48, 64)
    painter.draw_text(Point(40.0, 230.0), "Resolution Scaling Test (16px, 24px, 32px, 48px, 64px)", Color(0, 195, 255, 240), 12.0, true);
    {
        const double sizes[] = {16.0, 24.0, 32.0, 48.0, 64.0};
        double cur_x = 40.0;
        const double row_y = 260.0;

        for (double sz : sizes) {
            Icon ic_term(IconType::Terminal, sz);
            ic_term.layout(Rect(cur_x, row_y + (64.0 - sz) * 0.5, sz, sz));
            ic_term.paint(painter);

            Icon ic_exec(IconType::Executable, sz);
            ic_exec.layout(Rect(cur_x + sz + 6.0, row_y + (64.0 - sz) * 0.5, sz, sz));
            ic_exec.paint(painter);

            Icon ic_set(IconType::Settings, sz);
            ic_set.layout(Rect(cur_x + (sz + 6.0) * 2.0, row_y + (64.0 - sz) * 0.5, sz, sz));
            ic_set.paint(painter);

            cur_x += (sz + 6.0) * 3.0 + 18.0;
        }
    }

    painter.end_frame();
    backend.execute(cmds, canvas);

    if (ImageWriter::save_png(canvas, "txui_icons_grid.png")) {
        std::cout << "[Visual Test] Successfully generated txui_icons_grid.png" << std::endl;
    } else {
        std::cerr << "[Visual Test] FAILED to save txui_icons_grid.png" << std::endl;
    }
}

int main() {
    render_widgets_showcase();
    render_icons_grid();
    return 0;
}
