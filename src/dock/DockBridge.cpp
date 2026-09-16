// ============================================================================
// DockBridge.cpp — C++20 QObject Bridge for tinexus-dock
// ============================================================================
#include "dock/DockBridge.hpp"
#include "dock/DockModel.hpp"
#include "dock/ToplevelTracker.hpp"
#include "dock/DockMenuPopup.hpp"
#include "dock/DockWindow.hpp"
#include "dock/StacksPopup.hpp"
#include <ipcd/protocol/dock_protocol.hpp>
#include <ipcd/protocol/header.hpp>
#include <common/RuntimePaths.hpp>
#include <common/logger.hpp>
#include <unordered_map>

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
        DockIconItem{"firefox",           "Firefox",   "env MOZ_ENABLE_WAYLAND=1 firefox", "firefox",  DockIconAppState::NotRunning}
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
    for (size_t i = 0; i < m_icons.size(); ++i) {
        const auto& icon = m_icons[i];
        QVariantMap map;
        map[QStringLiteral("appId")]         = icon.appId;
        map[QStringLiteral("label")]         = icon.label;
        map[QStringLiteral("exec")]          = icon.exec;
        map[QStringLiteral("iconType")]      = icon.iconType;
        map[QStringLiteral("appState")]      = static_cast<int>(icon.appState);
        map[QStringLiteral("scale")]         = icon.scaleSpring.value + icon.bounceSpring.value;
        map[QStringLiteral("bounceOffset")]  = icon.bounceSpring.value;
        map[QStringLiteral("centerX")]       = icon.centerX;
        map[QStringLiteral("toplevelCount")] = icon.toplevelCount;
        map[QStringLiteral("badgeCount")]    = static_cast<quint32>(icon.badgeCount);
        map[QStringLiteral("isRunning")]     = (icon.toplevelCount > 0 || icon.appState != DockIconAppState::NotRunning);
        map[QStringLiteral("isActive")]      = (icon.appState == DockIconAppState::RunningFocused);
        map[QStringLiteral("isSeparator")]   = (icon.iconType == QStringLiteral("separator"));
        map[QStringLiteral("isDropTarget")]  = (m_dropTargetIndex == static_cast<int>(i));
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

void DockBridge::updateScreenGeometry(double totalWindowWidth, double totalWindowHeight) {
    bool changed = false;
    if (std::abs(m_windowWidth - totalWindowWidth) > 0.5) {
        m_windowWidth = totalWindowWidth;
        changed = true;
    }
    if (std::abs(m_screenHeight - totalWindowHeight) > 0.5) {
        m_screenHeight = totalWindowHeight;
        changed = true;
    }
    if (changed) {
        recomputeLayout();
        emit layoutChanged();
        emit iconsChanged();
    }
    if (m_menuPopup) {
        m_menuPopup->updateScreenGeometry(m_windowWidth, m_screenHeight);
    }
    if (m_stacksPopup) {
        m_stacksPopup->updateScreenGeometry(m_windowWidth, m_screenHeight);
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

void DockBridge::attachModel(DockModel* model) {
    if (m_model == model) return;
    m_model = model;
    if (!m_model) return;

    connect(m_model, &DockModel::dockItemsChanged, this, &DockBridge::syncFromModel);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &DockBridge::syncFromModel);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &DockBridge::syncFromModel);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &DockBridge::syncFromModel);
    connect(m_model, &QAbstractItemModel::rowsMoved, this, &DockBridge::syncFromModel);
    connect(m_model, &QAbstractItemModel::modelReset, this, &DockBridge::syncFromModel);

    syncFromModel();
}

void DockBridge::syncFromModel() {
    if (!m_model) return;
    const auto& items = m_model->mergedItems();

    std::unordered_map<std::string, std::pair<tinexus::animation::SpringState, tinexus::animation::SpringState>> prevSprings;
    for (const auto& old : m_icons) {
        prevSprings[old.appId.toStdString()] = {old.scaleSpring, old.bounceSpring};
    }

    std::vector<DockIconItem> newIcons;
    newIcons.reserve(items.size());

    for (const auto& item : items) {
        DockIconAppState st = DockIconAppState::NotRunning;
        switch (item.appState) {
        case DockAppState::RunningFocused: st = DockIconAppState::RunningFocused; break;
        case DockAppState::RunningBg:      st = DockIconAppState::RunningBg; break;
        case DockAppState::Minimized:      st = DockIconAppState::Minimized; break;
        case DockAppState::NotRunning:
        default:                           st = DockIconAppState::NotRunning; break;
        }

        DockIconItem icon(item.appId, item.displayName, item.execCmd, item.iconType, st,
                          item.toplevelCount, item.badgeCount);

        auto it = prevSprings.find(item.appId.toStdString());
        if (it != prevSprings.end()) {
            icon.scaleSpring  = it->second.first;
            icon.bounceSpring = it->second.second;
        } else {
            icon.scaleSpring.reset(1.0, 1.0);
            icon.bounceSpring.reset(0.2, 0.0);
        }
        newIcons.push_back(std::move(icon));
    }

    m_icons = std::move(newIcons);
    recomputeLayout();
    emit layoutChanged();
    emit iconsChanged();
}

void DockBridge::attachMenuPopup(DockMenuPopup* popup) {
    if (m_menuPopup == popup) return;
    m_menuPopup = popup;
    if (!m_menuPopup) return;

    m_menuPopup->updateScreenGeometry(m_windowWidth, m_screenHeight);
    connect(m_menuPopup, &DockMenuPopup::actionTriggered,
            this, &DockBridge::onMenuActionTriggered);
}

void DockBridge::attachDockWindow(DockWindow* dockWindow) {
    if (m_dockWindow == dockWindow) return;
    m_dockWindow = dockWindow;
    if (!m_dockWindow) return;

    connect(m_dockWindow, &DockWindow::revealRequested, this, &DockBridge::requestReveal);
    connect(m_dockWindow, &DockWindow::hideRequested, this, &DockBridge::requestHide);
    connect(m_dockWindow, &DockWindow::autoHideStateChanged, this, [this](int st) {
        if (m_autoHideState != st) {
            m_autoHideState = st;
            emit autoHideStateChanged(m_autoHideState);
        }
    });
}

void DockBridge::attachStacksPopup(StacksPopup* popup) {
    if (m_stacksPopup == popup) return;
    m_stacksPopup = popup;
    if (!m_stacksPopup) return;

    m_stacksPopup->updateScreenGeometry(m_windowWidth, m_screenHeight);
}

void DockBridge::setAutoHideEnabled(bool val) {
    if (m_autoHideEnabled != val) {
        m_autoHideEnabled = val;
        emit autoHideEnabledChanged(m_autoHideEnabled);
        if (m_dockWindow) {
            m_dockWindow->setAutoHideEnabled(m_autoHideEnabled);
        }
    }
}

void DockBridge::setAutoHideState(int val) {
    if (m_autoHideState != val) {
        m_autoHideState = val;
        emit autoHideStateChanged(m_autoHideState);
        if (m_dockWindow) {
            m_dockWindow->setAutoHideStateInt(m_autoHideState);
        }
    }
}

void DockBridge::requestHide() {
    setAutoHideState(2); // Hidden
    emit hideRequested();
}

void DockBridge::requestReveal() {
    setAutoHideState(0); // Visible
    emit revealRequested();
}

void DockBridge::toggleAutoHide() {
    setAutoHideEnabled(!m_autoHideEnabled);
}

void DockBridge::moveItem(int fromIndex, int toIndex) {
    if (m_model) {
        m_model->moveItem(fromIndex, toIndex);
    }
}

void DockBridge::unpinApp(const QString& appId) {
    if (m_model) {
        m_model->unpinApp(appId);
    }
}

void DockBridge::commitMove() {
    tinexus::log::info("[DockBridge] Drag rearrange committed. Saving new layout.");
    if (m_model) {
        m_model->commitMove();
    }
}

void DockBridge::setDropTargetIndex(int idx) {
    if (m_dropTargetIndex != idx) {
        m_dropTargetIndex = idx;
        emit dropTargetChanged(m_dropTargetIndex);
        emit iconsChanged();
    }
}

void DockBridge::launchWithUris(const QString& appId, const QStringList& uris) {
    if (uris.isEmpty()) return;

    QString execCmd;
    for (auto& icon : m_icons) {
        if (icon.appId == appId || icon.appId.compare(appId, Qt::CaseInsensitive) == 0) {
            execCmd = icon.exec;
            icon.bounceSpring.reset(0.40, 0.0);
            emit iconsChanged();
            break;
        }
    }
    if (execCmd.isEmpty()) {
        execCmd = appId;
    }

    tinexus::log::info("[DockBridge] Launching '{}' with {} URIs via file drop",
                       appId.toStdString(), uris.size());

    QStringList args = QProcess::splitCommand(execCmd);
    if (args.isEmpty()) return;

    QString program = args.takeFirst();
    args.append(uris);

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Strip layer-shell integration so launched application opens as normal window
    env.remove(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"));
    process.setProcessEnvironment(env);

    qint64 pid = 0;
    bool ok = QProcess::startDetached(program, args, QString(), &pid);
    if (ok) {
        tinexus::log::info("[DockBridge] Successfully launched '{}' (PID={}) with files",
                           program.toStdString(), pid);
    } else {
        tinexus::log::error("[DockBridge] Failed to startDetached for '{}'", program.toStdString());
    }
}

void DockBridge::onIconClicked(int index) {
    if (index < 0 || index >= static_cast<int>(m_icons.size())) return;
    const auto& icon = m_icons[static_cast<size_t>(index)];
    activateApp(icon.appId);
}

void DockBridge::activateApp(const QString& appId) {
    auto it = std::find_if(m_icons.begin(), m_icons.end(), [&](const DockIconItem& item) {
        return item.appId == appId || item.appId.compare(appId, Qt::CaseInsensitive) == 0;
    });
    if (it == m_icons.end()) {
        tinexus::log::warn("[DockBridge] activateApp: app '{}' not found in icons", appId.toStdString());
        return;
    }
    auto& icon = *it;

    tinexus::log::info("[DockBridge] activateApp app='{}' exec='{}' state={} toplevels={}",
                       icon.appId.toStdString(), icon.exec.toStdString(),
                       static_cast<int>(icon.appState), icon.toplevelCount);

    if (icon.iconType == QStringLiteral("separator")) {
        return;
    }

    if (icon.iconType == QStringLiteral("stack") || icon.appId.startsWith(QStringLiteral("__stack_"))) {
        const double screenH = m_screenHeight > 0.0 ? m_screenHeight : 1080.0;
        const double dockPillTop = screenH - DOCK_BOT_MARGIN - pillHeight();
        const double iconScale = icon.scaleSpring.value + icon.bounceSpring.value;
        const double lift = (BASE_SIZE * iconScale - BASE_SIZE) + (icon.bounceSpring.value * 40.0);
        const double targetTopY = dockPillTop - lift - 8.0;

        if (m_stacksPopup) {
            m_stacksPopup->togglePopup(icon.centerX, targetTopY, icon.exec, icon.label);
        }
        if (!m_reducedMotion) icon.bounceSpring.reset(0.28, 0.0);
        return;
    }

    if (icon.iconType == QStringLiteral("trash")) {
        spawnApp(QStringLiteral("tinexus-files trash://"));
        if (!m_reducedMotion) icon.bounceSpring.reset(0.25, 0.0);
        return;
    }

    std::string canonical = tinexus::common::get_canonical_app_id(icon.appId.toStdString());
    if (txui::is_single_instance_app(canonical) && txui::SingleInstance::is_app_running(canonical)) {
        tinexus::log::info("[DockBridge] Single-instance app '{}' is already running — focusing", canonical);
        txui::SingleInstance::focus_app(canonical);
        if (m_model) m_model->activateApp(icon.appId);
        if (!m_reducedMotion) icon.bounceSpring.reset(0.25, 0.0);
        return;
    }

    if (icon.appState != DockIconAppState::NotRunning || icon.toplevelCount > 0) {
        if (m_model) {
            m_model->activateApp(icon.appId);
        }
        sendIpc(static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_RAISE_AND_FOCUS), icon.appId);
        if (!m_reducedMotion) icon.bounceSpring.reset(0.22, 0.0);
        return;
    }

    spawnApp(icon.exec);
    if (!m_reducedMotion) icon.bounceSpring.reset(0.40, 0.0);
}

void DockBridge::requestContextMenu(int index) {
    if (index < 0 || index >= static_cast<int>(m_icons.size())) return;
    const auto& icon = m_icons[static_cast<size_t>(index)];
    if (icon.iconType == QStringLiteral("separator")) return;

    DockItemData itemData;
    if (m_model) {
        itemData = m_model->getItemData(icon.appId);
    }
    if (itemData.appId.isEmpty()) {
        itemData.appId        = icon.appId;
        itemData.displayName  = icon.label;
        itemData.execCmd      = icon.exec;
        itemData.iconType     = icon.iconType;
        itemData.toplevelCount= icon.toplevelCount;
        itemData.badgeCount   = icon.badgeCount;
        if (icon.iconType == QStringLiteral("trash")) {
            itemData.kind = DockItemKind::TrashCan;
        } else {
            itemData.kind = (icon.toplevelCount > 0) ? DockItemKind::PinnedRunning : DockItemKind::PinnedApp;
        }
    }

    bool isPinned = m_model ? m_model->isPinned(icon.appId) : true;

    const double screenCenterX = icon.centerX;
    const double screenH = m_screenHeight > 0.0 ? m_screenHeight : 1080.0;
    const double dockPillTop = screenH - DOCK_BOT_MARGIN - pillHeight();
    const double iconScale = icon.scaleSpring.value + icon.bounceSpring.value;
    const double lift = (BASE_SIZE * iconScale - BASE_SIZE) + (icon.bounceSpring.value * 40.0);
    const double targetTopY = dockPillTop - lift - 8.0;

    tinexus::log::info("[DockBridge] requestContextMenu: index={} app='{}' center=({:.1f}, {:.1f})",
                       index, icon.appId.toStdString(), screenCenterX, targetTopY);

    if (m_menuPopup) {
        m_menuPopup->showMenu(index, screenCenterX, targetTopY, itemData, isPinned);
    }
}

void DockBridge::minimizeApp(const QString& appId) {
    if (m_model) {
        m_model->minimizeApp(appId);
    }
}

void DockBridge::closeApp(const QString& appId) {
    if (m_model) {
        m_model->closeApp(appId);
    }
}

void DockBridge::forceQuitApp(const QString& appId) {
    tinexus::log::info("[DockBridge] Force quitting app '{}'", appId.toStdString());
    closeApp(appId);
    QString binName = appId;
    if (binName.startsWith(QStringLiteral("io.tinexus.shell."))) {
        binName = binName.mid(17);
    }
    QProcess::startDetached(QStringLiteral("pkill"), {QStringLiteral("-9"), QStringLiteral("-f"), binName});
}

void DockBridge::onMenuActionTriggered(const QString& action, const QString& appId, const QVariantMap& params) {
    tinexus::log::info("[DockBridge] onMenuActionTriggered: action='{}' appId='{}'",
                       action.toStdString(), appId.toStdString());

    if (action == QStringLiteral("open")) {
        activateApp(appId);
    } else if (action == QStringLiteral("activate_window")) {
        if (params.contains(QStringLiteral("handle")) && m_model) {
            quintptr hVal = params.value(QStringLiteral("handle")).value<quintptr>();
            m_model->activateToplevel(reinterpret_cast<struct zwlr_foreign_toplevel_handle_v1*>(hVal));
        }
    } else if (action == QStringLiteral("hide_all")) {
        minimizeApp(appId);
    } else if (action == QStringLiteral("show_all")) {
        if (m_model) {
            auto item = m_model->getItemData(appId);
            for (const auto& t : item.toplevels) {
                if (m_model->tracker()) m_model->tracker()->setMinimized(t.handle, false);
            }
            m_model->activateApp(appId);
        }
    } else if (action == QStringLiteral("toggle_keep_in_dock")) {
        if (m_model) m_model->toggleKeepInDock(appId);
    } else if (action == QStringLiteral("remove_from_dock")) {
        if (m_model) m_model->removeFromDock(appId);
    } else if (action == QStringLiteral("quit")) {
        closeApp(appId);
    } else if (action == QStringLiteral("force_quit")) {
        forceQuitApp(appId);
    } else if (action == QStringLiteral("open_trash")) {
        spawnApp(QStringLiteral("tinexus-files trash://"));
    } else if (action == QStringLiteral("empty_trash")) {
        spawnApp(QStringLiteral("rm -rf ~/.local/share/Trash/files/* ~/.local/share/Trash/info/*"));
    }
}

void DockBridge::spawnApp(const QString& execCmd) {
    tinexus::log::info("[DockBridge] Spawning app: '{}'", execCmd.toStdString());
    QStringList parts = QProcess::splitCommand(execCmd);
    if (parts.isEmpty()) return;
    QString prog = parts.takeFirst();
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"));
    proc.setProcessEnvironment(env);
    proc.setProgram(prog);
    proc.setArguments(parts);
    bool started = proc.startDetached();
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
