// Stress test for the LRU glyph cache in PixmanBackend.
// Bypasses libvterm entirely — drives PixmanBackend::execute() directly with
// DrawTextCommands containing 3000 unique Unicode codepoints.
// Expected result: Glyph Cache size caps at 2000 and eviction fires.

#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/Command.hpp>
#include <txui/core/Logger.hpp>
#include <iostream>
#include <string>

// Encode a Unicode codepoint to a UTF-8 string
static std::string utf8_encode(uint32_t cp) {
    std::string out;
    if (cp <= 0x7F) {
        out += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

int main() {
    txui::Canvas canvas(1280, 720);
    canvas.clear(txui::Color(20, 20, 20, 255));
    
    txui::PixmanBackend backend;
    
    const int TOTAL_GLYPHS = 3000;
    const int COLS = 80;
    const double CELL_W = 12.0;
    const double CELL_H = 18.0;

    txui::CommandBuffer cmdbuf;
    txui::Painter painter(cmdbuf);
    painter.begin_frame();
    
    for (int i = 0; i < TOTAL_GLYPHS; ++i) {
        // Unicode Latin Extended, Greek, Cyrillic, Hebrew, Arabic etc.
        uint32_t cp = static_cast<uint32_t>(0x0100 + i);
        // Skip surrogates (D800–DFFF)
        if (cp >= 0xD800 && cp <= 0xDFFF) { cp += 0x0800u; }
        
        std::string glyph_str = utf8_encode(cp);
        
        double x = (i % COLS) * CELL_W;
        double y = (i / COLS) * CELL_H;
        
        // Painter::draw_text signature: (pos, text, color, scale, bold, italic)
        painter.draw_text(
            txui::Point(x, y),
            glyph_str,
            txui::Color(200, 200, 200, 255),
            1.0,   // scale
            false, // bold
            false  // italic
        );
    }
    
    painter.end_frame();
    backend.execute(cmdbuf, canvas);

    std::cout << "LRU stress test complete.\n"
              << "Fed 3000 unique Unicode glyphs to PixmanBackend.\n"
              << "Check [TXUI:INFO] Glyph Cache log lines above for size capping.\n"
              << "Expected: final size == 2000 (evictions must have fired).\n";
    return 0;
}
