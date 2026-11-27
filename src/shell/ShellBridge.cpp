// ============================================================================
// ShellBridge.cpp — C++20 QObject Bridge for tinexus-shell
// ============================================================================
#include "ShellBridge.hpp"
#include <common/AudioUtils.hpp>
#include <common/BacklightUtils.hpp>
#include <common/RuntimePaths.hpp>
#include <common/AppId.hpp>
#include <common/logger.hpp>
#include <common/ui/WaylandBlurHelper.hpp>
#include "ipcd/protocol/header.hpp"
#include "ipcd/protocol/display_protocol.hpp"

#include <QtCore/QProcess>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <QtNetwork/QLocalSocket>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

#include <sys/reboot.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <linux/reboot.h>
#include <unistd.h>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace fs = std::filesystem;

namespace tinexus::shell {

struct ShellBridge::BlurProtocolState : public tinexus::common::ui::WaylandBlurHelper {};

namespace {
std::string run_shell_nmcli(const std::vector<std::string>& args, int timeout_ms = 5000, int* rc_out = nullptr) {
    if (!fs::exists("/usr/bin/nmcli")) {
        if (rc_out) *rc_out = 127;
        return {};
    }
    int pipefd[2]{-1, -1};
    if (::pipe2(pipefd, O_CLOEXEC) != 0) {
        if (rc_out) *rc_out = 1;
        return {};
    }
    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        if (rc_out) *rc_out = 1;
        return {};
    }
    if (pid == 0) {
        ::close(pipefd[0]);
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::dup2(pipefd[1], STDERR_FILENO);
        int devnull = ::open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            if (devnull > 2) ::close(devnull);
        }
        if (pipefd[1] > 2) ::close(pipefd[1]);
        std::vector<char*> c_args;
        c_args.reserve(args.size() + 2);
        c_args.push_back(const_cast<char*>("/usr/bin/nmcli"));
        for (const auto& a : args) c_args.push_back(const_cast<char*>(a.c_str()));
        c_args.push_back(nullptr);
        ::execv("/usr/bin/nmcli", c_args.data());
        _exit(127);
    }
    ::close(pipefd[1]);
    int flags = ::fcntl(pipefd[0], F_GETFL, 0);
    if (flags >= 0) ::fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    std::string out;
    char buf[1024];
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    bool eof = false;
    while (!eof) {
        int rem = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count());
        if (rem <= 0) {
            ::kill(pid, SIGKILL);
            break;
        }
        struct pollfd pfd{pipefd[0], POLLIN, 0};
        int pr = ::poll(&pfd, 1, rem);
        if (pr > 0) {
            while (true) {
                ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
                if (n > 0) out.append(buf, static_cast<size_t>(n));
                else if (n == 0) { eof = true; break; }
                else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                else if (errno == EINTR) continue;
                else { eof = true; break; }
            }
        } else if (pr == 0) {
            ::kill(pid, SIGKILL);
            break;
        } else if (errno != EINTR) {
            break;
        }
    }
    ::close(pipefd[0]);
    int st = 0;
    while (::waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    if (rc_out) *rc_out = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
    return out;
}

std::vector<std::string> split_nmcli_line(const std::string& line) {
    std::vector<std::string> cols;
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size() && (line[i + 1] == ':' || line[i + 1] == '\\')) {
            cur.push_back(line[i + 1]);
            ++i;
        } else if (line[i] == ':') {
            cols.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(line[i]);
        }
    }
    cols.push_back(cur);
    return cols;
}

std::string trim_shell_str(std::string s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}
} // namespace

ShellBridge::ShellBridge(QObject* parent)
    : QObject(parent)
    , m_blurState(std::make_unique<BlurProtocolState>())
{
    // Initialize fast lightweight defaults for immediate zero-lag mapping
    m_volume = 75;
    m_soundMuted = false;
    m_brightness = 80;

    resolveLogoUrl();

    // Defer all I/O, subprocess audio probing, .desktop scanning, and D-Bus subscription
    // until immediately after the TopBar maps cleanly onto the compositor output.
    QTimer::singleShot(0, this, [this]() {
        auto bus = QDBusConnection::sessionBus();
        bool addedOk = bus.connect(
            QString(),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("NotificationAdded"),
            this,
            SLOT(onNotificationAdded(uint,QString,QString,QString,QString,int))
        );
        bus.connect(
            QString(),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("NotificationClosed"),
            this,
            SLOT(onNotificationClosed(uint,uint))
        );
        if (!addedOk) {
            tinexus::log::warn("[Shell] Failed to connect to org.freedesktop.Notifications NotificationAdded signal");
        } else {
            tinexus::log::info("[Shell] Connected to org.freedesktop.Notifications signals");
        }

        syncActiveNotifications();
        setupIpcdClient();
        pollBattery();
        syncAudioState();
        syncWifiState(false);
        scanApplications();

        QPointer<ShellBridge> self(this);
        std::thread([self]() {
            int bri = hardware::BacklightUtils::get_brightness_percent();
            if (!self) return;
            QMetaObject::invokeMethod(self, [self, bri]() {
                if (!self) return;
                if (!self->m_draggingBrightness && self->m_brightness != bri) {
                    self->m_brightness = bri;
                    emit self->brightnessChanged();
                }
            }, Qt::QueuedConnection);
        }).detach();
    });

    updateClock();
    connect(&m_clockTimer, &QTimer::timeout, this, &ShellBridge::updateClock);
    m_clockTimer.start(5000);

    connect(&m_batteryTimer, &QTimer::timeout, this, &ShellBridge::pollBattery);
    m_batteryTimer.start(30000);

    connect(&m_audioTimer, &QTimer::timeout, this, &ShellBridge::syncAudioState);
    m_audioTimer.start(8000);

    connect(&m_wifiTimer, &QTimer::timeout, this, [this]() {
        syncWifiState(m_wifiFlyoutOpen);
    });
    m_wifiTimer.start(15000);

    m_volumeApplyTimer.setSingleShot(true);
    m_volumeApplyTimer.setInterval(40);
    connect(&m_volumeApplyTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingVolume < 0 || m_audioWriting->exchange(true)) {
            if (m_pendingVolume >= 0 && !m_volumeApplyTimer.isActive()) {
                m_volumeApplyTimer.start(40);
            }
            return;
        }
        const int targetVol = m_pendingVolume;
        const bool dragActive = m_draggingVolume;
        m_pendingVolume = -1;
        QPointer<ShellBridge> self(this);
        auto writingFlag = m_audioWriting;
        std::thread([self, writingFlag, targetVol, dragActive]() {
            hardware::AudioUtils::set_volume_percent(targetVol, /*persist=*/!dragActive, /*throttle=*/dragActive);
            const int actualVol = hardware::AudioUtils::get_volume_percent();
            const bool actualMuted = hardware::AudioUtils::is_muted() || (actualVol == 0);
            if (!self) {
                writingFlag->store(false);
                return;
            }
            QMetaObject::invokeMethod(self, [self, writingFlag, actualVol, actualMuted]() {
                writingFlag->store(false);
                if (!self) return;
                if (self->m_pendingVolume >= 0) {
                    self->m_volumeApplyTimer.start(0);
                    return;
                }
                if (self->m_draggingVolume) {
                    return;
                }
                if (self->m_volume != actualVol || self->m_soundMuted != actualMuted) {
                    self->m_volume = actualVol;
                    self->m_soundMuted = actualMuted;
                    emit self->volumeChanged();
                }
            }, Qt::QueuedConnection);
        }).detach();
    });

    m_brightnessApplyTimer.setSingleShot(true);
    m_brightnessApplyTimer.setInterval(40);
    connect(&m_brightnessApplyTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingBrightness < 0 || m_brightnessWriting->exchange(true)) {
            if (m_pendingBrightness >= 0 && !m_brightnessApplyTimer.isActive()) {
                m_brightnessApplyTimer.start(40);
            }
            return;
        }
        const int targetBri = m_pendingBrightness;
        const bool dragActive = m_draggingBrightness;
        m_pendingBrightness = -1;
        QPointer<ShellBridge> self(this);
        auto writingFlag = m_brightnessWriting;
        std::thread([self, writingFlag, targetBri, dragActive]() {
            hardware::BacklightUtils::set_brightness_percent(targetBri, /*persist=*/!dragActive, /*throttle=*/false);
            if (!self) {
                writingFlag->store(false);
                return;
            }
            QMetaObject::invokeMethod(self, [self, writingFlag]() {
                writingFlag->store(false);
                if (!self) return;
                if (self->m_pendingBrightness >= 0 && !self->m_brightnessApplyTimer.isActive()) {
                    self->m_brightnessApplyTimer.start(0);
                }
            }, Qt::QueuedConnection);
        }).detach();
    });

    m_toastTimer.setSingleShot(true);
    connect(&m_toastTimer, &QTimer::timeout, this, &ShellBridge::dismissToast);
}

ShellBridge::~ShellBridge() = default;

void ShellBridge::setShellWindow(QQuickWindow* window) {
    m_shellWindow = window;
}

void ShellBridge::syncBlurRegion(const QRegion& region) {
    m_lastBlurRegion = region;
    if (!m_blurState) return;
    m_blurState->applyToWindow(m_shellWindow, m_blurEnabled, region);
}

void ShellBridge::syncOverlayGeometry() {
    emit overlayGeometrySyncRequested();
}

void ShellBridge::resolveLogoUrl() {
    const QStringList candidates = {
        QStringLiteral("/usr/share/icons/hicolor/32x32/apps/tinexus-logo.png"),
        QStringLiteral("/usr/share/tinexus/tinexus-logo.png"),
        QStringLiteral("/usr/share/pixmaps/tinexus-logo.png"),
        QStringLiteral("assets/logo/tinexus-logo-32.png"),
        QStringLiteral("../assets/logo/tinexus-logo-32.png"),
        QStringLiteral("../../assets/logo/tinexus-logo-32.png"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../assets/logo/tinexus-logo-32.png"),
        QStringLiteral("/workspace/assets/logo/tinexus-logo-32.png"),
        QStringLiteral("assets/logo/tinexus-logo.svg"),
        QStringLiteral("/workspace/assets/logo/tinexus-logo.svg")
    };

    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            m_logoUrl = QUrl::fromLocalFile(QFileInfo(cand).absoluteFilePath()).toString();
            break;
        }
    }
}

void ShellBridge::scanApplications() {
    if (m_appsScanning->exchange(true)) {
        return;
    }

    struct DesktopEntryRaw {
        std::string name;
        std::string exec;
        std::string exec_key;
        std::string icon;
        std::string comment;
        bool is_tinexus{false};
    };

    QPointer<ShellBridge> self(this);
    auto scanningFlag = m_appsScanning;

    std::thread([self, scanningFlag]() {
        std::vector<DesktopEntryRaw> scanned;
        std::unordered_set<std::string> seen_canonical_paths;

        const std::vector<std::string> dirs = {
            "/usr/share/applications",
            "/usr/local/share/applications"
        };

        auto to_lower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        };

        auto list_contains_ci = [&](const std::string& semi_list, const char* target) {
            std::string tgt = to_lower(target);
            std::stringstream ss(semi_list);
            std::string tok;
            while (std::getline(ss, tok, ';')) {
                while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
                while (!tok.empty() && tok.back() == ' ') tok.pop_back();
                if (to_lower(tok) == tgt) return true;
            }
            return false;
        };

        auto is_executable_available = [](const std::string& bin) {
            if (bin.empty()) return false;
            if (bin.find('/') != std::string::npos) {
                return ::access(bin.c_str(), X_OK) == 0;
            }
            const char* path_env = std::getenv("PATH");
            if (!path_env) path_env = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
            std::stringstream ss(path_env);
            std::string dir;
            while (std::getline(ss, dir, ':')) {
                if (dir.empty()) continue;
                std::string full = dir + "/" + bin;
                if (::access(full.c_str(), X_OK) == 0) return true;
            }
            return false;
        };

        auto normalize_exec_key = [&](const std::string& raw_exec) {
            std::string cleaned;
            cleaned.reserve(raw_exec.size());
            for (char c : raw_exec) {
                if (c != '"' && c != '\'') cleaned.push_back(c);
            }
            std::istringstream iss(cleaned);
            std::string tok;
            while (iss >> tok) {
                std::string low = to_lower(tok);
                if (low == "env" || low == "sudo" || low == "pkexec" ||
                    low == "sh" || low == "/bin/sh" || low == "bash" || low == "/bin/bash" ||
                    low == "-c" || low == "-e") {
                    continue;
                }
                if (!low.empty() && low[0] == '-') continue;
                if (tok.find('=') != std::string::npos) continue;
                auto slash = low.find_last_of('/');
                if (slash != std::string::npos) low = low.substr(slash + 1);
                return low;
            }
            return std::string{};
        };

        auto is_hidden_desktop_id = [](const std::string& filename) {
            static const std::unordered_set<std::string> kHiddenIds = {
                "tinexus-dock.desktop",
                "tinexus-app-installer.desktop",
                "footclient.desktop",
                "foot-server.desktop",
                "org.freedesktop.Xwayland.desktop",
                "org.kde.kded6.desktop",
                "org.kde.kiod6.desktop",
                "org.kde.ksecretd.desktop",
                "ktelnetservice6.desktop",
                "python3.14.desktop"
            };
            return kHiddenIds.find(filename) != kHiddenIds.end();
        };

        for (const auto& d : dirs) {
            std::error_code ec;
            if (!fs::exists(d, ec)) continue;
            try {
                std::vector<fs::directory_entry> dir_entries;
                for (const auto& entry : fs::directory_iterator(d, ec)) {
                    dir_entries.push_back(entry);
                }
                // Process tinexus-*.desktop entries before third-party entries so branded
                // entries (e.g. tinexus-installer) win deduplication over upstream wrappers.
                std::sort(dir_entries.begin(), dir_entries.end(),
                          [](const fs::directory_entry& a, const fs::directory_entry& b) {
                              std::string fa = a.path().filename().string();
                              std::string fb = b.path().filename().string();
                              bool ta = fa.rfind("tinexus-", 0) == 0;
                              bool tb = fb.rfind("tinexus-", 0) == 0;
                              if (ta != tb) return ta;
                              return fa < fb;
                          });

                for (const auto& entry : dir_entries) {
                    if (!entry.is_regular_file(ec) || entry.path().extension() != ".desktop") continue;
                    const std::string base_id = entry.path().filename().string();
                    if (is_hidden_desktop_id(base_id)) continue;

                    std::error_code canon_ec;
                    fs::path canon = fs::canonical(entry.path(), canon_ec);
                    std::string canon_str = canon_ec ? entry.path().string() : canon.string();
                    if (!seen_canonical_paths.insert(canon_str).second) continue;

                    std::ifstream f(entry.path());
                    if (!f.is_open()) continue;

                    std::string line;
                    std::string name;
                    std::string exec;
                    std::string try_exec;
                    std::string icon;
                    std::string comment;
                    std::string type;
                    std::string only_show_in;
                    std::string not_show_in;
                    bool no_display = false;
                    bool hidden = false;
                    bool is_desktop_entry = false;

                    while (std::getline(f, line)) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        if (line == "[Desktop Entry]") { is_desktop_entry = true; continue; }
                        if (!line.empty() && line[0] == '[' && is_desktop_entry) break;
                        if (!is_desktop_entry) continue;

                        if (line.rfind("Name=", 0) == 0 && name.empty()) {
                            name = line.substr(5);
                        } else if (line.rfind("Exec=", 0) == 0 && exec.empty()) {
                            exec = line.substr(5);
                        } else if (line.rfind("TryExec=", 0) == 0 && try_exec.empty()) {
                            try_exec = line.substr(8);
                        } else if (line.rfind("Icon=", 0) == 0 && icon.empty()) {
                            icon = line.substr(5);
                        } else if (line.rfind("Comment=", 0) == 0 && comment.empty()) {
                            comment = line.substr(8);
                        } else if (line.rfind("Type=", 0) == 0 && type.empty()) {
                            type = line.substr(5);
                        } else if (line.rfind("OnlyShowIn=", 0) == 0 && only_show_in.empty()) {
                            only_show_in = line.substr(11);
                        } else if (line.rfind("NotShowIn=", 0) == 0 && not_show_in.empty()) {
                            not_show_in = line.substr(10);
                        } else if (line == "NoDisplay=true" || line == "NoDisplay=1") {
                            no_display = true;
                        } else if (line == "Hidden=true" || line == "Hidden=1") {
                            hidden = true;
                        }
                    }

                    if (no_display || hidden || name.empty() || exec.empty()) continue;
                    if (!type.empty() && type != "Application") continue;
                    if (!only_show_in.empty() && !list_contains_ci(only_show_in, "Tinexus")) continue;
                    if (!not_show_in.empty() && list_contains_ci(not_show_in, "Tinexus")) continue;
                    if (!try_exec.empty() && !is_executable_available(try_exec)) continue;

                    size_t pct = exec.find('%');
                    while (pct != std::string::npos) {
                        if (pct + 1 < exec.size()) {
                            exec.erase(pct, 2);
                        } else {
                            exec.erase(pct, 1);
                        }
                        pct = exec.find('%');
                    }
                    while (!exec.empty() && exec.back() == ' ') exec.pop_back();

                    const std::string name_low = to_lower(name);
                    const std::string exec_key = normalize_exec_key(exec);
                    const bool is_tinexus_entry = (base_id.rfind("tinexus-", 0) == 0);

                    bool dup = false;
                    for (auto& existing : scanned) {
                        if (to_lower(existing.name) == name_low ||
                            (!exec_key.empty() && existing.exec_key == exec_key)) {
                            if (is_tinexus_entry && !existing.is_tinexus) {
                                existing = {name, exec, exec_key, icon, comment, true};
                            }
                            dup = true;
                            break;
                        }
                    }
                    if (dup) continue;

                    scanned.push_back({std::move(name), std::move(exec), std::move(exec_key),
                                       std::move(icon), std::move(comment), is_tinexus_entry});
                }
            } catch (...) {}
        }

        bool has_firefox = false;
        for (const auto& existing : scanned) {
            if (to_lower(existing.name).find("firefox") != std::string::npos ||
                existing.exec_key == "firefox") {
                has_firefox = true;
                break;
            }
        }
        std::error_code ec;
        if (!has_firefox && (fs::exists("/usr/bin/firefox", ec) || fs::exists("/usr/share/applications/firefox.desktop", ec))) {
            scanned.push_back({
                "Firefox",
                "env MOZ_ENABLE_WAYLAND=1 firefox",
                "firefox",
                "firefox",
                "Mozilla Firefox Web Browser",
                false
            });
        }

        if (!self) {
            scanningFlag->store(false);
            return;
        }

        QMetaObject::invokeMethod(self, [self, scanningFlag, scanned = std::move(scanned)]() {
            scanningFlag->store(false);
            if (!self) return;

            QVariantList newList;
            newList.reserve(static_cast<int>(scanned.size()));
            for (const auto& e : scanned) {
                QVariantMap item;
                item[QStringLiteral("name")] = QString::fromStdString(e.name);
                item[QStringLiteral("exec")] = QString::fromStdString(e.exec);
                item[QStringLiteral("icon")] = QString::fromStdString(e.icon);
                item[QStringLiteral("comment")] = QString::fromStdString(e.comment);
                newList.append(item);
            }

            std::sort(newList.begin(), newList.end(), [](const QVariant& a, const QVariant& b) {
                return a.toMap().value(QStringLiteral("name")).toString().toLower() <
                       b.toMap().value(QStringLiteral("name")).toString().toLower();
            });

            self->m_applicationsList = std::move(newList);
            emit self->applicationsListChanged();
        }, Qt::QueuedConnection);
    }).detach();
}

void ShellBridge::pollBattery() {
    if (!fs::exists("/sys/class/power_supply")) {
        if (m_batteryPercent != -1) {
            m_batteryPercent = -1;
            emit batteryChanged();
        }
        return;
    }

    double total_cap = 0.0;
    int battery_count = 0;

    try {
        for (const auto& entry : fs::directory_iterator("/sys/class/power_supply")) {
            if (!entry.is_directory()) continue;
            std::string type;
            fs::path type_file = entry.path() / "type";
            if (fs::exists(type_file)) {
                std::ifstream tf(type_file);
                tf >> type;
            }

            std::string name = entry.path().filename().string();
            bool is_battery = (type == "Battery" || name.rfind("BAT", 0) == 0 || name.find("battery") != std::string::npos);
            if (!is_battery) continue;

            fs::path present_file = entry.path() / "present";
            if (fs::exists(present_file)) {
                std::ifstream pf(present_file);
                int present = 1;
                if (pf >> present && present == 0) continue;
            }

            fs::path cap_file = entry.path() / "capacity";
            if (fs::exists(cap_file)) {
                std::ifstream cf(cap_file);
                int cap = -1;
                if (cf >> cap && cap >= 0 && cap <= 100) {
                    total_cap += cap;
                    battery_count++;
                }
            }
        }
    } catch (...) {}

    int new_pct = (battery_count > 0) ? static_cast<int>(total_cap / battery_count) : -1;
    if (m_batteryPercent != new_pct) {
        m_batteryPercent = new_pct;
        emit batteryChanged();
    }
}

void ShellBridge::syncAudioState() {
    if (m_draggingVolume || m_audioWriting->load() || m_audioPolling->exchange(true)) {
        return;
    }

    QPointer<ShellBridge> self(this);
    auto pollingFlag = m_audioPolling;

    std::thread([self, pollingFlag]() {
        int cur_vol = hardware::AudioUtils::get_volume_percent();
        bool cur_muted = hardware::AudioUtils::is_muted() || (cur_vol == 0);
        QString dev_name = QStringLiteral("Audio Output");
        auto devs = hardware::AudioUtils::get_output_devices();
        int idx = hardware::AudioUtils::get_current_output_device_index();
        if (!devs.empty() && idx >= 0 && idx < static_cast<int>(devs.size())) {
            const auto& d = devs[static_cast<size_t>(idx)];
            if (!d.empty() && d != "No Audio Output Devices Available") {
                dev_name = QString::fromStdString(d);
            }
        }
        if (!self) {
            pollingFlag->store(false);
            return;
        }
        QMetaObject::invokeMethod(self, [self, pollingFlag, cur_vol, cur_muted, dev_name]() {
            pollingFlag->store(false);
            if (!self || self->m_draggingVolume) return;
            if (self->m_volume != cur_vol || self->m_soundMuted != cur_muted || self->m_outputDeviceName != dev_name) {
                self->m_volume = cur_vol;
                self->m_soundMuted = cur_muted;
                self->m_outputDeviceName = dev_name;
                emit self->volumeChanged();
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void ShellBridge::updateClock() {
    const QDateTime now = QDateTime::currentDateTime();
    const QString newTime = now.toString(QStringLiteral("h:mm AP"));
    const QString newDate = now.toString(QStringLiteral("ddd, MMM d"));

    if (newTime != m_currentTime || newDate != m_currentDate) {
        m_currentTime = newTime;
        m_currentDate = newDate;
        emit timeChanged();
    }
}

void ShellBridge::setVolume(int vol) {
    vol = std::clamp(vol, 0, 100);
    const bool newMuted = (vol == 0);
    if (m_volume != vol || m_soundMuted != newMuted) {
        m_volume = vol;
        m_soundMuted = newMuted;
        emit volumeChanged();
    }
    m_pendingVolume = vol;
    if (!m_volumeApplyTimer.isActive()) {
        m_volumeApplyTimer.start(40);
    }
}

void ShellBridge::setSoundMuted(bool muted) {
    if (m_soundMuted != muted) {
        m_soundMuted = muted;
        emit volumeChanged();
    }
    QPointer<ShellBridge> self(this);
    std::thread([self, muted]() {
        if (hardware::AudioUtils::is_muted() != muted) {
            hardware::AudioUtils::toggle_mute(/*persist=*/true);
        }
        const bool actualMuted = hardware::AudioUtils::is_muted();
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, actualMuted]() {
            if (!self) return;
            if (self->m_soundMuted != actualMuted) {
                self->m_soundMuted = actualMuted;
                emit self->volumeChanged();
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void ShellBridge::setBrightness(int bri) {
    bri = std::clamp(bri, 5, 100);
    if (m_brightness != bri) {
        m_brightness = bri;
        emit brightnessChanged();
    }
    m_pendingBrightness = bri;
    if (!m_brightnessApplyTimer.isActive()) {
        m_brightnessApplyTimer.start(40);
    }
}

void ShellBridge::beginVolumeDrag() {
    m_draggingVolume = true;
}

void ShellBridge::endVolumeDrag() {
    m_draggingVolume = false;
    const int targetVol = m_volume;
    QPointer<ShellBridge> self(this);
    std::thread([self, targetVol]() {
        hardware::AudioUtils::set_volume_percent(targetVol, /*persist=*/true, /*throttle=*/false);
    }).detach();
}

void ShellBridge::beginBrightnessDrag() {
    m_draggingBrightness = true;
}

void ShellBridge::endBrightnessDrag() {
    m_draggingBrightness = false;
    const int targetBri = m_brightness;
    QPointer<ShellBridge> self(this);
    std::thread([self, targetBri]() {
        hardware::BacklightUtils::set_brightness_percent(targetBri, /*persist=*/true, /*throttle=*/false);
    }).detach();
}

void ShellBridge::setActiveAppName(const QString& name) {
    if (m_activeAppName != name) {
        m_activeAppName = name;
        emit activeAppChanged();
    }
}

void ShellBridge::closeAllFlyouts() {
    bool changed = m_logoMenuOpen || m_appMenuOpen || m_calendarOpen || m_notificationsOpen ||
                   m_volumeFlyoutOpen || m_brightnessFlyoutOpen || m_wifiFlyoutOpen ||
                   m_rebootConfirmationOpen || m_shutdownConfirmationOpen;

    if (m_brightnessFlyoutOpen) {
        const int targetBri = m_brightness;
        std::thread([targetBri]() {
            hardware::BacklightUtils::set_brightness_percent(targetBri, /*persist=*/true, /*throttle=*/false);
        }).detach();
    }

    m_logoMenuOpen             = false;
    m_appMenuOpen              = false;
    m_calendarOpen             = false;
    m_notificationsOpen        = false;
    m_volumeFlyoutOpen         = false;
    m_brightnessFlyoutOpen     = false;
    m_wifiFlyoutOpen           = false;
    m_rebootConfirmationOpen   = false;
    m_shutdownConfirmationOpen = false;

    if (changed) {
        emit flyoutStateChanged();
    }
}

void ShellBridge::setLogoMenuOpen(bool open) {
    if (open) closeAllFlyouts();
    m_logoMenuOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setAppMenuOpen(bool open) {
    if (open) closeAllFlyouts();
    m_appMenuOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setCalendarOpen(bool open) {
    if (open) closeAllFlyouts();
    m_calendarOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setNotificationsOpen(bool open) {
    if (open) closeAllFlyouts();
    m_notificationsOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setVolumeFlyoutOpen(bool open) {
    if (open) {
        closeAllFlyouts();
        syncAudioState();
    }
    m_volumeFlyoutOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setBrightnessFlyoutOpen(bool open) {
    if (open) closeAllFlyouts();
    m_brightnessFlyoutOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setWifiFlyoutOpen(bool open) {
    if (open) {
        closeAllFlyouts();
        syncWifiState(true);
    }
    m_wifiFlyoutOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setRebootConfirmationOpen(bool open) {
    if (open) closeAllFlyouts();
    m_rebootConfirmationOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setShutdownConfirmationOpen(bool open) {
    if (open) closeAllFlyouts();
    m_shutdownConfirmationOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::toggleLogoMenu() {
    bool next = !m_logoMenuOpen;
    closeAllFlyouts();
    m_logoMenuOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleAppMenu() {
    bool next = !m_appMenuOpen;
    closeAllFlyouts();
    m_appMenuOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleCalendar() {
    bool next = !m_calendarOpen;
    closeAllFlyouts();
    m_calendarOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleNotifications() {
    bool next = !m_notificationsOpen;
    closeAllFlyouts();
    m_notificationsOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleVolume() {
    bool next = !m_volumeFlyoutOpen;
    closeAllFlyouts();
    if (next) {
        syncAudioState();
    }
    m_volumeFlyoutOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleBrightness() {
    bool next = !m_brightnessFlyoutOpen;
    closeAllFlyouts();
    m_brightnessFlyoutOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleWifiFlyout() {
    bool next = !m_wifiFlyoutOpen;
    closeAllFlyouts();
    if (next) {
        syncWifiState(true);
    }
    m_wifiFlyoutOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::syncWifiState(bool activeRescan) {
    if (m_wifiPolling->exchange(true)) return;
    if (activeRescan && !m_wifiScanning) {
        m_wifiScanning = true;
        emit wifiChanged();
    }

    QPointer<ShellBridge> self(this);
    auto pollingFlag = m_wifiPolling;

    std::thread([self, pollingFlag, activeRescan]() {
        int rc = 1;
        std::string gen = run_shell_nmcli({"-t", "-f", "RUNNING", "general"}, 1200, &rc);
        if (rc != 0 || gen.find("running") == std::string::npos) {
            if (self) {
                QMetaObject::invokeMethod(self, [self, pollingFlag]() {
                    pollingFlag->store(false);
                    if (!self) return;
                    self->m_wifiScanning = false;
                    self->m_wifiStatusMessage = QStringLiteral("NetworkManager unavailable");
                    emit self->wifiChanged();
                }, Qt::QueuedConnection);
            } else {
                pollingFlag->store(false);
            }
            return;
        }

        std::string radio = trim_shell_str(run_shell_nmcli({"radio", "wifi"}, 1200, &rc));
        bool enabled = (radio != "disabled");

        // Check if any ethernet/wifi device is connected
        bool anyConnected = false;
        int bestBars = 0;
        std::string dev_raw = run_shell_nmcli({"-t", "-f", "TYPE,STATE", "dev"}, 1200, &rc);
        if (rc == 0) {
            std::istringstream diss(dev_raw);
            std::string dline;
            while (std::getline(diss, dline)) {
                auto cols = split_nmcli_line(dline);
                if (cols.size() >= 2 && cols[1] == "connected") {
                    anyConnected = true;
                    if (cols[0] == "ethernet") bestBars = std::max(bestBars, 4);
                }
            }
        }

        QVariantList nets;
        QString activeSsid;
        QString ipAddr;
        QString statusMsg;

        if (!enabled) {
            statusMsg = QStringLiteral("Wi-Fi is turned off");
        } else {
            std::vector<std::string> listArgs = {"-t", "-f", "ACTIVE,SSID,BSSID,SIGNAL,SECURITY", "dev", "wifi", "list"};
            if (activeRescan) {
                listArgs.push_back("--rescan");
                listArgs.push_back("yes");
            }
            std::string raw = run_shell_nmcli(listArgs, activeRescan ? 6500 : 2500, &rc);
            if (rc == 0) {
                struct NetTmp {
                    QString ssid;
                    QString bssid;
                    int signal{0};
                    int bars{1};
                    bool secured{false};
                    QString security;
                    bool connected{false};
                };
                std::vector<NetTmp> parsed;
                std::istringstream iss(raw);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.empty()) continue;
                    auto cols = split_nmcli_line(line);
                    if (cols.size() < 5) continue;
                    const bool isAct = (cols[0] == "yes");
                    const std::string ssid = cols[1];
                    if (ssid.empty()) continue;
                    int sig = 50;
                    try { sig = std::clamp(std::stoi(cols[3]), 0, 100); } catch (...) {}
                    int bars = (sig >= 75) ? 4 : (sig >= 50) ? 3 : (sig >= 25) ? 2 : 1;
                    QString sec = QString::fromStdString((cols[4].empty() || cols[4] == "--") ? "Open" : cols[4]);
                    bool isSec = (sec != QStringLiteral("Open"));
                    QString qSsid = QString::fromStdString(ssid);

                    if (isAct) {
                        activeSsid = qSsid;
                        anyConnected = true;
                        bestBars = std::max(bestBars, bars);
                    }

                    auto it = std::find_if(parsed.begin(), parsed.end(), [&](const NetTmp& n) { return n.ssid == qSsid; });
                    if (it == parsed.end()) {
                        parsed.push_back({qSsid, QString::fromStdString(cols[2]), sig, bars, isSec, sec, isAct});
                    } else if (isAct || (!it->connected && sig > it->signal)) {
                        it->bssid = QString::fromStdString(cols[2]);
                        it->signal = sig;
                        it->bars = bars;
                        it->secured = isSec;
                        it->security = sec;
                        it->connected = it->connected || isAct;
                    }
                }
                std::sort(parsed.begin(), parsed.end(), [](const NetTmp& a, const NetTmp& b) {
                    if (a.connected != b.connected) return a.connected > b.connected;
                    return a.signal > b.signal;
                });
                for (const auto& n : parsed) {
                    QVariantMap m;
                    m[QStringLiteral("ssid")] = n.ssid;
                    m[QStringLiteral("bssid")] = n.bssid;
                    m[QStringLiteral("signal")] = n.signal;
                    m[QStringLiteral("bars")] = n.bars;
                    m[QStringLiteral("secured")] = n.secured;
                    m[QStringLiteral("security")] = n.security;
                    m[QStringLiteral("connected")] = n.connected;
                    nets.append(m);
                }
            }

            if (!activeSsid.isEmpty()) {
                int drc = 1;
                std::string ifaces = run_shell_nmcli({"-t", "-f", "DEVICE,TYPE,STATE", "dev"}, 1200, &drc);
                std::istringstream iss(ifaces);
                std::string line;
                while (std::getline(iss, line)) {
                    auto cols = split_nmcli_line(line);
                    if (cols.size() >= 3 && cols[1] == "wifi" && cols[2] == "connected") {
                        std::string ip_raw = run_shell_nmcli({"-t", "-f", "IP4.ADDRESS", "dev", "show", cols[0]}, 1200, &drc);
                        auto col = ip_raw.find(':');
                        if (col != std::string::npos) {
                            std::string ip = trim_shell_str(ip_raw.substr(col + 1));
                            auto slash = ip.find('/');
                            if (slash != std::string::npos) ip = ip.substr(0, slash);
                            ipAddr = QString::fromStdString(ip);
                        }
                        break;
                    }
                }
                statusMsg = ipAddr.isEmpty() ? QStringLiteral("Connected") : (QStringLiteral("Connected • ") + ipAddr);
            } else {
                statusMsg = nets.isEmpty() ? QStringLiteral("No Wi-Fi networks found") : QStringLiteral("Not connected");
            }
        }

        if (!self) {
            pollingFlag->store(false);
            return;
        }
        QMetaObject::invokeMethod(self, [self, pollingFlag, enabled, anyConnected, bestBars, activeSsid, ipAddr, statusMsg, nets]() {
            pollingFlag->store(false);
            if (!self) return;
            self->m_wifiScanning = false;
            self->m_wifiEnabled = enabled;
            if (self->m_networkConnected != anyConnected || self->m_networkBars != bestBars) {
                self->m_networkConnected = anyConnected;
                self->m_networkBars = bestBars;
                emit self->networkChanged();
            }
            self->m_connectedSsid = activeSsid;
            self->m_wifiIpAddress = ipAddr;
            self->m_wifiStatusMessage = statusMsg;
            self->m_wifiNetworks = nets;
            emit self->wifiChanged();
        }, Qt::QueuedConnection);
    }).detach();
}

void ShellBridge::setWifiEnabled(bool enabled) {
    if (m_wifiEnabled != enabled) {
        m_wifiEnabled = enabled;
        m_wifiStatusMessage = enabled ? QStringLiteral("Turning Wi-Fi on...") : QStringLiteral("Wi-Fi is turned off");
        if (!enabled) {
            m_connectedSsid.clear();
            m_wifiNetworks.clear();
        }
        emit wifiChanged();
    }
    QPointer<ShellBridge> self(this);
    std::thread([self, enabled]() {
        int rc = 1;
        run_shell_nmcli({"radio", "wifi", enabled ? "on" : "off"}, 2500, &rc);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, enabled]() {
            if (!self) return;
            self->syncWifiState(enabled);
        }, Qt::QueuedConnection);
    }).detach();
}

void ShellBridge::scanWifiNetworks() {
    syncWifiState(true);
}

void ShellBridge::connectWifi(const QString& ssid, const QString& password) {
    if (ssid.isEmpty() || m_wifiConnectingFlag->exchange(true)) return;
    m_wifiConnecting = true;
    m_connectingSsid = ssid;
    m_wifiStatusMessage = QStringLiteral("Connecting to ") + ssid + QStringLiteral("...");
    emit wifiChanged();

    QPointer<ShellBridge> self(this);
    auto connFlag = m_wifiConnectingFlag;
    std::string s_ssid = ssid.toStdString();
    std::string s_pw = password.toStdString();

    std::thread([self, connFlag, s_ssid, s_pw]() {
        int rc = 1;
        std::vector<std::string> args = {"dev", "wifi", "connect", s_ssid};
        if (!s_pw.empty()) {
            args.push_back("password");
            args.push_back(s_pw);
        }
        std::string res = run_shell_nmcli(args, 15000, &rc);
        if (rc != 0 && !s_pw.empty()) {
            (void)run_shell_nmcli({"con", "delete", "id", s_ssid}, 2500, nullptr);
            res = run_shell_nmcli(args, 15000, &rc);
        }
        QString errMsg = QString::fromStdString(trim_shell_str(res));
        if (!self) {
            connFlag->store(false);
            return;
        }
        QMetaObject::invokeMethod(self, [self, connFlag, rc, errMsg]() {
            connFlag->store(false);
            if (!self) return;
            self->m_wifiConnecting = false;
            self->m_connectingSsid.clear();
            if (rc != 0) {
                self->m_wifiStatusMessage = errMsg.isEmpty() ? QStringLiteral("Connection failed") : errMsg;
            }
            emit self->wifiChanged();
            self->syncWifiState(false);
        }, Qt::QueuedConnection);
    }).detach();
}

void ShellBridge::disconnectWifi() {
    QPointer<ShellBridge> self(this);
    std::thread([self]() {
        int rc = 1;
        std::string ifaces = run_shell_nmcli({"-t", "-f", "DEVICE,TYPE,STATE", "dev"}, 1200, &rc);
        std::istringstream iss(ifaces);
        std::string line;
        while (std::getline(iss, line)) {
            auto cols = split_nmcli_line(line);
            if (cols.size() >= 3 && cols[1] == "wifi" && cols[2] == "connected") {
                run_shell_nmcli({"dev", "disconnect", cols[0]}, 3000, &rc);
                break;
            }
        }
        if (!self) return;
        QMetaObject::invokeMethod(self, [self]() {
            if (!self) return;
            self->m_connectedSsid.clear();
            self->m_wifiIpAddress.clear();
            self->m_wifiStatusMessage = QStringLiteral("Disconnected");
            emit self->wifiChanged();
            self->syncWifiState(false);
        }, Qt::QueuedConnection);
    }).detach();
}

void ShellBridge::clearNotifications() {
    m_notifications.clear();
    emit notificationsChanged();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("ClearAll")
    );
    QDBusConnection::sessionBus().asyncCall(msg);
}

void ShellBridge::launchApp(const QString& execCmd) {
    tinexus::log::info("[Shell] launchApp: {}", execCmd.toStdString());
    QStringList parts = QProcess::splitCommand(execCmd);
    if (parts.isEmpty()) return;
    QString prog = parts.takeFirst();

    // Resolve full path if binary is in /usr/bin or /usr/local/bin
    if (!prog.startsWith('/')) {
        if (QFile::exists(QStringLiteral("/usr/bin/") + prog)) {
            prog = QStringLiteral("/usr/bin/") + prog;
        } else if (QFile::exists(QStringLiteral("/usr/local/bin/") + prog)) {
            prog = QStringLiteral("/usr/local/bin/") + prog;
        }
    }

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"));
    env.remove(QStringLiteral("QSG_RHI_BACKEND"));
    if (!fs::exists("/run/tinexus/safe_graphics_mode")) {
        env.remove(QStringLiteral("LIBGL_ALWAYS_SOFTWARE"));
    }
    QString pathEnv = env.value(QStringLiteral("PATH"));
    if (pathEnv.isEmpty()) {
        env.insert(QStringLiteral("PATH"), QStringLiteral("/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"));
    }
    proc.setProcessEnvironment(env);
    proc.setProgram(prog);
    proc.setArguments(parts);
    bool started = proc.startDetached();
    tinexus::log::info("[Shell] App '{}' startDetached result: {}", prog.toStdString(), started);
}

void ShellBridge::openWifiSettings() {
    tinexus::log::info("[Shell] Opening Network Settings in tinexus-settings-ui");
    launchApp(QStringLiteral("tinexus-settings-ui --page wifi"));
    closeAllFlyouts();
}

void ShellBridge::onNotchCenterClicked() {
    tinexus::log::info("[Shell] Notch center clicked -> toggling launcher");
    QLocalSocket socket;
    socket.connectToServer(QStringLiteral("tinexus-launcher-single"));
    if (socket.waitForConnected(250)) {
        socket.write("toggle\n");
        socket.waitForBytesWritten(250);
        socket.flush();
    } else {
        launchApp(QStringLiteral("tinexus-launcher"));
    }
}

void ShellBridge::requestReboot() {
    closeAllFlyouts();
    m_rebootConfirmationOpen = true;
    emit flyoutStateChanged();
}

void ShellBridge::requestShutdown() {
    closeAllFlyouts();
    m_shutdownConfirmationOpen = true;
    emit flyoutStateChanged();
}

void ShellBridge::powerReboot() {
    tinexus::log::info("[Shell] Executing system Reboot");
    closeAllFlyouts();
    bool dbusSuccess = false;
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"),
                          QDBusConnection::systemBus());
    if (login1.isValid()) {
        QDBusMessage reply = login1.call(QStringLiteral("Reboot"), false);
        if (reply.type() != QDBusMessage::ErrorMessage) {
            dbusSuccess = true;
        } else {
            tinexus::log::warn("[Shell] logind Reboot D-Bus call returned error: {}",
                              reply.errorMessage().toStdString());
        }
    }
    if (!dbusSuccess) {
        tinexus::log::info("[Shell] Falling back to systemctl reboot");
        bool ok = QProcess::startDetached(QStringLiteral("sudo"), {QStringLiteral("/bin/systemctl"), QStringLiteral("reboot")});
        if (!ok) {
            QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("reboot")});
        }
    }
}

void ShellBridge::powerShutdown() {
    tinexus::log::info("[Shell] Executing system PowerOff");
    closeAllFlyouts();
    bool dbusSuccess = false;
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"),
                          QDBusConnection::systemBus());
    if (login1.isValid()) {
        QDBusMessage reply = login1.call(QStringLiteral("PowerOff"), false);
        if (reply.type() != QDBusMessage::ErrorMessage) {
            dbusSuccess = true;
        } else {
            tinexus::log::warn("[Shell] logind PowerOff D-Bus call returned error: {}",
                              reply.errorMessage().toStdString());
        }
    }
    if (!dbusSuccess) {
        tinexus::log::info("[Shell] Falling back to systemctl poweroff");
        bool ok = QProcess::startDetached(QStringLiteral("sudo"), {QStringLiteral("/bin/systemctl"), QStringLiteral("poweroff")});
        if (!ok) {
            QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("poweroff")});
        }
    }
}

void ShellBridge::powerSleep() {
    tinexus::log::info("[Shell] Executing system Suspend");
    closeAllFlyouts();
    bool dbusSuccess = false;
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"),
                          QDBusConnection::systemBus());
    if (login1.isValid()) {
        QDBusMessage reply = login1.call(QStringLiteral("Suspend"), false);
        if (reply.type() != QDBusMessage::ErrorMessage) {
            dbusSuccess = true;
        } else {
            tinexus::log::warn("[Shell] logind Suspend D-Bus call returned error: {}",
                              reply.errorMessage().toStdString());
        }
    }
    if (!dbusSuccess) {
        tinexus::log::info("[Shell] Falling back to systemctl suspend");
        bool ok = QProcess::startDetached(QStringLiteral("sudo"), {QStringLiteral("/bin/systemctl"), QStringLiteral("suspend")});
        if (!ok) {
            QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("suspend")});
        }
    }
}

void ShellBridge::requestBlur(bool enable, int radius) {
    if (m_blurEnabled == enable && m_blurRadius == radius) {
        return;
    }
    m_blurEnabled = enable;
    m_blurRadius = radius;
    tinexus::log::debug("[Shell] Surface requested compositor backdrop blur: enable={}, radius={}", enable, radius);
    syncBlurRegion(m_lastBlurRegion);
}

void ShellBridge::setNotchExpanded(bool exp) {
    if (m_notchExpanded != exp) {
        m_notchExpanded = exp;
        emit notchExpandedChanged();
    }
}

void ShellBridge::syncActiveNotifications() {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("GetActiveNotifications")
    );
    auto pending = QDBusConnection::sessionBus().asyncCall(msg);
    auto* watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* w) {
        w->deleteLater();
        QDBusMessage reply = w->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            return;
        }
        const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
        QVariantList loaded;
        arg.beginArray();
        while (!arg.atEnd()) {
            arg.beginStructure();
            quint32 id = 0;
            QString appName, appIcon, summary, body;
            qint32 expireTimeout = 0;
            arg >> id >> appName >> appIcon >> summary >> body >> expireTimeout;
            arg.endStructure();

            QVariantMap item;
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = summary.isEmpty() ? (appName.isEmpty() ? QStringLiteral("Notification") : appName) : summary;
            item[QStringLiteral("message")] = body;
            item[QStringLiteral("time")] = QStringLiteral("Active");
            item[QStringLiteral("appId")] = appName;
            item[QStringLiteral("icon")] = appIcon.isEmpty() ? QStringLiteral("dialog-information-symbolic") : appIcon;
            loaded.prepend(item);
        }
        arg.endArray();
        if (!loaded.isEmpty()) {
            m_notifications = loaded;
            emit notificationsChanged();
        }
    });
}

void ShellBridge::dismissNotification(int index) {
    if (index >= 0 && index < m_notifications.size()) {
        const QVariantMap item = m_notifications.at(index).toMap();
        const uint id = item.value(QStringLiteral("id")).toUInt();
        m_notifications.removeAt(index);
        emit notificationsChanged();

        if (id > 0) {
            QDBusMessage msg = QDBusMessage::createMethodCall(
                QStringLiteral("org.freedesktop.Notifications"),
                QStringLiteral("/org/freedesktop/Notifications"),
                QStringLiteral("org.freedesktop.Notifications"),
                QStringLiteral("CloseNotification")
            );
            msg << id;
            QDBusConnection::sessionBus().asyncCall(msg);
        }
    }
}

void ShellBridge::onNotificationClosed(uint id, uint /*reason*/) {
    if (id == 0) return;
    for (int i = 0; i < m_notifications.size(); ++i) {
        if (m_notifications.at(i).toMap().value(QStringLiteral("id")).toUInt() == id) {
            m_notifications.removeAt(i);
            emit notificationsChanged();
            break;
        }
    }
}

void ShellBridge::setToastVisible(bool visible) {
    if (m_toastVisible != visible) {
        m_toastVisible = visible;
        if (!visible) {
            m_toastTimer.stop();
        }
        emit toastVisibleChanged();
    }
}

void ShellBridge::dismissToast() {
    setToastVisible(false);
}

void ShellBridge::onNotificationAdded(uint id, const QString& appName, const QString& appIcon,
                                     const QString& summary, const QString& body, int expireTimeout) {
    tinexus::log::info("[Shell] Received NotificationAdded: id={}, app='{}', summary='{}'",
                       id, appName.toStdString(), summary.toStdString());

    QVariantMap item;
    item[QStringLiteral("id")] = id;
    item[QStringLiteral("title")] = summary.isEmpty() ? (appName.isEmpty() ? QStringLiteral("Notification") : appName) : summary;
    item[QStringLiteral("message")] = body;
    item[QStringLiteral("time")] = QDateTime::currentDateTime().toString(QStringLiteral("h:mm AP"));
    item[QStringLiteral("appId")] = appName;
    item[QStringLiteral("icon")] = appIcon.isEmpty() ? QStringLiteral("dialog-information-symbolic") : appIcon;

    // Replace existing notification with same ID if replaces_id was used
    for (int i = 0; i < m_notifications.size(); ++i) {
        if (m_notifications.at(i).toMap().value(QStringLiteral("id")).toUInt() == id) {
            m_notifications.removeAt(i);
            break;
        }
    }

    m_notifications.prepend(item);
    emit notificationsChanged();

    // Show popup toast
    m_toastTitle = item[QStringLiteral("title")].toString();
    m_toastMessage = body;
    m_toastIcon = item[QStringLiteral("icon")].toString();
    emit toastContentChanged();

    setToastVisible(true);

    int duration = (expireTimeout > 0) ? expireTimeout : 5000;
    m_toastTimer.start(duration);
}

void ShellBridge::toggleMediaPlayback() {
    m_mediaPlaying = !m_mediaPlaying;
    tinexus::log::info("[Shell] Media playback toggled: playing={}", m_mediaPlaying);
    emit mediaStateChanged();
}

void ShellBridge::nextMediaTrack() {
    m_mediaTitle = QStringLiteral("Solaris Echo");
    m_mediaArtist = QStringLiteral("Tinexus Synthwave");
    m_mediaProgress = 0.15;
    m_mediaTimeStr = QStringLiteral("0:32 / 4:10");
    m_mediaPlaying = true;
    tinexus::log::info("[Shell] Next media track: {}", m_mediaTitle.toStdString());
    emit mediaStateChanged();
}

void ShellBridge::prevMediaTrack() {
    m_mediaTitle = QStringLiteral("Neon Horizon");
    m_mediaArtist = QStringLiteral("Aura Collective");
    m_mediaProgress = 0.85;
    m_mediaTimeStr = QStringLiteral("3:15 / 3:50");
    m_mediaPlaying = true;
    tinexus::log::info("[Shell] Previous media track: {}", m_mediaTitle.toStdString());
    emit mediaStateChanged();
}

void ShellBridge::setupIpcdClient() {
    if (m_ipcSocket) return;
    m_ipcSocket = new QLocalSocket(this);
    m_ipcSocket->setReadBufferSize(32768);
    connect(m_ipcSocket, &QLocalSocket::connected, this, &ShellBridge::onIpcdConnected);
    connect(m_ipcSocket, &QLocalSocket::readyRead, this, &ShellBridge::onIpcdReadyRead);

    auto scheduleReconnect = [this]() {
        if (!m_ipcReconnectTimer.isActive()) {
            m_ipcReconnectTimer.start(m_ipcBackoffMs);
            m_ipcBackoffMs = std::min(m_ipcBackoffMs * 2, 30000);
        }
    };
    connect(m_ipcSocket, &QLocalSocket::disconnected, this, [this, scheduleReconnect]() {
        m_ipcReadBuf.clear();
        scheduleReconnect();
    });
    connect(m_ipcSocket, &QLocalSocket::errorOccurred, this, [scheduleReconnect](QLocalSocket::LocalSocketError) {
        scheduleReconnect();
    });

    m_ipcBackoffMs = 500;
    m_ipcReconnectTimer.setInterval(m_ipcBackoffMs);
    m_ipcReconnectTimer.setSingleShot(true);
    connect(&m_ipcReconnectTimer, &QTimer::timeout, this, &ShellBridge::tryConnectIpcd);
    tryConnectIpcd();
}

void ShellBridge::tryConnectIpcd() {
    if (!m_ipcSocket) return;
    if (m_ipcSocket->state() == QLocalSocket::ConnectedState ||
        m_ipcSocket->state() == QLocalSocket::ConnectingState) {
        return;
    }
    const QString sockPath = QString::fromStdString(tinexus::common::RuntimePaths::get_ipc_socket_path());
    m_ipcSocket->connectToServer(sockPath);
}

void ShellBridge::onIpcdConnected() {
    m_ipcReconnectTimer.stop();
    m_ipcBackoffMs = 500;
    m_ipcReadBuf.clear();

    const uint16_t topic = static_cast<uint16_t>(tinexus::ipcd::protocol::DisplayMessageType::WORKSPACE_STATE_CHANGED);
    sendIpcFrame(static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_SUBSCRIBE_TOPIC),
                 &topic, sizeof(topic));
    tinexus::log::info("[Shell] Connected to ipcd socket for Spaces telemetry");
}

void ShellBridge::onIpcdReadyRead() {
    if (!m_ipcSocket) return;
    m_ipcReadBuf.append(m_ipcSocket->readAll());

    constexpr qsizetype kHdrSize = static_cast<qsizetype>(sizeof(tinexus::ipcd::protocol::Header));
    while (m_ipcReadBuf.size() >= kHdrSize) {
        tinexus::ipcd::protocol::Header hdr{};
        std::memcpy(&hdr, m_ipcReadBuf.constData(), sizeof(hdr));
        if (hdr.magic != tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC ||
            hdr.version != tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1 ||
            hdr.payload_len > 65536) {
            m_ipcReadBuf.remove(0, 1);
            continue;
        }
        const qsizetype frameSize = kHdrSize + static_cast<qsizetype>(hdr.payload_len);
        if (m_ipcReadBuf.size() < frameSize) break;

        if (hdr.msg_type == static_cast<uint16_t>(tinexus::ipcd::protocol::DisplayMessageType::WORKSPACE_STATE_CHANGED) &&
            hdr.payload_len >= sizeof(tinexus::ipcd::protocol::WorkspaceStatePayload)) {
            tinexus::ipcd::protocol::WorkspaceStatePayload pld{};
            std::memcpy(&pld, m_ipcReadBuf.constData() + kHdrSize, sizeof(pld));
            char safeLabel[65]{};
            std::memcpy(safeLabel, pld.label, sizeof(pld.label));
            const QString label = (safeLabel[0] != '\0')
                ? QString::fromUtf8(safeLabel)
                : (pld.is_fullscreen ? QStringLiteral("Fullscreen") : QStringLiteral("Desktop %1").arg(pld.active_id));

            const bool fs = (pld.is_fullscreen != 0);
            if (m_activeWorkspaceId != static_cast<int>(pld.active_id) ||
                m_activeWorkspaceSlot != static_cast<int>(pld.active_slot) ||
                m_activeSpacesCount != static_cast<int>(pld.active_spaces) ||
                m_activeSpaceFullscreen != fs ||
                m_activeSpaceLabel != label) {
                m_activeWorkspaceId = static_cast<int>(pld.active_id);
                m_activeWorkspaceSlot = static_cast<int>(pld.active_slot);
                m_activeSpacesCount = static_cast<int>(pld.active_spaces);
                m_activeSpaceFullscreen = fs;
                m_activeSpaceLabel = label;
                emit workspaceStateChanged();
            }
        }
        m_ipcReadBuf.remove(0, frameSize);
    }
}

void ShellBridge::sendIpcFrame(uint16_t msgType, const void* payload, uint32_t payloadLen) {
    if (!m_ipcSocket || m_ipcSocket->state() != QLocalSocket::ConnectedState) return;
    tinexus::ipcd::protocol::Header hdr{};
    hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
    hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
    hdr.msg_type = msgType;
    hdr.sequence_id = ++m_ipcSeq;
    hdr.payload_len = payloadLen;

    QByteArray frame;
    frame.resize(static_cast<qsizetype>(sizeof(hdr) + payloadLen));
    std::memcpy(frame.data(), &hdr, sizeof(hdr));
    if (payloadLen > 0 && payload != nullptr) {
        std::memcpy(frame.data() + sizeof(hdr), payload, payloadLen);
    }
    m_ipcSocket->write(frame);
}

void ShellBridge::switchWorkspace(int workspaceId) {
    if (workspaceId <= 0) return;
    tinexus::ipcd::protocol::WorkspaceSwitchPayload pld{};
    pld.workspace_id = static_cast<uint32_t>(workspaceId);
    sendIpcFrame(static_cast<uint16_t>(tinexus::ipcd::protocol::DisplayMessageType::WORKSPACE_SWITCH_REQUEST),
                 &pld, sizeof(pld));
}

} // namespace tinexus::shell

#include "moc_ShellBridge.cpp"
