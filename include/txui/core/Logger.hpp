#pragma once

#include <iostream>
#include <string_view>

namespace txui {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

inline void log_message(LogLevel level, std::string_view msg) noexcept {
    const char* prefix = "[TXUI:INFO]";
    switch (level) {
        case LogLevel::Debug: prefix = "[TXUI:DEBUG]"; break;
        case LogLevel::Info:  prefix = "[TXUI:INFO]"; break;
        case LogLevel::Warn:  prefix = "[TXUI:WARN]"; break;
        case LogLevel::Error: prefix = "[TXUI:ERROR]"; break;
    }
    std::cerr << prefix << " " << msg << std::endl;
}

} // namespace txui
