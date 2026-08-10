#include <txui/window/Window.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/pixman/PixmanBackend.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include "../src/files/ui/ColumnBrowserWidget.hpp"
#include <iostream>
#include <fstream>
#include <vector>

void write_ppm(const std::string& filename, const txui::RenderTarget& target) {
    std::ofstream out(filename, std::ios::binary);
    out << "P6\n" << target.width() << " " << target.height() << "\n255\n";
    const uint32_t* data = target.data();
    for (size_t i = 0; i < target.width() * target.height(); ++i) {
        uint32_t pixel = data[i];
        // ARGB to RGB
        uint8_t r = (pixel >> 16) & 0xFF;
        uint8_t g = (pixel >> 8) & 0xFF;
        uint8_t b = pixel & 0xFF;
        out.put(r);
        out.put(g);
        out.put(b);
    }
}

int main() {
    auto files = txui::make_ref<tinexus::files::ui::ColumnBrowserWidget>();
    auto chrome = txui::make_ref<txui::ChromeWidget>("Tinexus Files", files, []{}, []{}, []{}, [](uint32_t){});
    
    // Simulate navigation
    files->navigate_to("/");

    // Layout
    txui::Constraints c(1000, 1000, 600, 600);
    chrome->measure(c);
    chrome->layout(txui::Rect(0, 0, 1000, 600));

    // Paint
    txui::CommandBuffer buffer;
    txui::Painter painter(buffer);
    painter.begin_frame();
    chrome->paint(painter);
    painter.end_frame();

    // Render
    std::vector<uint32_t> pixels(1000 * 600, 0xFF000000);
    txui::RenderTarget target{pixels.data(), 1000, 600};
    txui::PixmanBackend backend;
    backend.execute(buffer, target);

    write_ppm("/tmp/tinexus_files_ui.ppm", target);
    std::cout << "Saved screenshot to /tmp/tinexus_files_ui.ppm\n";
    return 0;
}
