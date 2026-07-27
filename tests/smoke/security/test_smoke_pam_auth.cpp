#include <cassert>
#include <iostream>
#include "login/pam_authenticator.hpp"

using namespace tinexus::login;

int main() {
    std::cout << "[+] Running smoke_pam_auth test suite..." << std::endl;

    PamAuthenticator pam_auth;
    assert(pam_auth.type_name() == "PamAuthenticator");

    AuthResult res = pam_auth.authenticate("testuser", "secret_pass");
    assert(res == AuthResult::Success);

    DummyAuthenticator dummy_auth(true);
    assert(dummy_auth.type_name() == "DummyAuthenticator");
    assert(dummy_auth.authenticate("alice", "password123") == AuthResult::Success);

    // Memory zeroing verification
    std::string secret = "SensitivePassword99";
    IAuthenticator::zero_memory(secret);
    assert(secret.empty());

    std::cout << "[+] smoke_pam_auth: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
