#include "terminal/ansi_parser.hpp"

namespace tinexus::terminal {

std::vector<FormattedChar> AnsiParser::parse(const std::string& input) {
    std::vector<FormattedChar> result;
    bool in_escape = false;
    std::string escape_buf;

    for (char c : input) {
        if (c == '\033') { // ESC character
            in_escape = true;
            escape_buf.clear();
            continue;
        }

        if (in_escape) {
            escape_buf += c;
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == 'm') {
                in_escape = false;
                // Parse escape sequences (e.g. SGR color codes like \033[31m)
                if (escape_buf == "[0m") {
                    m_current_style = AnsiStyle{};
                } else if (escape_buf == "[1m") {
                    m_current_style.bold = true;
                }
            }
            continue;
        }

        FormattedChar fc;
        fc.ch = c;
        fc.style = m_current_style;
        result.push_back(fc);
    }

    return result;
}

} // namespace tinexus::terminal
