#ifndef TINEXUS_TERMINAL_ANSI_PARSER_HPP
#define TINEXUS_TERMINAL_ANSI_PARSER_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace tinexus::terminal {

struct AnsiStyle {
    uint32_t fg_color{0xFFFFFF};
    uint32_t bg_color{0x000000};
    bool bold{false};
    bool dim{false};
    bool underline{false};
};

struct FormattedChar {
    char ch{' '};
    AnsiStyle style;
};

class AnsiParser {
public:
    AnsiParser() = default;
    ~AnsiParser() = default;

    std::vector<FormattedChar> parse(const std::string& input);

private:
    AnsiStyle m_current_style;
};

} // namespace tinexus::terminal

#endif // TINEXUS_TERMINAL_ANSI_PARSER_HPP
