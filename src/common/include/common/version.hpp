#ifndef TINEXUS_COMMON_VERSION_HPP
#define TINEXUS_COMMON_VERSION_HPP

#include <string_view>

namespace tinexus {

constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 1;
constexpr int VERSION_PATCH = 0;

constexpr std::string_view VERSION_STRING = "0.1.0";
constexpr std::string_view PLATFORM_NAME  = "Tinexus Platform";

} // namespace tinexus

#endif // TINEXUS_COMMON_VERSION_HPP
