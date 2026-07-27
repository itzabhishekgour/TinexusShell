#ifndef TINEXUS_LOGIN_PAM_AUTHENTICATOR_HPP
#define TINEXUS_LOGIN_PAM_AUTHENTICATOR_HPP

#include <string>
#include <vector>
#include <memory>

namespace tinexus::login {

enum class AuthResult {
    Success,
    InvalidCredentials,
    UserNotFound,
    AccountLocked,
    PamError
};

class IAuthenticator {
public:
    virtual ~IAuthenticator() = default;

    [[nodiscard]] virtual std::string type_name() const noexcept = 0;
    [[nodiscard]] virtual AuthResult authenticate(const std::string& username, const std::string& password) = 0;
    
    static void zero_memory(std::string& str) noexcept {
        for (volatile char& c : str) {
            c = 0;
        }
        str.clear();
    }
};

class PamAuthenticator : public IAuthenticator {
public:
    PamAuthenticator() = default;
    ~PamAuthenticator() override = default;

    std::string type_name() const noexcept override { return "PamAuthenticator"; }
    AuthResult authenticate(const std::string& username, const std::string& password) override;
};

class DummyAuthenticator : public IAuthenticator {
public:
    explicit DummyAuthenticator(bool allow_all = true) : m_allow_all(allow_all) {}
    ~DummyAuthenticator() override = default;

    std::string type_name() const noexcept override { return "DummyAuthenticator"; }
    AuthResult authenticate(const std::string& username, const std::string& password) override;

    void set_allow_all(bool allow) noexcept { m_allow_all = allow; }

private:
    bool m_allow_all{true};
};

} // namespace tinexus::login

#endif // TINEXUS_LOGIN_PAM_AUTHENTICATOR_HPP
