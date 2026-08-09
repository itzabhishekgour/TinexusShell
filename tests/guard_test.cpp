#include "guard/crypto_validator.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>

void create_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f.write(content.c_str(), content.size());
}

void test_guard_validation() {
    using namespace tinexus::guard;

    std::string priv = "test_priv.pem";
    std::string pub = "test_pub.pem";
    std::string payload = "test_payload.dat";
    std::string sig = "test_sig.sig";

    // 1. Keygen
    assert(CryptoValidator::generate_keypair(priv, pub) == true);

    // 2. Sign valid payload
    create_file(payload, "Hello Tinexus Guard");
    assert(CryptoValidator::sign_payload(payload, priv, sig) == true);

    // 3. Verify valid signature
    assert(CryptoValidator::verify_signature(payload, sig, pub) == true);

    // 4. Verify tampered payload
    create_file(payload, "Hello Tinexus Guard!"); // 1 byte diff
    assert(CryptoValidator::verify_signature(payload, sig, pub) == false);

    // 5. Verify 0-byte payload
    create_file(payload, "");
    assert(CryptoValidator::verify_signature(payload, sig, pub) == false);

    // 6. Verify missing files
    assert(CryptoValidator::verify_signature("nonexistent", sig, pub) == false);
    assert(CryptoValidator::verify_signature(payload, "nonexistent", pub) == false);

    // 7. Verify corrupted signature (wrong length)
    create_file(sig, "bad_sig_data");
    assert(CryptoValidator::verify_signature(payload, sig, pub) == false);

    // Cleanup
    unlink(priv.c_str());
    unlink(pub.c_str());
    unlink(payload.c_str());
    unlink(sig.c_str());
    
    std::cout << "All tinexus-guard tests passed successfully!\n";
}

int main() {
    test_guard_validation();
    return 0;
}
