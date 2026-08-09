#include "guard/crypto_validator.hpp"
#include <iostream>
#include <string>

void print_usage() {
    std::cerr << "Usage:\n"
              << "  txapp-signer keygen <priv_key_out> <pub_key_out>\n"
              << "  txapp-signer sign <payload_in> <priv_key_in> <sig_out>\n"
              << "  txapp-signer verify <payload_in> <sig_in> <pub_key_in>\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "keygen" && argc == 4) {
        if (tinexus::guard::CryptoValidator::generate_keypair(argv[2], argv[3])) {
            std::cout << "Successfully generated keypair.\n";
            return 0;
        } else {
            std::cerr << "Failed to generate keypair.\n";
            return 1;
        }
    } else if (cmd == "sign" && argc == 5) {
        if (tinexus::guard::CryptoValidator::sign_payload(argv[2], argv[3], argv[4])) {
            std::cout << "Successfully signed payload.\n";
            return 0;
        } else {
            std::cerr << "Failed to sign payload.\n";
            return 1;
        }
    } else if (cmd == "verify" && argc == 5) {
        if (tinexus::guard::CryptoValidator::verify_signature(argv[2], argv[3], argv[4])) {
            std::cout << "Signature is VALID.\n";
            return 0;
        } else {
            std::cerr << "Signature is INVALID.\n";
            return 1;
        }
    } else {
        print_usage();
        return 1;
    }
}
