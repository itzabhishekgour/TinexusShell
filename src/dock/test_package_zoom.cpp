#include <txui/widgets/Icon.hpp>
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <txui/render/FontMetrics.hpp>
#include <iostream>
#include <vector>

using namespace txui;

int main() {
    std::cout << "[Visual Test] Rendering Package Icon Standalone Zoom & Hierarchy Test..." << std::endl;

    const uint32_t W = 820, H = 440;
    Canvas canvas(W, H);
    canvas.clear(Color(16, 18, 24, 255));
    PixmanBackend backend;

    CommandBuffer cmds;
    Painter painter(cmds);
    painter.begin_frame();

    // Background gradient panel
    painter.fill_gradient_rect(Rect(0, 0, W, H), Color(24, 26, 36, 255), Color(14, 15, 22, 255));

    // Header Title
    painter.draw_text(Point(40.0, 36.0), "TxUI Package Icon Standalone Zoom & Hierarchy Test", Color(255, 255, 255, 255), 18.0, true);
    painter.draw_text(Point(40.0, 64.0), "High-contrast kraft parcel geometry across multiple resolutions", Color(140, 145, 160, 220), 12.0);

    // Section 1: Package icon at standalone scales (96px, 64px, 48px, 32px, 24px)
    painter.draw_text(Point(40.0, 110.0), "1. PACKAGE ICON SCALING (96px, 64px, 48px, 32px, 24px)", Color(0, 195, 255, 240), 12.0, true);

    struct ScaleTest {
        double size;
        const char* label;
    };
    const ScaleTest scales[] = {
        {96.0, "96px"},
        {64.0, "64px"},
        {48.0, "48px (Dock)"},
        {32.0, "32px"},
        {24.0, "24px (Launcher)"}
    };

    double cur_x = 40.0;
    const double baseline_y = 230.0;

    for (const auto& st : scales) {
        double sz = st.size;
        double iy = baseline_y - sz;

        // Card tile backdrop
        painter.fill_rounded_rect(Rect(cur_x - 8.0, iy - 8.0, sz + 16.0, sz + 38.0), 8.0, Color(255, 255, 255, 8));
        painter.fill_rounded_rect(Rect(cur_x - 8.0, iy - 8.0, sz + 16.0, 1.0), 1.0, Color(255, 255, 255, 25));

        // Render Package Icon via canonical Icon::render
        Icon::render(painter, IconType::Package, Rect(cur_x, iy, sz, sz));

        // Label below
        double tw = FontMetrics::measure(st.label, 11.0).width;
        painter.draw_text(Point(cur_x + (sz - tw) * 0.5, baseline_y + 12.0), st.label, Color(200, 205, 220, 220), 11.0);

        cur_x += sz + 32.0;
    }

    // Section 2: Full Dock Icon Hierarchy Comparison at 48px (Dock Size)
    painter.draw_text(Point(40.0, 290.0), "2. DOCK CO-PRESENCE HIERARCHY TEST (48px - Terminal, Files, Settings, Monitor, Package)", Color(0, 195, 255, 240), 12.0, true);

    struct DockComparison {
        IconType type;
        const char* name;
    };
    const DockComparison dock_icons[] = {
        {IconType::Terminal, "Terminal"},
        {IconType::Folder,   "Files"},
        {IconType::Gear,     "Settings"},
        {IconType::BarChart, "Monitor"},
        {IconType::Package,  "Package"}
    };

    double comp_x = 40.0;
    const double comp_y = 330.0;
    const double c_sz   = 48.0;

    for (const auto& di : dock_icons) {
        // Frosted squircle container
        painter.fill_rounded_rect(Rect(comp_x - 8.0, comp_y - 8.0, c_sz + 16.0, c_sz + 36.0), 10.0, Color(28, 30, 46, 215));
        painter.fill_rounded_rect(Rect(comp_x - 9.0, comp_y - 9.0, c_sz + 18.0, c_sz + 38.0), 11.0, Color(255, 255, 255, 20));

        Icon::render(painter, di.type, Rect(comp_x, comp_y, c_sz, c_sz));

        double tw = FontMetrics::measure(di.name, 11.0).width;
        painter.draw_text(Point(comp_x + (c_sz - tw) * 0.5, comp_y + c_sz + 10.0), di.name, Color(220, 225, 240, 230), 11.0);

        comp_x += c_sz + 36.0;
    }

    painter.end_frame();
    backend.execute(cmds, canvas);

    if (ImageWriter::save_png(canvas, "package_icon_zoom.png")) {
        std::cout << "  [SUCCESS] Saved package_icon_zoom.png" << std::endl;
    } else {
        std::cerr << "  [FAILURE] Could not save package_icon_zoom.png" << std::endl;
    }

    return 0;
}
