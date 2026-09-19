// ============================================================================
// DockModel.cpp — QAbstractListModel implementation for tinexus-dock (Slice 2)
// Ref: Architecture Blueprint §3, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#include "dock/DockModel.hpp"
#include "dock/ToplevelTracker.hpp"
#include <common/logger.hpp>
#include <common/AppId.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace tinexus::dock {

static std::filesystem::path getDockConfigPath() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig && *xdgConfig) {
        return std::filesystem::path(xdgConfig) / "tinexus" / "dock.toml";
    }
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : "/tmp") / ".config" / "tinexus" / "dock.toml";
}

// ── Constructor ──────────────────────────────────────────────────────────────
DockModel::DockModel(QObject* parent)
    : QAbstractListModel(parent)
{
    loadPinnedFromConfig();
    appendSeparatorAndStacks();
    rebuildMergedView();

    tinexus::log::info("[DockModel] Initialized with {} items (Slice 2: Live Window Tracking)",
                       m_merged.size());
}

// ── Data Loading ─────────────────────────────────────────────────────────────
void DockModel::loadHardcodedPinnedItems() {
    m_pinned.clear();

    auto make = [](int idx, DockItemKind kind, const char* appId, const char* name,
                   const char* exec, const char* iconType) -> DockItemData {
        DockItemData item;
        item.kind        = kind;
        item.appId       = QString::fromUtf8(appId);
        item.displayName = QString::fromUtf8(name);
        item.execCmd     = QString::fromUtf8(exec);
        item.iconType    = QString::fromUtf8(iconType);
        item.configIndex = idx;
        return item;
    };

    m_pinned = {
        make(0, DockItemKind::PinnedApp, "tinexus-terminal",  "Terminal",    "tinexus-terminal",    "terminal"),
        make(1, DockItemKind::PinnedApp, "tinexus-files",     "Files",       "tinexus-files",       "folder"),
        make(2, DockItemKind::PinnedApp, "tinexus-settings",  "Settings",    "tinexus-settings-ui", "gear"),
        make(3, DockItemKind::PinnedApp, "tinexus-monitor",   "Monitor",     "tinexus-monitor",     "barchart"),
        make(4, DockItemKind::PinnedApp, "firefox",           "Firefox",
             "env MOZ_ENABLE_WAYLAND=1 firefox",               "firefox"),
        make(5, DockItemKind::PinnedApp, "tinexus-appstore",  "App Store",   "tinexus-appstore",    "package"),
    };
}

void DockModel::loadPinnedFromConfig() {
    auto configPath = getDockConfigPath();
    if (!std::filesystem::exists(configPath)) {
        tinexus::log::info("[DockModel] No dock.toml found at '{}' — loading hardcoded defaults", configPath.string());
        loadHardcodedPinnedItems();
        savePinnedToConfig();
        return;
    }

    std::ifstream in(configPath);
    if (!in.is_open()) {
        tinexus::log::warn("[DockModel] Could not open config file '{}' — loading hardcoded defaults", configPath.string());
        loadHardcodedPinnedItems();
        return;
    }

    m_pinned.clear();
    std::string line;
    bool inDockSection = false;
    bool inPinnedTable = false;
    DockItemData currentItem;
    currentItem.kind = DockItemKind::PinnedApp;
    int parsedOrder = -1;

    auto finishItem = [&]() {
        if (inPinnedTable && !currentItem.appId.isEmpty()) {
            if (currentItem.displayName.isEmpty()) {
                currentItem.displayName = resolveDisplayName(currentItem.appId, QString());
            }
            if (currentItem.execCmd.isEmpty()) {
                currentItem.execCmd = currentItem.appId;
            }
            if (currentItem.iconType.isEmpty()) {
                currentItem.iconType = resolveIconType(currentItem.appId);
            }
            currentItem.configIndex = (parsedOrder >= 0) ? parsedOrder : static_cast<int>(m_pinned.size());
            m_pinned.append(currentItem);
        }
        currentItem = DockItemData{};
        currentItem.kind = DockItemKind::PinnedApp;
        parsedOrder = -1;
    };

    while (std::getline(in, line)) {
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        auto end = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(start, end - start + 1);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        if (trimmed == "[[pinned]]") {
            finishItem();
            inPinnedTable = true;
            inDockSection = false;
            continue;
        } else if (trimmed == "[dock]") {
            finishItem();
            inPinnedTable = false;
            inDockSection = true;
            continue;
        } else if (trimmed.front() == '[' && trimmed.back() == ']') {
            finishItem();
            inPinnedTable = false;
            inDockSection = false;
            continue;
        }

        auto eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trimmed.substr(0, eqPos);
        std::string val = trimmed.substr(eqPos + 1);

        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t\"'"));
        val.erase(val.find_last_not_of(" \t\"'") + 1);

        if (inDockSection) {
            if (key == "auto_hide") {
                m_autoHide = (val == "true" || val == "1");
            }
        } else if (inPinnedTable) {
            if (key == "app_id" || key == "appId") {
                currentItem.appId = QString::fromStdString(val);
            } else if (key == "name" || key == "displayName") {
                currentItem.displayName = QString::fromStdString(val);
            } else if (key == "exec" || key == "execCmd") {
                currentItem.execCmd = QString::fromStdString(val);
            } else if (key == "icon" || key == "iconType") {
                currentItem.iconType = QString::fromStdString(val);
            } else if (key == "order" || key == "order_index") {
                try {
                    parsedOrder = std::stoi(val);
                } catch (...) {
                    parsedOrder = -1;
                }
            }
        }
    }
    finishItem();

    if (m_pinned.isEmpty()) {
        tinexus::log::info("[DockModel] Config file '{}' had no [[pinned]] entries — using defaults", configPath.string());
        loadHardcodedPinnedItems();
        savePinnedToConfig();
    } else {
        std::stable_sort(m_pinned.begin(), m_pinned.end(), [](const DockItemData& a, const DockItemData& b) {
            return a.configIndex < b.configIndex;
        });
        for (int i = 0; i < m_pinned.size(); ++i) {
            m_pinned[i].configIndex = i;
        }
        tinexus::log::info("[DockModel] Successfully loaded {} pinned items from '{}'", m_pinned.size(), configPath.string());
    }
}

void DockModel::savePinnedToConfig() {
    auto configPath = getDockConfigPath();
    std::error_code ec;
    std::filesystem::create_directories(configPath.parent_path(), ec);

    std::filesystem::path tmpPath = configPath;
    tmpPath += ".tmp." + std::to_string(getpid());

    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if (!out.is_open()) {
            tinexus::log::error("[DockModel] Failed to create temp config file '{}'", tmpPath.string());
            return;
        }
        out << "# Tinexus Dock Configuration\n";
        out << "[dock]\n";
        out << "auto_hide = " << (m_autoHide ? "true" : "false") << "\n\n";

        for (int i = 0; i < m_pinned.size(); ++i) {
            const auto& item = m_pinned.at(i);
            out << "[[pinned]]\n";
            out << "app_id = \"" << item.appId.toStdString() << "\"\n";
            out << "name = \"" << item.displayName.toStdString() << "\"\n";
            out << "exec = \"" << item.execCmd.toStdString() << "\"\n";
            out << "icon = \"" << item.iconType.toStdString() << "\"\n";
            out << "order = " << i << "\n\n";
        }
        out.flush();
    }

    std::filesystem::rename(tmpPath, configPath, ec);
    if (ec) {
        tinexus::log::error("[DockModel] Failed to atomically rename '{}' -> '{}': {}",
                           tmpPath.string(), configPath.string(), ec.message());
        std::filesystem::remove(tmpPath, ec);
    } else {
        tinexus::log::info("[DockModel] Persisted {} pinned items (auto_hide={}) to '{}'",
                           m_pinned.size(), m_autoHide, configPath.string());
    }
}

void DockModel::setAutoHideEnabled(bool enabled) {
    if (m_autoHide != enabled) {
        m_autoHide = enabled;
        savePinnedToConfig();
    }
}

void DockModel::appendSeparatorAndStacks() {
    m_stacks.clear();

    // Separator (always separates pinned/running apps zone from Stacks zone)
    DockItemData sep;
    sep.kind        = DockItemKind::Separator;
    sep.appId       = QStringLiteral("__separator__");
    sep.displayName = {};
    sep.iconType    = QStringLiteral("separator");
    sep.configIndex = 9000;
    m_stacks.append(sep);

    // Stacks zone: Downloads Stack folder (Slice 7)
    DockItemData downloadsStack;
    downloadsStack.kind        = DockItemKind::Stack;
    downloadsStack.appId       = QStringLiteral("__stack_downloads__");
    downloadsStack.displayName = QStringLiteral("Downloads");
    downloadsStack.iconType    = QStringLiteral("stack");
    downloadsStack.execCmd     = QStringLiteral("~/Downloads");
    downloadsStack.configIndex = 9100;
    m_stacks.append(downloadsStack);

    // Trash can — permanent last item
    DockItemData trash;
    trash.kind        = DockItemKind::TrashCan;
    trash.appId       = QStringLiteral("__trash__");
    trash.displayName = QStringLiteral("Trash");
    trash.iconType    = QStringLiteral("trash");
    trash.configIndex = 9999;
    m_stacks.append(trash);
}

void DockModel::rebuildMergedView() {
    beginResetModel();
    m_merged.clear();

    // 1. Pinned items (PinnedApp | PinnedRunning)
    for (const auto& item : m_pinned) {
        m_merged.append(item);
    }

    // 2. Transient running-only items (not in pinned list)
    for (const auto& item : m_transient) {
        m_merged.append(item);
    }

    // 3. Stacks zone (Separator + Stacks + Trash)
    for (const auto& item : m_stacks) {
        m_merged.append(item);
    }

    endResetModel();
    emit dockItemsChanged();
}

// ── QAbstractListModel Overrides ─────────────────────────────────────────────
int DockModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_merged.size());
}

QVariant DockModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_merged.size())
        return {};

    const DockItemData& item = m_merged.at(index.row());

    switch (role) {
    case DockRole::AppId:          return item.appId;
    case DockRole::DisplayName:    return item.displayName;
    case DockRole::ExecCmd:        return item.execCmd;
    case DockRole::IconType:       return item.iconType;
    case DockRole::IconUrl:        return item.iconUrl;
    case DockRole::Kind:           return static_cast<int>(item.kind);
    case DockRole::AppState:       return static_cast<int>(item.appState);
    case DockRole::IsRunning:      return item.toplevelCount > 0;
    case DockRole::IsActive:       return item.appState == DockAppState::RunningFocused;
    case DockRole::IsMinimized:    return item.appState == DockAppState::Minimized;
    case DockRole::ToplevelCount:  return item.toplevelCount;
    case DockRole::BadgeCount:     return static_cast<quint32>(item.badgeCount);
    case DockRole::NeedsAttention: return item.needsAttention;
    case DockRole::IsDeleted:      return item.isDeletedFromFS;
    case DockRole::IsSeparator:    return item.kind == DockItemKind::Separator;
    default:                       return {};
    }
}

QHash<int, QByteArray> DockModel::roleNames() const {
    return {
        { DockRole::AppId,          "appId"          },
        { DockRole::DisplayName,    "displayName"    },
        { DockRole::ExecCmd,        "execCmd"        },
        { DockRole::IconType,       "iconType"       },
        { DockRole::IconUrl,        "iconUrl"        },
        { DockRole::Kind,           "kind"           },
        { DockRole::AppState,       "appState"       },
        { DockRole::IsRunning,      "isRunning"      },
        { DockRole::IsActive,       "isActive"       },
        { DockRole::IsMinimized,    "isMinimized"    },
        { DockRole::ToplevelCount,  "toplevelCount"  },
        { DockRole::BadgeCount,     "badgeCount"     },
        { DockRole::NeedsAttention, "needsAttention" },
        { DockRole::IsDeleted,      "isDeleted"      },
        { DockRole::IsSeparator,    "isSeparator"    },
    };
}

// ── App ID & Icon Resolution Heuristics ──────────────────────────────────────
bool DockModel::matchesAppId(const QString& itemAppId, const QString& incomingAppId) const {
    if (itemAppId.compare(incomingAppId, Qt::CaseInsensitive) == 0) return true;

    const std::string canA = tinexus::common::get_canonical_app_id(itemAppId.toStdString());
    const std::string canB = tinexus::common::get_canonical_app_id(incomingAppId.toStdString());
    if (!canA.empty() && canA == canB) return true;

    const QString lowerInc = incomingAppId.toLower();
    if (itemAppId == QStringLiteral("tinexus-terminal") &&
        (lowerInc.contains(QStringLiteral("terminal")) || lowerInc.contains(QStringLiteral("foot")) ||
         lowerInc.contains(QStringLiteral("kitty")) || lowerInc.contains(QStringLiteral("alacritty")))) {
        return true;
    }
    if (itemAppId == QStringLiteral("tinexus-files") &&
        (lowerInc.contains(QStringLiteral("files")) || lowerInc.contains(QStringLiteral("dolphin")) ||
         lowerInc.contains(QStringLiteral("nautilus")) || lowerInc.contains(QStringLiteral("thunar")))) {
        return true;
    }
    if (itemAppId == QStringLiteral("tinexus-settings") && lowerInc.contains(QStringLiteral("settings"))) {
        return true;
    }
    if (itemAppId == QStringLiteral("tinexus-monitor") &&
        (lowerInc.contains(QStringLiteral("monitor")) || lowerInc.contains(QStringLiteral("top")))) {
        return true;
    }
    if (itemAppId == QStringLiteral("firefox") && lowerInc.contains(QStringLiteral("firefox"))) {
        return true;
    }
    if (itemAppId == QStringLiteral("tinexus-appstore") &&
        (lowerInc.contains(QStringLiteral("store")) || lowerInc.contains(QStringLiteral("pkg")))) {
        return true;
    }
    return false;
}

QString DockModel::resolveDisplayName(const QString& appId, const QString& title) const {
    if (!title.isEmpty() && title.length() <= 30) {
        // Use clean window title if descriptive
        return title;
    }
    QString name = appId;
    if (name.startsWith(QStringLiteral("io.tinexus.shell."))) {
        name = name.mid(17);
    } else if (name.startsWith(QStringLiteral("tinexus-"))) {
        name = name.mid(8);
    } else if (name.contains(QLatin1Char('.'))) {
        name = name.section(QLatin1Char('.'), -1);
    }
    if (!name.isEmpty()) {
        name[0] = name[0].toUpper();
    }
    return name;
}

QString DockModel::resolveIconType(const QString& appId) const {
    const QString low = appId.toLower();
    if (low.contains(QStringLiteral("terminal")) || low.contains(QStringLiteral("foot")) ||
        low.contains(QStringLiteral("kitty"))    || low.contains(QStringLiteral("alacritty"))) {
        return QStringLiteral("terminal");
    }
    if (low.contains(QStringLiteral("files")) || low.contains(QStringLiteral("dolphin")) ||
        low.contains(QStringLiteral("nautilus")) || low.contains(QStringLiteral("folder"))) {
        return QStringLiteral("folder");
    }
    if (low.contains(QStringLiteral("settings")) || low.contains(QStringLiteral("config")) ||
        low.contains(QStringLiteral("control"))) {
        return QStringLiteral("gear");
    }
    if (low.contains(QStringLiteral("monitor")) || low.contains(QStringLiteral("top")) ||
        low.contains(QStringLiteral("task"))) {
        return QStringLiteral("barchart");
    }
    if (low.contains(QStringLiteral("firefox")) || low.contains(QStringLiteral("browser")) ||
        low.contains(QStringLiteral("chrome"))) {
        return QStringLiteral("firefox");
    }
    if (low.contains(QStringLiteral("store")) || low.contains(QStringLiteral("pkg")) ||
        low.contains(QStringLiteral("package"))) {
        return QStringLiteral("package");
    }
    return QStringLiteral("package");
}

void DockModel::recomputeItemState(DockItemData& item) {
    if (item.toplevels.isEmpty()) {
        item.toplevelCount = 0;
        item.appState      = DockAppState::NotRunning;
        if (item.kind == DockItemKind::PinnedRunning) {
            item.kind = DockItemKind::PinnedApp;
        }
        return;
    }

    item.toplevelCount = static_cast<int>(item.toplevels.size());
    if (item.kind == DockItemKind::PinnedApp) {
        item.kind = DockItemKind::PinnedRunning;
    }

    bool hasActive = false;
    bool allMinimized = true;
    for (const auto& t : item.toplevels) {
        if (t.isActivated) hasActive = true;
        if (!t.isMinimized) allMinimized = false;
    }

    if (hasActive) {
        item.appState = DockAppState::RunningFocused;
    } else if (allMinimized) {
        item.appState = DockAppState::Minimized;
    } else {
        item.appState = DockAppState::RunningBg;
    }
}

void DockModel::updateActiveStateAcrossAll(struct zwlr_foreign_toplevel_handle_v1* activeHandle) {
    auto updateList = [&](QList<DockItemData>& list) {
        for (auto& item : list) {
            bool changed = false;
            for (auto& t : item.toplevels) {
                const bool wasActive = t.isActivated;
                t.isActivated = (t.handle == activeHandle);
                if (wasActive != t.isActivated) changed = true;
            }
            if (changed) {
                recomputeItemState(item);
                const int row = findItemIndex(item.appId);
                if (row >= 0) {
                    m_merged[row] = item;
                    emit dataChanged(createIndex(row, 0), createIndex(row, 0),
                                     { DockRole::AppState, DockRole::IsActive });
                }
            }
        }
    };
    updateList(m_pinned);
    updateList(m_transient);
}

// ── Live Window Tracking Slots (Slice 2) ─────────────────────────────────────
void DockModel::onToplevelAddedWithHandle(struct zwlr_foreign_toplevel_handle_v1* handle,
                                        const QString& appId,
                                        const QString& title,
                                        bool isActivated,
                                        bool isMinimized,
                                        bool isMaximized)
{
    if (!handle) return;

    // Guard: check if handle already tracked
    DockItemData* existingItem = nullptr;
    const int existingRow = findItemIndexForHandle(handle, &existingItem);
    if (existingRow >= 0 && existingItem) {
        onToplevelUpdatedWithHandle(handle, appId, title, isActivated, isMinimized, isMaximized);
        return;
    }

    // 1. Check pinned items
    for (auto& pinned : m_pinned) {
        if (matchesAppId(pinned.appId, appId)) {
            pinned.toplevels.append(ToplevelRef{handle, title, isActivated, isMinimized, isMaximized});
            recomputeItemState(pinned);

            const int row = findItemIndex(pinned.appId);
            if (row >= 0) {
                m_merged[row] = pinned;
                emit dataChanged(createIndex(row, 0), createIndex(row, 0),
                                 { DockRole::Kind, DockRole::AppState, DockRole::IsRunning,
                                   DockRole::IsActive, DockRole::IsMinimized, DockRole::ToplevelCount });
            }
            if (isActivated) updateActiveStateAcrossAll(handle);
            tinexus::log::info("[DockModel] Pinned app '{}' updated with new toplevel (total: {})",
                               pinned.appId.toStdString(), pinned.toplevelCount);
            emit dockItemsChanged();
            return;
        }
    }

    // 2. Check transient items
    for (auto& trans : m_transient) {
        if (matchesAppId(trans.appId, appId)) {
            trans.toplevels.append(ToplevelRef{handle, title, isActivated, isMinimized, isMaximized});
            recomputeItemState(trans);

            const int row = findItemIndex(trans.appId);
            if (row >= 0) {
                m_merged[row] = trans;
                emit dataChanged(createIndex(row, 0), createIndex(row, 0),
                                 { DockRole::AppState, DockRole::IsRunning, DockRole::IsActive,
                                   DockRole::IsMinimized, DockRole::ToplevelCount });
            }
            if (isActivated) updateActiveStateAcrossAll(handle);
            tinexus::log::info("[DockModel] Transient app '{}' updated with new toplevel (total: {})",
                               trans.appId.toStdString(), trans.toplevelCount);
            emit dockItemsChanged();
            return;
        }
    }

    // 3. Not found anywhere — create new transient running item
    DockItemData transient;
    transient.kind         = DockItemKind::RunningApp;
    transient.appId        = appId;
    transient.displayName  = resolveDisplayName(appId, title);
    transient.execCmd      = appId;
    transient.iconType     = resolveIconType(appId);
    transient.toplevels.append(ToplevelRef{handle, title, isActivated, isMinimized, isMaximized});
    recomputeItemState(transient);
    transient.configIndex  = 5000 + static_cast<int>(m_transient.size());

    // Insert cleanly into m_merged right after pinned items + existing transient items
    const int insertRow = static_cast<int>(m_pinned.size() + m_transient.size());
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_transient.append(transient);
    m_merged.insert(insertRow, transient);
    endInsertRows();

    if (isActivated) updateActiveStateAcrossAll(handle);
    tinexus::log::info("[DockModel] Dynamically inserted transient app '{}' at row {}",
                       appId.toStdString(), insertRow);
    emit dockItemsChanged();
}

void DockModel::onToplevelUpdatedWithHandle(struct zwlr_foreign_toplevel_handle_v1* handle,
                                          const QString&,
                                          const QString& title,
                                          bool isActivated,
                                          bool isMinimized,
                                          bool isMaximized)
{
    if (!handle) return;

    DockItemData* targetItem = nullptr;
    const int row = findItemIndexForHandle(handle, &targetItem);
    if (row < 0 || !targetItem) return;

    bool modified = false;
    for (auto& t : targetItem->toplevels) {
        if (t.handle == handle) {
            t.title       = title;
            t.isActivated = isActivated;
            t.isMinimized = isMinimized;
            t.isMaximized = isMaximized;
            modified = true;
            break;
        }
    }

    if (modified) {
        recomputeItemState(*targetItem);
        m_merged[row] = *targetItem;
        emit dataChanged(createIndex(row, 0), createIndex(row, 0),
                         { DockRole::AppState, DockRole::IsActive, DockRole::IsMinimized,
                           DockRole::DisplayName, DockRole::ToplevelCount });
        if (isActivated) updateActiveStateAcrossAll(handle);
        emit dockItemsChanged();
    }
}

void DockModel::onToplevelRemovedWithHandle(struct zwlr_foreign_toplevel_handle_v1* handle,
                                          const QString&)
{
    if (!handle) return;

    // Check pinned items
    for (auto& pinned : m_pinned) {
        for (int i = 0; i < pinned.toplevels.size(); ++i) {
            if (pinned.toplevels.at(i).handle == handle) {
                pinned.toplevels.removeAt(i);
                recomputeItemState(pinned);

                const int row = findItemIndex(pinned.appId);
                if (row >= 0) {
                    m_merged[row] = pinned;
                    emit dataChanged(createIndex(row, 0), createIndex(row, 0),
                                     { DockRole::Kind, DockRole::AppState, DockRole::IsRunning,
                                       DockRole::IsActive, DockRole::IsMinimized, DockRole::ToplevelCount });
                }
                tinexus::log::info("[DockModel] Toplevel removed from pinned app '{}' (remaining: {})",
                                   pinned.appId.toStdString(), pinned.toplevelCount);
                emit dockItemsChanged();
                return;
            }
        }
    }

    // Check transient items
    for (int tIdx = 0; tIdx < m_transient.size(); ++tIdx) {
        auto& trans = m_transient[tIdx];
        for (int i = 0; i < trans.toplevels.size(); ++i) {
            if (trans.toplevels.at(i).handle == handle) {
                trans.toplevels.removeAt(i);
                if (trans.toplevels.isEmpty()) {
                    // No more windows for this transient app: remove from dock
                    const int row = findItemIndex(trans.appId);
                    if (row >= 0) {
                        beginRemoveRows(QModelIndex(), row, row);
                        m_merged.removeAt(row);
                        m_transient.removeAt(tIdx);
                        endRemoveRows();
                    }
                    tinexus::log::info("[DockModel] Transient app '{}' closed all windows — removed from dock",
                                       trans.appId.toStdString());
                } else {
                    recomputeItemState(trans);
                    const int row = findItemIndex(trans.appId);
                    if (row >= 0) {
                        m_merged[row] = trans;
                        emit dataChanged(createIndex(row, 0), createIndex(row, 0),
                                         { DockRole::AppState, DockRole::IsRunning, DockRole::IsActive,
                                           DockRole::IsMinimized, DockRole::ToplevelCount });
                    }
                }
                emit dockItemsChanged();
                return;
            }
        }
    }
}

// ── Legacy API ───────────────────────────────────────────────────────────────
void DockModel::onToplevelAdded(const QString& appId) {
    static quintptr nextHandle = 100;
    auto* handle = reinterpret_cast<zwlr_foreign_toplevel_handle_v1*>(++nextHandle);
    onToplevelAddedWithHandle(handle, appId, appId, false, false, false);
}

void DockModel::onToplevelRemoved(const QString& appId) {
    for (auto& item : m_pinned) {
        if (matchesAppId(item.appId, appId) && !item.toplevels.isEmpty()) {
            onToplevelRemovedWithHandle(item.toplevels.first().handle, appId);
            return;
        }
    }
    for (auto& item : m_transient) {
        if (matchesAppId(item.appId, appId) && !item.toplevels.isEmpty()) {
            onToplevelRemovedWithHandle(item.toplevels.first().handle, appId);
            return;
        }
    }
}

void DockModel::onFocusChanged(const QString& focusedAppId) {
    for (auto& item : m_pinned) {
        if (matchesAppId(item.appId, focusedAppId) && !item.toplevels.isEmpty()) {
            updateActiveStateAcrossAll(item.toplevels.first().handle);
            return;
        }
    }
    for (auto& item : m_transient) {
        if (matchesAppId(item.appId, focusedAppId) && !item.toplevels.isEmpty()) {
            updateActiveStateAcrossAll(item.toplevels.first().handle);
            return;
        }
    }
}

void DockModel::setBadgeCount(const QString& appId, uint32_t count) {
    auto* item = findMutableItem(appId);
    if (!item) return;
    item->badgeCount = count;
    const int row = findItemIndex(appId);
    if (row >= 0) {
        m_merged[row].badgeCount = count;
        emit dataChanged(createIndex(row, 0), createIndex(row, 0), { DockRole::BadgeCount });
    }
}

void DockModel::setNeedsAttention(const QString& appId, bool attention) {
    auto* item = findMutableItem(appId);
    if (!item) return;
    if (item->needsAttention != attention) {
        item->needsAttention = attention;
        const int row = findItemIndex(appId);
        if (row >= 0) {
            m_merged[row].needsAttention = attention;
            emit dataChanged(createIndex(row, 0), createIndex(row, 0), { DockRole::NeedsAttention });
        }
        emit dockItemsChanged();
        tinexus::log::info("[DockModel] App '{}' needsAttention set to {}", appId.toStdString(), attention);
    }
}

// ── Pin / Unpin Management (Slice 3) ─────────────────────────────────────────
bool DockModel::isPinned(const QString& appId) const {
    for (const auto& item : m_pinned) {
        if (matchesAppId(item.appId, appId)) return true;
    }
    return false;
}

DockItemData DockModel::getItemData(const QString& appId) const {
    for (const auto& item : m_merged) {
        if (matchesAppId(item.appId, appId)) return item;
    }
    return {};
}

void DockModel::moveItem(int fromDisplayIndex, int toDisplayIndex) {
    if (fromDisplayIndex == toDisplayIndex) return;
    if (fromDisplayIndex < 0 || fromDisplayIndex >= m_merged.size()) return;
    if (toDisplayIndex < 0 || toDisplayIndex >= m_merged.size()) return;

    const auto& fromItem = m_merged.at(fromDisplayIndex);
    int fromPinnedIdx = -1;
    for (int i = 0; i < m_pinned.size(); ++i) {
        if (matchesAppId(m_pinned[i].appId, fromItem.appId)) {
            fromPinnedIdx = i;
            break;
        }
    }
    if (fromPinnedIdx < 0) {
        tinexus::log::warn("[DockModel] moveItem: item at display pos {} ({}) is not pinned",
                           fromDisplayIndex, fromItem.appId.toStdString());
        return;
    }

    int toPinnedIdx = -1;
    const auto& toItem = m_merged.at(toDisplayIndex);
    for (int i = 0; i < m_pinned.size(); ++i) {
        if (matchesAppId(m_pinned[i].appId, toItem.appId)) {
            toPinnedIdx = i;
            break;
        }
    }

    if (toPinnedIdx < 0) {
        // Dragged outside pinned boundary (e.g. into transient or separator area)
        if (toDisplayIndex < fromDisplayIndex) {
            toPinnedIdx = 0;
        } else {
            toPinnedIdx = static_cast<int>(m_pinned.size()) - 1;
        }
    }

    if (fromPinnedIdx == toPinnedIdx) return;

    DockItemData item = m_pinned.takeAt(fromPinnedIdx);
    m_pinned.insert(toPinnedIdx, std::move(item));

    for (int i = 0; i < m_pinned.size(); ++i) {
        m_pinned[i].configIndex = i;
    }

    rebuildMergedView();
    savePinnedToConfig();
    tinexus::log::info("[DockModel] Moved pinned item '{}' from pinned pos {} to {} (display pos {} -> {})",
                       fromItem.appId.toStdString(), fromPinnedIdx, toPinnedIdx, fromDisplayIndex, toDisplayIndex);
}

void DockModel::commitMove() {
    tinexus::log::info("[DockModel] Drag rearrange committed. Saving new layout.");
    for (int i = 0; i < m_pinned.size(); ++i) {
        m_pinned[i].configIndex = i;
    }
    savePinnedToConfig();
    emit dockItemsChanged();
}

void DockModel::pinApp(const QString& appId, const QString& displayName, const QString& execCmd, const QString& iconType) {
    if (appId.isEmpty() || isPinned(appId)) return;

    // 1. Check if app is in m_transient (currently running)
    for (int i = 0; i < m_transient.size(); ++i) {
        if (matchesAppId(m_transient.at(i).appId, appId)) {
            DockItemData item = m_transient.takeAt(i);
            item.kind = (item.toplevelCount > 0) ? DockItemKind::PinnedRunning : DockItemKind::PinnedApp;
            if (!displayName.isEmpty()) item.displayName = displayName;
            if (!execCmd.isEmpty()) item.execCmd = execCmd;
            if (!iconType.isEmpty()) item.iconType = iconType;
            item.configIndex = static_cast<int>(m_pinned.size());
            m_pinned.append(std::move(item));
            rebuildMergedView();
            savePinnedToConfig();
            tinexus::log::info("[DockModel] Pinned running app '{}' to dock", appId.toStdString());
            return;
        }
    }

    // 2. Non-running app: create new pinned item
    DockItemData item;
    item.kind = DockItemKind::PinnedApp;
    item.appId = appId;
    item.displayName = !displayName.isEmpty() ? displayName : resolveDisplayName(appId, QString());
    item.execCmd = !execCmd.isEmpty() ? execCmd : appId;
    item.iconType = !iconType.isEmpty() ? iconType : resolveIconType(appId);
    item.configIndex = static_cast<int>(m_pinned.size());
    item.appState = DockAppState::NotRunning;
    item.toplevelCount = 0;

    m_pinned.append(std::move(item));
    rebuildMergedView();
    savePinnedToConfig();
    tinexus::log::info("[DockModel] Pinned non-running app '{}' to dock", appId.toStdString());
}

void DockModel::unpinApp(const QString& appId) {
    for (int i = 0; i < m_pinned.size(); ++i) {
        if (matchesAppId(m_pinned.at(i).appId, appId)) {
            DockItemData item = m_pinned.takeAt(i);
            if (item.toplevelCount > 0) {
                item.kind = DockItemKind::RunningApp;
                m_transient.append(std::move(item));
            }
            for (int p = 0; p < m_pinned.size(); ++p) {
                m_pinned[p].configIndex = p;
            }
            rebuildMergedView();
            savePinnedToConfig();
            tinexus::log::info("[DockModel] Unpinned app '{}' from dock", appId.toStdString());
            return;
        }
    }
}

void DockModel::toggleKeepInDock(const QString& appId) {
    if (isPinned(appId)) {
        unpinApp(appId);
    } else {
        pinApp(appId);
    }
}

void DockModel::removeFromDock(const QString& appId) {
    unpinApp(appId);
}

// ── Window Manipulation Commands ─────────────────────────────────────────────
void DockModel::activateApp(const QString& appId) {
    emit itemActivationRequested(appId);

    if (!m_tracker || !m_tracker->isConnected()) return;
    auto* item = findMutableItem(appId);
    if (!item || item->toplevels.isEmpty() || !item->toplevels.first().handle) return;

    if (item->appState == DockAppState::RunningFocused) {
        // App is already focused -> toggle minimization (Slice 3)
        minimizeApp(appId);
    } else {
        // App is minimized or background -> unminimize and raise
        if (item->appState == DockAppState::Minimized) {
            for (const auto& t : item->toplevels) {
                if (t.handle) m_tracker->setMinimized(t.handle, false);
            }
        }
        if (!item->toplevels.isEmpty() && item->toplevels.first().handle) {
            m_tracker->activateWindow(item->toplevels.first().handle);
        }
    }
}

void DockModel::minimizeApp(const QString& appId) {
    if (!m_tracker || !m_tracker->isConnected()) return;
    auto* item = findMutableItem(appId);
    if (!item || item->toplevels.isEmpty()) return;

    const bool allMin = (item->appState == DockAppState::Minimized);
    for (const auto& t : item->toplevels) {
        if (t.handle) {
            m_tracker->setMinimized(t.handle, !allMin);
        }
    }
}

void DockModel::closeApp(const QString& appId) {
    if (!m_tracker || !m_tracker->isConnected()) return;
    auto* item = findMutableItem(appId);
    if (!item || item->toplevels.isEmpty()) return;

    for (const auto& t : item->toplevels) {
        if (t.handle) {
            m_tracker->closeWindow(t.handle);
        }
    }
}

void DockModel::activateToplevel(struct zwlr_foreign_toplevel_handle_v1* handle) {
    if (!m_tracker || !m_tracker->isConnected() || !handle) return;
    m_tracker->activateWindow(handle);
}

void DockModel::clearAllToplevels() {
    tinexus::log::warn("[DockModel] Compositor/manager disconnected: flushing all toplevel handles");

    for (auto& item : m_pinned) {
        item.toplevels.clear();
        item.toplevelCount = 0;
        item.appState = DockAppState::NotRunning;
        item.kind = DockItemKind::PinnedApp;
    }

    m_transient.clear();

    rebuildMergedView();
    emit dockItemsChanged();
}

// ── Helpers ──────────────────────────────────────────────────────────────────
int DockModel::findItemIndex(const QString& appId) const {
    for (int i = 0; i < m_merged.size(); ++i) {
        if (m_merged.at(i).appId == appId) return i;
    }
    return -1;
}

DockItemData* DockModel::findMutableItem(const QString& appId) {
    for (auto& item : m_pinned)    { if (item.appId == appId || matchesAppId(item.appId, appId)) return &item; }
    for (auto& item : m_transient) { if (item.appId == appId || matchesAppId(item.appId, appId)) return &item; }
    for (auto& item : m_stacks)    { if (item.appId == appId) return &item; }
    return nullptr;
}

int DockModel::findItemIndexForHandle(struct zwlr_foreign_toplevel_handle_v1* handle,
                                     DockItemData** outItem) const {
    for (int i = 0; i < m_pinned.size(); ++i) {
        for (const auto& t : m_pinned.at(i).toplevels) {
            if (t.handle == handle) {
                if (outItem) *outItem = const_cast<DockItemData*>(&m_pinned.at(i));
                return findItemIndex(m_pinned.at(i).appId);
            }
        }
    }
    for (int i = 0; i < m_transient.size(); ++i) {
        for (const auto& t : m_transient.at(i).toplevels) {
            if (t.handle == handle) {
                if (outItem) *outItem = const_cast<DockItemData*>(&m_transient.at(i));
                return findItemIndex(m_transient.at(i).appId);
            }
        }
    }
    return -1;
}

} // namespace tinexus::dock

#include "moc_DockModel.cpp"
