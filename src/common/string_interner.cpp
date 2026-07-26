#include "common/string_interner.hpp"
#include <mutex>

namespace tinexus {

StringInterner& StringInterner::instance() noexcept {
    static StringInterner s_instance;
    return s_instance;
}

std::string_view StringInterner::intern(std::string_view str) {
    {
        std::shared_lock lock(m_mutex);
        auto it = m_pool.find(str);
        if (it != m_pool.end()) {
            return *it;
        }
    }

    std::unique_lock lock(m_mutex);
    auto [it, inserted] = m_pool.emplace(std::string(str));
    return *it;
}

size_t StringInterner::unique_string_count() const noexcept {
    std::shared_lock lock(m_mutex);
    return m_pool.size();
}

} // namespace tinexus
