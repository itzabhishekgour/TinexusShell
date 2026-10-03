// ============================================================================
// TinexusVectorIcon.qml — Resolution-Independent Vector Glyphs for Tinexus OS
// Strict Zero-Emoji standard: replaces all text emojis with crisp QtQuick.Shapes
// ============================================================================
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property string name: "search"
    property color color: "#FFFFFF"
    property real size: 16.0

    width: size
    height: size

    readonly property bool isStrokeType: {
        return name === "search" ||
               name === "close" ||
               name === "refresh" ||
               name === "chevron-left" ||
               name === "chevron-right" ||
               name === "arrow-up" ||
               name === "arrow-down" ||
               name === "plus" ||
               name === "check" ||
               name === "view-list" ||
               name === "chevron-right-small" ||
               name === "folder";
    }

    Item {
        id: scaler
        width: 16
        height: 16
        scale: root.size / 16.0
        transformOrigin: Item.TopLeft
        anchors.centerIn: parent

        Shape {
            anchors.fill: parent
            asynchronous: false
            layer.enabled: true
            layer.smooth: true

            ShapePath {
                strokeColor: root.isStrokeType ? root.color : "transparent"
                strokeWidth: {
                    if (root.name === "check") return 2.2;
                    if (root.name === "plus") return 2.0;
                    if (root.name === "chevron-left" || root.name === "chevron-right") return 2.0;
                    if (root.name === "close") return 1.8;
                    if (root.name === "refresh") return 1.8;
                    if (root.name === "search") return 1.8;
                    if (root.name === "view-list") return 2.0;
                    if (root.name === "chevron-right-small") return 1.6;
                    if (root.name === "folder") return 1.5;
                    return 1.6;
                }
                fillColor: root.isStrokeType ? "transparent" : root.color
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathSvg {
                    path: {
                        switch (root.name) {
                        case "search":
                            // Magnifying glass: lens + angled handle
                            return "M 6.5 2.5 A 4 4 0 1 0 10.5 6.5 A 4 4 0 0 0 6.5 2.5 Z M 9.5 9.5 L 14 14";

                        case "close":
                            // Geometric diagonal cross lines
                            return "M 3.5 3.5 L 12.5 12.5 M 12.5 3.5 L 3.5 12.5";

                        case "refresh":
                            // Circular arc + arrowhead
                            return "M 13.5 8 A 5.5 5.5 0 1 1 11.5 4 L 11.5 1.5 M 11.5 4 L 8.5 4";

                        case "chevron-left":
                            // Left navigation chevron
                            return "M 10.5 3.5 L 5.5 8 L 10.5 12.5";

                        case "chevron-right":
                            // Right navigation chevron
                            return "M 5.5 3.5 L 10.5 8 L 5.5 12.5";

                        case "chevron-right-small":
                            return "M 6.5 5 L 9.5 8 L 6.5 11";

                        case "arrow-up":
                            // Up arrow/chevron
                            return "M 8 13 L 8 3.5 M 4 7.5 L 8 3.5 L 12 7.5";

                        case "arrow-down":
                            // Down arrow/chevron
                            return "M 8 3 L 8 12.5 M 4 8.5 L 8 12.5 L 12 8.5";

                        case "sort-asc":
                            // Solid upward sort indicator triangle
                            return "M 8 4 L 12 11 L 4 11 Z";

                        case "sort-desc":
                            // Solid downward sort indicator triangle
                            return "M 8 12 L 12 5 L 4 5 Z";

                        case "plus":
                            // Crisp plus glyph
                            return "M 8 3 L 8 13 M 3 8 L 13 8";

                        case "check":
                            // Angled checkmark
                            return "M 3.5 8.5 L 6.5 12 L 13 4";

                        case "view-icons":
                            // 4 micro-grid squares
                            return "M 2 2 H 6.5 V 6.5 H 2 Z M 9.5 2 H 14 V 6.5 H 9.5 Z M 2 9.5 H 6.5 V 14 H 2 Z M 9.5 9.5 H 14 V 14 H 9.5 Z";

                        case "view-list":
                            // 3 horizontal stacked bars
                            return "M 2 3.5 H 14 M 2 8 H 14 M 2 12.5 H 14";

                        case "view-columns":
                            // 3 vertical column rectangles
                            return "M 2 2 H 5 V 14 H 2 Z M 6.5 2 H 9.5 V 14 H 6.5 Z M 11 2 H 14 V 14 H 11 Z";

                        case "view-gallery":
                            // Main showcase frame + bottom indicator strip
                            return "M 2 2 H 14 V 10 H 2 Z M 2 12 H 5 V 14 H 2 Z M 6.5 12 H 9.5 V 14 H 6.5 Z M 11 12 H 14 V 14 H 11 Z";

                        case "warning":
                            // Triangle with exclamation
                            return "M 8 2 L 14.5 13.5 H 1.5 Z M 8 6 V 9 M 8 11.5 V 12";

                        case "document":
                            // Document page with top-right fold
                            return "M 3 2 H 10 L 13 5 V 14 H 3 Z M 9 2 V 6 H 13";

                        case "folder":
                            // Clean folder shape
                            return "M 2 4 C 2 3.5 2.4 3 3 3 H 6.5 L 8 4.8 H 13 C 13.6 4.8 14 5.2 14 5.8 V 12.2 C 14 12.8 13.6 13.2 13 13.2 H 3 C 2.4 13.2 2 12.8 2 12.2 Z";

                        default:
                            return "";
                        }
                    }
                }
            }
        }
    }
}
