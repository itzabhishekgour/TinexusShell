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

    pid_t execute_action(const ActionRequest& req, int app_fd = -1);
    pid_t launch_app(const std::string& app_id, const std::string& exec_cmd);
    
    // Returns a valid file descriptor (>0) if it's a secured app, 
    // -2 if it's a base system binary (no validation needed),
    // and -1 if validation failed.
    int validate_and_get_fd(const std::string& exec_cmd) const;
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_LAUNCH_AUTHORITY_HPP
