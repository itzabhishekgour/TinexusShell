#ifndef TINEXUS_COMMON_LOGGER_HPP
#define TINEXUS_COMMON_LOGGER_HPP

#include <string_view>
#include <format>
#include <string>

namespace tinexus::log {

enum class Level {
    Debug,
    Info,
    Warn,
    Error,
    Critical
};

void set_component_name(std::string_view name);
void set_level(Level level);

void write_log(Level level, std::string_view message);

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    std::string msg = std::format(fmt, std::forward<Args>(args)...);
    write_log(Level::Info, msg);
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    std::string msg = std::format(fmt, std::forward<Args>(args)...);
    write_log(Level::Warn, msg);
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    std::string msg = std::format(fmt, std::forward<Args>(args)...);
    write_log(Level::Error, msg);
}

template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    std::string msg = std::format(fmt, std::forward<Args>(args)...);
    write_log(Level::Debug, msg);
}

} // namespace tinexus::log

#endif // TINEXUS_COMMON_LOGGER_HPP
