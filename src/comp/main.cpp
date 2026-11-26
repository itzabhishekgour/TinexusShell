#include "common/logger.hpp"
#include "common/RuntimePaths.hpp"
#include "common/version.hpp"
#include "comp/server/server.hpp"
#include <csignal>
#include <cstdio>
#include <cstring>
#include <execinfo.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {
tinexus::comp::TinexusServer* g_server{nullptr};

void write_all_fd(int fd, const char* data, size_t len) noexcept {
    while (len > 0) {
        ssize_t n = ::write(fd, data, len);
        if (n <= 0) break;
        data += n;
        len -= static_cast<size_t>(n);
    }
}

const char* crash_signal_name(int sig) noexcept {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        default:      return "UNKNOWN";
    }
}

void signal_handler(int signal) {
    if (g_server) {
        tinexus::log::info("Received signal {}, initiating compositor shutdown...", signal);
        g_server->stop();
    }
}

void crash_signal_handler(int sig, siginfo_t* info, void* /*ucontext*/) {
    char header[256];
    void* addr = info ? info->si_addr : nullptr;
    int len = std::snprintf(
        header, sizeof(header),
        "\n=== tinexus-comp CRASH (%s, sig=%d, addr=%p) ===\n",
        crash_signal_name(sig), sig, addr);

    void* frames[64];
    int count = ::backtrace(frames, 64);

    if (len > 0) {
        write_all_fd(STDERR_FILENO, header, static_cast<size_t>(len));
    }
    ::backtrace_symbols_fd(frames, count, STDERR_FILENO);

    int log_fd = ::open("/run/tinexus/comp_crash.log",
                        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (log_fd >= 0) {
        if (len > 0) {
            write_all_fd(log_fd, header, static_cast<size_t>(len));
        }
        ::backtrace_symbols_fd(frames, count, log_fd);
        ::close(log_fd);
    }

    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

void install_crash_handlers() noexcept {
    struct sigaction sa{};
    sa.sa_sigaction = crash_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    ::sigaction(SIGSEGV, &sa, nullptr);
    ::sigaction(SIGABRT, &sa, nullptr);
    ::sigaction(SIGBUS,  &sa, nullptr);
    ::sigaction(SIGFPE,  &sa, nullptr);
    ::sigaction(SIGILL,  &sa, nullptr);
}
} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    tinexus::log::set_component_name("tinexus-comp");

    int boot_tier = 1;
    for (const std::string& p : {
             std::string("/run/tinexus/fallback_tier"),
             tinexus::common::RuntimePaths::get_runtime_dir() + "/fallback_tier"}) {
        std::ifstream tf(p);
        if (tf.is_open()) {
            int val = 1;
            if (tf >> val && val >= 1 && val <= 4) {
                boot_tier = val;
            }
            break;
        }
    }
    tinexus::log::info("Starting tinexus-comp v{} - Wayland Compositor (tier={})",
                       tinexus::VERSION_STRING, boot_tier);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);
    install_crash_handlers();

    tinexus::comp::TinexusServer server;
    g_server = &server;

    if (!server.initialize()) {
        std::error_code ec;
        std::filesystem::remove("/run/tinexus/fallback_tier", ec);
        std::filesystem::remove(tinexus::common::RuntimePaths::get_runtime_dir() + "/fallback_tier", ec);
        tinexus::log::error("Failed to initialize Tinexus Compositor Server");
        return 1;
    }

    if (!server.run()) {
        tinexus::log::error("Tinexus Compositor run failed");
        return 1;
    }
    return 0;
}
