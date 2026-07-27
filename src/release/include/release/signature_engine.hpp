#ifndef TINEXUS_RELEASE_SIGNATURE_ENGINE_HPP
#define TINEXUS_RELEASE_SIGNATURE_ENGINE_HPP

#include <string>

namespace tinexus::release {

class SignatureEngine {
public:
    static bool sign_file(const std::string& input_file, const std::string& output_sig, bool dry_run = false);
    static bool verify_signature(const std::string& input_file, const std::string& sig_file, bool dry_run = false);
};

} // namespace tinexus::release

#endif // TINEXUS_RELEASE_SIGNATURE_ENGINE_HPP
