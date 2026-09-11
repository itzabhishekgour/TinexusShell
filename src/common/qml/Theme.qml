// ============================================================================
// Theme.qml — Tinexus Shell Design Tokens Singleton
// Ref: docs/05_UI_UX_GUIDELINES.md §4, §6.4, §6.5, docs/06_COMPONENT_DESIGN.md §8.4, §9
// ============================================================================
pragma Singleton
import QtQuick

QtObject {
    id: root

    // ── Motion & Accessibility ──────────────────────────────────
    // Single source of truth for reducedMotion across all Tier 1 shell apps
    property bool reducedMotion: false

    readonly property var motion: ({
        reducedMotion: root.reducedMotion,
        durationInstant: 0,
        durationFast: root.reducedMotion ? 0 : 150,
        durationNormal: root.reducedMotion ? 0 : 250,
        durationSlow: root.reducedMotion ? 0 : 350
    })

    // ── Color Tokens (Dark Mode) ─────────────────────────────────
    readonly property var color: ({
        accent: {
            primary: "#6B8CEF",
            hover:   "#82A1FF",
            pressed: "#5575D8"
        },
        background: {
            desktop:     "#0A0A0E",
            surface:     "#13131A",
            surface_alt: "#1A1A24",
            card:        "#1C1C28",
            overlay:     "#0A0A0E99"
        },
        text: {
            primary:   "#F0F0F8",
            secondary: "#9090A8",
            muted:     "#606078"
        },
        border: {
            default:   "#FFFFFF14",
            subtle:    "#FFFFFF08",
            focus:     "#6B8CEF80"
        }
    })

    // ── Radius Tokens ───────────────────────────────────────────
    readonly property var radius: ({
        xs:   4,
        sm:   6,
        md:   10,
        lg:   14,
        xl:   18,
        full: 999
    })

    // ── Material System (§6.4) ──────────────────────────────────
    readonly property var material: ({
        popover: {
            background:    "#1C1C28F2",
            blur_radius:   12,
            border_color:  "#FFFFFF14",
            border_width:  1,
            corner_radius: 12,
            fallback:      "#1C1C28"
        },
        toolbar: {
            background:    "#13131AE6",
            blur_radius:   16,
            border_color:  "#FFFFFF0F",
            border_width:  1,
            corner_radius: 0,
            fallback:      "#13131A"
        },
        sidebar: {
            background:    "#141420CC",
            blur_radius:   20,
            border_color:  "#FFFFFF0D",
            border_width:  1,
            corner_radius: 0,
            fallback:      "#141420"
        },
        menu: {
            background:    "#1C1C28F0",
            blur_radius:   10,
            border_color:  "#FFFFFF14",
            border_width:  1,
            corner_radius: 10,
            fallback:      "#1C1C28"
        },
        launcher: {
            // Preserved glass.background outside material system per §6.1 / §6.4
            background:    "#10121DF0",
            blur_radius:   40,
            border_color:  "#FFFFFF18",
            border_width:  1,
            corner_radius: 18,
            fallback:      "#10121D"
        }
    })
}
