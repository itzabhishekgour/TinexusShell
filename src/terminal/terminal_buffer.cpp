#include "terminal/terminal_buffer.hpp"
#include <sstream>

namespace tinexus::terminal {

TerminalBuffer::TerminalBuffer(size_t max_scrollback)
    : m_max_scrollback(max_scrollback) {}

void TerminalBuffer::append_string(const std::string& text) {
    auto formatted = m_parser.parse(text);

    TerminalLine current_line;
    if (!m_lines.empty()) {
        current_line = m_lines.back();
        m_lines.pop_back();
    }

    for (const auto& fc : formatted) {
        if (fc.ch == '\n') {
            m_lines.push_back(current_line);
            current_line = TerminalLine{};
        } else if (fc.ch == '\r') {
            current_line.chars.clear();
        } else {
            current_line.chars.push_back(fc);
        }
    }
    m_lines.push_back(current_line);

    while (m_lines.size() > m_max_scrollback) {
        m_lines.pop_front();
    }
}

void TerminalBuffer::clear() {
    m_lines.clear();
}

size_t TerminalBuffer::line_count() const noexcept {
    return m_lines.size();
}

} // namespace tinexus::terminal
