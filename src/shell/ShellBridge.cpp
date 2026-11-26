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

#include <sys/reboot.h>
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
        // Subscribe to notification signals from tinexus-notifications daemon
        bool connected = QDBusConnection::sessionBus().connect(
            QString(),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("NotificationAdded"),
            this,
            SLOT(onNotificationAdded(uint,QString,QString,QString,QString,int))
        );
        if (!connected) {
            tinexus::log::warn("[Shell] Failed to connect to org.freedesktop.Notifications NotificationAdded signal");
        } else {
            tinexus::log::info("[Shell] Connected to org.freedesktop.Notifications NotificationAdded signal");
        }

        pollBattery();
        syncAudioState();
        scanApplications();

        QPointer<ShellBridge> self(this);
        std::thread([self]() {
            int bri = hardware::BacklightUtils::get_brightness_percent();
            if (!self) return;
            QMetaObject::invokeMethod(self, [self, bri]() {
                if (!self) return;
                if (self->m_brightness != bri) {
                    self->m_brightness = bri;
                    emit self->brightnessChanged();
                }
            }, Qt::QueuedConnection);
        }).detach();
    });

    // Populate standard notifications for desktop notification flyout
    QVariantMap n1;
    n1[QStringLiteral("title")]   = QStringLiteral("System Update");
    n1[QStringLiteral("message")] = QStringLiteral("Tinexus Platform v1.1 update ready to install.");
    n1[QStringLiteral("time")]    = QStringLiteral("10m ago");
    n1[QStringLiteral("appId")]   = QStringLiteral("tinexus-settings");
    n1[QStringLiteral("icon")]    = QStringLiteral("system-software-update-symbolic");

    QVariantMap n2;
    n2[QStringLiteral("title")]   = QStringLiteral("Power & Battery");
    n2[QStringLiteral("message")] = QStringLiteral("Fast charging enabled. Power adapter connected.");
    n2[QStringLiteral("time")]    = QStringLiteral("25m ago");
    n2[QStringLiteral("appId")]   = QStringLiteral("tinexus-monitor");
    n2[QStringLiteral("icon")]    = QStringLiteral("battery-level-100-charged-symbolic");

    QVariantMap n3;
    n3[QStringLiteral("title")]   = QStringLiteral("Sound & Media");
    n3[QStringLiteral("message")] = QStringLiteral("Audio output switched to High-Definition Speakers.");
    n3[QStringLiteral("time")]    = QStringLiteral("1h ago");
    n3[QStringLiteral("appId")]   = QStringLiteral("tinexus-media");
    n3[QStringLiteral("icon")]    = QStringLiteral("audio-volume-high-symbolic");

    m_notifications.append(n1);
    m_notifications.append(n2);
    m_notifications.append(n3);

    updateClock();
    connect(&m_clockTimer, &QTimer::timeout, this, &ShellBridge::updateClock);
    m_clockTimer.start(5000);

    connect(&m_batteryTimer, &QTimer::timeout, this, &ShellBridge::pollBattery);
    m_batteryTimer.start(30000);

    connect(&m_audioTimer, &QTimer::timeout, this, &ShellBridge::syncAudioState);
    m_audioTimer.start(8000);

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
        m_pendingVolume = -1;
        QPointer<ShellBridge> self(this);
        auto writingFlag = m_audioWriting;
        std::thread([self, writingFlag, targetVol]() {
            hardware::AudioUtils::set_volume_percent(targetVol, /*persist=*/true, /*throttle=*/false);
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
                if (self->m_volume != actualVol || self->m_soundMuted != actualMuted) {
                    self->m_volume = actualVol;
                    self->m_soundMuted = actualMuted;
                    emit self->volumeChanged();
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
    if (m_audioPolling->exchange(true)) {
        return;
    }

    QPointer<ShellBridge> self(this);
    auto pollingFlag = m_audioPolling;

    std::thread([self, pollingFlag]() {
        int cur_vol = hardware::AudioUtils::get_volume_percent();
        bool cur_muted = hardware::AudioUtils::is_muted() || (cur_vol == 0);
        if (!self) {
            pollingFlag->store(false);
            return;
        }
        QMetaObject::invokeMethod(self, [self, pollingFlag, cur_vol, cur_muted]() {
            pollingFlag->store(false);
            if (!self) return;
            if (self->m_volume != cur_vol || self->m_soundMuted != cur_muted) {
                self->m_volume = cur_vol;
                self->m_soundMuted = cur_muted;
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
    // Set hardware backlight with throttling enabled to avoid GUI stutter
    hardware::BacklightUtils::set_brightness_percent(bri, /*persist=*/false, /*throttle=*/true);
}

void ShellBridge::setActiveAppName(const QString& name) {
    if (m_activeAppName != name) {
        m_activeAppName = name;
        emit activeAppChanged();
    }
}

void ShellBridge::closeAllFlyouts() {
    bool changed = m_logoMenuOpen || m_appMenuOpen || m_calendarOpen || m_notificationsOpen ||
                   m_volumeFlyoutOpen || m_brightnessFlyoutOpen || m_rebootConfirmationOpen || m_shutdownConfirmationOpen;

    if (m_brightnessFlyoutOpen) {
        hardware::BacklightUtils::set_brightness_percent(m_brightness, /*persist=*/true, /*throttle=*/false);
    }

    m_logoMenuOpen             = false;
    m_appMenuOpen              = false;
    m_calendarOpen             = false;
    m_notificationsOpen        = false;
    m_volumeFlyoutOpen         = false;
    m_brightnessFlyoutOpen     = false;
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

void ShellBridge::clearNotifications() {
    m_notifications.clear();
    emit notificationsChanged();
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
    tinexus::log::info("[Shell] Wi-Fi icon clicked -> launching tinexus-settings-ui");
    launchApp(QStringLiteral("tinexus-settings-ui"));
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

void ShellBridge::dismissNotification(int index) {
    if (index >= 0 && index < m_notifications.size()) {
        m_notifications.removeAt(index);
        emit notificationsChanged();
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
    item[QStringLiteral("time")] = QStringLiteral("Just now");
    item[QStringLiteral("appId")] = appName;
    item[QStringLiteral("icon")] = appIcon.isEmpty() ? QStringLiteral("dialog-information-symbolic") : appIcon;

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

} // namespace tinexus::shell

#include "moc_ShellBridge.cpp"
