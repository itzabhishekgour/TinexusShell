#ifndef TINEXUS_SERVICED_LAUNCH_AUTHORITY_HPP
#define TINEXUS_SERVICED_LAUNCH_AUTHORITY_HPP

#include "common/action_request.hpp"
#include <string>
#include <sys/types.h>

namespace tinexus::serviced {

class LaunchAuthority {
public:
    static LaunchAuthority& instance() noexcept;

    LaunchAuthority() = default;
    ~LaunchAuthority() = default;

    pid_t execute_action(const ActionRequest& req);
    pid_t launch_app(const std::string& app_id, const std::string& exec_cmd);
    bool is_valid_executable(const std::string& exec_cmd) const;
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_LAUNCH_AUTHORITY_HPP
