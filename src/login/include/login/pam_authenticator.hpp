#ifndef TINEXUS_LOGIN_PAM_AUTHENTICATOR_HPP
#define TINEXUS_LOGIN_PAM_AUTHENTICATOR_HPP

#include <string>

namespace tinexus::login {

class PamAuthenticator {
public:
    PamAuthenticator() = default;
    ~PamAuthenticator() = default;

    bool authenticate(const std::string& username, std::string password);

private:
    void secure_zero(std::string& str);
};

} // namespace tinexus::login

#endif // TINEXUS_LOGIN_PAM_AUTHENTICATOR_HPP
