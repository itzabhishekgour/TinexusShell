// ============================================================================
// LiquidGlass.qml — Tinexus Shell Specular Glass Material Component
// Ref: docs/05_UI_UX_GUIDELINES.md §6.1 (Backdrop Blur), §6.4 (Materials)
// ============================================================================
import QtQuick
import QtQuick.Effects

Item {
    id: root

    // Note: Do NOT enable layer clipping on root, otherwise outer MultiEffect
    // shadows are clipped at the item bounding box.

    // ── Semantic Material Preset Types ───────────────────────────
    property string materialType: "popover" // "popover" | "toolbar" | "sidebar" | "launcher" | "notch"

    // ── Public API ──────────────────────────────────────────────
    property Item sourceItem: null // Maintained for backward compatibility

    // Canonical Tinexus Glass Presets (from docs/05_UI_UX_GUIDELINES.md)
    readonly property var _presetConfig: {
        switch (root.materialType) {
        case "toolbar":
            return { radius: 0,  opacity: 0.56, tint: "#10121A", rimOp: 0.22, fallback: "#10121A" }
        case "notch":
            return { radius: 15, opacity: 0.70, tint: "#10121A", rimOp: 0.24, fallback: "#10121A" }
        case "sidebar":
            return { radius: 0,  opacity: 0.66, tint: "#141622", rimOp: 0.22, fallback: "#141622" }
        case "launcher":
            return { radius: 18, opacity: 0.68, tint: "#10121D", rimOp: 0.28, fallback: "#10121D" }
        case "popover":
        default:
            return { radius: 14, opacity: 0.64, tint: "#161824", rimOp: 0.25, fallback: "#161824" }
        }
    }

    property real cornerRadius:       _presetConfig.radius
    property real fluidOpacity:       _presetConfig.opacity
    property color tintColor:         _presetConfig.tint
    property real rimOpacity:         _presetConfig.rimOp
    property real rimWidth:           1.0
    property real blurAmount:         28 // Requested blur radius for compositor
    property real topSpecularOpacity: 0.14
    property bool shadowEnabled:      (materialType === "popover")

    // Accessibility fallback
    property bool reduceTransparency: false
    property color fallbackColor:     _presetConfig.fallback

    // On creation, signal compositor-level backdrop blur
    Component.onCompleted: {
        if (typeof bridge !== "undefined" && typeof bridge.requestBlur === "function") {
            bridge.requestBlur(true, root.blurAmount);
        }
    }

    // ── 0. Dual-Layer Depth Shadows (Ambient + Key Contact) ─────
    Rectangle {
        id: shadowSource
        anchors.fill: parent
        radius: root.cornerRadius
        color: "#FF000000"
        visible: false
    }

    MultiEffect {
        id: ambientShadow
        anchors.fill: shadowSource
        source: shadowSource
        autoPaddingEnabled: true
        blurMax: 12
        visible: root.shadowEnabled && !root.reduceTransparency
        shadowEnabled: true
        shadowColor: Qt.rgba(0.0, 0.0, 0.0, 0.30)
        shadowBlur: 0.58
        shadowVerticalOffset: 4
        shadowHorizontalOffset: 0
        shadowOpacity: 0.45
    }

    MultiEffect {
        id: keyShadow
        anchors.fill: shadowSource
        source: shadowSource
        autoPaddingEnabled: true
        blurMax: 6
        visible: root.shadowEnabled && !root.reduceTransparency
        shadowEnabled: true
        shadowColor: Qt.rgba(0.0, 0.0, 0.0, 0.44)
        shadowBlur: 0.32
        shadowVerticalOffset: 1
        shadowHorizontalOffset: 0
        shadowOpacity: 0.56
    }

    // ── 1. Translucent Fluid Glass Body ──────────────────────────
    Rectangle {
        id: fluidBody
        anchors.fill: parent
        radius: root.cornerRadius
        color: root.reduceTransparency
               ? root.fallbackColor
               : Qt.rgba(root.tintColor.r, root.tintColor.g, root.tintColor.b, root.fluidOpacity)
        border.color: Qt.rgba(1.0, 1.0, 1.0, root.rimOpacity * 0.34)
        border.width: root.rimWidth
    }

    // ── 2. Top Edge Crisp 1px Inner Specular Highlight ───────────
    Rectangle {
        id: topHighlight
        anchors.top: fluidBody.top
        anchors.topMargin: 1
        anchors.left: fluidBody.left
        anchors.right: fluidBody.right
        anchors.leftMargin: root.cornerRadius > 2 ? Math.round(root.cornerRadius * 0.45) : 0
        anchors.rightMargin: root.cornerRadius > 2 ? Math.round(root.cornerRadius * 0.45) : 0
        height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.00; color: Qt.rgba(1.0, 1.0, 1.0, root.cornerRadius > 2 ? 0.04 : root.topSpecularOpacity) }
            GradientStop { position: 0.12; color: Qt.rgba(1.0, 1.0, 1.0, root.topSpecularOpacity) }
            GradientStop { position: 0.88; color: Qt.rgba(1.0, 1.0, 1.0, root.topSpecularOpacity) }
            GradientStop { position: 1.00; color: Qt.rgba(1.0, 1.0, 1.0, root.cornerRadius > 2 ? 0.04 : root.topSpecularOpacity) }
        }
        visible: !root.reduceTransparency
    }

    // ── 3. Bottom Edge 3D Inner Volume Shadow (1px) ──────────────
    Rectangle {
        id: bottomShadow
        anchors.bottom: fluidBody.bottom
        anchors.bottomMargin: 1
        anchors.left: fluidBody.left
        anchors.right: fluidBody.right
        anchors.leftMargin: root.cornerRadius > 2 ? Math.round(root.cornerRadius * 0.45) : 0
        anchors.rightMargin: root.cornerRadius > 2 ? Math.round(root.cornerRadius * 0.45) : 0
        height: 1
        color: Qt.rgba(0.0, 0.0, 0.0, 0.34)
        visible: !root.reduceTransparency
    }
}
