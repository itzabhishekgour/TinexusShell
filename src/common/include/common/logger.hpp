#ifndef TINEXUS_COMMON_LOGGER_HPP
#define TINEXUS_COMMON_LOGGER_HPP

#include <string_view>
#include <format>
#include <string>
#include <atomic>

namespace tinexus::log {

enum class Level {
    Debug,
    Info,
    Warn,
    Error,
    Critical
};

inline std::atomic<Level> g_current_level{Level::Info};

inline bool is_enabled(Level level) noexcept {
    return level >= g_current_level.load(std::memory_order_relaxed);
}

void set_component_name(std::string_view name);
void set_level(Level level);

void write_log(Level level, std::string_view message);

template <typename... Args>
inline void info(std::format_string<Args...> fmt, Args&&... args) {
    if (!is_enabled(Level::Info)) return;
    std::string msg = std::format(fmt, std::forward<Args>(args)...);
    write_log(Level::Info, msg);
}

template <typename... Args>
inline void warn(std::format_string<Args...> fmt, Args&&... args) {
    if (!is_enabled(Level::Warn)) return;
    std::string msg = std::format(fmt, std::forward<Args>(args)...);
    write_log(Level::Warn, msg);
}

template <typename... Args>
inline void error(std::format_string<Args...> fmt, Args&&... args) {
    if (!is_enabled(Level::Error)) return;
    std::string msg = std::format(fmt, std::forward<Args>(args)...);
    write_log(Level::Error, msg);
}

template <typename... Args>
inline void debug(std::format_string<Args...> fmt, Args&&... args) {
    if (!is_enabled(Level::Debug)) return;
    std::string msg = std::format(fmt, std::forward<Args>(args)...);
    write_log(Level::Debug, msg);
}

} // namespace tinexus::log

#endif // TINEXUS_COMMON_LOGGER_HPP

