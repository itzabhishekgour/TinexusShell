#include "clipboard/privacy_filter.hpp"

namespace tinexus::clipboard {

bool PrivacyFilter::is_sensitive(const std::string& content) {
    if (content.empty()) return false;

    // Detect Private Key Headers
    if (content.find("-----BEGIN PRIVATE KEY-----") != std::string::npos ||
        content.find("-----BEGIN RSA PRIVATE KEY-----") != std::string::npos) {
        return true;
    }

    // Detect OAuth Bearer Tokens & JWTs
    if (content.rfind("Bearer ", 0) == 0 || content.rfind("eyJ", 0) == 0) {
        return true;
    }

    // Detect explicit password string assignments
    if (content.find("password=") != std::string::npos ||
        content.find("passwd=") != std::string::npos ||
        content.find("secret=") != std::string::npos) {
        return true;
    }

    return false;
}

} // namespace tinexus::clipboard
