#ifndef TINEXUS_SDK_CLIENT_HPP
#define TINEXUS_SDK_CLIENT_HPP

#include "tinexus/result.hpp"
#include "tinexus/search.hpp"
#include "tinexus/actions.hpp"
#include <memory>

namespace tinexus {

class Client {
public:
    Client();
    ~Client();

    Result<bool> connect(const std::string& endpoint = "");
    void disconnect();
    [[nodiscard]] bool is_connected() const noexcept;

    // Service Accessors
    SearchService& search() noexcept;
    ActionService& actions() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace tinexus

#endif // TINEXUS_SDK_CLIENT_HPP
