#pragma once

#include <txui/core/NonCopyable.hpp>
#include <txui/core/Types.hpp>
#include <atomic>

namespace txui {

class Object : public NonCopyable {
private:
    mutable std::atomic<uint32> m_ref_count{0};

protected:
    Object() noexcept = default;
    virtual ~Object() = default;

public:
    void ref() const noexcept {
        m_ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    void unref() const noexcept {
        if (m_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }

    [[nodiscard]] uint32 ref_count() const noexcept {
        return m_ref_count.load(std::memory_order_relaxed);
    }
};

} // namespace txui
