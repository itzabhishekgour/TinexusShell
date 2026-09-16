// ============================================================================
// ToplevelTracker.cpp — Wayland Foreign Toplevel Management Client (Slice 2)
// Ref: Architecture Blueprint §2.2, §3.2, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#include "dock/ToplevelTracker.hpp"
#include <common/logger.hpp>
#include "wayland/protocol/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include <QtGui/QGuiApplication>
#include <wayland-client.h>
#include <cstring>

namespace tinexus::dock {

// ── Constructor & Destructor ────────────────────────────────────────────────
ToplevelTracker::ToplevelTracker(QObject* parent)
    : QObject(parent)
{
}

ToplevelTracker::~ToplevelTracker() {
    shutdown();
}

// ── Initialization & Teardown ────────────────────────────────────────────────
bool ToplevelTracker::init(struct wl_display* display) {
    if (m_display) {
        tinexus::log::warn("[ToplevelTracker] Already initialized");
        return true;
    }

    if (display) {
        m_display = display;
        m_ownsDisplay = false;
    } else {
        m_display = wl_display_connect(nullptr);
        if (!m_display) {
            tinexus::log::error("[ToplevelTracker] Failed to connect to Wayland display");
            return false;
        }
        m_ownsDisplay = true;
    }

    tinexus::log::info("[ToplevelTracker] Connected to Wayland display (fd={})",
                       wl_display_get_fd(m_display));

    // Bind Wayland registry to discover foreign toplevel manager global
    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        tinexus::log::error("[ToplevelTracker] Failed to get Wayland registry");
        shutdown();
        return false;
    }

    static const struct wl_registry_listener registry_listener = {
        .global        = handleRegistryGlobal,
        .global_remove = handleRegistryGlobalRemove,
    };
    wl_registry_add_listener(m_registry, &registry_listener, this);

    // Initial roundtrip to announce and bind globals
    if (wl_display_roundtrip(m_display) < 0) {
        tinexus::log::error("[ToplevelTracker] Initial roundtrip failed");
        shutdown();
        return false;
    }

    if (!m_manager) {
        tinexus::log::warn("[ToplevelTracker] zwlr_foreign_toplevel_manager_v1 global NOT advertised by compositor — running in fallback mode");
    } else {
        static const struct zwlr_foreign_toplevel_manager_v1_listener manager_listener = {
            .toplevel = handleManagerToplevel,
            .finished = handleManagerFinished,
        };
        zwlr_foreign_toplevel_manager_v1_add_listener(m_manager, &manager_listener, this);

        // Second roundtrip to receive initial toplevels already open at startup
        wl_display_roundtrip(m_display);
        tinexus::log::info("[ToplevelTracker] Bound foreign toplevel manager. Active tracked windows: {}",
                           m_windows.size());
    }

    setupSocketNotifier();
    wl_display_flush(m_display);
    return true;
}

void ToplevelTracker::shutdown() {
    if (m_socketNotifier) {
        m_socketNotifier->setEnabled(false);
        m_socketNotifier.reset();
    }

    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.key()) {
            zwlr_foreign_toplevel_handle_v1_destroy(it.key());
        }
    }
    m_windows.clear();

    if (m_manager) {
        zwlr_foreign_toplevel_manager_v1_stop(m_manager);
        zwlr_foreign_toplevel_manager_v1_destroy(m_manager);
        m_manager = nullptr;
    }

    if (m_seat) {
        wl_seat_destroy(m_seat);
        m_seat = nullptr;
    }

    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }

    if (m_display && m_ownsDisplay) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
}

void ToplevelTracker::setupSocketNotifier() {
    if (!m_display) return;
    const int fd = wl_display_get_fd(m_display);
    if (fd < 0) return;

    m_socketNotifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read, this);
    connect(m_socketNotifier.get(), &QSocketNotifier::activated,
            this, &ToplevelTracker::onWaylandSocketReadable);
}

void ToplevelTracker::onWaylandSocketReadable() {
    if (!m_display) return;

    while (wl_display_prepare_read(m_display) != 0) {
        wl_display_dispatch_pending(m_display);
    }
    if (wl_display_read_events(m_display) < 0) {
        tinexus::log::error("[ToplevelTracker] wl_display_read_events failed — compositor connection lost");
        shutdown();
        return;
    }
    wl_display_dispatch_pending(m_display);
    wl_display_flush(m_display);
}

// ── Window Actions ───────────────────────────────────────────────────────────
void ToplevelTracker::activateWindow(struct zwlr_foreign_toplevel_handle_v1* handle, struct wl_seat* seat) {
    if (!handle) return;
    struct wl_seat* targetSeat = seat ? seat : m_seat;
    if (!targetSeat) {
        tinexus::log::warn("[ToplevelTracker] activateWindow requested with no wl_seat available");
        return;
    }
    zwlr_foreign_toplevel_handle_v1_activate(handle, targetSeat);
    if (m_display) wl_display_flush(m_display);
}

void ToplevelTracker::setMinimized(struct zwlr_foreign_toplevel_handle_v1* handle, bool minimized) {
    if (!handle) return;
    if (minimized) {
        zwlr_foreign_toplevel_handle_v1_set_minimized(handle);
    } else {
        zwlr_foreign_toplevel_handle_v1_unset_minimized(handle);
    }
    if (m_display) wl_display_flush(m_display);
}

void ToplevelTracker::setMaximized(struct zwlr_foreign_toplevel_handle_v1* handle, bool maximized) {
    if (!handle) return;
    if (maximized) {
        zwlr_foreign_toplevel_handle_v1_set_maximized(handle);
    } else {
        zwlr_foreign_toplevel_handle_v1_unset_maximized(handle);
    }
    if (m_display) wl_display_flush(m_display);
}

void ToplevelTracker::closeWindow(struct zwlr_foreign_toplevel_handle_v1* handle) {
    if (!handle) return;
    zwlr_foreign_toplevel_handle_v1_close(handle);
    if (m_display) wl_display_flush(m_display);
}

// ── Static Registry Callbacks ────────────────────────────────────────────────
void ToplevelTracker::handleRegistryGlobal(void* data, struct wl_registry* registry,
                                          uint32_t name, const char* interface, uint32_t version) {
    auto* self = static_cast<ToplevelTracker*>(data);
    if (std::strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        const uint32_t bindVersion = (version >= 3 ? 3 : (version >= 1 ? version : 1));
        self->m_manager = static_cast<zwlr_foreign_toplevel_manager_v1*>(
            wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface, bindVersion));
        self->m_managerGlobalName = name;
        tinexus::log::info("[ToplevelTracker] Bound zwlr_foreign_toplevel_manager_v1 version {}", bindVersion);
    } else if (std::strcmp(interface, "wl_seat") == 0) {
        if (!self->m_seat) {
            self->m_seat = static_cast<wl_seat*>(
                wl_registry_bind(registry, name, &wl_seat_interface, version >= 7 ? 7 : version));
            tinexus::log::info("[ToplevelTracker] Bound default wl_seat (global={})", name);
        }
    }
}

void ToplevelTracker::handleRegistryGlobalRemove(void* data, struct wl_registry*, uint32_t name) {
    auto* self = static_cast<ToplevelTracker*>(data);
    if (self->m_managerGlobalName == name && self->m_manager) {
        tinexus::log::warn("[ToplevelTracker] zwlr_foreign_toplevel_manager_v1 global removed");
        emit self->managerFinished();
        zwlr_foreign_toplevel_manager_v1_destroy(self->m_manager);
        self->m_manager = nullptr;
        self->m_managerGlobalName = 0;
    }
}

// ── Static Manager Callbacks ─────────────────────────────────────────────────
void ToplevelTracker::handleManagerToplevel(void* data, struct zwlr_foreign_toplevel_manager_v1*,
                                           struct zwlr_foreign_toplevel_handle_v1* toplevel) {
    auto* self = static_cast<ToplevelTracker*>(data);
    if (!toplevel) return;

    TrackedWindow win;
    win.handle = toplevel;
    self->m_windows.insert(toplevel, win);

    static const struct zwlr_foreign_toplevel_handle_v1_listener handle_listener = {
        .title        = handleToplevelTitle,
        .app_id       = handleToplevelAppId,
        .output_enter = handleToplevelOutputEnter,
        .output_leave = handleToplevelOutputLeave,
        .state        = handleToplevelState,
        .done         = handleToplevelDone,
        .closed       = handleToplevelClosed,
        .parent       = handleToplevelParent,
    };
    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel, &handle_listener, self);
}

void ToplevelTracker::handleManagerFinished(void* data, struct zwlr_foreign_toplevel_manager_v1*) {
    auto* self = static_cast<ToplevelTracker*>(data);
    tinexus::log::warn("[ToplevelTracker] Compositor emitted foreign toplevel manager finished");
    emit self->managerFinished();
}

// ── Static Handle Callbacks ──────────────────────────────────────────────────
void ToplevelTracker::handleToplevelTitle(void* data, struct zwlr_foreign_toplevel_handle_v1* handle,
                                         const char* title) {
    auto* self = static_cast<ToplevelTracker*>(data);
    auto it = self->m_windows.find(handle);
    if (it == self->m_windows.end()) return;
    it.value().pending.title = title ? QString::fromUtf8(title) : QString();
}

void ToplevelTracker::handleToplevelAppId(void* data, struct zwlr_foreign_toplevel_handle_v1* handle,
                                         const char* app_id) {
    auto* self = static_cast<ToplevelTracker*>(data);
    auto it = self->m_windows.find(handle);
    if (it == self->m_windows.end()) return;
    it.value().pending.appId = app_id ? QString::fromUtf8(app_id) : QString();
}

void ToplevelTracker::handleToplevelOutputEnter(void*, struct zwlr_foreign_toplevel_handle_v1*, struct wl_output*) {
    // Reserved for multi-monitor dock output affinity
}

void ToplevelTracker::handleToplevelOutputLeave(void*, struct zwlr_foreign_toplevel_handle_v1*, struct wl_output*) {
    // Reserved for multi-monitor dock output affinity
}

void ToplevelTracker::handleToplevelState(void* data, struct zwlr_foreign_toplevel_handle_v1* handle,
                                         struct wl_array* state) {
    auto* self = static_cast<ToplevelTracker*>(data);
    auto it = self->m_windows.find(handle);
    if (it == self->m_windows.end()) return;

    WindowState& pending = it.value().pending;
    pending.isMaximized  = false;
    pending.isMinimized  = false;
    pending.isActivated  = false;
    pending.isFullscreen = false;

    if (state && state->data) {
        const auto* entries = static_cast<const uint32_t*>(state->data);
        const size_t count = state->size / sizeof(uint32_t);
        for (size_t i = 0; i < count; ++i) {
            switch (entries[i]) {
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED:
                pending.isMaximized = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED:
                pending.isMinimized = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED:
                pending.isActivated = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN:
                pending.isFullscreen = true;
                break;
            default:
                break;
            }
        }
    }
}

void ToplevelTracker::handleToplevelDone(void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
    auto* self = static_cast<ToplevelTracker*>(data);
    auto it = self->m_windows.find(handle);
    if (it == self->m_windows.end()) return;

    TrackedWindow& win = it.value();
    const bool isFirstDone = !win.isCommittedOnce;
    const WindowState prev = win.committed;
    win.committed = win.pending;
    win.isCommittedOnce = true;

    // Treat empty appId as fallback
    if (win.committed.appId.isEmpty() && !win.committed.title.isEmpty()) {
        win.committed.appId = win.committed.title;
    }

    if (isFirstDone) {
        tinexus::log::info("[ToplevelTracker] toplevelAdded: app_id='{}' title='{}' activated={} min={} max={}",
                           win.committed.appId.toStdString(),
                           win.committed.title.toStdString(),
                           win.committed.isActivated,
                           win.committed.isMinimized,
                           win.committed.isMaximized);
        emit self->toplevelAdded(handle,
                                 win.committed.appId,
                                 win.committed.title,
                                 win.committed.isActivated,
                                 win.committed.isMinimized,
                                 win.committed.isMaximized);
    } else {
        const bool stateChanged = (prev.isActivated  != win.committed.isActivated  ||
                                   prev.isMinimized  != win.committed.isMinimized  ||
                                   prev.isMaximized  != win.committed.isMaximized  ||
                                   prev.isFullscreen != win.committed.isFullscreen ||
                                   prev.title        != win.committed.title        ||
                                   prev.appId        != win.committed.appId);
        if (stateChanged) {
            emit self->toplevelUpdated(handle,
                                       win.committed.appId,
                                       win.committed.title,
                                       win.committed.isActivated,
                                       win.committed.isMinimized,
                                       win.committed.isMaximized);
        }
    }
}

void ToplevelTracker::handleToplevelClosed(void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
    auto* self = static_cast<ToplevelTracker*>(data);
    auto it = self->m_windows.find(handle);
    if (it == self->m_windows.end()) return;

    QString appId = it.value().committed.appId;
    if (appId.isEmpty()) appId = it.value().pending.appId;

    tinexus::log::info("[ToplevelTracker] toplevelRemoved: app_id='{}'", appId.toStdString());
    emit self->toplevelRemoved(handle, appId);

    zwlr_foreign_toplevel_handle_v1_destroy(handle);
    self->m_windows.erase(it);
}

void ToplevelTracker::handleToplevelParent(void*, struct zwlr_foreign_toplevel_handle_v1*,
                                          struct zwlr_foreign_toplevel_handle_v1*) {
    // Reserved for parent dialog / window grouping
}

} // namespace tinexus::dock

#include "moc_ToplevelTracker.cpp"
