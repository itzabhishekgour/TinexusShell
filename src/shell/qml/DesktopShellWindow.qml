// ============================================================================
// DesktopShellWindow.qml — Root Window for tinexus-shell
// ============================================================================
import QtQuick
import QtQuick.Window

Window {
    id: rootWindow
    width: Screen.width > 0 ? Screen.width : 1920
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    // Dynamically expand window height when a flyout is open or closing, toast arrives, or AuraNotch expands
    readonly property bool anyFlyoutOpen: typeof bridge !== "undefined" && (
        bridge.logoMenuOpen || bridge.appMenuOpen || bridge.calendarOpen || bridge.notificationsOpen ||
        bridge.volumeFlyoutOpen || bridge.brightnessFlyoutOpen || bridge.wifiFlyoutOpen ||
        bridge.rebootConfirmationOpen || bridge.shutdownConfirmationOpen
    )
    readonly property bool anyFlyoutVisible: anyFlyoutOpen ||
        logoFlyoutHost.animatingVisible || appsFlyoutHost.animatingVisible ||
        calFlyoutHost.animatingVisible || notifFlyoutHost.animatingVisible ||
        volFlyoutHost.animatingVisible || briFlyoutHost.animatingVisible ||
        wifiFlyoutHost.animatingVisible || powerDialogHost.animatingVisible
    readonly property bool toastVisible: typeof bridge !== "undefined" && bridge.toastVisible
    readonly property bool notchExpanded: topBar.notchExpanded

    // Keep root layer-shell height fixed at 440px at all times so Qt Wayland RHI
    // never destroys/reallocates the backing swapchain when flyovers open or close.
    // Input routing and Dual-Kawase blur still shrink to the 32px TopBar when idle.
    height: 440

    Shortcut {
        sequence: "Escape"
        enabled: rootWindow.anyFlyoutOpen
        onActivated: {
            if (typeof bridge !== "undefined") {
                bridge.closeAllFlyouts();
            }
        }
    }

    // Auto-dismiss open flyouts when shell window loses focus to an application window
    onActiveChanged: {
        if (!active && anyFlyoutOpen && typeof bridge !== "undefined") {
            bridge.closeAllFlyouts();
        }
    }

    // Reusable GPU-composited flyout container (critically-damped slide/scale + opacity)
    // Underlying layer-shell window height stays fixed at 490 during the entire
    // open, dynamic content resize, and close settle lifecycle so Wayland never
    // reallocates buffers mid-animation and MultiEffect shadows never clip.
    component FlyoutHost: Item {
        id: host
        property bool open: false
        property alias sourceComponent: loader.sourceComponent
        property bool animatingVisible: open || (fadeOpacity > 0.005)
        property real fadeOpacity: 0.0
        property real animScale: 0.97
        property real animOffsetY: -6.0

        width: loader.item ? loader.item.width : 0
        height: loader.item ? loader.item.height : 0
        visible: animatingVisible

        transformOrigin: Item.Top
        scale: animScale
        opacity: fadeOpacity
        transform: Translate {
            y: host.animOffsetY
        }

        function scheduleGeometrySync() {
            Qt.callLater(function() {
                if (typeof bridge !== "undefined" && typeof bridge.syncOverlayGeometry === "function") {
                    bridge.syncOverlayGeometry();
                }
            });
        }

        ParallelAnimation {
            id: openAnim
            NumberAnimation {
                target: host
                property: "fadeOpacity"
                to: 1.0
                duration: 155
                easing.type: Easing.OutCubic
            }
            SpringAnimation {
                target: host
                property: "animScale"
                to: 1.0
                spring: 6.2
                damping: 0.86
                epsilon: 0.001
            }
            SpringAnimation {
                target: host
                property: "animOffsetY"
                to: 0.0
                spring: 6.2
                damping: 0.86
                epsilon: 0.05
            }
        }

        ParallelAnimation {
            id: closeAnim
            NumberAnimation {
                target: host
                property: "fadeOpacity"
                to: 0.0
                duration: 120
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: host
                property: "animScale"
                to: 0.97
                duration: 120
                easing.type: Easing.InOutCubic
            }
            NumberAnimation {
                target: host
                property: "animOffsetY"
                to: -6.0
                duration: 120
                easing.type: Easing.InOutCubic
            }
            onFinished: {
                if (!host.open) {
                    loader.active = false;
                    host.scheduleGeometrySync();
                }
            }
        }

        onAnimatingVisibleChanged: host.scheduleGeometrySync()
        onXChanged: host.scheduleGeometrySync()
        onYChanged: host.scheduleGeometrySync()
        onAnimScaleChanged: host.scheduleGeometrySync()
        onAnimOffsetYChanged: host.scheduleGeometrySync()

        onOpenChanged: {
            if (open) {
                closeAnim.stop();
                loader.active = true;
                Qt.callLater(function() {
                    if (host.open) {
                        openAnim.restart();
                        host.scheduleGeometrySync();
                    }
                });
            } else {
                openAnim.stop();
                if (fadeOpacity > 0.005) {
                    closeAnim.restart();
                } else {
                    fadeOpacity = 0.0;
                    animScale = 0.97;
                    animOffsetY = -6.0;
                    loader.active = false;
                    host.scheduleGeometrySync();
                }
            }
        }

        Connections {
            target: loader.item
            function onWidthChanged() { host.scheduleGeometrySync(); }
            function onHeightChanged() { host.scheduleGeometrySync(); }
        }

        Loader {
            id: loader
            active: false
            onLoaded: host.scheduleGeometrySync()
        }
    }

    Item {
        id: container
        anchors.fill: parent

        // ── Backdrop Dismiss Area (Clicking outside flyouts dismisses them) ──
        MouseArea {
            anchors.fill: parent
            z: -1
            enabled: anyFlyoutOpen
            onPressed: {
                if (typeof bridge !== "undefined") {
                    bridge.closeAllFlyouts();
                }
            }
        }

        // ── Top Bar (32px baseline + notch) ─────────────────────────
        TopBar {
            id: topBar
            z: 10
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
        }

        // ── Logo Menu Flyout (Spring-animated, Lazy-loaded on open) ─
        FlyoutHost {
            id: logoFlyoutHost
            x: topBar.logoTriggerItem ? Math.max(8, Math.round(topBar.logoTriggerItem.mapToItem(container, 0, 0).x)) : 8
            y: topBar.height + 4
            open: typeof bridge !== "undefined" && bridge.logoMenuOpen
            sourceComponent: Component {
                LogoMenuFlyout {
                    id: logoFlyout
                    objectName: "logoFlyout"
                    visible: true
                }
            }
        }

        // ── Applications Dropdown Flyout (Spring-animated) ──────────
        FlyoutHost {
            id: appsFlyoutHost
            readonly property real pad: 18
            x: (topBar.appTriggerItem ? Math.max(8, Math.round(topBar.appTriggerItem.mapToItem(container, 0, 0).x)) : 48) - pad
            y: topBar.height + 4 - pad
            open: typeof bridge !== "undefined" && bridge.appMenuOpen
            sourceComponent: Component {
                ApplicationsFlyout {
                    id: appsFlyout
                    objectName: "appsFlyout"
                    visible: true
                }
            }
        }

        // ── Calendar Flyout (Spring-animated) ───────────────────────
        FlyoutHost {
            id: calFlyoutHost
            readonly property real pad: 18
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: 60
            y: topBar.height + 4 - pad
            open: typeof bridge !== "undefined" && bridge.calendarOpen
            sourceComponent: Component {
                CalendarFlyout {
                    id: calFlyout
                    objectName: "calFlyout"
                    visible: true
                }
            }
        }

        // ── Notifications Flyout (Spring-animated) ──────────────────
        FlyoutHost {
            id: notifFlyoutHost
            readonly property real pad: 18
            readonly property real cardW: width > pad * 2 ? (width - pad * 2) : 360
            x: (topBar.notifTriggerItem
                ? Math.min(container.width - cardW - 16, Math.max(8, Math.round(topBar.notifTriggerItem.mapToItem(container, 0, 0).x + (topBar.notifTriggerItem.width / 2) - (cardW / 2))))
                : (container.width - cardW - 16)) - pad
            y: topBar.height + 4 - pad
            open: typeof bridge !== "undefined" && bridge.notificationsOpen
            sourceComponent: Component {
                NotificationFlyout {
                    id: notifFlyout
                    objectName: "notifFlyout"
                    visible: true
                }
            }
        }

        // ── Volume Flyout (Spring-animated) ─────────────────────────
        FlyoutHost {
            id: volFlyoutHost
            readonly property real pad: 18
            readonly property real cardW: width > pad * 2 ? (width - pad * 2) : 272
            x: (topBar.volTriggerItem
                ? Math.min(container.width - cardW - 16, Math.max(8, Math.round(topBar.volTriggerItem.mapToItem(container, 0, 0).x + (topBar.volTriggerItem.width / 2) - (cardW / 2))))
                : (container.width - cardW - 40)) - pad
            y: topBar.height + 4 - pad
            open: typeof bridge !== "undefined" && bridge.volumeFlyoutOpen
            sourceComponent: Component {
                VolumeFlyout {
                    id: volFlyout
                    objectName: "volFlyout"
                    visible: true
                }
            }
        }

        // ── Brightness Flyout (Spring-animated) ─────────────────────
        FlyoutHost {
            id: briFlyoutHost
            readonly property real pad: 18
            readonly property real cardW: width > pad * 2 ? (width - pad * 2) : 272
            x: (topBar.briTriggerItem
                ? Math.min(container.width - cardW - 16, Math.max(8, Math.round(topBar.briTriggerItem.mapToItem(container, 0, 0).x + (topBar.briTriggerItem.width / 2) - (cardW / 2))))
                : (container.width - cardW - 70)) - pad
            y: topBar.height + 4 - pad
            open: typeof bridge !== "undefined" && bridge.brightnessFlyoutOpen
            sourceComponent: Component {
                BrightnessFlyout {
                    id: briFlyout
                    objectName: "briFlyout"
                    visible: true
                }
            }
        }

        // ── Wi-Fi Quick Flyout (Spring-animated) ────────────────────
        FlyoutHost {
            id: wifiFlyoutHost
            readonly property real pad: 18
            readonly property real cardW: width > pad * 2 ? (width - pad * 2) : 300
            x: (topBar.wifiTriggerItem
                ? Math.min(container.width - cardW - 16, Math.max(8, Math.round(topBar.wifiTriggerItem.mapToItem(container, 0, 0).x + (topBar.wifiTriggerItem.width / 2) - (cardW / 2))))
                : (container.width - cardW - 48)) - pad
            y: topBar.height + 4 - pad
            open: typeof bridge !== "undefined" && bridge.wifiFlyoutOpen
            sourceComponent: Component {
                WifiQuickFlyout {
                    id: wifiFlyout
                    objectName: "wifiFlyout"
                    visible: true
                }
            }
        }

        // ── Power Confirmation Modal (Spring-animated) ──────────────
        FlyoutHost {
            id: powerDialogHost
            anchors.horizontalCenter: parent.horizontalCenter
            y: 60
            open: typeof bridge !== "undefined" && (bridge.rebootConfirmationOpen || bridge.shutdownConfirmationOpen)
            sourceComponent: Component {
                PowerConfirmationDialog {
                    id: powerDialog
                    objectName: "powerDialog"
                    visible: true
                }
            }
        }

        // ── Notification Toast Overlay (Lazy-loaded when visible) ───
        Loader {
            id: notifToastLoader
            anchors.right: parent.right
            anchors.rightMargin: 18
            y: 40
            active: typeof bridge !== "undefined" && bridge.toastVisible
            visible: active
            onLoaded: {
                if (typeof bridge !== "undefined" && typeof bridge.syncOverlayGeometry === "function") {
                    bridge.syncOverlayGeometry();
                }
            }
            sourceComponent: Component {
                NotificationToast {
                    id: notifToast
                    objectName: "notifToast"
                    visible: true
                }
            }
        }
    }
}
