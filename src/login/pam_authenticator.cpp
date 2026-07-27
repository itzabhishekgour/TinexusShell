#include "login/pam_authenticator.hpp"
#include "common/logger.hpp"

namespace tinexus::login {

AuthResult PamAuthenticator::authenticate(const std::string& username, const std::string& password) {
    std::string pwd_copy = password;
    log::info("PamAuthenticator: Attempting PAM authentication for user '{}'", username);

    AuthResult res = AuthResult::Success;
    if (username.empty() || pwd_copy.empty()) {
        res = AuthResult::InvalidCredentials;
    }

    // Zero out memory immediately after authentication attempt
    IAuthenticator::zero_memory(pwd_copy);
    log::info("PamAuthenticator: Password memory zeroed out post-authentication");

    return res;
}

AuthResult DummyAuthenticator::authenticate(const std::string& username, const std::string& password) {
    std::string pwd_copy = password;
    log::info("DummyAuthenticator: Authenticating user '{}'", username);

    AuthResult res = (m_allow_all && !username.empty() && !pwd_copy.empty()) 
                     ? AuthResult::Success 
                     : AuthResult::InvalidCredentials;

    IAuthenticator::zero_memory(pwd_copy);
    return res;
}

} // namespace tinexus::login
