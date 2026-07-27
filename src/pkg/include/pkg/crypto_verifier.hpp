#ifndef TINEXUS_PKG_CRYPTO_VERIFIER_HPP
#define TINEXUS_PKG_CRYPTO_VERIFIER_HPP

#include <string>

namespace tinexus::pkg {

class CryptoVerifier {
public:
    static std::string calculate_sha256(const std::string& data);
    static bool verify_sha256(const std::string& data, const std::string& expected_checksum);
    static bool verify_signature(const std::string& data, const std::string& signature);
};

} // namespace tinexus::pkg

#endif // TINEXUS_PKG_CRYPTO_VERIFIER_HPP
