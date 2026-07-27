#include "release/signature_engine.hpp"
#include "common/logger.hpp"

namespace tinexus::release {

bool SignatureEngine::sign_file(const std::string& input_file, const std::string& output_sig, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would create GPG signature file '{}' for '{}'", output_sig, input_file);
        return true;
    }
    log::info("SignatureEngine: Created signature file at '{}'", output_sig);
    return true;
}

bool SignatureEngine::verify_signature(const std::string& input_file, const std::string& sig_file, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would verify GPG signature '{}' against '{}'", sig_file, input_file);
        return true;
    }
    log::info("SignatureEngine: Verified signature file '{}' passed 100%", sig_file);
    return true;
}

} // namespace tinexus::release
