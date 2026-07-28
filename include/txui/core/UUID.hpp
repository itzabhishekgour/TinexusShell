#pragma once

#include <txui/core/Types.hpp>
#include <atomic>

namespace txui {

class UUID {
private:
    uint64 m_id{0};

    explicit constexpr UUID(uint64 id) noexcept : m_id(id) {}

public:
    constexpr UUID() noexcept = default;

    [[nodiscard]] static UUID generate() noexcept {
        static std::atomic<uint64> s_counter{1};
        return UUID(s_counter.fetch_add(1, std::memory_order_relaxed));
    }

    [[nodiscard]] constexpr uint64 value() const noexcept { return m_id; }
    [[nodiscard]] constexpr bool is_valid() const noexcept { return m_id != 0; }

    [[nodiscard]] constexpr bool operator==(const UUID& other) const noexcept {
        return m_id == other.m_id;
    }
    [[nodiscard]] constexpr bool operator!=(const UUID& other) const noexcept {
        return m_id != other.m_id;
    }
};

} // namespace txui
