import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: Math.min(parent.width - 48, 680)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 14

        Item { height: 4 }

        // Section Title
        Text {
            text: "SECURITY & ACCESS CONTROL"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card: Security Controls ───────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: secCol.implicitHeight

            ColumnLayout {
                id: secCol
                anchors.fill: parent
                spacing: 0

                MacSettingRow {
                    title: "Platform Guard & Ed25519 Signatures"
                    subtitle: "Audits installed applications against root public keys"
                    showSeparator: true

                    MacSwitch {
                        checked: true
                    }
                }

                MacSettingRow {
                    title: "Application Sandbox"
                    subtitle: "Confines untrusted third-party apps via platform security sandbox"
                    showSeparator: true

                    MacSwitch {
                        checked: true
                    }
                }

                MacSettingRow {
                    title: "Application Binary Audit"
                    subtitle: "Scans /opt/tinexus-apps for unverified or modified binaries"
                    showSeparator: false

                    MacButton {
                        text: "Rescan Apps"
                        onClicked: bridge.refreshUnverifiedApps()
                    }
                }
            }
        }

        // Section Title: Unverified Apps (if any)
        Text {
            visible: bridge.unverifiedAppsCount > 0
            text: "UNVERIFIED APPLICATIONS"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // Grouped Card: Unverified Apps
        MacCard {
            visible: bridge.unverifiedAppsCount > 0
            Layout.fillWidth: true
            implicitHeight: unverifiedCol.implicitHeight

            ColumnLayout {
                id: unverifiedCol
                anchors.fill: parent
                spacing: 0

                Repeater {
                    model: bridge.unverifiedApps
                    delegate: MacSettingRow {
                        title: modelData.name || "Unknown Binary"
                        subtitle: "SHA256: " + (modelData.hash ? modelData.hash.substring(0, 16) + "..." : "Untrusted")
                        showSeparator: index < bridge.unverifiedAppsCount - 1

                        MacButton {
                            text: "Trust & Allow"
                            primary: true
                            onClicked: bridge.trustApp(modelData.hash)
                        }
                    }
                }
            }
        }

        Item { height: 10 }
    }
}
