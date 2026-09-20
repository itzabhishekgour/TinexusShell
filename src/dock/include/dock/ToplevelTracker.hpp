// ============================================================================
// ToplevelTracker.hpp — Wayland Foreign Toplevel Management Client (Slice 2)
// Tracks running application windows, titles, app_ids, states (activated,
// minimized, maximized), and ensures atomic updates on the 'done' event.
// Ref: Architecture Blueprint §2.2, §3.2, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QSocketNotifier>
#include <memory>

struct wl_display;
struct wl_registry;
struct wl_seat;
struct wl_output;
struct wl_array;
struct zwlr_foreign_toplevel_manager_v1;
struct zwlr_foreign_toplevel_handle_v1;

namespace tinexus::dock {

struct WindowState {
    QString title;
    QString appId;
    bool isActivated{false};
    bool isMinimized{false};
    bool isMaximized{false};
    bool isFullscreen{false};
};

struct TrackedWindow {
    struct zwlr_foreign_toplevel_handle_v1* handle{nullptr};
    WindowState committed{};
    WindowState pending{};
    bool isCommittedOnce{false};
};

class ToplevelTracker : public QObject {
    Q_OBJECT

public:
    explicit ToplevelTracker(QObject* parent = nullptr);
    ~ToplevelTracker() override;

    /// Connects to Wayland display, binds the foreign toplevel manager global,
    /// sets up QSocketNotifier on the Wayland display FD, and performs initial roundtrip.
    bool init(struct wl_display* display = nullptr);
    void shutdown();

    [[nodiscard]] bool isConnected() const { return m_display != nullptr && m_manager != nullptr; }
    [[nodiscard]] struct wl_display* display() const { return m_display; }
    [[nodiscard]] struct wl_seat* seat() const { return m_seat; }

    [[nodiscard]] const QHash<struct zwlr_foreign_toplevel_handle_v1*, TrackedWindow>& windows() const {
        return m_windows;
    }

    // ── Window Actions (Layer 3 interaction hooks) ───────────────────────────
    void activateWindow(struct zwlr_foreign_toplevel_handle_v1* handle, struct wl_seat* seat = nullptr);
    void setMinimized(struct zwlr_foreign_toplevel_handle_v1* handle, bool minimized);
    void setMaximized(struct zwlr_foreign_toplevel_handle_v1* handle, bool maximized);
    void closeWindow(struct zwlr_foreign_toplevel_handle_v1* handle);

signals:
    /// Emitted atomically when a new toplevel window has received its initial 'done' event.
    void toplevelAdded(struct zwlr_foreign_toplevel_handle_v1* handle,
                       const QString& appId,
                       const QString& title,
                       bool isActivated,
                       bool isMinimized,
                       bool isMaximized,
                       bool isFullscreen = false);

    /// Emitted atomically when an existing toplevel window's state or properties change (on 'done').
    void toplevelUpdated(struct zwlr_foreign_toplevel_handle_v1* handle,
                         const QString& appId,
                         const QString& title,
                         bool isActivated,
                         bool isMinimized,
                         bool isMaximized,
                         bool isFullscreen = false);

    /// Emitted when a toplevel window is closed and destroyed.
    void toplevelRemoved(struct zwlr_foreign_toplevel_handle_v1* handle,
                         const QString& appId);

    /// Emitted when the compositor tears down foreign toplevel manager global.
    void managerFinished();

private slots:
    void onWaylandSocketReadable();

private:
    void setupSocketNotifier();

    // ── Static C Wayland callbacks ───────────────────────────────────────────
    static void handleRegistryGlobal(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
    static void handleRegistryGlobalRemove(void* data, struct wl_registry* registry, uint32_t name);

    static void handleManagerToplevel(void* data, struct zwlr_foreign_toplevel_manager_v1* manager, struct zwlr_foreign_toplevel_handle_v1* toplevel);
    static void handleManagerFinished(void* data, struct zwlr_foreign_toplevel_manager_v1* manager);

    static void handleToplevelTitle(void* data, struct zwlr_foreign_toplevel_handle_v1* handle, const char* title);
    static void handleToplevelAppId(void* data, struct zwlr_foreign_toplevel_handle_v1* handle, const char* app_id);
    static void handleToplevelOutputEnter(void* data, struct zwlr_foreign_toplevel_handle_v1* handle, struct wl_output* output);
    static void handleToplevelOutputLeave(void* data, struct zwlr_foreign_toplevel_handle_v1* handle, struct wl_output* output);
    static void handleToplevelState(void* data, struct zwlr_foreign_toplevel_handle_v1* handle, struct wl_array* state);
    static void handleToplevelDone(void* data, struct zwlr_foreign_toplevel_handle_v1* handle);
    static void handleToplevelClosed(void* data, struct zwlr_foreign_toplevel_handle_v1* handle);
    static void handleToplevelParent(void* data, struct zwlr_foreign_toplevel_handle_v1* handle, struct zwlr_foreign_toplevel_handle_v1* parent);

    struct wl_display* m_display{nullptr};
    struct wl_registry* m_registry{nullptr};
    struct wl_seat* m_seat{nullptr};
    struct zwlr_foreign_toplevel_manager_v1* m_manager{nullptr};
    uint32_t m_managerGlobalName{0};

    bool m_ownsDisplay{false};
    std::unique_ptr<QSocketNotifier> m_socketNotifier;

    QHash<struct zwlr_foreign_toplevel_handle_v1*, TrackedWindow> m_windows;
};

} // namespace tinexus::dock
