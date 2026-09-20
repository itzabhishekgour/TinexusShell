// ============================================================================
// DockBridge.cpp — C++20 QObject Bridge for tinexus-dock
// ============================================================================
#include "dock/DockBridge.hpp"
#include "dock/DockModel.hpp"
#include "dock/ToplevelTracker.hpp"
#include "dock/DockMenuPopup.hpp"
#include "dock/DockWindow.hpp"
#include "dock/StacksPopup.hpp"
#include "dock/DockAdaptor.hpp"
#include <common/logger.hpp>
#include <unordered_map>

#include <QtCore/QProcess>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <common/SingleInstance.hpp>
#include <files/trash_manager.hpp>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace tinexus::dock {

static fs::path getDockConfigPath() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig && *xdgConfig) {
        return fs::path(xdgConfig) / "tinexus" / "dock.toml";
    }
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : "/tmp") / ".config" / "tinexus" / "dock.toml";
}

DockBridge::DockBridge(QObject* parent)
    : QObject(parent)
{
    loadConfig();

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
    setupDBus();

    // Setup live filesystem watcher for Trash
    QString trashFilesPath = QDir::homePath() + QStringLiteral("/.local/share/Trash/files");
    QDir().mkpath(trashFilesPath);
    QDir().mkpath(QDir::homePath() + QStringLiteral("/.local/share/Trash/info"));
    m_trashWatcher.addPath(trashFilesPath);
    connect(&m_trashWatcher, &QFileSystemWatcher::directoryChanged, this, &DockBridge::updateTrashBadge);
    updateTrashBadge();

    connect(&m_animTimer, &QTimer::timeout, this, &DockBridge::onAnimationTimer);
    m_animTimer.start(16); // 60 FPS
}

DockBridge::~DockBridge() = default;

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
        map[QStringLiteral("needsAttention")] = icon.needsAttention;
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

    if (m_dockWindow) {
        m_dockWindow->setBaseExclusiveZone(static_cast<int>(exclusiveZone()));
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

    m_autoHideEnabled = m_model->autoHideEnabled();
    emit autoHideEnabledChanged(m_autoHideEnabled);

    connect(m_model, &DockModel::dockItemsChanged, this, &DockBridge::syncFromModel);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &DockBridge::syncFromModel);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &DockBridge::syncFromModel);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &DockBridge::syncFromModel);
    connect(m_model, &QAbstractItemModel::rowsMoved, this, &DockBridge::syncFromModel);
    connect(m_model, &QAbstractItemModel::modelReset, this, &DockBridge::syncFromModel);

    syncFromModel();
    updateTrashBadge();
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
                          item.toplevelCount, item.badgeCount, item.needsAttention);

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

    if (m_autoHideEnabled) {
        m_dockWindow->setAutoHideEnabled(m_autoHideEnabled);
    }

    connect(m_dockWindow, &DockWindow::revealRequested, this, &DockBridge::requestReveal);
    connect(m_dockWindow, &DockWindow::hideRequested, this, &DockBridge::requestHide);
    connect(m_dockWindow, &DockWindow::autoHideStateChanged, this, [this](int st) {
        if (m_autoHideState != st) {
            m_autoHideState = st;
            emit autoHideStateChanged(m_autoHideState);
        }
    });
    connect(m_dockWindow, &DockWindow::autoHideEnabledChanged, this, [this](bool en) {
        if (m_autoHideEnabled != en) {
            m_autoHideEnabled = en;
            emit autoHideEnabledChanged(m_autoHideEnabled);
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
        saveConfig();
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

    if (appId == QStringLiteral("__trash__")) {
        tinexus::log::info("[DockBridge] Moving {} URIs to Trash via TrashManager", uris.size());
        for (const auto& uriStr : uris) {
            QUrl url(uriStr);
            QString localPath = url.isLocalFile() ? url.toLocalFile() : uriStr;
            if (!localPath.isEmpty()) {
                tinexus::files::TrashManager::instance().move_to_trash(localPath.toStdString());
            }
        }
        updateTrashBadge();
        return;
    }

    QString execCmd;
    for (const auto& icon : m_icons) {
        if (icon.appId == appId && !icon.exec.isEmpty()) {
            execCmd = icon.exec;
            break;
        }
    }
    if (execCmd.isEmpty()) {
        execCmd = appId;
    }

    tinexus::log::info("[DockBridge] Launching '{}' with {} URIs via file drop",
                       appId.toStdString(), uris.size());

    launchWithFiles(execCmd, uris);
}

static void enrichEnvironmentWithRuntime(QProcessEnvironment& env) {
    env.remove(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"));
    if (!env.contains(QStringLiteral("DISPLAY")) || !env.contains(QStringLiteral("WAYLAND_DISPLAY"))) {
        QFile envFile(QStringLiteral("/run/tinexus/env"));
        if (envFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!envFile.atEnd()) {
                QByteArray line = envFile.readLine().trimmed();
                int eq = line.indexOf('=');
                if (eq > 0) {
                    QString k = QString::fromUtf8(line.left(eq));
                    QString v = QString::fromUtf8(line.mid(eq + 1));
                    if (!env.contains(k)) {
                        env.insert(k, v);
                    }
                }
            }
        }
    }
}

void DockBridge::launchWithFiles(const QString& execLine, const QStringList& uris) {
    QStringList args = QProcess::splitCommand(execLine);
    if (args.isEmpty()) return;

    QString program = args.takeFirst();
    args.append(uris);

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    enrichEnvironmentWithRuntime(env);
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
    if (tinexus::common::is_single_instance_app(canonical) && tinexus::common::SingleInstance::is_app_running(canonical)) {
        tinexus::log::info("[DockBridge] Single-instance app '{}' is already running — focusing", canonical);
        tinexus::common::SingleInstance::focus_app(canonical);
        if (m_model) m_model->activateApp(icon.appId);
        if (!m_reducedMotion) icon.bounceSpring.reset(0.25, 0.0);
        return;
    }

    if (icon.appState != DockIconAppState::NotRunning || icon.toplevelCount > 0) {
        if (m_model) {
            m_model->activateApp(icon.appId);
        }
        sendRaiseAndFocus(icon.appId);
        if (!m_reducedMotion) icon.bounceSpring.reset(0.22, 0.0);
        return;
    }

    icon.needsAttention = false;
    if (m_model) {
        m_model->setNeedsAttention(icon.appId, false);
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
    } else if (binName.contains(QLatin1Char('.'))) {
        binName = binName.section(QLatin1Char('.'), -1);
    }

    std::string target = binName.toStdString();
    std::vector<pid_t> killedPids;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/proc", ec)) {
        if (!entry.is_directory()) continue;
        std::string dirName = entry.path().filename().string();
        if (dirName.empty() || !std::all_of(dirName.begin(), dirName.end(), ::isdigit)) {
            continue;
        }
        pid_t pid = std::stoi(dirName);
        if (pid <= 1 || pid == getpid()) continue;

        std::ifstream commFile(entry.path() / "comm");
        std::string comm;
        if (commFile >> comm) {
            if (comm == target) {
                ::kill(pid, SIGKILL);
                killedPids.push_back(pid);
                continue;
            }
        }

        auto exePath = fs::read_symlink(entry.path() / "exe", ec);
        if (!ec && exePath.filename().string() == target) {
            ::kill(pid, SIGKILL);
            killedPids.push_back(pid);
        }
    }

    if (!killedPids.empty()) {
        tinexus::log::info("[DockBridge] Force quit sent SIGKILL to {} process(es) for app '{}'",
                           killedPids.size(), appId.toStdString());
    } else {
        tinexus::log::info("[DockBridge] No processes found in /proc matching binary '{}' for app '{}'",
                           target, appId.toStdString());
    }
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
        emptyTrash();
    } else if (action == QStringLiteral("toggle_autohide")) {
        toggleAutoHide();
    }
}

void DockBridge::loadConfig() {
    fs::path configPath = getDockConfigPath();
    if (!fs::exists(configPath)) {
        tinexus::log::info("[DockBridge] No dock.toml found at '{}' — using defaults", configPath.string());
        return;
    }

    std::ifstream in(configPath);
    if (!in.is_open()) {
        tinexus::log::warn("[DockBridge] Could not open config file '{}'", configPath.string());
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t\"'"));
        val.erase(val.find_last_not_of(" \t\"'") + 1);

        if (key == "auto_hide") {
            m_autoHideEnabled = (val == "true" || val == "1");
            tinexus::log::info("[DockBridge] Loaded auto_hide={} from {}", m_autoHideEnabled, configPath.string());
        }
    }
}

void DockBridge::emptyTrash() {
    tinexus::log::info("[DockBridge] Emptying trash via native filesystem API");
    const char* home = std::getenv("HOME");
    fs::path base = home ? fs::path(home) : fs::path("/tmp");
    auto trash_dir = base / ".local/share/Trash";
    std::error_code ec;
    fs::remove_all(trash_dir / "files", ec);
    fs::create_directories(trash_dir / "files", ec);
    fs::remove_all(trash_dir / "info", ec);
    fs::create_directories(trash_dir / "info", ec);
    updateTrashBadge();
}

void DockBridge::updateTrashBadge() {
    QString trashPath = QDir::homePath() + QStringLiteral("/.local/share/Trash/files");
    QDir dir(trashPath);
    int count = 0;
    if (dir.exists()) {
        count = static_cast<int>(dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).count());
    }
    setBadgeCount(QStringLiteral("__trash__"), static_cast<uint32_t>(count));
    if (m_model) {
        m_model->setBadgeCount(QStringLiteral("__trash__"), static_cast<uint32_t>(count));
    }
    tinexus::log::info("[DockBridge] Trash badge count updated to {}", count);
}

void DockBridge::pinApp(const QString& appId) {
    if (m_model) {
        m_model->pinApp(appId);
    }
}

void DockBridge::requestAttention(const QString& appId) {
    tinexus::log::info("[DockBridge] Attention requested for app '{}'", appId.toStdString());
    for (size_t i = 0; i < m_icons.size(); ++i) {
        auto& icon = m_icons[i];
        if (icon.appId == appId || icon.appId.endsWith(appId) || appId.endsWith(icon.appId)) {
            icon.needsAttention = true;
            if (!m_reducedMotion) {
                icon.bounceSpring.reset(0.50, 0.0);
            }
            break;
        }
    }
    if (m_model) {
        m_model->setNeedsAttention(appId, true);
    }
    emit iconsChanged();
}

void DockBridge::saveConfig() {
    if (m_model) {
        m_model->setAutoHideEnabled(m_autoHideEnabled);
    } else {
        fs::path configPath = getDockConfigPath();
        std::error_code ec;
        fs::create_directories(configPath.parent_path(), ec);

        fs::path tmpPath = configPath;
        tmpPath += ".tmp." + std::to_string(getpid());

        {
            std::ofstream out(tmpPath, std::ios::trunc);
            if (!out.is_open()) {
                tinexus::log::error("[DockBridge] Failed to create temp config file '{}'", tmpPath.string());
                return;
            }
            out << "# Tinexus Dock Configuration\n";
            out << "[dock]\n";
            out << "auto_hide = " << (m_autoHideEnabled ? "true" : "false") << "\n";
            out.flush();
        }

        fs::rename(tmpPath, configPath, ec);
        if (ec) {
            tinexus::log::error("[DockBridge] Failed to atomically rename '{}' to '{}': {}",
                                tmpPath.string(), configPath.string(), ec.message());
        } else {
            tinexus::log::info("[DockBridge] Persisted config (auto_hide={}) to '{}'",
                               m_autoHideEnabled, configPath.string());
        }
    }
}

void DockBridge::spawnApp(const QString& execCmd) {
    tinexus::log::info("[DockBridge] Spawning app: '{}'", execCmd.toStdString());
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
    enrichEnvironmentWithRuntime(env);
    QString pathEnv = env.value(QStringLiteral("PATH"));
    if (pathEnv.isEmpty()) {
        env.insert(QStringLiteral("PATH"), QStringLiteral("/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"));
    }
    proc.setProcessEnvironment(env);
    proc.setProgram(prog);
    proc.setArguments(parts);
    const QString home = QDir::homePath();
    proc.setWorkingDirectory(!home.isEmpty() ? home : QStringLiteral("/"));
    bool started = proc.startDetached();
    tinexus::log::info("[DockBridge] App '{}' startDetached result: {}", prog.toStdString(), started);
}

void DockBridge::updateIconState(const QString& appId, DockIconAppState state) {
    for (auto& icon : m_icons) {
        if (icon.appId == appId) {
            icon.appState = state;
            emit iconsChanged();
            if (m_dockAdaptor) {
                emit m_dockAdaptor->AppStateChanged(appId, static_cast<int>(state));
            }
            break;
        }
    }
}

void DockBridge::setupDBus() {
    m_dockAdaptor = new DockAdaptor(this);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        bus.registerService(QStringLiteral("io.tinexus.Dock"));
        bus.registerObject(QStringLiteral("/Dock"), this);
        bus.registerObject(QStringLiteral("/io/tinexus/Dock"), this);

        // Subscribe to compositor notifications if compositor emits D-Bus signals
        bus.connect(
            QStringLiteral("io.tinexus.Compositor"),
            QStringLiteral("/io/tinexus/Compositor"),
            QStringLiteral("io.tinexus.Compositor"),
            QStringLiteral("WindowMinimized"),
            this,
            SLOT(onDBusNotifyMinimized(QString, qulonglong))
        );
        bus.connect(
            QStringLiteral("io.tinexus.Compositor"),
            QStringLiteral("/io/tinexus/Compositor"),
            QStringLiteral("io.tinexus.Compositor"),
            QStringLiteral("WindowRestored"),
            this,
            SLOT(onDBusNotifyRestored(QString, qulonglong))
        );
        bus.connect(
            QStringLiteral("io.tinexus.Compositor"),
            QStringLiteral("/io/tinexus/Compositor"),
            QStringLiteral("io.tinexus.Compositor"),
            QStringLiteral("FocusChanged"),
            this,
            SLOT(onDBusNotifyFocusChanged(QString, bool))
        );
        tinexus::log::info("[DockBridge] Registered on session D-Bus as io.tinexus.Dock at /Dock");
    } else {
        tinexus::log::warn("[DockBridge] Session D-Bus not connected — running in standalone mode");
    }
}

void DockBridge::sendRaiseAndFocus(const QString& appId) {
    if (m_model) {
        m_model->activateApp(appId);
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("io.tinexus.Compositor"),
            QStringLiteral("/io/tinexus/Compositor"),
            QStringLiteral("io.tinexus.Compositor"),
            QStringLiteral("RaiseAndFocus")
        );
        msg << appId;
        bus.send(msg);
    }
}

void DockBridge::onDBusNotifyMinimized(const QString& appId, qulonglong surfaceId) {
    Q_UNUSED(surfaceId);
    updateIconState(appId, DockIconAppState::Minimized);
}

void DockBridge::onDBusNotifyRestored(const QString& appId, qulonglong surfaceId) {
    Q_UNUSED(surfaceId);
    updateIconState(appId, DockIconAppState::RunningFocused);
}

void DockBridge::onDBusNotifyFocusChanged(const QString& appId, bool isFocused) {
    if (isFocused) {
        updateIconState(appId, DockIconAppState::RunningFocused);
        for (auto& icon : m_icons) {
            if (icon.appId != appId && icon.appState == DockIconAppState::RunningFocused) {
                updateIconState(icon.appId, DockIconAppState::RunningBg);
            }
        }
        if (m_model) {
            m_model->onFocusChanged(appId);
        }
    } else {
        updateIconState(appId, DockIconAppState::RunningBg);
    }
}

void DockBridge::onDBusNotifyAppStarted(const QString& appId, qulonglong surfaceId) {
    Q_UNUSED(surfaceId);
    updateIconState(appId, DockIconAppState::RunningFocused);
    if (m_model) {
        m_model->onToplevelAdded(appId);
        m_model->onFocusChanged(appId);
    }
}

void DockBridge::onDBusNotifyAppClosed(const QString& appId, qulonglong surfaceId) {
    Q_UNUSED(surfaceId);
    updateIconState(appId, DockIconAppState::NotRunning);
    if (m_model) {
        m_model->onToplevelRemoved(appId);
    }
}

void DockBridge::onDBusQueryIconPosition(const QString& appId) {
    int x = 0, y = 0, w = 0, h = 0;
    queryIconPosition(appId, x, y, w, h);
}

void DockBridge::queryIconPosition(const QString& appId, int& x, int& y, int& w, int& h) const {
    x = y = w = h = 0;
    for (const auto& icon : m_icons) {
        if (icon.appId == appId) {
            double scale = icon.scaleSpring.value + icon.bounceSpring.value;
            double size = BASE_SIZE * scale;
            w = static_cast<int32_t>(size);
            h = static_cast<int32_t>(size);
            x = static_cast<int32_t>(icon.centerX - size / 2.0);
            y = static_cast<int32_t>(120.0 - DOCK_BOT_MARGIN - pillHeight() + (pillHeight() - size) / 2.0);
            break;
        }
    }
}

QStringList DockBridge::runningApps() const {
    QStringList list;
    for (const auto& icon : m_icons) {
        if (icon.appState != DockIconAppState::NotRunning) {
            list.append(icon.appId);
        }
    }
    return list;
}

QString DockBridge::focusedApp() const {
    if (m_model) {
        for (const auto& item : m_model->mergedItems()) {
            if (item.appState == DockAppState::RunningFocused) {
                return item.appId;
            }
        }
    }
    for (const auto& icon : m_icons) {
        if (icon.appState == DockIconAppState::RunningFocused) {
            return icon.appId;
        }
    }
    return {};
}

uint DockBridge::totalBadgeCount() const {
    uint total = 0;
    for (const auto& [_, count] : m_badgeMap) {
        total += count;
    }
    if (total == 0) {
        for (const auto& icon : m_icons) {
            total += icon.badgeCount;
        }
    }
    return total;
}

uint DockBridge::badgeCount(const QString& appId) const {
    auto it = m_badgeMap.find(appId.toStdString());
    if (it != m_badgeMap.end()) return it->second;
    for (const auto& icon : m_icons) {
        if (icon.appId == appId) return icon.badgeCount;
    }
    return 0u;
}

QVariantMap DockBridge::badgeCounts() const {
    QVariantMap map;
    for (const auto& [app, count] : m_badgeMap) {
        if (count > 0) map.insert(QString::fromStdString(app), count);
    }
    for (const auto& icon : m_icons) {
        if (icon.badgeCount > 0 && !map.contains(icon.appId)) {
            map.insert(icon.appId, icon.badgeCount);
        }
    }
    return map;
}

void DockBridge::setBadgeCount(const QString& appId, uint count) {
    m_badgeMap[appId.toStdString()] = count;
    for (auto& icon : m_icons) {
        if (icon.appId == appId) {
            icon.badgeCount = count;
            emit iconsChanged();
            break;
        }
    }
    if (m_model) {
        m_model->setBadgeCount(appId, count);
    }
}

void DockBridge::restoreWindow(const QString& appId) {
    for (auto& icon : m_icons) {
        if (icon.appId == appId) {
            if (m_model) m_model->activateApp(appId);
            updateIconState(appId, DockIconAppState::RunningFocused);
            break;
        }
    }
}

void DockBridge::raiseApp(const QString& appId) {
    sendRaiseAndFocus(appId);
}

} // namespace tinexus::dock

#include "moc_DockBridge.cpp"
