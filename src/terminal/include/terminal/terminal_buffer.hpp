#ifndef TINEXUS_TERMINAL_TERMINAL_BUFFER_HPP
#define TINEXUS_TERMINAL_TERMINAL_BUFFER_HPP

#include "terminal/ansi_parser.hpp"
#include <deque>
#include <vector>
#include <string>

namespace tinexus::terminal {

struct TerminalLine {
    std::vector<FormattedChar> chars;
};

class TerminalBuffer {
public:
    TerminalBuffer(size_t max_scrollback = 10000);
    ~TerminalBuffer() = default;

    void append_string(const std::string& text);
    void clear();

    [[nodiscard]] size_t line_count() const noexcept;
    [[nodiscard]] const std::deque<TerminalLine>& lines() const noexcept { return m_lines; }

private:
    std::deque<TerminalLine> m_lines;
    size_t m_max_scrollback;
    AnsiParser m_parser;
};

} // namespace tinexus::terminal

#endif // TINEXUS_TERMINAL_TERMINAL_BUFFER_HPP
