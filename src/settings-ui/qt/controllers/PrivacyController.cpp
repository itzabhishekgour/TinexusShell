#include "PrivacyController.hpp"
#include <guard/crypto_validator.hpp>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <QVariantMap>

namespace tinexus::settings_ui {

PrivacyController::PrivacyController(QObject* parent)
    : QObject(parent)
{
    rescanApps();
}

static bool is_hash_trusted(const std::string& sha256_hex) {
    std::ifstream file("/var/lib/tinexus/trust-overrides.conf");
    if (!file.is_open()) return false;
    
    std::string line;
    while (std::getline(file, line)) {
        if (line == sha256_hex) return true;
    }
    return false;
}

static std::vector<uint8_t> read_sig(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void PrivacyController::rescanApps() {
    m_unverifiedApps.clear();
    try {
        if (std::filesystem::exists("/opt/tinexus-apps")) {
            for (const auto& entry : std::filesystem::directory_iterator("/opt/tinexus-apps")) {
                if (entry.is_regular_file()) {
                    std::string path_str = entry.path().string();
                    if (path_str.ends_with(".sig")) continue;

                    int fd = open(path_str.c_str(), O_RDONLY | O_CLOEXEC);
                    if (fd < 0) continue;

                    std::string hash = tinexus::guard::CryptoValidator::compute_sha256_fd(fd);
                    bool is_trusted = is_hash_trusted(hash);

                    if (!is_trusted) {
                        std::string sig_path = path_str + ".sig";
                        auto sig_bytes = read_sig(sig_path);
                        bool sig_valid = false;
                        if (!sig_bytes.empty() && std::filesystem::exists("/etc/tinexus/keys/root.pub")) {
                            sig_valid = tinexus::guard::CryptoValidator::verify_signature_fd(
                                fd, sig_bytes, "/etc/tinexus/keys/root.pub");
                        }

                        if (!sig_valid) {
                            QVariantMap map;
                            map["name"] = QString::fromStdString(entry.path().filename().string());
                            map["path"] = QString::fromStdString(path_str);
                            map["hash"] = QString::fromStdString(hash);
                            map["reason"] = sig_bytes.empty() ? QStringLiteral("No cryptographic signature found")
                                                              : QStringLiteral("Signature does not match system root authority");
                            m_unverifiedApps.append(map);
                        }
                    }

                    close(fd);
                }
            }
        }
    } catch (...) {}
    emit unverifiedAppsChanged();
}

void PrivacyController::trustApp(const QString& hash) {
    std::error_code ec;
    std::filesystem::create_directories("/var/lib/tinexus", ec);
    std::ofstream file("/var/lib/tinexus/trust-overrides.conf", std::ios::app);
    if (file.is_open()) {
        file << hash.toStdString() << "\n";
    }
    rescanApps();
    emit toastRequested(QStringLiteral("Application binary hash trusted and allowed"), false);
}

} // namespace tinexus::settings_ui
