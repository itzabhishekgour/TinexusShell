// ============================================================================
// DockModel.cpp — QAbstractListModel implementation for tinexus-dock (Slice 1)
// Ref: Architecture Blueprint §3, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#include "dock/DockModel.hpp"
#include <common/logger.hpp>
#include <algorithm>

namespace tinexus::dock {

// ── Constructor ──────────────────────────────────────────────────────────────
DockModel::DockModel(QObject* parent)
    : QAbstractListModel(parent)
{
    loadHardcodedPinnedItems();
    appendSeparatorAndStacks();
    rebuildMergedView();

    tinexus::log::info("[DockModel] Initialized with {} merged items", m_merged.size());
}

// ── Data Loading (Slice 1: hardcoded — Slice 3+ reads from dock.toml via Settings IPC) ──
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

void DockModel::appendSeparatorAndStacks() {
    m_stacks.clear();

    // Separator (always separates pinned zone from Stacks zone)
    DockItemData sep;
    sep.kind        = DockItemKind::Separator;
    sep.appId       = QStringLiteral("__separator__");
    sep.displayName = {};
    sep.iconType    = QStringLiteral("separator");
    sep.configIndex = 9000;
    m_stacks.append(sep);

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
}

// ── QAbstractListModel ───────────────────────────────────────────────────────
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

// ── Merge Engine — Slice 2 stubs ─────────────────────────────────────────────
void DockModel::onToplevelAdded(const QString& appId) {
    // Check if this app is in the pinned list
    for (auto& item : m_pinned) {
        if (item.appId == appId) {
            item.kind = DockItemKind::PinnedRunning;
            item.appState = DockAppState::RunningBg;
            item.toplevelCount++;
            // Find and update the merged row without full reset
            const int row = findItemIndex(appId);
            if (row >= 0) {
                m_merged[row] = item;
                emit dataChanged(createIndex(row, 0), createIndex(row, 0));
            }
            tinexus::log::info("[DockModel] Pinned app '{}' is now running", appId.toStdString());
            return;
        }
    }

    // Not pinned — add as transient
    DockItemData transient;
    transient.kind         = DockItemKind::RunningApp;
    transient.appId        = appId;
    transient.displayName  = appId; // Will be resolved from .desktop in Slice 2
    transient.appState     = DockAppState::RunningBg;
    transient.toplevelCount = 1;
    m_transient.append(transient);
    rebuildMergedView();

    tinexus::log::info("[DockModel] Transient app '{}' added", appId.toStdString());
}

void DockModel::onToplevelRemoved(const QString& appId) {
    // Check pinned first
    for (auto& item : m_pinned) {
        if (item.appId == appId && item.toplevelCount > 0) {
            item.toplevelCount--;
            if (item.toplevelCount == 0) {
                item.kind     = DockItemKind::PinnedApp;
                item.appState = DockAppState::NotRunning;
            }
            const int row = findItemIndex(appId);
            if (row >= 0) {
                m_merged[row] = item;
                emit dataChanged(createIndex(row, 0), createIndex(row, 0));
            }
            return;
        }
    }

    // Remove transient
    for (int i = 0; i < m_transient.size(); ++i) {
        if (m_transient.at(i).appId == appId) {
            m_transient.remove(i);
            rebuildMergedView();
            return;
        }
    }
}

void DockModel::onFocusChanged(const QString& focusedAppId) {
    // Update all items — only the focused one becomes RunningFocused
    bool changed = false;
    for (auto& item : m_pinned) {
        const DockAppState newState = (item.appId == focusedAppId && item.toplevelCount > 0)
            ? DockAppState::RunningFocused
            : (item.toplevelCount > 0 ? DockAppState::RunningBg : DockAppState::NotRunning);
        if (item.appState != newState) {
            item.appState = newState;
            changed = true;
        }
    }
    for (auto& item : m_transient) {
        const DockAppState newState = (item.appId == focusedAppId)
            ? DockAppState::RunningFocused : DockAppState::RunningBg;
        if (item.appState != newState) {
            item.appState = newState;
            changed = true;
        }
    }
    if (changed) {
        // Sync merged view states without full reset (O(n) scan)
        for (auto& merged : m_merged) {
            if (merged.kind == DockItemKind::Separator ||
                merged.kind == DockItemKind::TrashCan) continue;
            merged.appState = (merged.appId == focusedAppId && merged.toplevelCount > 0)
                ? DockAppState::RunningFocused
                : (merged.toplevelCount > 0 ? DockAppState::RunningBg : DockAppState::NotRunning);
        }
        emit dataChanged(createIndex(0, 0), createIndex(m_merged.size() - 1, 0),
                         { DockRole::AppState, DockRole::IsActive });
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

void DockModel::moveItem(int fromDisplayIndex, int toDisplayIndex) {
    if (fromDisplayIndex == toDisplayIndex) return;
    if (fromDisplayIndex < 0 || fromDisplayIndex >= m_merged.size()) return;
    if (toDisplayIndex   < 0 || toDisplayIndex   >= m_merged.size()) return;

    // Guard: never move a Separator or Trash
    const DockItemKind fromKind = m_merged.at(fromDisplayIndex).kind;
    if (fromKind == DockItemKind::Separator || fromKind == DockItemKind::TrashCan) return;

    beginMoveRows({}, fromDisplayIndex, fromDisplayIndex, {},
                  toDisplayIndex > fromDisplayIndex ? toDisplayIndex + 1 : toDisplayIndex);
    m_merged.move(fromDisplayIndex, toDisplayIndex);
    endMoveRows();
    // TODO Slice 5: persist new order to dock.toml via Settings IPC
}

// ── Helpers ──────────────────────────────────────────────────────────────────
int DockModel::findItemIndex(const QString& appId) const {
    for (int i = 0; i < m_merged.size(); ++i) {
        if (m_merged.at(i).appId == appId) return i;
    }
    return -1;
}

DockItemData* DockModel::findMutableItem(const QString& appId) {
    for (auto& item : m_pinned)    { if (item.appId == appId) return &item; }
    for (auto& item : m_transient) { if (item.appId == appId) return &item; }
    for (auto& item : m_stacks)    { if (item.appId == appId) return &item; }
    return nullptr;
}

} // namespace tinexus::dock

#include "moc_DockModel.cpp"
