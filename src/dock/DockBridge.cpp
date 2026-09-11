// ============================================================================
// DockBridge.cpp — C++20 QObject Bridge for tinexus-dock
// ============================================================================
#include "dock/DockBridge.hpp"
#include <ipcd/protocol/dock_protocol.hpp>
#include <ipcd/protocol/header.hpp>
#include <common/RuntimePaths.hpp>
#include <common/logger.hpp>

#include <QtCore/QProcess>
#include <QtCore/QCoreApplication>
#include <txui/core/SingleInstance.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace tinexus::dock {

DockBridge::DockBridge(QObject* parent)
    : QObject(parent)
{
    m_icons = {
        DockIconItem{"tinexus-terminal",  "Terminal",  "tinexus-terminal",    "terminal", DockIconAppState::NotRunning},
        DockIconItem{"tinexus-files",     "Files",     "tinexus-files",       "folder",   DockIconAppState::NotRunning},
        DockIconItem{"tinexus-settings",  "Settings",  "tinexus-settings-ui", "gear",     DockIconAppState::NotRunning},
        DockIconItem{"tinexus-monitor",   "Monitor",   "tinexus-monitor",     "barchart", DockIconAppState::NotRunning},
        DockIconItem{"tinexus-store",     "App Store", "tinexus-store",       "package",  DockIconAppState::NotRunning}
    };

    for (auto& icon : m_icons) {
        icon.scaleSpring.reset(1.0, 1.0);
        icon.bounceSpring.reset(0.0, 0.0);
    }

    recomputeLayout();
    setupIpc();

    connect(&m_animTimer, &QTimer::timeout, this, &DockBridge::onAnimationTimer);
    m_animTimer.start(16); // 60 FPS
}

DockBridge::~DockBridge() {
    if (m_ipcFd >= 0) {
        ::close(m_ipcFd);
        m_ipcFd = -1;
    }
}

void DockBridge::setReducedMotion(bool val) {
    if (m_reducedMotion != val) {
        m_reducedMotion = val;
        emit reducedMotionChanged();
        if (m_reducedMotion) {
            for (auto& icon : m_icons) {
                icon.scaleSpring.value = icon.scaleSpring.target;
                icon.bounceSpring.value = icon.bounceSpring.target;
            }
            recomputeLayout();
            emit iconsChanged();
        }
    }
}

QVariantList DockBridge::iconsList() const {
    QVariantList list;
    list.reserve(static_cast<int>(m_icons.size()));
    for (const auto& icon : m_icons) {
        QVariantMap map;
        map[QStringLiteral("appId")]        = icon.appId;
        map[QStringLiteral("label")]        = icon.label;
        map[QStringLiteral("exec")]         = icon.exec;
        map[QStringLiteral("iconType")]     = icon.iconType;
        map[QStringLiteral("appState")]     = static_cast<int>(icon.appState);
        map[QStringLiteral("scale")]        = icon.scaleSpring.value + icon.bounceSpring.value;
        map[QStringLiteral("bounceOffset")] = icon.bounceSpring.value;
        map[QStringLiteral("centerX")]      = icon.centerX;
        list.append(map);
    }
    return list;
}

void DockBridge::updateLayout(double totalWindowWidth) {
    if (std::abs(m_windowWidth - totalWindowWidth) > 0.5) {
        m_windowWidth = totalWindowWidth;
        recomputeLayout();
        emit layoutChanged();
        emit iconsChanged();
    }
}

void DockBridge::recomputeLayout() {
    // Compute total pill width
    double width = DOCK_PAD * 2.0;
    for (const auto& icon : m_icons) {
        double scale = icon.scaleSpring.value + icon.bounceSpring.value;
        width += BASE_SIZE * scale;
    }
    width += GAP * static_cast<double>(std::max(0, static_cast<int>(m_icons.size()) - 1));
    m_pillWidth = width;

    // Distribute centers relative to window
    const double pillX = (m_windowWidth - m_pillWidth) / 2.0;
    double curX = pillX + DOCK_PAD;
    for (auto& icon : m_icons) {
        double scale = icon.scaleSpring.value + icon.bounceSpring.value;
        double size = BASE_SIZE * scale;
        icon.centerX = curX + size / 2.0;
        curX += size + GAP;
    }
}

void DockBridge::handleHover(double mouseX) {
    m_mouseX = mouseX;
    m_hoveredIndex = -1;

    for (size_t i = 0; i < m_icons.size(); ++i) {
        auto& icon = m_icons[i];
        double target = 1.0;
        if (m_mouseX >= 0.0) {
            double dist = std::abs(m_mouseX - icon.centerX);
            if (dist < INFLUENCE_R) {
                double norm = dist / INFLUENCE_R;
                double falloff = (std::cos(M_PI * norm) + 1.0) / 2.0;
                target = 1.0 + (MAX_SCALE - 1.0) * falloff;
            }
        }

        icon.scaleSpring.target = target;
        if (m_reducedMotion) {
            icon.scaleSpring.value = target;
            icon.scaleSpring.velocity = 0.0;
        }

        double scale = icon.scaleSpring.value + icon.bounceSpring.value;
        double half = (BASE_SIZE * scale) / 2.0;
        if (std::abs(m_mouseX - icon.centerX) <= half) {
            m_hoveredIndex = static_cast<int>(i);
        }
    }

    emit hoverChanged();
    if (m_reducedMotion) {
        recomputeLayout();
        emit layoutChanged();
        emit iconsChanged();
    }
}

void DockBridge::resetHover() {
    m_mouseX = -1.0;
    m_hoveredIndex = -1;
    for (auto& icon : m_icons) {
        icon.scaleSpring.target = 1.0;
        if (m_reducedMotion) {
            icon.scaleSpring.value = 1.0;
            icon.scaleSpring.velocity = 0.0;
        }
    }
    emit hoverChanged();
    if (m_reducedMotion) {
        recomputeLayout();
        emit layoutChanged();
        emit iconsChanged();
    }
}

void DockBridge::tickAnimations(double dt) {
    bool animating = false;
    for (auto& icon : m_icons) {
        if (!m_reducedMotion) {
            bool s = icon.scaleSpring.step(dt);
            bool b = icon.bounceSpring.step(dt);
            if (!s || !b) animating = true;
        }
    }

    if (animating || m_reducedMotion) {
        recomputeLayout();
        emit layoutChanged();
        emit iconsChanged();
    }
}

void DockBridge::onAnimationTimer() {
    tickAnimations(0.016);
}

void DockBridge::onIconClicked(int index) {
    if (index < 0 || index >= static_cast<int>(m_icons.size())) return;
    auto& icon = m_icons[static_cast<size_t>(index)];

    tinexus::log::info("[DockBridge] onIconClicked index={} app='{}' exec='{}' state={}",
                       index, icon.appId.toStdString(), icon.exec.toStdString(), static_cast<int>(icon.appState));

    std::string canonical = tinexus::common::get_canonical_app_id(icon.appId.toStdString());
    if (txui::is_single_instance_app(canonical) && txui::SingleInstance::is_app_running(canonical)) {
        tinexus::log::info("[DockBridge] Single-instance app '{}' is already running — focusing existing instance", canonical);
        txui::SingleInstance::focus_app(canonical);
        if (!m_reducedMotion) icon.bounceSpring.reset(0.25, 0.0);
        return;
    }

    switch (icon.appState) {
    case DockIconAppState::Minimized:
        sendIpc(static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_RESTORE_REQUEST), icon.appId);
        break;
    case DockIconAppState::RunningBg:
        sendIpc(static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_RAISE_AND_FOCUS), icon.appId);
        break;
    case DockIconAppState::RunningFocused:
        if (!m_reducedMotion) icon.bounceSpring.reset(0.22, 0.0);
        break;
    case DockIconAppState::NotRunning:
    default:
        spawnApp(icon.exec);
        if (!m_reducedMotion) icon.bounceSpring.reset(0.4, 0.0);
        break;
    }
}

void DockBridge::spawnApp(const QString& execCmd) {
    tinexus::log::info("[DockBridge] Spawning app: '{}'", execCmd.toStdString());
    QStringList parts = QProcess::splitCommand(execCmd);
    if (parts.isEmpty()) return;
    QString prog = parts.takeFirst();
    bool started = QProcess::startDetached(prog, parts);
    tinexus::log::info("[DockBridge] App '{}' startDetached result: {}", prog.toStdString(), started);
}

void DockBridge::updateIconState(const QString& appId, DockIconAppState state) {
    for (auto& icon : m_icons) {
        if (icon.appId == appId) {
            icon.appState = state;
            emit iconsChanged();
            break;
        }
    }
}

void DockBridge::setupIpc() {
    m_ipcFd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (m_ipcFd < 0) return;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::string sock_path = tinexus::common::RuntimePaths::get_ipc_socket_path();
    std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(m_ipcFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0 || errno == EINPROGRESS) {
        uint16_t sub_types[] = {
            static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_MINIMIZED),
            static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_RESTORED),
            static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_FOCUS_CHANGED),
            static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_QUERY_ICON_POSITION),
            static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_APP_STARTED),
            static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_APP_CLOSED)
        };

        for (uint16_t t : sub_types) {
            struct {
                tinexus::ipcd::protocol::Header hdr;
                uint16_t topic;
            } __attribute__((packed)) msg{};
            msg.hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
            msg.hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
            msg.hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_SUBSCRIBE_TOPIC);
            msg.hdr.payload_len = sizeof(msg.topic);
            msg.topic = t;
            ::send(m_ipcFd, &msg, sizeof(msg), MSG_NOSIGNAL);
        }

        m_notifier = std::make_unique<QSocketNotifier>(m_ipcFd, QSocketNotifier::Read, this);
        connect(m_notifier.get(), &QSocketNotifier::activated, this, &DockBridge::onSocketReadable);
    } else {
        ::close(m_ipcFd);
        m_ipcFd = -1;
    }
}

void DockBridge::sendIpc(uint16_t msgType, const QString& appId) {
    if (m_ipcFd < 0) return;

    tinexus::ipcd::protocol::Header hdr{};
    tinexus::ipcd::protocol::DockNotifyPayload pld{};
    hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
    hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
    hdr.msg_type = msgType;
    hdr.payload_len = sizeof(pld);

    QByteArray id_bytes = appId.toUtf8();
    std::strncpy(pld.app_id, id_bytes.constData(), sizeof(pld.app_id) - 1);
    pld.surface_id = 0;

    ::send(m_ipcFd, &hdr, sizeof(hdr), MSG_NOSIGNAL);
    ::send(m_ipcFd, &pld, sizeof(pld), MSG_NOSIGNAL);
}

void DockBridge::onSocketReadable() {
    using namespace tinexus::ipcd::protocol;
    Header hdr{};

    while (true) {
        ssize_t peek_n = ::recv(m_ipcFd, &hdr, sizeof(hdr), MSG_PEEK | MSG_DONTWAIT);
        if (peek_n == 0) {
            if (m_notifier) m_notifier->setEnabled(false);
            ::close(m_ipcFd);
            m_ipcFd = -1;
            break;
        }
        if (peek_n < static_cast<ssize_t>(sizeof(hdr))) break;

        if (hdr.magic != TINEXUS_IPC_MAGIC) {
            char c;
            if (::recv(m_ipcFd, &c, 1, 0) <= 0) break;
            continue;
        }

        const size_t total = sizeof(hdr) + hdr.payload_len;
        std::vector<uint8_t> buf(total);
        ssize_t peek_total = ::recv(m_ipcFd, buf.data(), total, MSG_PEEK | MSG_DONTWAIT);
        if (peek_total < static_cast<ssize_t>(total)) break;

        ssize_t n = ::recv(m_ipcFd, buf.data(), total, MSG_DONTWAIT);
        if (n != static_cast<ssize_t>(total)) break;

        const void* payload = buf.data() + sizeof(hdr);

        if (hdr.msg_type == static_cast<uint16_t>(DockMessageType::DOCK_NOTIFY_MINIMIZED)) {
            auto* p = static_cast<const DockNotifyPayload*>(payload);
            updateIconState(QString::fromUtf8(p->app_id), DockIconAppState::Minimized);
        } else if (hdr.msg_type == static_cast<uint16_t>(DockMessageType::DOCK_NOTIFY_RESTORED)) {
            auto* p = static_cast<const DockNotifyPayload*>(payload);
            updateIconState(QString::fromUtf8(p->app_id), DockIconAppState::RunningFocused);
        } else if (hdr.msg_type == static_cast<uint16_t>(DockMessageType::DOCK_NOTIFY_FOCUS_CHANGED)) {
            auto* p = static_cast<const DockFocusChangedPayload*>(payload);
            QString appId = QString::fromUtf8(p->app_id);
            if (p->is_focused) {
                updateIconState(appId, DockIconAppState::RunningFocused);
                for (auto& icon : m_icons) {
                    if (icon.appId != appId && icon.appState == DockIconAppState::RunningFocused) {
                        updateIconState(icon.appId, DockIconAppState::RunningBg);
                    }
                }
            } else {
                updateIconState(appId, DockIconAppState::RunningBg);
            }
        } else if (hdr.msg_type == static_cast<uint16_t>(DockMessageType::DOCK_NOTIFY_APP_STARTED)) {
            auto* p = static_cast<const DockNotifyPayload*>(payload);
            updateIconState(QString::fromUtf8(p->app_id), DockIconAppState::RunningFocused);
        } else if (hdr.msg_type == static_cast<uint16_t>(DockMessageType::DOCK_NOTIFY_APP_CLOSED)) {
            auto* p = static_cast<const DockNotifyPayload*>(payload);
            updateIconState(QString::fromUtf8(p->app_id), DockIconAppState::NotRunning);
        } else if (hdr.msg_type == static_cast<uint16_t>(DockMessageType::DOCK_QUERY_ICON_POSITION)) {
            auto* p = static_cast<const DockQueryIconPositionPayload*>(payload);
            QString queryApp = QString::fromUtf8(p->app_id);
            int32_t x = 0, y = 0, w = 0, h = 0;

            for (const auto& icon : m_icons) {
                if (icon.appId == queryApp) {
                    double scale = icon.scaleSpring.value + icon.bounceSpring.value;
                    double size = BASE_SIZE * scale;
                    w = static_cast<int32_t>(size);
                    h = static_cast<int32_t>(size);
                    x = static_cast<int32_t>(icon.centerX - size / 2.0);
                    y = static_cast<int32_t>(120.0 - DOCK_BOT_MARGIN - pillHeight() + (pillHeight() - size) / 2.0);
                    break;
                }
            }

            struct {
                Header reply_hdr;
                DockIconPositionPayload reply_pld;
            } __attribute__((packed)) reply{};

            reply.reply_hdr.magic = TINEXUS_IPC_MAGIC;
            reply.reply_hdr.version = TINEXUS_IPC_VERSION_1;
            reply.reply_hdr.msg_type = static_cast<uint16_t>(DockMessageType::DOCK_ICON_POSITION);
            reply.reply_hdr.payload_len = sizeof(reply.reply_pld);
            reply.reply_hdr.sequence_id = hdr.sequence_id;

            std::strncpy(reply.reply_pld.app_id, p->app_id, sizeof(reply.reply_pld.app_id) - 1);
            reply.reply_pld.x = x;
            reply.reply_pld.y = y;
            reply.reply_pld.w = w;
            reply.reply_pld.h = h;

            ::send(m_ipcFd, &reply, sizeof(reply), MSG_NOSIGNAL);
        }
    }
}

} // namespace tinexus::dock

#include "moc_DockBridge.cpp"
