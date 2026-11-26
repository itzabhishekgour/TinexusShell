#include "DisplayController.hpp"
#include <common/BacklightUtils.hpp>
#include <common/DisplayUtils.hpp>
#include <common/RuntimePaths.hpp>
#include <common/logger.hpp>
#include <ipcd/protocol/header.hpp>
#include <ipcd/protocol/display_protocol.hpp>
#include <QTimer>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace tinexus::settings_ui {

namespace {
constexpr float kScaleFactors[] = {1.0f, 1.25f, 1.5f, 2.0f};

int nearestScaleIndex(float scale) {
    int best = 0;
    float bestDiff = 99.0f;
    for (int i = 0; i < 4; ++i) {
        float d = std::abs(scale - kScaleFactors[i]);
        if (d < bestDiff) {
            bestDiff = d;
            best = i;
        }
    }
    return best;
}
} // namespace

DisplayController::DisplayController(QObject* parent)
    : QObject(parent)
{
    refreshHardware();
    m_brightness = hardware::BacklightUtils::get_brightness_percent();
    m_compositorInfo = QString::fromStdString(hardware::DisplayUtils::get_compositor_version_string());
}

void DisplayController::rebuildModeLists(int activeW, int activeH, int activeMhz) {
    m_availableResolutions.clear();
    std::vector<std::pair<int, int>> uniqueRes;
    for (const auto& m : m_modes) {
        auto it = std::find_if(uniqueRes.begin(), uniqueRes.end(), [&](const auto& p) {
            return p.first == m.width && p.second == m.height;
        });
        if (it == uniqueRes.end()) {
            uniqueRes.emplace_back(m.width, m.height);
        }
    }
    if (uniqueRes.empty() && activeW > 0 && activeH > 0) {
        uniqueRes.emplace_back(activeW, activeH);
    }

    m_currentResolutionIndex = 0;
    for (size_t i = 0; i < uniqueRes.size(); ++i) {
        m_availableResolutions.append(QStringLiteral("%1 × %2").arg(uniqueRes[i].first).arg(uniqueRes[i].second));
        if (uniqueRes[i].first == activeW && uniqueRes[i].second == activeH) {
            m_currentResolutionIndex = static_cast<int>(i);
        }
    }

    m_availableRefreshRates.clear();
    std::vector<int> rates;
    for (const auto& m : m_modes) {
        if (m.width == activeW && m.height == activeH) {
            if (std::find(rates.begin(), rates.end(), m.refresh_mhz) == rates.end()) {
                rates.push_back(m.refresh_mhz);
            }
        }
    }
    if (rates.empty()) {
        rates.push_back(activeMhz > 0 ? activeMhz : 60000);
    }
    std::sort(rates.begin(), rates.end(), std::greater<int>());

    m_currentRefreshIndex = 0;
    int bestDiff = INT32_MAX;
    for (size_t i = 0; i < rates.size(); ++i) {
        const double hz = static_cast<double>(rates[i]) / 1000.0;
        const int rounded = static_cast<int>(std::round(hz));
        QString label = (std::abs(hz - rounded) < 0.08)
            ? QStringLiteral("%1 Hz").arg(rounded)
            : QStringLiteral("%1 Hz").arg(QString::number(hz, 'f', 2));
        m_availableRefreshRates.append(label);
        int diff = std::abs(rates[i] - activeMhz);
        if (diff < bestDiff) {
            bestDiff = diff;
            m_currentRefreshIndex = static_cast<int>(i);
        }
    }
}

void DisplayController::parseOutputsTelemetry() {
    m_modes.clear();
    const std::string userOutputs = tinexus::common::RuntimePaths::get_runtime_dir() + "/outputs";
    std::ifstream ifs(userOutputs);
    if (!ifs.is_open()) {
        ifs.open("/run/tinexus/outputs");
    }
    if (!ifs.is_open()) return;

    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.starts_with("output|")) continue;
        std::vector<std::string> cols;
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, '|')) {
            cols.push_back(item);
        }
        if (cols.size() < 8) continue;

        m_connectorName = QString::fromStdString(cols[1]);
        auto xpos = cols[2].find('x');
        if (xpos != std::string::npos) {
            try {
                m_curWidth = std::stoi(cols[2].substr(0, xpos));
                m_curHeight = std::stoi(cols[2].substr(xpos + 1));
            } catch (...) {}
        }
        try { m_curRefreshMhz = std::stoi(cols[3]); } catch (...) {}
        try {
            float sc = std::stof(cols[4]);
            m_displayScaleIndex = nearestScaleIndex(sc);
        } catch (...) {}
        m_vrrSupported = (cols[5] == "1");
        if (m_vrrSupported) {
            m_vrrEnabled = (cols[6] == "1");
        }

        std::stringstream mss(cols[7]);
        std::string mtok;
        while (std::getline(mss, mtok, ',')) {
            auto mx = mtok.find('x');
            auto ma = mtok.find('@');
            if (mx != std::string::npos && ma != std::string::npos && ma > mx) {
                try {
                    ModeInfo mi;
                    mi.width = std::stoi(mtok.substr(0, mx));
                    mi.height = std::stoi(mtok.substr(mx + 1, ma - mx - 1));
                    mi.refresh_mhz = std::stoi(mtok.substr(ma + 1));
                    m_modes.push_back(mi);
                } catch (...) {}
            }
        }
        break; // primary output first
    }
}

void DisplayController::refreshHardware() {
    auto disp = hardware::DisplayUtils::get_primary_display();
    if (disp.connected && !disp.resolution.empty() && disp.resolution != "unknown") {
        m_resolution = QString::fromStdString(disp.resolution);
        m_refreshRate = QString::fromStdString(disp.refresh_rate);
        m_connectorName = QString::fromStdString(disp.connector_name);
        auto x = disp.resolution.find("×");
        if (x == std::string::npos) x = disp.resolution.find('x');
        if (x != std::string::npos) {
            try {
                std::string left = disp.resolution.substr(0, x);
                std::string right = disp.resolution.substr(x + (disp.resolution[x] == 'x' ? 1 : 2));
                m_curWidth = std::stoi(left);
                m_curHeight = std::stoi(right);
            } catch (...) {}
        }
        // refresh_rate is a string like "60.00 Hz" or "144" — parse it
        {
            const std::string& rr = disp.refresh_rate;
            size_t end = 0;
            try {
                double hz = std::stod(rr, &end);
                if (hz > 0.0)
                    m_curRefreshMhz = static_cast<int>(std::round(hz * 1000.0));
            } catch (...) {}
        }
    } else {
        m_resolution = QStringLiteral("Native Display");
        m_refreshRate = QStringLiteral("Auto");
        m_connectorName = QStringLiteral("Primary Display");
    }

    parseOutputsTelemetry();
    if (m_curWidth > 0 && m_curHeight > 0) {
        m_resolution = QStringLiteral("%1 × %2").arg(m_curWidth).arg(m_curHeight);
        const double hz = static_cast<double>(m_curRefreshMhz) / 1000.0;
        const int r = static_cast<int>(std::round(hz));
        m_refreshRate = (std::abs(hz - r) < 0.08)
            ? QStringLiteral("%1 Hz").arg(r)
            : QStringLiteral("%1 Hz").arg(QString::number(hz, 'f', 2));
    }

    if (m_connectorName.contains(QStringLiteral("eDP")) || m_connectorName.contains(QStringLiteral("LVDS"))) {
        m_displaySubtitle = QStringLiteral("Built-in Display (%1)  •  %2").arg(m_connectorName, m_refreshRate);
    } else {
        m_displaySubtitle = QStringLiteral("%1  •  %2").arg(m_connectorName, m_refreshRate);
    }

    rebuildModeLists(m_curWidth, m_curHeight, m_curRefreshMhz);
    m_compositorInfo = QString::fromStdString(hardware::DisplayUtils::get_compositor_version_string());

    emit displayScaleIndexChanged();
    emit displayHardwareChanged();
    emit compositorInfoChanged();
}

void DisplayController::sendDisplayIpc(uint8_t flags) {
    using namespace tinexus::ipcd::protocol;
    const std::string sock_path = tinexus::common::RuntimePaths::get_ipc_socket_path();
    int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
        struct {
            Header hdr;
            DisplayApplyPayload pld;
        } __attribute__((packed)) msg{};
        msg.hdr.magic = TINEXUS_IPC_MAGIC;
        msg.hdr.version = TINEXUS_IPC_VERSION_1;
        msg.hdr.msg_type = static_cast<uint16_t>(DisplayMessageType::DISPLAY_SET_CONFIG);
        msg.hdr.payload_len = sizeof(DisplayApplyPayload);

        std::strncpy(msg.pld.connector, m_connectorName.toStdString().c_str(), sizeof(msg.pld.connector) - 1);
        msg.pld.width = m_curWidth;
        msg.pld.height = m_curHeight;
        msg.pld.refresh_mhz = m_curRefreshMhz;
        msg.pld.scale = kScaleFactors[std::clamp(m_displayScaleIndex, 0, 3)];
        msg.pld.vrr_enabled = m_vrrEnabled ? 1 : 0;
        msg.pld.night_light = m_nightLight ? 1 : 0;
        msg.pld.flags = flags;
        ::send(fd, &msg, sizeof(msg), MSG_NOSIGNAL);
    }
    ::close(fd);

    QTimer::singleShot(140, this, &DisplayController::refreshHardware);
}

void DisplayController::setDisplayScaleIndex(int index) {
    index = std::clamp(index, 0, 3);
    if (m_displayScaleIndex != index) {
        m_displayScaleIndex = index;
        sendDisplayIpc(tinexus::ipcd::protocol::DISPLAY_FLAG_SCALE);
        emit displayScaleIndexChanged();
        emit settingModified(QStringLiteral("display_scale_idx"), index);
        emit toastRequested(QStringLiteral("Display scaling set to %1%").arg(static_cast<int>(kScaleFactors[index] * 100)), false);
    }
}

void DisplayController::setResolutionIndex(int index) {
    if (index < 0 || index >= m_availableResolutions.size()) return;
    const QString resStr = m_availableResolutions.at(index);
    const QStringList parts = resStr.split(QStringLiteral("×"));
    if (parts.size() != 2) return;
    bool okW = false, okH = false;
    const int nw = parts[0].trimmed().toInt(&okW);
    const int nh = parts[1].trimmed().toInt(&okH);
    if (!okW || !okH || nw <= 0 || nh <= 0) return;

    m_curWidth = nw;
    m_curHeight = nh;
    // Select highest available refresh rate for new resolution
    int bestMhz = m_curRefreshMhz;
    int maxMhz = 0;
    for (const auto& m : m_modes) {
        if (m.width == nw && m.height == nh && m.refresh_mhz > maxMhz) {
            maxMhz = m.refresh_mhz;
        }
    }
    if (maxMhz > 0) bestMhz = maxMhz;
    m_curRefreshMhz = bestMhz;
    m_currentResolutionIndex = index;

    sendDisplayIpc(tinexus::ipcd::protocol::DISPLAY_FLAG_MODE);
    rebuildModeLists(m_curWidth, m_curHeight, m_curRefreshMhz);
    emit displayHardwareChanged();
    emit settingModified(QStringLiteral("resolution"), QStringLiteral("%1x%2").arg(nw).arg(nh));
    emit toastRequested(QStringLiteral("Display resolution set to %1").arg(resStr), false);
}

void DisplayController::setRefreshIndex(int index) {
    if (index < 0 || index >= m_availableRefreshRates.size()) return;
    QString rateStr = m_availableRefreshRates.at(index);
    rateStr.remove(QStringLiteral("Hz"));
    bool ok = false;
    const double hz = rateStr.trimmed().toDouble(&ok);
    if (!ok || hz <= 0.0) return;

    m_curRefreshMhz = static_cast<int>(std::round(hz * 1000.0));
    m_currentRefreshIndex = index;
    sendDisplayIpc(tinexus::ipcd::protocol::DISPLAY_FLAG_MODE);
    emit displayHardwareChanged();
    emit toastRequested(QStringLiteral("Refresh rate set to %1").arg(m_availableRefreshRates.at(index)), false);
}

void DisplayController::setBrightness(int percent) {
    percent = std::clamp(percent, 10, 100);
    if (m_brightness != percent) {
        m_brightness = percent;
        hardware::BacklightUtils::set_brightness_percent(percent, true, true);
        emit brightnessChanged();
        emit settingModified(QStringLiteral("brightness"), percent);
    }
}

void DisplayController::setNightLight(bool enabled) {
    if (m_nightLight != enabled) {
        m_nightLight = enabled;
        sendDisplayIpc(tinexus::ipcd::protocol::DISPLAY_FLAG_NIGHT_LIGHT);
        emit nightLightChanged();
        emit settingModified(QStringLiteral("night_light"), enabled);
        emit toastRequested(enabled ? QStringLiteral("Night Light enabled (warm color curve)")
                                    : QStringLiteral("Night Light disabled"), false);
    }
}

void DisplayController::setVrrEnabled(bool enabled) {
    if (m_vrrEnabled != enabled) {
        m_vrrEnabled = enabled;
        sendDisplayIpc(tinexus::ipcd::protocol::DISPLAY_FLAG_VRR);
        emit vrrEnabledChanged();
        emit settingModified(QStringLiteral("vrr_enabled"), enabled);
    }
}

} // namespace tinexus::settings_ui
