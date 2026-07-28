#pragma once

#include <txui/core/BuildConfig.hpp>
#include <cstdlib>
#include <iostream>

namespace txui {

[[noreturn]] inline void assertion_failed(const char* expr, const char* file, int line, const char* msg = "") noexcept {
    std::cerr << "[TXUI ASSERTION FAILED] " << file << ":" << line << " -> (" << expr << ") " << msg << std::endl;
    std::abort();
}

} // namespace txui

#if TXUI_ENABLE_ASSERTIONS
#define TXUI_ASSERT(expr, ...) \
    do { \
        if (!(expr)) { \
            ::txui::assertion_failed(#expr, __FILE__, __LINE__, ##__VA_ARGS__); \
        } \
    } while (false)
#else
#define TXUI_ASSERT(expr, ...) do { (void)sizeof(expr); } while (false)
#endif
