#include "login/login_view.hpp"
#include "common/logger.hpp"

namespace tinexus::login {

void LoginView::render(const LoginModel& model) const {
    log::info("LoginView: Rendering surface state (State: {}, User: '{}')",
              static_cast<int>(model.state()), model.username());
}

} // namespace tinexus::login
