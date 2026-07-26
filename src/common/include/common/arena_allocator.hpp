#ifndef TINEXUS_COMMON_ARENA_ALLOCATOR_HPP
#define TINEXUS_COMMON_ARENA_ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <stdexcept>
#include <array>

namespace tinexus {

template <size_t BufferSize = 256 * 1024>
class SearchArena {
public:
    SearchArena() : m_offset(0) {}

    SearchArena(const SearchArena&) = delete;
    SearchArena& operator=(const SearchArena&) = delete;

    template <typename T, typename... Args>
    T* allocate(Args&&... args) {
        constexpr size_t alignment = alignof(T);
        size_t current_addr = reinterpret_cast<size_t>(&m_buffer[m_offset]);
        size_t aligned_addr = (current_addr + (alignment - 1)) & ~(alignment - 1);
        size_t padding = aligned_addr - current_addr;

        if (m_offset + padding + sizeof(T) > BufferSize) {
            throw std::bad_alloc();
        }

        m_offset += padding;
        T* ptr = reinterpret_cast<T*>(&m_buffer[m_offset]);
        m_offset += sizeof(T);

        return ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
    }

    void reset() noexcept {
        m_offset = 0;
    }

    [[nodiscard]] size_t used_bytes() const noexcept {
        return m_offset;
    }

    [[nodiscard]] constexpr size_t total_capacity() const noexcept {
        return BufferSize;
    }

private:
    alignas(64) std::array<uint8_t, BufferSize> m_buffer;
    size_t m_offset;
};

} // namespace tinexus

#endif // TINEXUS_COMMON_ARENA_ALLOCATOR_HPP
