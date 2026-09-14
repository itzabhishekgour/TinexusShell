// ============================================================================
// AuraNotch.qml — Reactive "Dynamic Island" Component for Tinexus Desktop
// Features: LiquidGlass material, SpringAnimation transitions,
//           Dual-state (Idle Clock/Avatar <-> Expanded Media / Visualizer)
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    width: notchPill.width
    height: notchPill.height

    property bool expanded: state === "media"
    state: (typeof bridge !== "undefined" && bridge.notchExpanded) ? "media" : "idle"

    Connections {
        target: typeof bridge !== "undefined" ? bridge : null
        function onNotchExpandedChanged() {
            root.state = bridge.notchExpanded ? "media" : "idle";
        }
    }

    readonly property bool mediaPlaying: typeof bridge !== "undefined" ? bridge.mediaPlaying : true
    readonly property string mediaTitle: typeof bridge !== "undefined" ? bridge.mediaTitle : "Stargazing"
    readonly property string mediaArtist: typeof bridge !== "undefined" ? bridge.mediaArtist : "Tinexus Sound System"
    readonly property real mediaProgress: typeof bridge !== "undefined" ? bridge.mediaProgress : 0.48
    readonly property string mediaTimeStr: typeof bridge !== "undefined" ? bridge.mediaTimeStr : "1:42 / 3:30"

    // ── Physical Spring-Driven LiquidGlass Capsule ─────────────────
    LiquidGlass {
        id: notchPill
        width: 236
        height: 30
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        materialType: "notch"
        cornerRadius: root.expanded ? 20 : 15
        fluidOpacity: root.expanded ? 0.88 : 0.72

        Behavior on cornerRadius {
            NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
        }

        // ─────────────────────────────────────────────────────────────
        // 1. IDLE STATE: Compact Clock + Tinexus Logo + Date Capsule
        // ─────────────────────────────────────────────────────────────
        Item {
            id: idleContent
            anchors.fill: parent
            opacity: 1.0
            visible: opacity > 0.01

            Row {
                anchors.centerIn: parent
                spacing: 8

                // Time Capsule text
                Rectangle {
                    width: timeText.implicitWidth + 14
                    height: 20
                    radius: 10
                    color: Qt.rgba(0, 0, 0, 0.25)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: timeText
                        anchors.centerIn: parent
                        text: typeof bridge !== "undefined" ? bridge.currentTime : "12:41 AM"
                        color: "#F5F5FA"
                        font.family: "Inter"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                }

                // Center Glowing Avatar Logo
                Rectangle {
                    id: logoAvatar
                    width: 22
                    height: 22
                    radius: 11
                    color: avatarMouse.containsMouse ? "#38BDF8" : "#2563EB"
                    border.color: Qt.rgba(147 / 255.0, 197 / 255.0, 253 / 255.0, 0.75)
                    border.width: 1.2
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        anchors.centerIn: parent
                        source: "image://icon/tinexus-logo"
                        width: 14
                        height: 14
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    MouseArea {
                        id: avatarMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bridge !== "undefined") {
                                bridge.setNotchExpanded(!bridge.notchExpanded);
                            } else {
                                root.state = (root.state === "idle" ? "media" : "idle");
                            }
                        }
                    }
                }

                // Date Capsule text (triggers calendar on click)
                Rectangle {
                    id: dateCapsule
                    width: dateText.implicitWidth + 14
                    height: 20
                    radius: 10
                    readonly property bool isOpen: typeof bridge !== "undefined" && bridge.calendarOpen
                    color: (dateMouse.containsMouse || isOpen) ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(0, 0, 0, 0.25)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: dateText
                        anchors.centerIn: parent
                        text: typeof bridge !== "undefined" ? bridge.currentDate : "Mon, Sep 14"
                        color: (dateMouse.containsMouse || dateCapsule.isOpen) ? "#38BDF8" : "#F5F5FA"
                        font.family: "Inter"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: dateMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bridge !== "undefined") {
                                bridge.toggleCalendar();
                            }
                        }
                    }
                }
            }
        }

        // ─────────────────────────────────────────────────────────────
        // 2. MEDIA / ACTIVE STATE: Equalizer + Track Info + Scrubber + Controls
        // ─────────────────────────────────────────────────────────────
        Item {
            id: mediaContent
            anchors.fill: parent
            anchors.margins: 8
            opacity: 0.0
            visible: opacity > 0.01

            Row {
                anchors.fill: parent
                spacing: 12

                // ── Left: Album Art & Equalizer Visualizer ─────────────
                Item {
                    width: 48
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        id: albumArt
                        width: 36
                        height: 36
                        radius: 8
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        color: Qt.rgba(1, 1, 1, 0.08)
                        border.color: Qt.rgba(1, 1, 1, 0.12)

                        Image {
                            anchors.centerIn: parent
                            source: "image://icon/media-optical-audio-symbolic?color=#38BDF8"
                            width: 20
                            height: 20
                            fillMode: Image.PreserveAspectFit
                        }
                    }

                    // 4-Bar Audio Visualizer Equalizer
                    Row {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Repeater {
                            model: [
                                { minH: 4, maxH: 18, dur: 480 },
                                { minH: 6, maxH: 22, dur: 360 },
                                { minH: 3, maxH: 15, dur: 540 },
                                { minH: 5, maxH: 20, dur: 420 }
                            ]

                            delegate: Rectangle {
                                width: 2.5
                                height: modelData.minH
                                radius: 1.25
                                color: "#38BDF8"
                                anchors.bottom: parent.bottom

                                SequentialAnimation on height {
                                    running: root.expanded && root.mediaPlaying
                                    loops: Animation.Infinite
                                    NumberAnimation { to: modelData.maxH; duration: modelData.dur; easing.type: Easing.InOutSine }
                                    NumberAnimation { to: modelData.minH; duration: modelData.dur; easing.type: Easing.InOutSine }
                                }
                            }
                        }
                    }
                }

                // ── Center: Track Details & Progress Scrubber ─────────
                Column {
                    width: 220
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Row {
                        width: parent.width
                        spacing: 6

                        Text {
                            id: titleText
                            text: root.mediaTitle
                            color: "#FFFFFF"
                            font.family: "Inter"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            elide: Text.ElideRight
                            width: Math.min(implicitWidth, 140)
                        }

                        Text {
                            text: "• " + root.mediaArtist
                            color: Qt.rgba(1, 1, 1, 0.60)
                            font.family: "Inter"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            width: 70
                        }
                    }

                    // Scrubber Bar
                    Item {
                        width: parent.width
                        height: 6

                        // Track background
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 3
                            radius: 1.5
                            color: Qt.rgba(1, 1, 1, 0.15)
                        }

                        // Elapsed fill
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width * Math.min(1.0, Math.max(0.0, root.mediaProgress))
                            height: 3
                            radius: 1.5
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "#38BDF8" }
                                GradientStop { position: 1.0; color: "#818CF8" }
                            }
                        }
                    }

                    // Time elapsed indicator
                    Text {
                        text: root.mediaTimeStr
                        color: Qt.rgba(1, 1, 1, 0.45)
                        font.family: "Inter"
                        font.pixelSize: 10
                    }
                }

                // ── Right: Playback Controls ──────────────────────────
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    // Previous Track Button
                    Rectangle {
                        width: 24
                        height: 24
                        radius: 12
                        color: prevMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: "image://icon/media-skip-backward-symbolic?color=" +
                                    (prevMouse.containsMouse ? "#38BDF8" : "#FFFFFF")
                            fillMode: Image.PreserveAspectFit
                        }

                        MouseArea {
                            id: prevMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") bridge.prevMediaTrack();
                            }
                        }
                    }

                    // Play/Pause Circular Button
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: playMouse.containsMouse ? "#38BDF8" : "#2563EB"
                        border.color: Qt.rgba(1, 1, 1, 0.20)
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: root.mediaPlaying
                                    ? "image://icon/media-playback-pause-symbolic?color=#FFFFFF"
                                    : "image://icon/media-playback-start-symbolic?color=#FFFFFF"
                            fillMode: Image.PreserveAspectFit
                        }

                        MouseArea {
                            id: playMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") bridge.toggleMediaPlayback();
                            }
                        }
                    }

                    // Next Track Button
                    Rectangle {
                        width: 24
                        height: 24
                        radius: 12
                        color: nextMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: "image://icon/media-skip-forward-symbolic?color=" +
                                    (nextMouse.containsMouse ? "#38BDF8" : "#FFFFFF")
                            fillMode: Image.PreserveAspectFit
                        }

                        MouseArea {
                            id: nextMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") bridge.nextMediaTrack();
                            }
                        }
                    }

                    // Compact Close / Collapse Notch Button
                    Rectangle {
                        width: 20
                        height: 20
                        radius: 10
                        color: closeMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.20) : "transparent"
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            anchors.centerIn: parent
                            width: 10
                            height: 10
                            source: "image://icon/window-close-symbolic?color=" +
                                    (closeMouse.containsMouse ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.50).toString())
                            fillMode: Image.PreserveAspectFit
                        }

                        MouseArea {
                            id: closeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") {
                                    bridge.setNotchExpanded(false);
                                } else {
                                    root.state = "idle";
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Double-click background to toggle notch expansion
    MouseArea {
        anchors.fill: notchPill
        z: -1
        cursorShape: Qt.PointingHandCursor
        onDoubleClicked: {
            if (typeof bridge !== "undefined") {
                bridge.setNotchExpanded(!bridge.notchExpanded);
            } else {
                root.state = (root.state === "idle" ? "media" : "idle");
            }
        }
    }

    // ── State Machine ─────────────────────────────────────────────
    states: [
        State {
            name: "idle"
            PropertyChanges { target: notchPill; width: 236; height: 30 }
            PropertyChanges { target: idleContent; opacity: 1.0 }
            PropertyChanges { target: mediaContent; opacity: 0.0 }
        },
        State {
            name: "media"
            PropertyChanges { target: notchPill; width: 420; height: 68 }
            PropertyChanges { target: idleContent; opacity: 0.0 }
            PropertyChanges { target: mediaContent; opacity: 1.0 }
        }
    ]

    // ── Physical Spring Transitions (Stiffness ~380, Damping ~28) ───
    transitions: [
        Transition {
            from: "idle"; to: "media"
            ParallelAnimation {
                SpringAnimation {
                    target: notchPill
                    properties: "width,height"
                    spring: 3.8
                    damping: 0.28
                    epsilon: 0.25
                }
                NumberAnimation {
                    target: idleContent
                    property: "opacity"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
                NumberAnimation {
                    target: mediaContent
                    property: "opacity"
                    duration: 200
                    easing.type: Easing.OutQuad
                }
            }
        },
        Transition {
            from: "media"; to: "idle"
            ParallelAnimation {
                SpringAnimation {
                    target: notchPill
                    properties: "width,height"
                    spring: 3.8
                    damping: 0.28
                    epsilon: 0.25
                }
                NumberAnimation {
                    target: mediaContent
                    property: "opacity"
                    duration: 100
                    easing.type: Easing.InQuad
                }
                NumberAnimation {
                    target: idleContent
                    property: "opacity"
                    duration: 180
                    easing.type: Easing.OutQuad
                }
            }
        }
    ]
}
