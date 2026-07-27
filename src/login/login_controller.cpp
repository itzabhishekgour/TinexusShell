#include "login/login_controller.hpp"
#include "common/logger.hpp"

namespace tinexus::login {

bool LoginController::submit_credentials(const std::string& username, std::string password) {
    m_model.set_username(username);
    m_model.set_state(LoginState::Authenticating);
    m_view.render(m_model);

    AuthResult res = m_authenticator.authenticate(username, std::move(password));
    bool ok = (res == AuthResult::Success);
    if (!ok) {
        m_model.set_state(LoginState::Failed);
        m_model.set_error_message("PAM authentication failed");
        m_view.render(m_model);
        return false;
    }

    m_model.set_state(LoginState::Authenticated);
    m_view.render(m_model);

    m_model.set_state(LoginState::LaunchingSession);
    m_view.render(m_model);

    bool launched = SessionLauncher::launch_session(username, 1000, 1000);
    if (launched) {
        m_model.set_state(LoginState::SessionHandedOff);
        m_view.render(m_model);
        return true;
    }

    m_model.set_state(LoginState::Failed);
    m_model.set_error_message("Session launch failed");
    m_view.render(m_model);
    return false;
}

} // namespace tinexus::login
