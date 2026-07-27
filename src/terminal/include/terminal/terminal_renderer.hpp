#ifndef TINEXUS_TERMINAL_TERMINAL_RENDERER_HPP
#define TINEXUS_TERMINAL_TERMINAL_RENDERER_HPP

#include "terminal/terminal_buffer.hpp"
#include <string>

namespace tinexus::terminal {

class TerminalRenderer {
public:
    TerminalRenderer() = default;
    ~TerminalRenderer() = default;

    std::string render_plain_text(const TerminalBuffer& buffer) const;
};

} // namespace tinexus::terminal

#endif // TINEXUS_TERMINAL_TERMINAL_RENDERER_HPP
