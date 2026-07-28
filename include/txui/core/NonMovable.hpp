#pragma once

namespace txui {

class NonMovable {
protected:
    constexpr NonMovable() noexcept = default;
    ~NonMovable() = default;

public:
    NonMovable(NonMovable&&) = delete;
    NonMovable& operator=(NonMovable&&) = delete;
    NonMovable(const NonMovable&) = delete;
    NonMovable& operator=(const NonMovable&) = delete;
};

} // namespace txui
