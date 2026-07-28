#pragma once

#include <txui/core/Object.hpp>
#include <utility>
#include <type_traits>

namespace txui {

template <typename T>
class Ref {
    static_assert(std::is_base_of_v<Object, T>, "txui::Ref<T> requires T to inherit from txui::Object");

private:
    T* m_ptr{nullptr};

    void retain() noexcept {
        if (m_ptr) {
            m_ptr->ref();
        }
    }

    void release() noexcept {
        if (m_ptr) {
            m_ptr->unref();
            m_ptr = nullptr;
        }
    }

public:
    constexpr Ref() noexcept = default;
    constexpr Ref(std::nullptr_t) noexcept : m_ptr(nullptr) {}

    explicit Ref(T* ptr) noexcept : m_ptr(ptr) {
        retain();
    }

    Ref(const Ref& other) noexcept : m_ptr(other.m_ptr) {
        retain();
    }

    template <typename U>
    Ref(const Ref<U>& other) noexcept : m_ptr(other.get()) {
        retain();
    }

    Ref(Ref&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    template <typename U>
    Ref(Ref<U>&& other) noexcept : m_ptr(other.release_raw()) {}

    ~Ref() {
        release();
    }

    Ref& operator=(const Ref& other) noexcept {
        if (this != &other) {
            release();
            m_ptr = other.m_ptr;
            retain();
        }
        return *this;
    }

    Ref& operator=(Ref&& other) noexcept {
        if (this != &other) {
            release();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    Ref& operator=(std::nullptr_t) noexcept {
        release();
        return *this;
    }

    void reset(T* ptr = nullptr) noexcept {
        release();
        m_ptr = ptr;
        retain();
    }

    [[nodiscard]] T* get() const noexcept { return m_ptr; }
    [[nodiscard]] T* operator->() const noexcept { return m_ptr; }
    [[nodiscard]] T& operator*() const noexcept { return *m_ptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return m_ptr != nullptr; }

    [[nodiscard]] bool operator==(const Ref& other) const noexcept { return m_ptr == other.m_ptr; }
    [[nodiscard]] bool operator!=(const Ref& other) const noexcept { return m_ptr != other.m_ptr; }
    [[nodiscard]] bool operator==(std::nullptr_t) const noexcept { return m_ptr == nullptr; }
    [[nodiscard]] bool operator!=(std::nullptr_t) const noexcept { return m_ptr != nullptr; }

    [[nodiscard]] T* release_raw() noexcept {
        T* temp = m_ptr;
        m_ptr = nullptr;
        return temp;
    }
};

template <typename T, typename... Args>
[[nodiscard]] inline Ref<T> make_ref(Args&&... args) {
    return Ref<T>(new T(std::forward<Args>(args)...));
}

} // namespace txui
