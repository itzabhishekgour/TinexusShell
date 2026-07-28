#pragma once

#include <string_view>
#include <cstdint>

namespace txui {

constexpr std::uint32_t TXUI_VERSION_MAJOR = 0;
constexpr std::uint32_t TXUI_VERSION_MINOR = 1;
constexpr std::uint32_t TXUI_VERSION_PATCH = 0;

constexpr std::string_view TXUI_VERSION_STRING = "0.1.0";

[[nodiscard]] constexpr std::uint32_t version_code() noexcept {
    return (TXUI_VERSION_MAJOR << 16) | (TXUI_VERSION_MINOR << 8) | TXUI_VERSION_PATCH;
}

} // namespace txui
