#ifndef TINEXUS_LOGIN_LOGIN_MODEL_HPP
#define TINEXUS_LOGIN_LOGIN_MODEL_HPP

#include <string>

namespace tinexus::login {

enum class LoginState {
    Idle,
    UsernameEntered,
    PasswordEntered,
    Authenticating,
    Authenticated,
    LaunchingSession,
    SessionHandedOff,
    Failed
};

class LoginModel {
public:
    LoginModel() = default;
    ~LoginModel() = default;

    void set_username(const std::string& user) { m_username = user; }
    [[nodiscard]] const std::string& username() const noexcept { return m_username; }

    void set_state(LoginState st) noexcept { m_state = st; }
    [[nodiscard]] LoginState state() const noexcept { return m_state; }

    void set_error_message(const std::string& err) { m_error_msg = err; }
    [[nodiscard]] const std::string& error_message() const noexcept { return m_error_msg; }

private:
    std::string m_username;
    LoginState m_state{LoginState::Idle};
    std::string m_error_msg;
};

} // namespace tinexus::login

#endif // TINEXUS_LOGIN_LOGIN_MODEL_HPP
