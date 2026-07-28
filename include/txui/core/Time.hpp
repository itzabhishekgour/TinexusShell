#pragma once

#include <txui/core/Types.hpp>
#include <chrono>

namespace txui {

class Time {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    [[nodiscard]] static TimePoint now() noexcept {
        return Clock::now();
    }

    [[nodiscard]] static float64 to_seconds(Duration dur) noexcept {
        return std::chrono::duration<float64>(dur).count();
    }

    [[nodiscard]] static float64 to_milliseconds(Duration dur) noexcept {
        return std::chrono::duration<float64, std::milli>(dur).count();
    }
};

} // namespace txui
