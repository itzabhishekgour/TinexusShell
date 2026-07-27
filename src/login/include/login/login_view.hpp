#ifndef TINEXUS_LOGIN_LOGIN_VIEW_HPP
#define TINEXUS_LOGIN_LOGIN_VIEW_HPP

#include "login/login_model.hpp"

namespace tinexus::login {

class LoginView {
public:
    LoginView() = default;
    ~LoginView() = default;

    void render(const LoginModel& model) const;
};

} // namespace tinexus::login

#endif // TINEXUS_LOGIN_LOGIN_VIEW_HPP
