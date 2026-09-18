#include "DisplayController.hpp"
#include <common/BacklightUtils.hpp>
#include <common/DisplayUtils.hpp>
#include <common/logger.hpp>
#include <algorithm>

namespace tinexus::settings_ui {

DisplayController::DisplayController(QObject* parent)
    : QObject(parent)
{
    refreshHardware();
    m_brightness = hardware::BacklightUtils::get_brightness_percent();
    m_compositorInfo = QString::fromStdString(hardware::DisplayUtils::get_compositor_version_string());
}

void DisplayController::refreshHardware() {
    auto disp = hardware::DisplayUtils::get_primary_display();
    if (disp.connected && !disp.resolution.empty() && disp.resolution != "unknown") {
        m_resolution = QString::fromStdString(disp.resolution);
        m_refreshRate = QString::fromStdString(disp.refresh_rate);
        m_connectorName = QString::fromStdString(disp.connector_name);
        if (disp.connector_name.find("eDP") != std::string::npos || disp.connector_name.find("LVDS") != std::string::npos) {
            m_displaySubtitle = QStringLiteral("Built-in Display (%1)  •  %2").arg(m_connectorName, m_refreshRate);
        } else {
            m_displaySubtitle = QStringLiteral("%1  •  %2").arg(m_connectorName, m_refreshRate);
        }
    } else {
        m_resolution = QStringLiteral("1920 × 1080");
        m_refreshRate = QStringLiteral("60 Hz");
        m_connectorName = QStringLiteral("Primary Display");
        m_displaySubtitle = QStringLiteral("Primary Display  •  60 Hz");
    }
    m_compositorInfo = QString::fromStdString(hardware::DisplayUtils::get_compositor_version_string());

    emit displayHardwareChanged();
    emit compositorInfoChanged();
}

void DisplayController::setDisplayScaleIndex(int index) {
    index = std::clamp(index, 0, 3);
    if (m_displayScaleIndex != index) {
        m_displayScaleIndex = index;
        emit displayScaleIndexChanged();
        emit settingModified(QStringLiteral("display_scale_idx"), index);
    }
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
        emit nightLightChanged();
        emit settingModified(QStringLiteral("night_light"), enabled);
    }
}

void DisplayController::setVrrEnabled(bool enabled) {
    if (m_vrrEnabled != enabled) {
        m_vrrEnabled = enabled;
        emit vrrEnabledChanged();
        emit settingModified(QStringLiteral("vrr_enabled"), enabled);
    }
}

} // namespace tinexus::settings_ui
