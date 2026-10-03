// ============================================================================
// LiquidGlass.qml — Tinexus Shell Specular Glass Material Component
// Ref: docs/05_UI_UX_GUIDELINES.md §6.1 (Backdrop Blur), §6.4 (Materials)
// ============================================================================
import QtQuick

Item {
    id: root

    // ── Semantic Material Preset Types ───────────────────────────
    property string materialType: "popover" // "popover" | "toolbar" | "sidebar" | "launcher"

    // ── Public API ──────────────────────────────────────────────
    property Item sourceItem: null // Maintained for backward compatibility

    // Canonical Tinexus Glass Presets (from docs/05_UI_UX_GUIDELINES.md)
    readonly property var _presetConfig: {
        switch (root.materialType) {
        case "toolbar":
            return { radius: 0,  opacity: 0.62, tint: "#12141C", rimOp: 0.20, fallback: "#12141C" }
        case "sidebar":
            return { radius: 0,  opacity: 0.68, tint: "#141622", rimOp: 0.22, fallback: "#141622" }
        case "launcher":
            return { radius: 18, opacity: 0.72, tint: "#10121D", rimOp: 0.28, fallback: "#10121D" }
        case "popover":
        default:
            return { radius: 12, opacity: 0.65, tint: "#181A26", rimOp: 0.25, fallback: "#181A26" }
        }
    }

    property real cornerRadius:     _presetConfig.radius
    property real fluidOpacity:     _presetConfig.opacity
    property color tintColor:       _presetConfig.tint
    property real rimOpacity:       _presetConfig.rimOp
    property real rimWidth:         1.0
    property real blurAmount:       28 // Requested blur radius for compositor

    // Accessibility fallback
    property bool reduceTransparency: false
    property color fallbackColor:   _presetConfig.fallback

    // On creation, signal compositor-level backdrop blur
    Component.onCompleted: {
        if (typeof bridge !== "undefined" && typeof bridge.requestBlur === "function") {
            bridge.requestBlur(true, root.blurAmount);
        }
    }

    // ── 1. Translucent Fluid Glass Body ──────────────────────────
    Rectangle {
        id: fluidBody
        anchors.fill: parent
        radius: root.cornerRadius
        color: root.reduceTransparency
               ? root.fallbackColor
               : Qt.rgba(root.tintColor.r, root.tintColor.g, root.tintColor.b, root.fluidOpacity)
        border.color: Qt.rgba(1.0, 1.0, 1.0, root.rimOpacity * 0.4)
        border.width: root.rimWidth
    }

    // ── 2. Top Edge Crisp Specular Highlight (1px) ───────────────
    Rectangle {
        id: topHighlight
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        radius: root.cornerRadius > 0 ? root.cornerRadius : 0
        color: Qt.rgba(1.0, 1.0, 1.0, 0.18)
        visible: !root.reduceTransparency
    }

    // ── 3. Bottom Edge 3D Inner Volume Shadow (1px) ──────────────
    Rectangle {
        id: bottomShadow
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        radius: root.cornerRadius > 0 ? root.cornerRadius : 0
        color: Qt.rgba(0.0, 0.0, 0.0, 0.32)
        visible: !root.reduceTransparency
    }
}
