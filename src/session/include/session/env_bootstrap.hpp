#ifndef TINEXUS_SESSION_ENV_BOOTSTRAP_HPP
#define TINEXUS_SESSION_ENV_BOOTSTRAP_HPP

#include <string>
#include <unordered_map>

namespace tinexus::session {

class EnvironmentBootstrapper {
public:
    static EnvironmentBootstrapper& instance() noexcept;

    EnvironmentBootstrapper() = default;
    ~EnvironmentBootstrapper() = default;

    bool bootstrap_environment();
    std::unordered_map<std::string, std::string> get_environment_map() const;
    void print_environment() const;
};

class EnvBootstrap {
public:
    static bool apply_environment();
};

} // namespace tinexus::session

#endif // TINEXUS_SESSION_ENV_BOOTSTRAP_HPP
