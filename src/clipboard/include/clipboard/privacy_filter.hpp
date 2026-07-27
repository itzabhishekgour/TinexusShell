#ifndef TINEXUS_CLIPBOARD_PRIVACY_FILTER_HPP
#define TINEXUS_CLIPBOARD_PRIVACY_FILTER_HPP

#include <string>

namespace tinexus::clipboard {

class PrivacyFilter {
public:
    static bool is_sensitive(const std::string& content);
};

} // namespace tinexus::clipboard

#endif // TINEXUS_CLIPBOARD_PRIVACY_FILTER_HPP
