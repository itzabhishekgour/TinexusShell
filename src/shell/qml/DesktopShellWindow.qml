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

        // ── Logo Menu Flyout ────────────────────────────────────────
        LogoMenuFlyout {
            id: logoFlyout
            x: 8
            y: 36
            visible: typeof bridge !== "undefined" && bridge.logoMenuOpen
        }

        // ── Applications Dropdown Flyout ────────────────────────────
        ApplicationsFlyout {
            id: appsFlyout
            x: 48
            y: 36
            visible: typeof bridge !== "undefined" && bridge.appMenuOpen
        }

        // ── Calendar Flyout (Directly under Aura Notch Date Pill) ──
        CalendarFlyout {
            id: calFlyout
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: 60
            y: 48
            visible: typeof bridge !== "undefined" && bridge.calendarOpen
        }

        // ── Notifications Flyout (Under Notification Bell) ───────────
        NotificationFlyout {
            id: notifFlyout
            anchors.right: parent.right
            anchors.rightMargin: 8
            y: 36
            visible: typeof bridge !== "undefined" && bridge.notificationsOpen
        }

        // ── Volume Flyout (Under Volume Tray Icon) ───────────────────
        VolumeFlyout {
            id: volFlyout
            anchors.right: parent.right
            anchors.rightMargin: 40
            y: 36
            visible: typeof bridge !== "undefined" && bridge.volumeFlyoutOpen
        }

        // ── Brightness Flyout (Under Brightness Tray Icon) ───────────
        BrightnessFlyout {
            id: briFlyout
            anchors.right: parent.right
            anchors.rightMargin: 70
            y: 36
            visible: typeof bridge !== "undefined" && bridge.brightnessFlyoutOpen
        }

        // ── Power Confirmation Modal ────────────────────────────────
        PowerConfirmationDialog {
            id: powerDialog
            anchors.horizontalCenter: parent.horizontalCenter
            y: 60
            visible: typeof bridge !== "undefined" && (bridge.rebootConfirmationOpen || bridge.shutdownConfirmationOpen)
        }

        // ── Notification Toast Overlay (Top Right below TopBar) ─────
        NotificationToast {
            id: notifToast
            anchors.right: parent.right
            anchors.rightMargin: 12
            y: 40
            visible: typeof bridge !== "undefined" && bridge.toastVisible
        }
    }
}
