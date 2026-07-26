#ifndef TINEXUS_COMMON_LOCKFREE_QUEUE_HPP
#define TINEXUS_COMMON_LOCKFREE_QUEUE_HPP

#include <atomic>
#include <array>
#include <cstddef>
#include <optional>
#include <utility>

namespace tinexus {

template <typename T, size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    SpscQueue() : m_head(0), m_tail(0) {}

    template <typename... Args>
    bool emplace(Args&&... args) {
        const auto current_tail = m_tail.load(std::memory_order_relaxed);
        const auto current_head = m_head.load(std::memory_order_acquire);

        if ((current_tail - current_head) >= Capacity) {
            return false; // Queue full
        }

        m_buffer[current_tail & Mask] = T(std::forward<Args>(args)...);
        m_tail.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    bool push(const T& item) {
        return emplace(item);
    }

    bool push(T&& item) {
        return emplace(std::move(item));
    }

    std::optional<T> pop() {
        const auto current_head = m_head.load(std::memory_order_relaxed);
        const auto current_tail = m_tail.load(std::memory_order_acquire);

        if (current_head == current_tail) {
            return std::nullopt; // Queue empty
        }

        T item = std::move(m_buffer[current_head & Mask]);
        m_head.store(current_head + 1, std::memory_order_release);
        return item;
    }

    [[nodiscard]] bool empty() const noexcept {
        return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
    }

    [[nodiscard]] size_t size() const noexcept {
        const auto head = m_head.load(std::memory_order_relaxed);
        const auto tail = m_tail.load(std::memory_order_relaxed);
        return (tail >= head) ? (tail - head) : 0;
    }

private:
    static constexpr size_t Mask = Capacity - 1;
    std::array<T, Capacity> m_buffer;

    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;
};

} // namespace tinexus

#endif // TINEXUS_COMMON_LOCKFREE_QUEUE_HPP
