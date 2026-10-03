#include "ui/ColumnBrowserWidget.hpp"
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <iostream>
#include <filesystem>
#include <fstream>

using namespace tinexus;
using namespace tinexus::files::ui;

namespace fs = std::filesystem;

static void create_test_fixtures(const fs::path& dir) {
    fs::create_directories(dir);
    fs::create_directories(dir / "Documents");
    fs::create_directories(dir / "Projects");
    fs::create_directories(dir / "Wallpapers");

    {
        std::ofstream f(dir / "Documents" / "Notes.txt");
        f << "# Tinexus Desktop Platform\n\n- Wayland native compositor\n- Vulkan renderer\n- TxUI modern C++ framework\n";
    }
    {
        std::ofstream f(dir / "Documents" / "Budget 2026.csv");
        f << "Month,Expense,Revenue\nJan,1200,4500\nFeb,1400,5200\nMar,1100,6100\n";
    }
    {
        std::ofstream f(dir / "Projects" / "main.cpp");
        f << "#include <iostream>\nint main() { std::cout << \"Hello Tinexus!\\n\"; return 0; }\n";
    }
    {
        std::ofstream f(dir / "README.md");
        f << "# Tinexus Files\n\nHigh performance Wayland desktop file manager.\n";
    }
}

int main() {
    std::cout << "[Visual Test] Starting Tinexus Files rendering..." << std::endl;

    fs::path test_dir = "/tmp/tinexus_files_test";
    create_test_fixtures(test_dir);

    const uint32_t W = 1000;
    const uint32_t H = 620;
    txui::PixmanBackend backend;

    auto widget = txui::make_ref<ColumnBrowserWidget>();
    widget->navigate_to(test_dir);

    txui::Constraints constraints(0, W, 0, H);

    // ── 1. Render Icon Grid View (1000 x 620) ──────────────────────────────
    {
        widget->set_view_mode(ViewMode::IconGrid);
        widget->measure(constraints);
        widget->layout(txui::Rect(0, 0, W, H));

        txui::Canvas canvas(W, H);
        canvas.clear(txui::Color(0, 0, 0, 0));

        txui::CommandBuffer buffer;
        txui::Painter painter(buffer);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer, canvas);

        if (!txui::ImageWriter::save_png(canvas, "files_grid_view.png")) {
            std::cerr << "FAIL: Could not save files_grid_view.png" << std::endl;
            return 1;
        }
        std::cout << "[Visual Test] Saved files_grid_view.png" << std::endl;
    }

    // ── 2. Render List View (1000 x 620) ───────────────────────────────────
    {
        widget->set_view_mode(ViewMode::List);
        widget->measure(constraints);
        widget->layout(txui::Rect(0, 0, W, H));

        txui::Canvas canvas(W, H);
        canvas.clear(txui::Color(0, 0, 0, 0));

        txui::CommandBuffer buffer;
        txui::Painter painter(buffer);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer, canvas);

        if (!txui::ImageWriter::save_png(canvas, "files_list_view.png")) {
            std::cerr << "FAIL: Could not save files_list_view.png" << std::endl;
            return 1;
        }
        std::cout << "[Visual Test] Saved files_list_view.png" << std::endl;
    }

    // ── 3. Render Column View with Inspector (1000 x 620) ───────────────────
    {
        widget->set_view_mode(ViewMode::Column);
        widget->measure(constraints);
        widget->layout(txui::Rect(0, 0, W, H));

        txui::Canvas canvas(W, H);
        canvas.clear(txui::Color(0, 0, 0, 0));

        txui::CommandBuffer buffer;
        txui::Painter painter(buffer);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer, canvas);

        if (!txui::ImageWriter::save_png(canvas, "files_column_view.png")) {
            std::cerr << "FAIL: Could not save files_column_view.png" << std::endl;
            return 1;
        }
        std::cout << "[Visual Test] Saved files_column_view.png" << std::endl;
    }

    // ── 4. Render Gallery View (1000 x 620) ────────────────────────────────
    {
        widget->set_view_mode(ViewMode::Gallery);
        widget->measure(constraints);
        widget->layout(txui::Rect(0, 0, W, H));

        txui::Canvas canvas(W, H);
        canvas.clear(txui::Color(0, 0, 0, 0));

        txui::CommandBuffer buffer;
        txui::Painter painter(buffer);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer, canvas);

        if (!txui::ImageWriter::save_png(canvas, "files_gallery_view.png")) {
            std::cerr << "FAIL: Could not save files_gallery_view.png" << std::endl;
            return 1;
        }
        std::cout << "[Visual Test] Saved files_gallery_view.png" << std::endl;
    }

    // ── 5. Render Quick Look Modal (1000 x 620) ────────────────────────────
    {
        widget->set_view_mode(ViewMode::IconGrid);
        widget->quick_look().open_file(test_dir / "README.md");
        widget->measure(constraints);
        widget->layout(txui::Rect(0, 0, W, H));

        txui::Canvas canvas(W, H);
        canvas.clear(txui::Color(0, 0, 0, 0));

        txui::CommandBuffer buffer;
        txui::Painter painter(buffer);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer, canvas);

        if (!txui::ImageWriter::save_png(canvas, "files_quick_look.png")) {
            std::cerr << "FAIL: Could not save files_quick_look.png" << std::endl;
            return 1;
        }
        std::cout << "[Visual Test] Saved files_quick_look.png" << std::endl;
        widget->quick_look().close_modal();
    }

    // ── 6. Render Context Menu (1000 x 620) ────────────────────────────────
    {
        widget->context_menu().show_at(420, 240, test_dir / "README.md", false);
        widget->measure(constraints);
        widget->layout(txui::Rect(0, 0, W, H));

        txui::Canvas canvas(W, H);
        canvas.clear(txui::Color(0, 0, 0, 0));

        txui::CommandBuffer buffer;
        txui::Painter painter(buffer);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer, canvas);

        if (!txui::ImageWriter::save_png(canvas, "files_context_menu.png")) {
            std::cerr << "FAIL: Could not save files_context_menu.png" << std::endl;
            return 1;
        }
        std::cout << "[Visual Test] Saved files_context_menu.png" << std::endl;
        widget->context_menu().dismiss();
    }

    std::cout << "[Visual Test] All 6 states successfully rendered and exported!" << std::endl;
    return 0;
}
