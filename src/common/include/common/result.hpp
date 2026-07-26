#ifndef TINEXUS_COMMON_RESULT_HPP
#define TINEXUS_COMMON_RESULT_HPP

#include <variant>
#include <utility>
#include <stdexcept>
#include <type_traits>

namespace tinexus {

template <typename E>
class Unexpected {
public:
    explicit constexpr Unexpected(E val) : m_value(std::move(val)) {}
    [[nodiscard]] constexpr const E& value() const & noexcept { return m_value; }
    [[nodiscard]] constexpr E& value() & noexcept { return m_value; }
    [[nodiscard]] constexpr E&& value() && noexcept { return std::move(m_value); }
private:
    E m_value;
};

template <typename E>
Unexpected(E) -> Unexpected<E>;

template <typename T, typename E>
class [[nodiscard]] Result {
public:
    using value_type = T;
    using error_type = E;

    constexpr Result(const T& val) : m_storage(std::in_place_index<0>, val) {}
    constexpr Result(T&& val) : m_storage(std::in_place_index<0>, std::move(val)) {}
    constexpr Result(Unexpected<E> err) : m_storage(std::in_place_index<1>, std::move(err.value())) {}

    [[nodiscard]] constexpr bool has_value() const noexcept {
        return m_storage.index() == 0;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] constexpr const T& value() const & {
        if (!has_value()) {
            throw std::logic_error("Attempted to access value of an error Result");
        }
        return std::get<0>(m_storage);
    }

    [[nodiscard]] constexpr T& value() & {
        if (!has_value()) {
            throw std::logic_error("Attempted to access value of an error Result");
        }
        return std::get<0>(m_storage);
    }

    [[nodiscard]] constexpr T&& value() && {
        if (!has_value()) {
            throw std::logic_error("Attempted to access value of an error Result");
        }
        return std::get<0>(std::move(m_storage));
    }

    [[nodiscard]] constexpr const E& error() const & {
        if (has_value()) {
            throw std::logic_error("Attempted to access error of a success Result");
        }
        return std::get<1>(m_storage);
    }

    [[nodiscard]] constexpr E& error() & {
        if (has_value()) {
            throw std::logic_error("Attempted to access error of a success Result");
        }
        return std::get<1>(m_storage);
    }

    [[nodiscard]] constexpr E&& error() && {
        if (has_value()) {
            throw std::logic_error("Attempted to access error of a success Result");
        }
        return std::get<1>(std::move(m_storage));
    }

    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& default_val) const & {
        return has_value() ? std::get<0>(m_storage) : static_cast<T>(std::forward<U>(default_val));
    }

private:
    std::variant<T, E> m_storage;
};

} // namespace tinexus

#endif // TINEXUS_COMMON_RESULT_HPP
