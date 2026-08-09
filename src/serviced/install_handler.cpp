#include "serviced/install_handler.hpp"
#include "guard/crypto_validator.hpp"
#include "common/logger.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <regex>
#include <fstream>

namespace tinexus::serviced {

// RAII helper to ensure FD is always closed
struct ScopedFd {
    int fd;
    explicit ScopedFd(int f) : fd(f) {}
    ~ScopedFd() { if (fd >= 0) close(fd); }
    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;
};

// RAII helper to manage active install counter
struct ScopedActiveInstall {
    std::atomic<int>& counter;
    bool acquired{false};
    
    ScopedActiveInstall(std::atomic<int>& c, int max) : counter(c) {
        int current = counter.fetch_add(1);
        if (current < max) {
            acquired = true;
        } else {
            counter.fetch_sub(1);
        }
    }
    ~ScopedActiveInstall() {
        if (acquired) {
            counter.fetch_sub(1);
        }
    }
};

bool InstallHandler::is_valid_app_name(const std::string& name) const {
    if (name.empty() || name == "." || name == "..") return false;
    // Only allow alphanumeric, underscore, dash, and dot. Prevents path traversal (like containing '/')
    std::regex safe_pattern("^[a-zA-Z0-9_.-]+$");
    return std::regex_match(name, safe_pattern);
}

bool InstallHandler::copy_fd_to_path(int fd, const std::string& target_path) const {
    std::string tmp_path = target_path + ".tmp";
    
    // Rewind fd just in case
    lseek(fd, 0, SEEK_SET);

    int out_fd = open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (out_fd < 0) {
        tinexus::log::error("[serviced] Failed to open temp install path: {}", tmp_path);
        return false;
    }

    ScopedFd scoped_out(out_fd);

    char buf[65536];
    while (true) {
        ssize_t bytes_read = read(fd, buf, sizeof(buf));
        if (bytes_read < 0) {
            tinexus::log::error("[serviced] Error reading from payload fd");
            unlink(tmp_path.c_str());
            return false;
        }
        if (bytes_read == 0) break; // EOF

        char* p = buf;
        while (bytes_read > 0) {
            ssize_t bytes_written = write(out_fd, p, bytes_read);
            if (bytes_written < 0) {
                tinexus::log::error("[serviced] Error writing to temp install path");
                unlink(tmp_path.c_str());
                return false;
            }
            p += bytes_written;
            bytes_read -= bytes_written;
        }
    }

    if (fsync(out_fd) != 0) {
        tinexus::log::error("[serviced] Failed to fsync installed payload");
        unlink(tmp_path.c_str());
        return false;
    }

    if (rename(tmp_path.c_str(), target_path.c_str()) != 0) {
        tinexus::log::error("[serviced] Failed to rename tmp install file to final target");
        unlink(tmp_path.c_str());
        return false;
    }

    return true;
}

bool InstallHandler::handle_install_request(const std::string& app_name, int payload_fd, const std::vector<uint8_t>& signature_bytes) {
    // 1. Take ownership of FD immediately to prevent leaks
    ScopedFd scoped_payload(payload_fd);

    // 2. Concurrency check via RAII guard
    ScopedActiveInstall active_guard(m_active_installs, MAX_CONCURRENT_INSTALLS);
    if (!active_guard.acquired) {
        tinexus::log::warn("[serviced] Install rejected: Too many concurrent installs");
        return false;
    }

    // 2b. Payload size limit (Max 2GB) to prevent disk exhaustion DoS
    struct stat st;
    if (fstat(payload_fd, &st) != 0) {
        tinexus::log::error("[serviced] Install rejected: Failed to stat payload fd");
        return false;
    }
    constexpr off_t MAX_PAYLOAD_SIZE = 2LL * 1024LL * 1024LL * 1024LL; // 2 GB
    if (st.st_size > MAX_PAYLOAD_SIZE) {
        tinexus::log::error("[serviced] Install rejected: Payload exceeds maximum allowed size (2GB)");
        return false;
    }

    // 3. Name validation (Path traversal guard)
    if (!is_valid_app_name(app_name)) {
        tinexus::log::error("[serviced] Install rejected: Invalid app name '{}'", app_name);
        return false;
    }

    // 4. Cryptographic Validation (Done in-memory BEFORE writing to disk)
    const std::string pub_key_path = "/etc/tinexus/keys/root.pub";
    if (!tinexus::guard::CryptoValidator::verify_signature_fd(payload_fd, signature_bytes, pub_key_path)) {
        tinexus::log::error("[serviced] Install rejected: Invalid Ed25519 signature for '{}'", app_name);
        return false;
    }

    // 5. File copy
    mkdir("/opt/tinexus-apps", 0755); // Ensure base directory exists
    std::string target_path = "/opt/tinexus-apps/" + app_name;

    tinexus::log::info("[serviced] Signature valid. Installing app '{}' to {}", app_name, target_path);

    bool success = copy_fd_to_path(payload_fd, target_path);

    if (success) {
        // Save the signature so LaunchAuthority can re-verify it on launch
        std::string sig_path = target_path + ".sig";
        int sig_fd = open(sig_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (sig_fd >= 0) {
            write(sig_fd, signature_bytes.data(), signature_bytes.size());
            close(sig_fd);
        } else {
            tinexus::log::warn("[serviced] Failed to save signature file to {}", sig_path);
        }
        tinexus::log::info("[serviced] App '{}' installed successfully", app_name);
    } else {
        tinexus::log::error("[serviced] App '{}' installation failed during disk flush", app_name);
    }

    return success;
}

} // namespace tinexus::serviced
