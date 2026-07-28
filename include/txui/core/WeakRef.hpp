#pragma once

#include <txui/core/Ref.hpp>

namespace txui {

// Stub for WeakRef implementation in future libtxui versions
template <typename T>
class WeakRef {
private:
    T* m_ptr{nullptr};

public:
    constexpr WeakRef() noexcept = default;
    explicit WeakRef(T* ptr) noexcept : m_ptr(ptr) {}
    WeakRef(const Ref<T>& ref) noexcept : m_ptr(ref.get()) {}

    [[nodiscard]] T* get_raw() const noexcept { return m_ptr; }
    [[nodiscard]] bool expired() const noexcept { return m_ptr == nullptr; }
};

} // namespace txui
