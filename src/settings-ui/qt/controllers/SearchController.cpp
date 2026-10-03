#include "SearchController.hpp"
#include <QVariantMap>
#include <QSet>

namespace tinexus::settings_ui {

SearchController::SearchController(QObject* parent)
    : QObject(parent)
{
    buildIndex();
}

void SearchController::buildIndex() {
    m_index = {
        // ── 0. Displays ───────────────────────────────────────────────────────
        {
            "Displays",
            "Resolution, display scaling, screen brightness, night light, VRR",
            0, "display", "#0A84FF",
            {"displays", "display", "screen", "monitor", "resolution", "scaling", "scale", "hidpi", "brightness", "backlight", "night light", "vrr", "adaptive sync", "refresh rate", "60hz", "144hz", "connector", "edp", "hdmi", "compositor", "wlroots"}
        },
        {
            "Display Scaling",
            "Adjust scale factor (100%, 125%, 150%, 200%) for high-DPI outputs",
            0, "display", "#0A84FF",
            {"scale", "scaling", "hidpi", "fractional", "zoom", "dpi", "size", "large"}
        },
        {
            "Screen Brightness",
            "Adjust hardware backlight brightness level",
            0, "display", "#0A84FF",
            {"brightness", "backlight", "dim", "light", "screen", "slider"}
        },
        {
            "Night Light",
            "Warmer screen colors to reduce eye strain at night",
            0, "display", "#0A84FF",
            {"night light", "blue light", "warm", "color temperature", "eyes"}
        },
        {
            "Variable Refresh Rate (VRR)",
            "Adaptive sync frame rate synchronization with GPU render pipeline",
            0, "display", "#0A84FF",
            {"vrr", "adaptive sync", "g-sync", "freesync", "refresh rate", "hz", "smooth", "fps"}
        },

        // ── 1. Sound ──────────────────────────────────────────────────────────
        {
            "Sound",
            "Volume, output device, mute, PipeWire & ALSA audio",
            1, "sound", "#FF2D55",
            {"sound", "audio", "volume", "mute", "speaker", "headphones", "pipewire", "alsa", "chime", "test sound", "output", "alerts"}
        },
        {
            "Output Volume",
            "Master audio output level and mute toggle",
            1, "sound", "#FF2D55",
            {"volume", "loudness", "level", "mute", "unmute", "sound"}
        },
        {
            "Audio Output Device",
            "Select active PipeWire / ALSA audio sink",
            1, "sound", "#FF2D55",
            {"output device", "speaker", "headphones", "dac", "hdmi audio", "sink", "audio device"}
        },
        {
            "Alert & Notification Sounds",
            "Play auditory feedback and sound chime",
            1, "sound", "#FF2D55",
            {"test sound", "chime", "alert", "beep", "notification sound"}
        },

        // ── 2. Appearance ─────────────────────────────────────────────────────
        {
            "Appearance",
            "Theme mode, accent colors, desktop wallpapers",
            2, "appearance", "#AF52DE",
            {"appearance", "personalization", "theme", "dark", "light", "auto", "wallpaper", "background", "accent", "color", "cyan", "blue", "violet", "pink", "orange", "green"}
        },
        {
            "Theme Mode",
            "Switch between Light, Dark, or Auto appearance",
            2, "appearance", "#AF52DE",
            {"theme", "dark mode", "light mode", "auto mode", "style", "colors"}
        },
        {
            "Accent Color",
            "Choose desktop system accent highlight color",
            2, "appearance", "#AF52DE",
            {"accent", "color", "highlight", "cyan", "blue", "violet", "pink", "orange", "green"}
        },
        {
            "Desktop Wallpaper",
            "Select desktop wallpaper background with live cross-fade",
            2, "appearance", "#AF52DE",
            {"wallpaper", "background", "image", "desktop picture", "matrix", "emerald"}
        },

        // ── 3. Wi-Fi & Network ────────────────────────────────────────────────
        {
            "Wi-Fi & Network",
            "Wireless networks, IP configuration, physical network adapters",
            3, "wifi", "#0A84FF",
            {"wifi", "wi-fi", "wireless", "network", "internet", "ssid", "ip", "address", "connection", "scan", "signal", "ethernet", "interface", "adapter", "wpa"}
        },
        {
            "Wi-Fi Networks Scan",
            "Discover and connect to nearby wireless access points",
            3, "wifi", "#0A84FF",
            {"wifi scan", "ssid", "join network", "wireless network", "connect wifi", "password"}
        },
        {
            "Physical Network Adapters",
            "Ethernet and wireless hardware interface status and IP addresses",
            3, "wifi", "#0A84FF",
            {"network adapter", "ethernet", "ip address", "wlan0", "eth0", "hardware interface"}
        },

        // ── 4. Battery & Power ────────────────────────────────────────────────
        {
            "Battery & Power",
            "Energy modes, display sleep timeout, computer sleep, lock screen",
            4, "power", "#30D158",
            {"battery", "power", "energy", "profile", "saver", "balanced", "performance", "sleep", "timeout", "screen timeout", "lock on sleep", "pam", "clipboard"}
        },
        {
            "Energy Mode Profile",
            "Power Saver, Balanced, or High Performance profile",
            4, "power", "#30D158",
            {"energy mode", "power profile", "power saver", "performance", "balanced", "battery life"}
        },
        {
            "Display & System Sleep",
            "Configure idle timeouts for display off and system sleep",
            4, "power", "#30D158",
            {"sleep", "timeout", "turn off display", "inactivity", "idle"}
        },
        {
            "Lock Screen & PAM",
            "Require password after screen turn off and local PAM authentication",
            4, "power", "#30D158",
            {"lock screen", "pam", "password", "security", "require password"}
        },
        {
            "Clipboard History",
            "Capacity limit and clear clipboard history",
            4, "power", "#30D158",
            {"clipboard", "copy paste", "history", "clipboard size", "clear clipboard"}
        },

        // ── 5. Keyboard Shortcuts ─────────────────────────────────────────────
        {
            "Keyboard Shortcuts",
            "System global hotkeys, window management, launcher shortcut",
            5, "keyboard", "#636366",
            {"keyboard", "shortcuts", "hotkeys", "keys", "ctrl+k", "command palette", "launcher", "workspace", "terminal", "files", "alt+tab", "window switcher", "snap"}
        },

        // ── 6. Privacy & Security ─────────────────────────────────────────────
        {
            "Privacy & Security",
            "Platform Guard, Ed25519 binary verification, application sandbox",
            6, "privacy", "#34C759",
            {"privacy", "security", "guard", "ed25519", "signatures", "sandbox", "confinement", "unverified", "apps", "trust", "allow", "audit", "sha256", "hash"}
        },

        // ── 7. General / About ────────────────────────────────────────────────
        {
            "General / About",
            "Processor, memory RAM, system storage, OS version, compositor info",
            7, "about", "#8E8E93",
            {"about", "system", "general", "specs", "specifications", "processor", "cpu", "memory", "ram", "storage", "disk", "ssd", "kernel", "linux", "os", "version", "graphics", "vulkan", "opengl", "license", "update"}
        },
        {
            "System Storage & Memory",
            "Real-time RAM usage and filesystem root storage capacity",
            7, "about", "#8E8E93",
            {"storage", "disk", "ram", "memory", "free space", "capacity"}
        },
        {
            "Processor & Graphics",
            "CPU model, core topology, DRM backend, and Vulkan/OpenGL engine",
            7, "about", "#8E8E93",
            {"processor", "cpu", "graphics", "gpu", "vulkan", "opengl", "compositor"}
        }
    };
}

void SearchController::setSearchQuery(const QString& query) {
    if (m_searchQuery != query) {
        m_searchQuery = query;
        performSearch();
        emit searchQueryChanged();
    }
}

void SearchController::clearSearch() {
    setSearchQuery(QString());
}

void SearchController::performSearch() {
    m_searchResults.clear();
    m_matchingPageIndices.clear();

    QString trimmed = m_searchQuery.trimmed().toLower();
    if (trimmed.isEmpty()) {
        emit searchResultsChanged();
        return;
    }

    QSet<int> matchedPages;

    for (const auto& item : m_index) {
        bool matches = false;

        if (item.title.toLower().contains(trimmed)) {
            matches = true;
        } else if (item.subtitle.toLower().contains(trimmed)) {
            matches = true;
        } else {
            for (const auto& kw : item.keywords) {
                if (kw.contains(trimmed)) {
                    matches = true;
                    break;
                }
            }
        }

        if (matches) {
            matchedPages.insert(item.pageIndex);

            QVariantMap map;
            map["title"] = item.title;
            map["subtitle"] = item.subtitle;
            map["page"] = item.pageIndex;
            map["iconId"] = item.iconId;
            map["iconColor"] = item.iconColor;
            m_searchResults.append(map);
        }
    }

    for (int pageIdx : matchedPages) {
        m_matchingPageIndices.append(pageIdx);
    }

    emit searchResultsChanged();
}

bool SearchController::isPageMatching(int pageIndex) const {
    if (!isSearching()) return true;
    return m_matchingPageIndices.contains(pageIndex);
}

} // namespace tinexus::settings_ui
