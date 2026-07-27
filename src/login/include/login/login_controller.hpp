#ifndef TINEXUS_LOGIN_LOGIN_CONTROLLER_HPP
#define TINEXUS_LOGIN_LOGIN_CONTROLLER_HPP

#include "login/login_model.hpp"
#include "login/login_view.hpp"
#include "login/pam_authenticator.hpp"
#include "login/session_launcher.hpp"

namespace tinexus::login {

class LoginController {
public:
    LoginController() = default;
    ~LoginController() = default;

    bool submit_credentials(const std::string& username, std::string password);
    [[nodiscard]] const LoginModel& model() const noexcept { return m_model; }

private:
    LoginModel m_model;
    LoginView m_view;
    PamAuthenticator m_authenticator;
};

} // namespace tinexus::login

#endif // TINEXUS_LOGIN_LOGIN_CONTROLLER_HPP
