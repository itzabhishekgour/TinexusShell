#pragma once

namespace txui {

#if defined(NDEBUG)
constexpr bool TXUI_DEBUG_BUILD = false;
constexpr bool TXUI_RELEASE_BUILD = true;
#else
constexpr bool TXUI_DEBUG_BUILD = true;
constexpr bool TXUI_RELEASE_BUILD = false;
#endif

constexpr bool TXUI_ENABLE_ASSERTIONS = TXUI_DEBUG_BUILD;

} // namespace txui
