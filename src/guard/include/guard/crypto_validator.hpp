#ifndef TINEXUS_GUARD_CRYPTO_VALIDATOR_HPP
#define TINEXUS_GUARD_CRYPTO_VALIDATOR_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace tinexus::guard {

class CryptoValidator {
public:
    CryptoValidator() = default;
    ~CryptoValidator() = default;

    /**
     * @brief Generates a new Ed25519 keypair and writes to disk
     * @param priv_key_path Path to write the private key (PEM)
     * @param pub_key_path Path to write the public key (PEM)
     * @return true if successful
     */
    static bool generate_keypair(const std::string& priv_key_path, const std::string& pub_key_path);

    /**
     * @brief Signs a payload using an Ed25519 private key
     * @param payload_path Path to the file to sign
     * @param priv_key_path Path to the private key
     * @param signature_out_path Path to write the detached signature
     * @return true if successful
     */
    static bool sign_payload(const std::string& payload_path, const std::string& priv_key_path, const std::string& signature_out_path);

    /**
     * @brief Verifies a detached Ed25519 signature against a payload (using file path)
     * @param payload_path Path to the payload file
     * @param signature_path Path to the detached signature file
     * @param pub_key_path Path to the public key to verify against
     * @return true if signature is valid, false otherwise
     */
    static bool verify_signature(const std::string& payload_path, const std::string& signature_path, const std::string& pub_key_path);

    /**
     * @brief Verifies a detached Ed25519 signature against a payload (using file descriptor)
     * @param payload_fd File descriptor of the payload
     * @param signature_bytes Raw bytes of the Ed25519 signature (must be 64 bytes)
     * @param pub_key_path Path to the public key (trusted system root key)
     * @return true if signature is valid, false otherwise
     */
    static bool verify_signature_fd(int payload_fd, const std::vector<uint8_t>& signature_bytes, const std::string& pub_key_path);

    /**
     * @brief Computes SHA256 hash of a file (useful for binary hash binding in Gatekeeper)
     * @param payload_fd File descriptor of the payload
     * @return hex string of the SHA256 hash, or empty string on failure
     */
    static std::string compute_sha256_fd(int payload_fd);
};

} // namespace tinexus::guard

#endif // TINEXUS_GUARD_CRYPTO_VALIDATOR_HPP
