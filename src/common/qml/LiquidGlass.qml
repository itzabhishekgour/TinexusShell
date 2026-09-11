// ============================================================================
// LiquidGlass.qml — Tinexus Shell v2.0 Material Component: "Liquid Glass"
// Ref: docs/05_UI_UX_GUIDELINES.md §6.1 (Backdrop Blur), §6.4 (Materials), §6.5 (Fallback)
//
// ── ARCHITECTURAL RECONCILIATION & DESIGN DECISION ──────────────────────────
// This component unifies two API paradigms:
//   1. Preset-based API (`materialType`: "popover" | "toolbar" | "sidebar" | "launcher"):
//      Provides instant semantic alignment with 05_UI_UX_GUIDELINES.md tokens.
//      When materialType is set, default blur amount, corner radius, fluid tint,
//      and fallback colors are automatically populated from canonical tokens.
//   2. Explicit-properties API (`blurAmount`, `cornerRadius`, `tintColor`, etc.):
//      Maintains granular per-instance control for specialized surfaces.
//      Any explicitly specified property overrides the materialType preset.
//
// ── 4-LAYER GLASS SYSTEM ───────────────────────────────────────────────────
//   Layer 0: Fallback container (active on reduceTransparency or no-source mode)
//   Layer 1: Refraction Layer — MultiEffect deep blur + saturation boost
//   Layer 2: Fluid Body       — translucent tinted container with liquid gradient
//   Layer 3: Specular Rim     — thin high-contrast border with arc highlight
//   Layer 4: Glossy Highlight — soft studio overhead reflection gradient
//
// Requires: Qt 6.5+ (QtQuick.Effects module for MultiEffect)
// ============================================================================

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes

Item {
    id: root

    // ── Preset Semantic Type ────────────────────────────────────
    property string materialType: "popover" // "popover" | "toolbar" | "sidebar" | "launcher"

    // ── Public API & Properties ─────────────────────────────────
    property Item sourceItem: null        // Content behind glass to refract/blur

    // Resolved defaults based on materialType preset
    readonly property var _presetConfig: {
        switch (root.materialType) {
        case "toolbar":
            return { blur: 16, radius: 0,  opacity: 0.90, tint: "#13131A", rimOp: 0.25, fallback: "#13131A" }
        case "sidebar":
            return { blur: 20, radius: 0,  opacity: 0.80, tint: "#141420", rimOp: 0.25, fallback: "#141420" }
        case "launcher":
            return { blur: 40, radius: 18, opacity: 0.94, tint: "#10121D", rimOp: 0.35, fallback: "#10121D" }
        case "popover":
        default:
            return { blur: 12, radius: 12, opacity: 0.95, tint: "#1C1C28", rimOp: 0.30, fallback: "#1C1C28" }
        }
    }

    property real cornerRadius:      _presetConfig.radius
    property real blurAmount:        _presetConfig.blur
    property real fluidOpacity:      _presetConfig.opacity
    property color tintColor:        _presetConfig.tint
    property real tintStrength:      0.85
    property color rimColor:         "#FFFFFF"
    property real rimOpacity:        _presetConfig.rimOp
    property real rimWidth:          1.0
    property real highlightOpacity:  0.15
    property real saturationBoost:   1.10

    // Fallback (matches §6.5 Material Fallback Hierarchy — Reduce Transparency)
    property bool reduceTransparency: false
    property color fallbackColor:    _presetConfig.fallback

    clip: true

    // ─────────────────────────────────────────────────────────────
    // LAYER 0 — Fallback / Standalone Base
    // When reduceTransparency is active, renders solid opaque surface.
    // When sourceItem is null (e.g. offscreen testing / isolated window),
    // renders the frosted translucent tint directly so the window is visible.
    // ─────────────────────────────────────────────────────────────
    Rectangle {
        id: fallbackRect
        anchors.fill: parent
        radius: root.cornerRadius
        color: root.reduceTransparency ? root.fallbackColor : Qt.rgba(root.tintColor.r, root.tintColor.g, root.tintColor.b, root.fluidOpacity)
        visible: root.reduceTransparency || root.sourceItem === null
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: root.rimWidth
    }

    // ─────────────────────────────────────────────────────────────
    // ACTIVE GLASS LAYERS (When sourceItem is available and blur enabled)
    // ─────────────────────────────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: !root.reduceTransparency && root.sourceItem !== null
        layer.enabled: true
        layer.smooth: true

        // ── 1. REFRACTION LAYER ──────────────────────────────────
        MultiEffect {
            id: refraction
            anchors.fill: parent
            source: root.sourceItem
            blurEnabled: true
            blur: 1.0
            blurMax: root.blurAmount
            saturation: root.saturationBoost - 1.0
            autoPaddingEnabled: false

            maskEnabled: true
            maskSource: cornerMask
        }

        Item {
            id: cornerMask
            width: refraction.width
            height: refraction.height
            layer.enabled: true
            visible: false
            Rectangle {
                anchors.fill: parent
                radius: root.cornerRadius
                color: "white"
            }
        }

        // ── 2. FLUID BODY ────────────────────────────────────────
        Rectangle {
            id: fluidBody
            anchors.fill: parent
            radius: root.cornerRadius
            color: Qt.rgba(
                root.tintColor.r, root.tintColor.g, root.tintColor.b,
                root.tintStrength
            )
            opacity: root.fluidOpacity

            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.05) }
                GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.00) }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.05) }
            }
        }
    }

    // ── 3. SPECULAR RIM (Always drawn over body) ─────────────────
    Rectangle {
        id: rimBase
        anchors.fill: parent
        radius: root.cornerRadius
        color: "transparent"
        border.width: root.rimWidth
        border.color: Qt.rgba(
            root.rimColor.r, root.rimColor.g, root.rimColor.b,
            root.rimOpacity * 0.6
        )
    }

    // ── 4. GLOSSY HIGHLIGHT (Subtle top sheen) ───────────────────
    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        enabled: false

        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0;  color: Qt.rgba(1, 1, 1, root.highlightOpacity) }
            GradientStop { position: 0.35; color: Qt.rgba(1, 1, 1, root.highlightOpacity * 0.25) }
            GradientStop { position: 0.6;  color: Qt.rgba(1, 1, 1, 0.0) }
            GradientStop { position: 1.0;  color: Qt.rgba(1, 1, 1, 0.0) }
        }
    }
}
