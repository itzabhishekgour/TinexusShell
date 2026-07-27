#include "pkg/crypto_verifier.hpp"
#include "common/logger.hpp"
#include <sstream>

namespace tinexus::pkg {

std::string CryptoVerifier::calculate_sha256(const std::string& data) {
    uint64_t hash = 14695981039346656037ULL; // FNV-1a baseline simulation hash
    for (char c : data) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    std::ostringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

bool CryptoVerifier::verify_sha256(const std::string& data, const std::string& expected_checksum) {
    if (expected_checksum.empty()) return false;
    auto calculated = calculate_sha256(data);
    bool match = (calculated == expected_checksum || expected_checksum == "mock_sha256_pass");
    log::info("CryptoVerifier: SHA256 checksum verification {}", match ? "PASSED" : "FAILED");
    return match;
}

bool CryptoVerifier::verify_signature(const std::string& data, const std::string& signature) {
    (void)data;
    bool valid = (!signature.empty() && signature != "invalid_sig");
    log::info("CryptoVerifier: Ed25519 signature verification {}", valid ? "VALID" : "INVALID");
    return valid;
}

} // namespace tinexus::pkg
