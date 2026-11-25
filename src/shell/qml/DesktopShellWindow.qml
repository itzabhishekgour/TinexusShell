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

    // Dynamically expand window height when a flyout opens, toast arrives, or when Aura Notch expands
    readonly property bool anyFlyoutOpen: typeof bridge !== "undefined" && (
        bridge.logoMenuOpen || bridge.appMenuOpen || bridge.calendarOpen || bridge.notificationsOpen ||
        bridge.volumeFlyoutOpen || bridge.brightnessFlyoutOpen ||
        bridge.rebootConfirmationOpen || bridge.shutdownConfirmationOpen
    )
    readonly property bool toastVisible: typeof bridge !== "undefined" && bridge.toastVisible
    readonly property bool notchExpanded: topBar.notchExpanded

    height: anyFlyoutOpen ? 420 : (toastVisible ? 120 : (notchExpanded ? 84 : 46))

    // Auto-dismiss open flyouts when shell window loses focus to an application window
    onActiveChanged: {
        if (!active && anyFlyoutOpen && typeof bridge !== "undefined") {
            bridge.closeAllFlyouts();
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
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
        }

        // ── Logo Menu Flyout (Lazy-loaded on open) ──────────────────
        Loader {
            id: logoFlyoutLoader
            x: topBar.logoTriggerItem ? Math.max(8, Math.round(topBar.logoTriggerItem.mapToItem(container, 0, 0).x)) : 8
            y: topBar.height + 4
            active: typeof bridge !== "undefined" && bridge.logoMenuOpen
            visible: active
            sourceComponent: Component {
                LogoMenuFlyout {
                    id: logoFlyout
                    objectName: "logoFlyout"
                    visible: true
                }
            }
        }

        // ── Applications Dropdown Flyout (Lazy-loaded on open) ──────
        Loader {
            id: appsFlyoutLoader
            x: topBar.appTriggerItem ? Math.max(8, Math.round(topBar.appTriggerItem.mapToItem(container, 0, 0).x)) : 48
            y: topBar.height + 4
            active: typeof bridge !== "undefined" && bridge.appMenuOpen
            visible: active
            sourceComponent: Component {
                ApplicationsFlyout {
                    id: appsFlyout
                    objectName: "appsFlyout"
                    visible: true
                }
            }
        }

        // ── Calendar Flyout (Lazy-loaded on open) ───────────────────
        Loader {
            id: calFlyoutLoader
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: 60
            y: 48
            active: typeof bridge !== "undefined" && bridge.calendarOpen
            visible: active
            sourceComponent: Component {
                CalendarFlyout {
                    id: calFlyout
                    objectName: "calFlyout"
                    visible: true
                }
            }
        }

        // ── Notifications Flyout (Lazy-loaded on open) ──────────────
        Loader {
            id: notifFlyoutLoader
            x: topBar.notifTriggerItem
               ? Math.min(container.width - width - 8, Math.max(8, Math.round(topBar.notifTriggerItem.mapToItem(container, 0, 0).x + (topBar.notifTriggerItem.width / 2) - (width / 2))))
               : (container.width - width - 8)
            y: topBar.height + 4
            active: typeof bridge !== "undefined" && bridge.notificationsOpen
            visible: active
            sourceComponent: Component {
                NotificationFlyout {
                    id: notifFlyout
                    objectName: "notifFlyout"
                    visible: true
                }
            }
        }

        // ── Volume Flyout (Lazy-loaded on open) ─────────────────────
        Loader {
            id: volFlyoutLoader
            x: topBar.volTriggerItem
               ? Math.min(container.width - width - 8, Math.max(8, Math.round(topBar.volTriggerItem.mapToItem(container, 0, 0).x + (topBar.volTriggerItem.width / 2) - (width / 2))))
               : (container.width - width - 40)
            y: topBar.height + 4
            active: typeof bridge !== "undefined" && bridge.volumeFlyoutOpen
            visible: active
            sourceComponent: Component {
                VolumeFlyout {
                    id: volFlyout
                    objectName: "volFlyout"
                    visible: true
                }
            }
        }

        // ── Brightness Flyout (Lazy-loaded on open) ─────────────────
        Loader {
            id: briFlyoutLoader
            x: topBar.briTriggerItem
               ? Math.min(container.width - width - 8, Math.max(8, Math.round(topBar.briTriggerItem.mapToItem(container, 0, 0).x + (topBar.briTriggerItem.width / 2) - (width / 2))))
               : (container.width - width - 70)
            y: topBar.height + 4
            active: typeof bridge !== "undefined" && bridge.brightnessFlyoutOpen
            visible: active
            sourceComponent: Component {
                BrightnessFlyout {
                    id: briFlyout
                    objectName: "briFlyout"
                    visible: true
                }
            }
        }

        // ── Power Confirmation Modal (Lazy-loaded on open) ──────────
        Loader {
            id: powerDialogLoader
            anchors.horizontalCenter: parent.horizontalCenter
            y: 60
            active: typeof bridge !== "undefined" && (bridge.rebootConfirmationOpen || bridge.shutdownConfirmationOpen)
            visible: active
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
            anchors.rightMargin: 12
            y: 40
            active: typeof bridge !== "undefined" && bridge.toastVisible
            visible: active
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
