#ifndef TINEXUS_COMMON_STRING_INTERNER_HPP
#define TINEXUS_COMMON_STRING_INTERNER_HPP

#include <string_view>
#include <unordered_set>
#include <string>
#include <shared_mutex>
#include <functional>

namespace tinexus {

struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const {
        return std::hash<std::string_view>{}(sv);
    }
};

class StringInterner {
public:
    static StringInterner& instance() noexcept;

    std::string_view intern(std::string_view str);
    [[nodiscard]] size_t unique_string_count() const noexcept;

private:
    StringInterner() = default;
    mutable std::shared_mutex m_mutex;
    std::unordered_set<std::string, StringHash, std::equal_to<>> m_pool;
};

} // namespace tinexus

#endif // TINEXUS_COMMON_STRING_INTERNER_HPP
