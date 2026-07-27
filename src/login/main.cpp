#include "login/login_controller.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("login");
    tinexus::log::info("Starting Tinexus Login Manager (tinexus-login)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Login: Connected to Tinexus Platform IPC broker via SDK.");
    }

    tinexus::login::LoginController controller;
    std::string user = (argc > 1) ? argv[1] : "tinexus-user";
    std::string pass = (argc > 2) ? argv[2] : "password";

    controller.submit_credentials(user, pass);

    sdk_client.disconnect();
    return 0;
}
