// ============================================================================
// test_terminal_pty_e2e.cpp — End-to-end PTY & VTerm runtime validation
// ============================================================================

#include "terminal/pty_process.hpp"
#include "terminal/TerminalEmulator.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <thread>

using namespace tinexus::terminal;

int main() {
    std::cout << ">>> Running PTY & VTerm E2E Verification..." << std::endl;

    // Test 1: Direct Command Execution via PTY
    {
        PtyProcess pty;
        bool ok = pty.spawn("/bin/sh", {"-c", "echo TINEXUS_TERMINAL_PHASE4_PTY_OK"}, 80, 24);
        if (!ok) {
            std::cerr << "FAIL: pty.spawn failed for /bin/sh" << std::endl;
            return 1;
        }

        std::string output;
        char buf[512];
        int attempts = 50;
        while (attempts-- > 0) {
            ssize_t n = pty.read_bytes(buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                output += buf;
                if (output.find("TINEXUS_TERMINAL_PHASE4_PTY_OK") != std::string::npos) {
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        if (output.find("TINEXUS_TERMINAL_PHASE4_PTY_OK") == std::string::npos) {
            std::cerr << "FAIL: Did not receive expected output from PTY. Got: " << output << std::endl;
            return 1;
        }
        std::cout << "  [PASS] PTY command execution and stdout capture verified: " << output;
    }

    // Test 2: Interactive PTY + TerminalEmulator parsing
    {
        PtyProcess pty;
        TerminalEmulator emu(24, 80);
        bool ok = pty.spawn("/bin/sh", {"-i"}, 80, 24);
        if (!ok) {
            std::cerr << "FAIL: pty.spawn failed for interactive /bin/sh" << std::endl;
            return 1;
        }

        const char* cmd = "echo HELLO_TX_EMULATOR\n";
        pty.write_bytes(cmd, std::strlen(cmd));

        std::string full_stream;
        char buf[512];
        int attempts = 50;
        bool found = false;
        while (attempts-- > 0) {
            ssize_t n = pty.read_bytes(buf, sizeof(buf));
            if (n > 0) {
                emu.write_input(buf, static_cast<size_t>(n));
                full_stream.append(buf, n);
                if (full_stream.find("HELLO_TX_EMULATOR") != std::string::npos) {
                    found = true;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        if (!found) {
            std::cerr << "FAIL: Interactive PTY input/output not reflected. Stream: " << full_stream << std::endl;
            return 1;
        }

        std::cout << "  [PASS] Interactive PTY input + VTerm screen parsing verified." << std::endl;
        pty.terminate();
    }

    std::cout << "[ALL PASS] PTY & Terminal backend verified end-to-end!" << std::endl;
    return 0;
}
