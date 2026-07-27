#include <cassert>
#include <iostream>
#include "login/pam_authenticator.hpp"

using namespace tinexus::login;

int main() {
    std::cout << "[+] Running smoke_failed_login test suite..." << std::endl;

    DummyAuthenticator auth(false); // Reject all logins
    assert(auth.authenticate("alice", "wrong_pass") == AuthResult::InvalidCredentials);
    assert(auth.authenticate("", "pass") == AuthResult::InvalidCredentials);
    assert(auth.authenticate("alice", "") == AuthResult::InvalidCredentials);

    std::cout << "[+] smoke_failed_login: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
