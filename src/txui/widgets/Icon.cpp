#include <txui/widgets/Icon.hpp>
#include <cmath>
#include <algorithm>

namespace txui {

Icon::Icon(IconType type, double size) noexcept : m_type(type), m_size(size) {}

void Icon::set_type(IconType type) noexcept {
    if (m_type != type) {
        m_type = type;
        mark_needs_paint();
    }
}

void Icon::set_size(double size) noexcept {
    if (m_size != size) {
        m_size = size;
        mark_needs_measure();
    }
}

Size Icon::measure_override(const Constraints& constraints) noexcept {
    return constraints.constrain(Size(m_size, m_size));
}

void Icon::paint_override(Painter& painter) const noexcept {
    render(painter, m_type, frame());
}

void Icon::render(Painter& painter, IconType type, const Rect& bounds) noexcept {
    const double s = std::min(bounds.width(), bounds.height());
    if (s <= 0.0) return;
    const double fx = bounds.left() + (bounds.width() - s) * 0.5;
    const double fy = bounds.top() + (bounds.height() - s) * 0.5;
    const double cx = fx + s * 0.5;
    const double cy = fy + s * 0.5;

    switch (type) {
        case IconType::Folder: {
            // macOS / Wayland tabbed folder
            // 1. Back folder tab
            painter.fill_rounded_rect(Rect(fx + s * 0.08, fy + s * 0.16, s * 0.40, s * 0.22), s * 0.06, Color(30, 130, 230, 255));
            // 2. Main folder body
            painter.fill_rounded_rect(Rect(fx + s * 0.08, fy + s * 0.26, s * 0.84, s * 0.58), s * 0.10, Color(45, 155, 255, 255));
            // 3. Front flap top highlight
            painter.fill_rounded_rect(Rect(fx + s * 0.08, fy + s * 0.26, s * 0.84, s * 0.04), s * 0.02, Color(255, 255, 255, 60));
            break;
        }
        case IconType::File: {
            // White document with dog-ear fold and text lines
            // Main page body
            painter.fill_rounded_rect(Rect(fx + s * 0.18, fy + s * 0.10, s * 0.64, s * 0.80), s * 0.08, Color(235, 240, 248, 255));
            // Folded corner
            painter.fill_rect(Rect(fx + s * 0.58, fy + s * 0.10, s * 0.24, s * 0.22), Color(195, 205, 220, 255));
            painter.fill_circle(Point(fx + s * 0.58, fy + s * 0.32), s * 0.04, Color(175, 185, 200, 255));
            // Document lines
            painter.fill_rect(Rect(fx + s * 0.28, fy + s * 0.42, s * 0.44, s * 0.06), Color(130, 140, 160, 200));
            painter.fill_rect(Rect(fx + s * 0.28, fy + s * 0.54, s * 0.44, s * 0.06), Color(130, 140, 160, 200));
            painter.fill_rect(Rect(fx + s * 0.28, fy + s * 0.66, s * 0.28, s * 0.06), Color(130, 140, 160, 200));
            break;
        }
        case IconType::Terminal: {
            // Dark terminal chassis with cyan prompt chevron and cursor
            painter.fill_rounded_rect(Rect(fx + s * 0.08, fy + s * 0.12, s * 0.84, s * 0.76), s * 0.14, Color(22, 24, 30, 255));
            // Chassis top border highlight
            painter.fill_rounded_rect(Rect(fx + s * 0.08, fy + s * 0.12, s * 0.84, 1.0), 1.0, Color(255, 255, 255, 35));
            // Prompt Chevron '>'
            painter.draw_line(Point(fx + s * 0.22, fy + s * 0.35), Point(fx + s * 0.38, fy + s * 0.48), s * 0.08, Color(0, 210, 255, 255));
            painter.draw_line(Point(fx + s * 0.38, fy + s * 0.48), Point(fx + s * 0.22, fy + s * 0.61), s * 0.08, Color(0, 210, 255, 255));
            // Cursor '_'
            painter.fill_rect(Rect(fx + s * 0.46, fy + s * 0.56, s * 0.24, s * 0.08), Color(255, 255, 255, 240));
            break;
        }
        case IconType::Settings:
        case IconType::Gear: {
            // Gear with 8 orthogonal and diagonal teeth + central hub
            // (Shared single implementation for Settings and Gear — zero duplication)
            // Center body
            painter.fill_circle(Point(cx, cy), s * 0.34, Color(160, 168, 185, 255));
            // 4 orthogonal teeth
            painter.fill_rounded_rect(Rect(cx - s * 0.45, cy - s * 0.10, s * 0.90, s * 0.20), s * 0.04, Color(160, 168, 185, 255));
            painter.fill_rounded_rect(Rect(cx - s * 0.10, cy - s * 0.45, s * 0.20, s * 0.90), s * 0.04, Color(160, 168, 185, 255));
            // 4 diagonal teeth
            painter.draw_line(Point(cx - s * 0.31, cy - s * 0.31), Point(cx + s * 0.31, cy + s * 0.31), s * 0.20, Color(160, 168, 185, 255));
            painter.draw_line(Point(cx - s * 0.31, cy + s * 0.31), Point(cx + s * 0.31, cy - s * 0.31), s * 0.20, Color(160, 168, 185, 255));
            // Inner gear body
            painter.fill_circle(Point(cx, cy), s * 0.28, Color(175, 182, 198, 255));
            // Center dark hub hole
            painter.fill_circle(Point(cx, cy), s * 0.14, Color(22, 24, 32, 255));
            break;
        }
        case IconType::BarChart: {
            // System Monitor / Activity icon:
            // 4 rounded ascending/varying data bars along baseline with peak active bar
            const double base_y = fy + s * 0.74;
            const double bar_w  = s * 0.12;
            const double bar_r  = s * 0.03;
            const double gap    = s * 0.06;
            const double start_x = cx - (bar_w * 4.0 + gap * 3.0) * 0.5;

            // Subtle baseline axis
            painter.draw_line(Point(fx + s * 0.12, base_y + s * 0.02),
                              Point(fx + s * 0.88, base_y + s * 0.02),
                              s * 0.04, Color(255, 255, 255, 80));

            const double heights[4] = { s * 0.22, s * 0.38, s * 0.28, s * 0.48 };
            const Color bar_colors[4] = {
                Color(255, 255, 255, 180),
                Color(255, 255, 255, 180),
                Color(255, 255, 255, 180),
                Color(80, 225, 160, 255) // Vibrant peak bar
            };

            for (int i = 0; i < 4; ++i) {
                const double bx = start_x + static_cast<double>(i) * (bar_w + gap);
                const double bh = heights[i];
                painter.fill_rounded_rect(Rect(bx, base_y - bh, bar_w, bh), bar_r, bar_colors[i]);
            }
            break;
        }
        case IconType::Package: {
            // Package Manager / Software icon:
            // High-contrast kraft parcel box with bold top lid flange and crisp white sealing tape.
            // Simplified geometry for clear silhouette readability at small dock scales (24px-48px).

            // 1. Box body (warm rich amber kraft cardboard)
            painter.fill_rounded_rect(Rect(cx - s * 0.32, cy - s * 0.12, s * 0.64, s * 0.52),
                                      s * 0.08, Color(224, 142, 38, 255));

            // 2. Subtle bottom base shadow
            painter.fill_rounded_rect(Rect(cx - s * 0.32, cy + s * 0.35, s * 0.64, s * 0.05),
                                      s * 0.04, Color(180, 100, 20, 255));

            // 3. Top lid flange overhang (lighter honey tone)
            painter.fill_rounded_rect(Rect(cx - s * 0.36, cy - s * 0.25, s * 0.72, s * 0.16),
                                      s * 0.06, Color(248, 178, 72, 255));

            // 4. Seam shadow under lid overhang
            painter.fill_rect(Rect(cx - s * 0.32, cy - s * 0.09, s * 0.64, s * 0.04),
                              Color(150, 85, 15, 180));

            // 5. Bold, high-contrast white/cream vertical sealing tape down center
            painter.fill_rounded_rect(Rect(cx - s * 0.07, cy - s * 0.25, s * 0.14, s * 0.65),
                                      s * 0.02, Color(255, 255, 255, 235));

            // 6. Horizontal tape seal accent across the lid
            painter.fill_rect(Rect(cx - s * 0.18, cy - s * 0.22, s * 0.36, s * 0.08),
                              Color(255, 255, 255, 190));
            break;
        }
        case IconType::Downloads: {
            // Green badge with download arrow and bottom tray
            painter.fill_circle(Point(cx, cy), s * 0.44, Color(40, 185, 80, 255));
            // Arrow shaft
            painter.draw_line(Point(cx, fy + s * 0.24), Point(cx, fy + s * 0.54), s * 0.09, Color(255, 255, 255, 255));
            // Arrowhead chevron
            painter.draw_line(Point(cx - s * 0.18, fy + s * 0.42), Point(cx, fy + s * 0.58), s * 0.09, Color(255, 255, 255, 255));
            painter.draw_line(Point(cx + s * 0.18, fy + s * 0.42), Point(cx, fy + s * 0.58), s * 0.09, Color(255, 255, 255, 255));
            // Bottom tray
            painter.draw_line(Point(cx - s * 0.24, fy + s * 0.72), Point(cx + s * 0.24, fy + s * 0.72), s * 0.09, Color(255, 255, 255, 255));
            break;
        }
        case IconType::Trash: {
            // Red bin with lid, handle, and vertical ribs
            const Color red(255, 69, 58, 255);
            // Lid handle
            painter.fill_rounded_rect(Rect(cx - s * 0.10, fy + s * 0.12, s * 0.20, s * 0.08), s * 0.04, red);
            // Lid rim
            painter.fill_rounded_rect(Rect(cx - s * 0.36, fy + s * 0.20, s * 0.72, s * 0.09), s * 0.04, red);
            // Bin body
            painter.fill_rounded_rect(Rect(cx - s * 0.28, fy + s * 0.31, s * 0.56, s * 0.56), s * 0.08, red);
            // Inner ribs
            painter.fill_rect(Rect(cx - s * 0.14, fy + s * 0.38, s * 0.06, s * 0.40), Color(255, 255, 255, 180));
            painter.fill_rect(Rect(cx - s * 0.03, fy + s * 0.38, s * 0.06, s * 0.40), Color(255, 255, 255, 180));
            painter.fill_rect(Rect(cx + s * 0.08, fy + s * 0.38, s * 0.06, s * 0.40), Color(255, 255, 255, 180));
            break;
        }
        case IconType::Archive: {
            // Amber parcel / zip box with top tape and zipper pull
            painter.fill_rounded_rect(Rect(fx + s * 0.12, fy + s * 0.16, s * 0.76, s * 0.72), s * 0.10, Color(230, 145, 45, 255));
            painter.fill_rect(Rect(fx + s * 0.12, fy + s * 0.16, s * 0.76, s * 0.18), Color(205, 125, 30, 255));
            // Zipper teeth (dashed lines down center)
            painter.fill_rect(Rect(cx - s * 0.03, fy + s * 0.36, s * 0.06, s * 0.46), Color(245, 245, 250, 230));
            // Zipper pull
            painter.fill_rounded_rect(Rect(cx - s * 0.08, fy + s * 0.46, s * 0.16, s * 0.22), s * 0.04, Color(225, 230, 240, 255));
            painter.fill_circle(Point(cx, fy + s * 0.60), s * 0.04, Color(120, 120, 130, 255));
            break;
        }
        case IconType::Image: {
            // Violet photo frame with sun and mountain peaks
            painter.fill_rounded_rect(Rect(fx + s * 0.10, fy + s * 0.14, s * 0.80, s * 0.72), s * 0.12, Color(155, 85, 225, 255));
            // Sun disk
            painter.fill_circle(Point(fx + s * 0.34, fy + s * 0.36), s * 0.12, Color(255, 220, 70, 255));
            // Mountain peak 1
            painter.draw_line(Point(fx + s * 0.18, fy + s * 0.74), Point(fx + s * 0.44, fy + s * 0.46), s * 0.08, Color(255, 255, 255, 240));
            painter.draw_line(Point(fx + s * 0.44, fy + s * 0.46), Point(fx + s * 0.70, fy + s * 0.74), s * 0.08, Color(255, 255, 255, 240));
            // Mountain peak 2
            painter.draw_line(Point(fx + s * 0.50, fy + s * 0.74), Point(fx + s * 0.68, fy + s * 0.54), s * 0.08, Color(255, 255, 255, 240));
            painter.draw_line(Point(fx + s * 0.68, fy + s * 0.54), Point(fx + s * 0.82, fy + s * 0.74), s * 0.08, Color(255, 255, 255, 240));
            break;
        }
        case IconType::Apps: {
            // Symmetrical 3x3 AppGrid of rounded squares
            const double tile_s = s * 0.20;
            const double gap = s * 0.08;
            const double start_x = fx + s * 0.16;
            const double start_y = fy + s * 0.16;
            const Color tile_col(0, 195, 255, 240);

            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    painter.fill_rounded_rect(Rect(start_x + c * (tile_s + gap),
                                                   start_y + r * (tile_s + gap),
                                                   tile_s, tile_s),
                                              s * 0.04, tile_col);
                }
            }
            break;
        }
        case IconType::Home: {
            // Blue house with gable roof and white door
            const Color house_col(0, 122, 255, 255);
            // Roof gable slopes
            painter.draw_line(Point(fx + s * 0.14, fy + s * 0.46), Point(cx, fy + s * 0.18), s * 0.09, house_col);
            painter.draw_line(Point(cx, fy + s * 0.18), Point(fx + s * 0.86, fy + s * 0.46), s * 0.09, house_col);
            // House body
            painter.fill_rect(Rect(fx + s * 0.22, fy + s * 0.44, s * 0.56, s * 0.42), house_col);
            // Door
            painter.fill_rounded_rect(Rect(cx - s * 0.10, fy + s * 0.56, s * 0.20, s * 0.30), s * 0.04, Color(255, 255, 255, 240));
            break;
        }
        case IconType::Executable: {
            // Emerald application badge with a solid white "Run / Execute" play triangle (▶)
            const Color emerald(40, 190, 80, 255);
            painter.fill_rounded_rect(Rect(fx + s * 0.10, fy + s * 0.10, s * 0.80, s * 0.80), s * 0.18, emerald);
            // Subtle top highlight border
            painter.fill_rounded_rect(Rect(fx + s * 0.12, fy + s * 0.10, s * 0.76, 1.0), 1.0, Color(255, 255, 255, 75));

            // Solid right-pointing "Run" triangle, optically centered in the badge
            const double tri_h = s * 0.42;
            const int x_start_px = static_cast<int>(std::round(cx - s * 0.13));
            const int x_end_px   = static_cast<int>(std::round(cx + s * 0.20));
            const double tri_w   = static_cast<double>(x_end_px - x_start_px);
            const Color white(255, 255, 255, 255);

            // Scanline fill for solid interior
            for (int px = x_start_px; px <= x_end_px; ++px) {
                const double t = (tri_w > 0.0) ? static_cast<double>(px - x_start_px) / tri_w : 0.0;
                const double half_h = (tri_h * 0.5) * (1.0 - t);
                painter.fill_rect(Rect(static_cast<double>(px), cy - half_h, 1.0, std::max(1.0, half_h * 2.0)), white);
            }

            // Outline strokes for smooth anti-aliased vertices and rounded corners
            const double stroke_t = std::max(1.0, s * 0.04);
            const double x_start_d = static_cast<double>(x_start_px);
            const double x_end_d   = static_cast<double>(x_end_px);
            painter.draw_line(Point(x_start_d, cy - tri_h * 0.5), Point(x_start_d, cy + tri_h * 0.5), stroke_t, white);
            painter.draw_line(Point(x_start_d, cy - tri_h * 0.5), Point(x_end_d, cy), stroke_t, white);
            painter.draw_line(Point(x_start_d, cy + tri_h * 0.5), Point(x_end_d, cy), stroke_t, white);
            break;
        }
        default: {
            // Fallback document
            painter.fill_rounded_rect(Rect(fx + s * 0.15, fy + s * 0.10, s * 0.70, s * 0.80), s * 0.08, Color(200, 205, 215, 255));
            break;
        }
    }
}

} // namespace txui
