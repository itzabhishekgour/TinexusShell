#include "terminal/terminal_renderer.hpp"
#include <sstream>

namespace tinexus::terminal {

std::string TerminalRenderer::render_plain_text(const TerminalBuffer& buffer) const {
    std::ostringstream ss;
    for (const auto& line : buffer.lines()) {
        for (const auto& fc : line.chars) {
            ss << fc.ch;
        }
        ss << "\n";
    }
    return ss.str();
}

} // namespace tinexus::terminal
