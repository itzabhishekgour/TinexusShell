// ============================================================================
// DockModel.hpp — QAbstractListModel for tinexus-dock (Slice 1)
// Merges pinned apps (from config/hardcoded) with future live Wayland toplevels.
// Ref: Architecture Blueprint §3, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QList>
#include <cstdint>

namespace tinexus::dock {

// ── Item Kind ────────────────────────────────────────────────────────────────
enum class DockItemKind : uint8_t {
    PinnedApp     = 0,  // Pinned only — not running
    RunningApp    = 1,  // Transient running — not pinned
    PinnedRunning = 2,  // Pinned AND running (has active toplevels)
    Separator     = 3,  // The visual divider line
    RecentApp     = 4,  // Recent (Stacks zone, not pinned)
    Stack         = 5,  // Folder Stack (Stacks zone)
    TrashCan      = 6,  // Permanent trash icon
};

// ── App State Flags (for running apps) ───────────────────────────────────────
enum class DockAppState : int {
    NotRunning     = 0,
    RunningBg      = 1,  // Running but not focused
    RunningFocused = 2,  // Active/focused window
    Minimized      = 3,
};

// ── Single dock item POD ─────────────────────────────────────────────────────
struct DockItemData {
    DockItemKind kind{DockItemKind::PinnedApp};

    // Identity
    QString appId;        // e.g. "org.gnome.Terminal" or "tinexus-terminal"
    QString displayName;
    QString execCmd;      // Shell-split exec command
    QString iconType;     // "terminal" | "folder" | "gear" | "barchart" | "firefox" | "package" | "separator" | "trash"
    QUrl    iconUrl;      // Future: resolved XDG icon path

    // Runtime state (populated in Slice 2 from ToplevelTracker)
    DockAppState appState{DockAppState::NotRunning};
    int          toplevelCount{0};      // number of open windows
    uint32_t     badgeCount{0};         // notification badge
    bool         needsAttention{false};
    bool         isDeletedFromFS{false}; // show "?" badge

    // Sort / display position
    int configIndex{0};  // stable sort key from config
};

// ── Qt Roles ─────────────────────────────────────────────────────────────────
namespace DockRole {
    inline constexpr int AppId          = Qt::UserRole + 1;
    inline constexpr int DisplayName    = Qt::UserRole + 2;
    inline constexpr int ExecCmd        = Qt::UserRole + 3;
    inline constexpr int IconType       = Qt::UserRole + 4;
    inline constexpr int IconUrl        = Qt::UserRole + 5;
    inline constexpr int Kind           = Qt::UserRole + 6;
    inline constexpr int AppState       = Qt::UserRole + 7;
    inline constexpr int IsRunning      = Qt::UserRole + 8;
    inline constexpr int IsActive       = Qt::UserRole + 9;
    inline constexpr int IsMinimized    = Qt::UserRole + 10;
    inline constexpr int ToplevelCount  = Qt::UserRole + 11;
    inline constexpr int BadgeCount     = Qt::UserRole + 12;
    inline constexpr int NeedsAttention = Qt::UserRole + 13;
    inline constexpr int IsDeleted      = Qt::UserRole + 14;
    inline constexpr int IsSeparator    = Qt::UserRole + 15;
} // namespace DockRole

// ── Model ────────────────────────────────────────────────────────────────────
class DockModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit DockModel(QObject* parent = nullptr);
    ~DockModel() override = default;

    // QAbstractListModel overrides
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // ── C++ API (for Slice 2 — ToplevelTracker integration) ─────────────────
    /// Called when a new Wayland toplevel appears with the given app_id.
    void onToplevelAdded(const QString& appId);
    /// Called when a Wayland toplevel is removed.
    void onToplevelRemoved(const QString& appId);
    /// Called when focus changes across toplevels.
    void onFocusChanged(const QString& focusedAppId);
    /// Update badge count (from Notifications D-Bus).
    void setBadgeCount(const QString& appId, uint32_t count);

    // Direct access for DockBridge animation layer
    [[nodiscard]] const QList<DockItemData>& pinnedItems() const { return m_pinned; }
    [[nodiscard]] int findItemIndex(const QString& appId) const;

public slots:
    /// Reorder a pinned item — called after drag-and-drop (Slice 5).
    void moveItem(int fromDisplayIndex, int toDisplayIndex);

signals:
    void itemActivationRequested(const QString& appId);

private:
    void loadHardcodedPinnedItems();
    void appendSeparatorAndStacks();
    void rebuildMergedView();

    DockItemData* findMutableItem(const QString& appId);

    // ── Data sources ─────────────────────────────────────────────────────────
    QList<DockItemData> m_pinned;     // Source 1: pinned apps (from config / hardcoded for Slice 1)
    QList<DockItemData> m_transient;  // Source 2: running-only apps (Slice 2 — ToplevelTracker)
    QList<DockItemData> m_stacks;     // Source 3: Stacks zone (Slice 7)

    // ── Merged flat list (what QML sees via ListView) ─────────────────────────
    QList<DockItemData> m_merged;
};

} // namespace tinexus::dock
