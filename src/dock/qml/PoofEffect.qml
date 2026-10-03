// ============================================================================
// PoofEffect.qml — Hardware-Accelerated Smoke & Particle Burst for tinexus-dock
// Slice 5: Drag Rearrange + Poof Effect
// Ref: Architecture Blueprint §4.1, docs/05_UI_UX_GUIDELINES.md
// Uses GPU-accelerated animators with zero CPU spikes.
// ============================================================================
import QtQuick

Item {
    id: root
    width:  80
    height: 80
    visible: false
    z:       10000

    signal finished(string appId)
    property string currentAppId: ""
    property real   expansion:    0.0

    function trigger(globalX, globalY, appId) {
        root.x = globalX - width / 2.0
        root.y = globalY - height / 2.0
        root.currentAppId = appId
        root.expansion = 0.0
        root.scale = 0.3
        root.opacity = 1.0
        root.visible = true
        poofAnimation.restart()
    }

    // ── 1. Central Core Smoke Puff ──────────────────────────────────────────
    Rectangle {
        id: corePuff
        width:  28
        height: 28
        radius: 14
        color:  Qt.rgba(0.92, 0.94, 0.98, 0.85)
        anchors.centerIn: parent
        scale: 1.0 + root.expansion * 0.8
    }

    // ── 2. Eight Radial Expanding Smoke Cloudlets ────────────────────────────
    Repeater {
        model: 8
        Rectangle {
            id: cloudlet
            readonly property real angle: (index / 8.0) * Math.PI * 2.0
            readonly property real maxDist: 38.0

            width:  20
            height: 20
            radius: 10
            color:  Qt.rgba(0.88, 0.90, 0.95, 0.78)

            x: (root.width / 2.0 - width / 2.0) + Math.cos(angle) * (maxDist * root.expansion)
            y: (root.height / 2.0 - height / 2.0) + Math.sin(angle) * (maxDist * root.expansion)
            scale: 0.4 + root.expansion * 0.9
        }
    }

    // ── 3. Eight Sparkle Debris Particles (Magical Dust) ─────────────────────
    Repeater {
        model: 8
        Rectangle {
            id: sparkle
            readonly property real angle: ((index + 0.5) / 8.0) * Math.PI * 2.0
            readonly property real maxDist: 52.0

            width:  4
            height: 4
            radius: 2
            color:  index % 2 === 0 ? Qt.rgba(1.0, 1.0, 1.0, 0.95) : Qt.rgba(0.55, 0.80, 1.0, 0.90)

            x: (root.width / 2.0 - width / 2.0) + Math.cos(angle) * (maxDist * root.expansion)
            y: (root.height / 2.0 - height / 2.0) + Math.sin(angle) * (maxDist * root.expansion)
            scale: 1.0 - root.expansion * 0.4
        }
    }

    // ── 4. GPU-Accelerated Animation Pipeline ────────────────────────────────
    ParallelAnimation {
        id: poofAnimation

        NumberAnimation {
            target:   root
            property: "expansion"
            from:     0.0
            to:       1.0
            duration: 320
            easing.type: Easing.OutCubic
        }

        ScaleAnimator {
            target:   root
            from:     0.3
            to:       1.45
            duration: 320
            easing.type: Easing.OutCubic
        }

        OpacityAnimator {
            target:   root
            from:     1.0
            to:       0.0
            duration: 320
            easing.type: Easing.InQuad
        }

        onFinished: {
            root.visible = false
            root.finished(root.currentAppId)
        }
    }
}
