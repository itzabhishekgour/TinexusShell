#pragma once

// ============================================================================
// WaylandBlurHelper.hpp — Shared org_kde_kwin_blur_manager Protocol Client
// ============================================================================
// Qt6 UI-only header (never include in pure C++20 non-UI daemons).
// Synchronizes dynamic QRegion blur areas and triggers compositor repaints.
// ============================================================================

#include <QtCore/QByteArray>
#include <QtGui/QGuiApplication>
#include <QtGui/QRegion>
#include <QtGui/qguiapplication_platform.h>
#include <QtQuick/QQuickWindow>
#include <cstring>

#if __has_include(<qpa/qplatformnativeinterface.h>)
#include <qpa/qplatformnativeinterface.h>
#define TINEXUS_HAS_QPA_NATIVE 1
#else
#define TINEXUS_HAS_QPA_NATIVE 0
#endif

extern "C" {
#include <wayland-client.h>
#include "org-kde-kwin-blur-client-protocol.h"
}

namespace tinexus::common::ui {

struct WaylandBlurHelper {
    struct wl_display*                display{nullptr};
    struct wl_compositor*             compositor{nullptr};
    struct wl_registry*               registry{nullptr};
    struct org_kde_kwin_blur_manager* manager{nullptr};
    struct org_kde_kwin_blur*         blur{nullptr};
    struct wl_surface*                bound_surface{nullptr};
    bool                              bound_attempted{false};

    ~WaylandBlurHelper() {
        if (blur) {
            org_kde_kwin_blur_release(blur);
            blur = nullptr;
        }
        if (manager) {
            org_kde_kwin_blur_manager_destroy(manager);
            manager = nullptr;
        }
        if (registry) {
            wl_registry_destroy(registry);
            registry = nullptr;
        }
    }

    static void handle_global(void* data, struct wl_registry* reg, uint32_t name,
                              const char* interface, uint32_t /*version*/) {
        auto* self = static_cast<WaylandBlurHelper*>(data);
        if (!self || !interface) return;
        if (std::strcmp(interface, org_kde_kwin_blur_manager_interface.name) == 0 && !self->manager) {
            self->manager = static_cast<struct org_kde_kwin_blur_manager*>(
                wl_registry_bind(reg, name, &org_kde_kwin_blur_manager_interface, 1));
        }
    }

    static void handle_global_remove(void* /*data*/, struct wl_registry* /*reg*/, uint32_t /*name*/) {}

    static inline const struct wl_registry_listener kRegistryListener = {
        .global = handle_global,
        .global_remove = handle_global_remove,
    };

    void ensureBound() {
        if (bound_attempted) return;
        bound_attempted = true;
        if (!qGuiApp) return;
        auto* wlApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        if (!wlApp) return;
        display = wlApp->display();
        compositor = wlApp->compositor();
        if (!display) return;
        registry = wl_display_get_registry(display);
        if (registry) {
            wl_registry_add_listener(registry, &kRegistryListener, this);
            wl_display_roundtrip(display);
        }
    }

    void apply(struct wl_surface* surface, bool enable, const QRegion& region) {
        ensureBound();
        if (!manager || !surface) return;

        if (!enable || region.isEmpty()) {
            if (blur) {
                org_kde_kwin_blur_release(blur);
                blur = nullptr;
            }
            if (bound_surface) {
                org_kde_kwin_blur_manager_unset(manager, bound_surface);
                bound_surface = nullptr;
            }
            if (display) wl_display_flush(display);
            return;
        }

        if (blur && bound_surface != surface) {
            org_kde_kwin_blur_release(blur);
            blur = nullptr;
        }
        if (!blur) {
            blur = org_kde_kwin_blur_manager_create(manager, surface);
            bound_surface = surface;
        }
        if (!blur) return;

        if (compositor) {
            struct wl_region* wlReg = wl_compositor_create_region(compositor);
            if (wlReg) {
                for (const QRect& r : region) {
                    if (r.width() > 0 && r.height() > 0) {
                        wl_region_add(wlReg, r.x(), r.y(), r.width(), r.height());
                    }
                }
                org_kde_kwin_blur_set_region(blur, wlReg);
                wl_region_destroy(wlReg);
            }
        } else {
            org_kde_kwin_blur_set_region(blur, nullptr);
        }
        org_kde_kwin_blur_commit(blur);
        if (display) wl_display_flush(display);
    }

    void applyToWindow(QQuickWindow* window, bool enable, const QRegion& region) {
        struct wl_surface* surf = nullptr;
#if TINEXUS_HAS_QPA_NATIVE
        if (window && QGuiApplication::platformNativeInterface()) {
            surf = static_cast<struct wl_surface*>(
                QGuiApplication::platformNativeInterface()->nativeResourceForWindow(
                    QByteArrayLiteral("surface"), window));
        }
#else
        Q_UNUSED(window);
#endif
        apply(surf, enable, region);
    }
};

} // namespace tinexus::common::ui
