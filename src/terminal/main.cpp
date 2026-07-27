#include "terminal/pty_process.hpp"
#include "terminal/terminal_buffer.hpp"
#include "terminal/terminal_renderer.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("terminal");
    tinexus::log::info("Starting Tinexus Terminal (tinexus-terminal)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Terminal: Connected to Tinexus Platform IPC broker via SDK.");
    }

    tinexus::terminal::PtyProcess pty;
    std::string shell = (argc > 1) ? argv[1] : "/bin/bash";
    if (pty.spawn(shell)) {
        tinexus::log::info("Tinexus Terminal: Successfully spawned PTY shell '{}'", shell);
    }

    sdk_client.disconnect();
    return 0;
}
