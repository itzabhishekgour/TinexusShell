// ============================================================================
// DockModel.hpp — QAbstractListModel for tinexus-dock (Slice 2: Live Window Tracking)
// Merges pinned apps (from config/hardcoded) with live Wayland toplevels.
// Ref: Architecture Blueprint §3, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QList>
#include <cstdint>

struct zwlr_foreign_toplevel_handle_v1;

namespace tinexus::dock {

class ToplevelTracker;

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

// ── Toplevel Reference (live Wayland window handle) ───────────────────────────
struct ToplevelRef {
    struct zwlr_foreign_toplevel_handle_v1* handle{nullptr};
    QString title;
    bool isActivated{false};
    bool isMinimized{false};
    bool isMaximized{false};
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

    // Runtime state (populated from ToplevelTracker in Slice 2)
    DockAppState       appState{DockAppState::NotRunning};
    int                toplevelCount{0};      // number of open windows
    uint32_t           badgeCount{0};         // notification badge
    bool               needsAttention{false};
    bool               isDeletedFromFS{false}; // show "?" badge
    QList<ToplevelRef> toplevels;             // live Wayland window references

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

    // ── Live Window Tracking Slots (Slice 2) ─────────────────────────────────
    /// Atomic toplevel registration from ToplevelTracker on 'done'
    void onToplevelAddedWithHandle(struct zwlr_foreign_toplevel_handle_v1* handle,
                                  const QString& appId,
                                  const QString& title,
                                  bool isActivated,
                                  bool isMinimized,
                                  bool isMaximized);

    /// Atomic toplevel update from ToplevelTracker on 'done'
    void onToplevelUpdatedWithHandle(struct zwlr_foreign_toplevel_handle_v1* handle,
                                    const QString& appId,
                                    const QString& title,
                                    bool isActivated,
                                    bool isMinimized,
                                    bool isMaximized);

    /// Atomic toplevel removal from ToplevelTracker on 'closed'
    void onToplevelRemovedWithHandle(struct zwlr_foreign_toplevel_handle_v1* handle,
                                    const QString& appId);

    // Legacy/convenience API
    void onToplevelAdded(const QString& appId);
    void onToplevelRemoved(const QString& appId);
    void onFocusChanged(const QString& focusedAppId);
    void setBadgeCount(const QString& appId, uint32_t count);
    void setNeedsAttention(const QString& appId, bool attention);

    [[nodiscard]] bool autoHideEnabled() const { return m_autoHide; }
    void setAutoHideEnabled(bool enabled);

    // Direct access for DockBridge animation layer
    [[nodiscard]] const QList<DockItemData>& pinnedItems() const { return m_pinned; }
    [[nodiscard]] const QList<DockItemData>& mergedItems() const { return m_merged; }
    [[nodiscard]] int pinnedCount() const { return static_cast<int>(m_pinned.size()); }
    [[nodiscard]] int findItemIndex(const QString& appId) const;
    [[nodiscard]] bool isPinned(const QString& appId) const;
    [[nodiscard]] DockItemData getItemData(const QString& appId) const;

    void setTracker(ToplevelTracker* tracker) { m_tracker = tracker; }
    [[nodiscard]] ToplevelTracker* tracker() const { return m_tracker; }

public slots:
    /// Reorder a pinned item — called after drag-and-drop (Slice 5).
    void moveItem(int fromDisplayIndex, int toDisplayIndex);
    void commitMove();

    /// Pin / Unpin management (Slice 3: Context Menu & D-Bus)
    void pinApp(const QString& appId, const QString& displayName = {}, const QString& execCmd = {}, const QString& iconType = {});
    void unpinApp(const QString& appId);
    void toggleKeepInDock(const QString& appId);
    void removeFromDock(const QString& appId);

    /// Window manipulation commands
    void activateApp(const QString& appId);
    void minimizeApp(const QString& appId);
    void closeApp(const QString& appId);
    void activateToplevel(struct zwlr_foreign_toplevel_handle_v1* handle);

    /// Compositor crash/disconnect handler (Slice 2 recovery)
    void clearAllToplevels();

signals:
    void itemActivationRequested(const QString& appId);
    void dockItemsChanged();

private:
    void loadHardcodedPinnedItems();
    void loadPinnedFromConfig();
    void savePinnedToConfig();
    void appendSeparatorAndStacks();
    void rebuildMergedView();

    DockItemData* findMutableItem(const QString& appId);
    int findItemIndexForHandle(struct zwlr_foreign_toplevel_handle_v1* handle,
                               DockItemData** outItem = nullptr) const;

    bool matchesAppId(const QString& itemAppId, const QString& incomingAppId) const;
    QString resolveDisplayName(const QString& appId, const QString& title) const;
    QString resolveIconType(const QString& appId) const;
    void recomputeItemState(DockItemData& item);
    void updateActiveStateAcrossAll(struct zwlr_foreign_toplevel_handle_v1* activeHandle);

    // ── Data sources ─────────────────────────────────────────────────────────
    QList<DockItemData> m_pinned;     // Source 1: pinned apps
    QList<DockItemData> m_transient;  // Source 2: running-only apps
    QList<DockItemData> m_stacks;     // Source 3: Stacks zone

    // ── Merged flat list (what QML sees via ListView) ─────────────────────────
    QList<DockItemData> m_merged;

    ToplevelTracker* m_tracker{nullptr};
    bool m_autoHide{false};
};

} // namespace tinexus::dock
