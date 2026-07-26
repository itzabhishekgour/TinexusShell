#include "common/logger.hpp"

#include <iostream>
#include <mutex>
#include <chrono>
#include <format>

namespace tinexus::log {

namespace {
    std::string g_component_name = "tinexus-unknown";
    Level g_current_level = Level::Info;
    std::mutex g_log_mutex;

    const char* level_to_string(Level lvl) noexcept {
        switch (lvl) {
            case Level::Debug:    return "DEBUG";
            case Level::Info:     return "INFO";
            case Level::Warn:     return "WARN";
            case Level::Error:    return "ERROR";
            case Level::Critical: return "CRITICAL";
        }
        return "UNKNOWN";
    }
}

void set_component_name(std::string_view name) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_component_name = std::string(name);
}

void set_level(Level level) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_current_level = level;
}

void write_log(Level level, std::string_view message) {
    if (level < g_current_level) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_log_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    // Formatted structured JSON output
    std::cout << std::format(
        "{{\"component\":\"{}\",\"level\":\"{}\",\"timestamp\":{},\"message\":\"{}\"}}\n",
        g_component_name,
        level_to_string(level),
        now_time_t,
        message
    );
    std::cout.flush();
}

} // namespace tinexus::log
