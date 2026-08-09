#include "guard/crypto_validator.hpp"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace tinexus::guard {

static void handle_openssl_error() {
    ERR_print_errors_fp(stderr);
}

// Helper to mmap a file for reading
struct MmapFile {
    void* data = MAP_FAILED;
    size_t size = 0;
    int map_fd = -1;
    bool own_fd = false;

    MmapFile(int fd) : map_fd(fd), own_fd(false) { map(); }
    MmapFile(const std::string& path) {
        map_fd = open(path.c_str(), O_RDONLY);
        if (map_fd >= 0) {
            own_fd = true;
            map();
        }
    }
    ~MmapFile() {
        if (data != MAP_FAILED) {
            munmap(data, size);
        }
        if (own_fd && map_fd >= 0) {
            close(map_fd);
        }
    }
    bool is_valid() const { return data != MAP_FAILED; }

private:
    void map() {
        struct stat st;
        if (fstat(map_fd, &st) == 0 && st.st_size > 0) {
            size = static_cast<size_t>(st.st_size);
            data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, map_fd, 0);
        }
    }
};

bool CryptoValidator::generate_keypair(const std::string& priv_key_path, const std::string& pub_key_path) {
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (!pctx) return false;

    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return false;
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return false;
    }
    EVP_PKEY_CTX_free(pctx);

    // Write Private Key
    FILE* fpriv = fopen(priv_key_path.c_str(), "w");
    if (!fpriv) {
        EVP_PKEY_free(pkey);
        return false;
    }
    PEM_write_PrivateKey(fpriv, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(fpriv);

    // Write Public Key
    FILE* fpub = fopen(pub_key_path.c_str(), "w");
    if (!fpub) {
        EVP_PKEY_free(pkey);
        return false;
    }
    PEM_write_PUBKEY(fpub, pkey);
    fclose(fpub);

    EVP_PKEY_free(pkey);
    return true;
}

bool CryptoValidator::sign_payload(const std::string& payload_path, const std::string& priv_key_path, const std::string& signature_out_path) {
    FILE* key_file = fopen(priv_key_path.c_str(), "r");
    if (!key_file) return false;

    EVP_PKEY* priv_key = PEM_read_PrivateKey(key_file, nullptr, nullptr, nullptr);
    fclose(key_file);
    if (!priv_key) return false;

    MmapFile payload(payload_path);
    if (!payload.is_valid()) {
        EVP_PKEY_free(priv_key);
        return false;
    }

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        EVP_PKEY_free(priv_key);
        return false;
    }

    // Ed25519 does not pre-hash, so type is NULL
    if (EVP_DigestSignInit(md_ctx, nullptr, nullptr, nullptr, priv_key) <= 0) {
        handle_openssl_error();
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(priv_key);
        return false;
    }

    size_t sig_len = 0;
    if (EVP_DigestSign(md_ctx, nullptr, &sig_len, static_cast<const unsigned char*>(payload.data), payload.size) <= 0) {
        handle_openssl_error();
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(priv_key);
        return false;
    }

    std::vector<unsigned char> sig(sig_len);
    if (EVP_DigestSign(md_ctx, sig.data(), &sig_len, static_cast<const unsigned char*>(payload.data), payload.size) <= 0) {
        handle_openssl_error();
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(priv_key);
        return false;
    }

    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(priv_key);

    std::ofstream out(signature_out_path, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(sig.data()), static_cast<std::streamsize>(sig_len));
    return true;
}

bool CryptoValidator::verify_signature_fd(int payload_fd, const std::vector<uint8_t>& signature_bytes, const std::string& pub_key_path) {
    if (signature_bytes.size() != 64) {
        return false; // Ed25519 signatures are exactly 64 bytes
    }

    FILE* key_file = fopen(pub_key_path.c_str(), "r");
    if (!key_file) return false;

    EVP_PKEY* pub_key = PEM_read_PUBKEY(key_file, nullptr, nullptr, nullptr);
    fclose(key_file);
    if (!pub_key) return false;

    MmapFile payload(payload_fd);
    if (!payload.is_valid()) {
        EVP_PKEY_free(pub_key);
        return false;
    }

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        EVP_PKEY_free(pub_key);
        return false;
    }

    if (EVP_DigestVerifyInit(md_ctx, nullptr, nullptr, nullptr, pub_key) <= 0) {
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pub_key);
        return false;
    }

    int ret = EVP_DigestVerify(md_ctx, signature_bytes.data(), signature_bytes.size(), static_cast<const unsigned char*>(payload.data), payload.size);
    
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pub_key);

    return ret == 1; // 1 means success
}

bool CryptoValidator::verify_signature(const std::string& payload_path, const std::string& signature_path, const std::string& pub_key_path) {
    std::ifstream sig_file(signature_path, std::ios::binary | std::ios::ate);
    if (!sig_file.is_open()) return false;
    
    size_t sig_size = static_cast<size_t>(sig_file.tellg());
    if (sig_size == 0) return false;
    
    sig_file.seekg(0, std::ios::beg);
    std::vector<uint8_t> signature(sig_size);
    if (!sig_file.read(reinterpret_cast<char*>(signature.data()), static_cast<std::streamsize>(sig_size))) {
        return false;
    }

    int fd = open(payload_path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    bool res = verify_signature_fd(fd, signature, pub_key_path);
    close(fd);
    return res;
}

std::string CryptoValidator::compute_sha256_fd(int payload_fd) {
    MmapFile payload(payload_fd);
    if (!payload.is_valid()) return "";

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    
    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (md_ctx) {
        EVP_DigestInit_ex(md_ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(md_ctx, payload.data, payload.size);
        EVP_DigestFinal_ex(md_ctx, hash, &hash_len);
        EVP_MD_CTX_free(md_ctx);
    }

    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

} // namespace tinexus::guard
