#include "comp/render/damage_tracker.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace tinexus::comp {

DamageRegion::DamageRegion(const DamageBox& box) {
    if (!box.empty()) {
        m_boxes.push_back(box);
    }
}

void DamageRegion::add_box(const DamageBox& box) {
    if (box.empty()) return;
    m_boxes.push_back(box);
    merge_overlapping();
}

void DamageRegion::merge_overlapping() {
    if (m_boxes.size() <= 1) return;

    bool merged = true;
    while (merged) {
        merged = false;
        for (size_t i = 0; i < m_boxes.size(); ++i) {
            for (size_t j = i + 1; j < m_boxes.size(); ++j) {
                if (m_boxes[i].intersects(m_boxes[j])) {
                    int32_t min_x = std::min(m_boxes[i].x, m_boxes[j].x);
                    int32_t min_y = std::min(m_boxes[i].y, m_boxes[j].y);
                    int32_t max_r = std::max(m_boxes[i].x + m_boxes[i].width, m_boxes[j].x + m_boxes[j].width);
                    int32_t max_b = std::max(m_boxes[i].y + m_boxes[i].height, m_boxes[j].y + m_boxes[j].height);

                    m_boxes[i] = DamageBox{min_x, min_y, max_r - min_x, max_b - min_y};
                    m_boxes.erase(m_boxes.begin() + static_cast<ptrdiff_t>(j));
                    merged = true;
                    break;
                }
            }
            if (merged) break;
        }
    }
}

void DamageRegion::clear() noexcept {
    m_boxes.clear();
}

int64_t DamageRegion::total_area() const noexcept {
    int64_t area = 0;
    for (const auto& box : m_boxes) {
        area += box.area();
    }
    return area;
}

DamageBox DamageRegion::bounding_box() const noexcept {
    if (m_boxes.empty()) return {};

    int32_t min_x = m_boxes[0].x;
    int32_t min_y = m_boxes[0].y;
    int32_t max_r = m_boxes[0].x + m_boxes[0].width;
    int32_t max_b = m_boxes[0].y + m_boxes[0].height;

    for (size_t i = 1; i < m_boxes.size(); ++i) {
        min_x = std::min(min_x, m_boxes[i].x);
        min_y = std::min(min_y, m_boxes[i].y);
        max_r = std::max(max_r, m_boxes[i].x + m_boxes[i].width);
        max_b = std::max(max_b, m_boxes[i].y + m_boxes[i].height);
    }

    return DamageBox{min_x, min_y, max_r - min_x, max_b - min_y};
}

DamageTracker& DamageTracker::instance() noexcept {
    static DamageTracker s_instance;
    return s_instance;
}

void DamageTracker::set_screen_dimensions(uint32_t width, uint32_t height) noexcept {
    m_screen_width = width;
    m_screen_height = height;
}

void DamageTracker::add_damage(const DamageBox& box) {
    if (box.empty()) return;
    m_current_region.add_box(box);

    int64_t total_screen_area = static_cast<int64_t>(m_screen_width) * static_cast<int64_t>(m_screen_height);
    int64_t threshold_area = static_cast<int64_t>(static_cast<double>(total_screen_area) * static_cast<double>(FALLBACK_THRESHOLD));
    if (total_screen_area > 0 && m_current_region.total_area() >= threshold_area) {
        log::info("Damage region exceeded threshold ({:.0f}%), falling back to full screen damage", FALLBACK_THRESHOLD * 100.0f);
        add_full_damage();
    }
}

void DamageTracker::add_full_damage() {
    m_current_region.clear();
    m_current_region.add_box(DamageBox{0, 0, static_cast<int32_t>(m_screen_width), static_cast<int32_t>(m_screen_height)});
}

void DamageTracker::accumulate_frame_damage() {
    for (const auto& box : m_current_region.boxes()) {
        m_accumulated_region.add_box(box);
    }
}

void DamageTracker::clear() noexcept {
    m_current_region.clear();
    m_accumulated_region.clear();
}

bool DamageTracker::has_damage() const noexcept {
    return !m_current_region.empty();
}

} // namespace tinexus::comp
