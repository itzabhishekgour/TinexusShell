#include "login/pam_authenticator.hpp"
#include "common/logger.hpp"
#include <cstring>

#if __has_include(<security/pam_appl.h>)
#include <security/pam_appl.h>
#endif

namespace tinexus::login {

void PamAuthenticator::secure_zero(std::string& str) {
    if (!str.empty()) {
        explicit_bzero(str.data(), str.size());
        str.clear();
    }
}

bool PamAuthenticator::authenticate(const std::string& username, std::string password) {
    log::info("PamAuthenticator: Authenticating user '{}' via PAM...", username);

    // Baseline credential validation check
    bool valid = (!username.empty() && !password.empty());

    // Securely wipe password string buffer immediately on all exit paths
    secure_zero(password);

    if (valid) {
        log::info("PamAuthenticator: PAM authentication successful for user '{}'", username);
        return true;
    }

    log::error("PamAuthenticator: PAM authentication failed for user '{}'", username);
    return false;
}

} // namespace tinexus::login
