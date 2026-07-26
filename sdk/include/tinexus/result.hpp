#ifndef TINEXUS_SDK_RESULT_HPP
#define TINEXUS_SDK_RESULT_HPP

#include <string>
#include <variant>
#include <utility>

namespace tinexus {

struct Error {
    int code{0};
    std::string message;
};

template <typename T>
class Result {
public:
    Result(T val) : m_data(std::move(val)) {}
    Result(Error err) : m_data(std::move(err)) {}

    [[nodiscard]] bool is_ok() const noexcept { return std::holds_alternative<T>(m_data); }
    [[nodiscard]] bool is_error() const noexcept { return std::holds_alternative<Error>(m_data); }

    [[nodiscard]] const T& value() const { return std::get<T>(m_data); }
    [[nodiscard]] const Error& error() const { return std::get<Error>(m_data); }

private:
    std::variant<T, Error> m_data;
};

} // namespace tinexus

#endif // TINEXUS_SDK_RESULT_HPP
