#include <txui/render/Canvas.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include "../../src/terminal/TerminalWidget.hpp"
#include "common/logger.hpp"
#include <iostream>

int main() {
    tinexus::log::set_component_name("lru_test");
    
    // Create widget
    auto widget = txui::make_ref<tinexus::terminal::TerminalWidget>();
    
    txui::Constraints constraints(0, 800, 0, 600);
    widget->measure(constraints);
    txui::Size size = widget->desired_size();
    widget->layout(txui::Rect(0, 0, size.width, size.height));
    
    txui::Canvas canvas(size.width, size.height);
    txui::PixmanBackend backend;

    // Send 3000 unique unicode characters
    std::string text;
    for (int i = 0x0100; i < 0x0100 + 3000; ++i) { // Extended Latin, Greek, Cyrillic, etc.
        // UTF-8 encode
        if (i <= 0x7FF) {
            text += static_cast<char>(0xC0 | (i >> 6));
            text += static_cast<char>(0x80 | (i & 0x3F));
        } else if (i <= 0xFFFF) {
            text += static_cast<char>(0xE0 | (i >> 12));
            text += static_cast<char>(0x80 | ((i >> 6) & 0x3F));
            text += static_cast<char>(0x80 | (i & 0x3F));
        }
    }
    
    // Write it directly to the emulator instead of through PTY for speed
    widget->emulator().write_input(text.data(), text.size());
    
    // Render
    txui::CommandBuffer buffer_cmds;
    txui::Painter painter(buffer_cmds);
    painter.begin_frame();
    widget->paint(painter);
    painter.end_frame();

    backend.execute(buffer_cmds, canvas);

    std::cout << "Done LRU test." << std::endl;
    return EXIT_SUCCESS;
}
