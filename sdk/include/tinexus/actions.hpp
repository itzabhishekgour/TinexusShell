#ifndef TINEXUS_SDK_ACTIONS_HPP
#define TINEXUS_SDK_ACTIONS_HPP

#include "common/action_request.hpp"
#include "tinexus/result.hpp"
#include <future>

namespace tinexus {

class ActionService {
public:
    ActionService() = default;
    ~ActionService() = default;

    Result<bool> execute(const ActionRequest& req);
    std::future<Result<bool>> execute_async(const ActionRequest& req);
};

} // namespace tinexus

#endif // TINEXUS_SDK_ACTIONS_HPP
