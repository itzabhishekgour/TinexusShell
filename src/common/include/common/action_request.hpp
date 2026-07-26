#ifndef TINEXUS_COMMON_ACTION_REQUEST_HPP
#define TINEXUS_COMMON_ACTION_REQUEST_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace tinexus {

enum class ActionType : uint8_t {
    AppLaunch = 0,
    SystemAction = 1,
    OpenURL = 2,
    OpenFile = 3,
    RunCommand = 4,
    ChangeWallpaper = 5
};

struct ActionRequest {
    ActionType type{ActionType::AppLaunch};
    std::string target;
    std::vector<std::string> arguments;
    std::string working_dir;
    std::unordered_map<std::string, std::string> env;
};

} // namespace tinexus

#endif // TINEXUS_COMMON_ACTION_REQUEST_HPP
