#ifndef TINEXUS_COMP_DAMAGE_TRACKER_HPP
#define TINEXUS_COMP_DAMAGE_TRACKER_HPP

#include <cstdint>
#include <vector>

namespace tinexus::comp {

struct DamageBox {
    int32_t x{0};
    int32_t y{0};
    int32_t width{0};
    int32_t height{0};

    [[nodiscard]] bool empty() const noexcept {
        return width <= 0 || height <= 0;
    }

    [[nodiscard]] int64_t area() const noexcept {
        if (empty()) return 0;
        return static_cast<int64_t>(width) * static_cast<int64_t>(height);
    }

    [[nodiscard]] bool intersects(const DamageBox& other) const noexcept {
        if (empty() || other.empty()) return false;
        return !(x + width <= other.x || other.x + other.width <= x ||
                 y + height <= other.y || other.y + other.height <= y);
    }
};

class DamageRegion {
public:
    DamageRegion() = default;
    explicit DamageRegion(const DamageBox& box);

    void add_box(const DamageBox& box);
    void merge_overlapping();
    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept { return m_boxes.empty(); }
    [[nodiscard]] const std::vector<DamageBox>& boxes() const noexcept { return m_boxes; }
    [[nodiscard]] int64_t total_area() const noexcept;
    [[nodiscard]] DamageBox bounding_box() const noexcept;

private:
    std::vector<DamageBox> m_boxes;
};

class DamageTracker {
public:
    static DamageTracker& instance() noexcept;

    DamageTracker() = default;
    ~DamageTracker() = default;

    void set_screen_dimensions(uint32_t width, uint32_t height) noexcept;
    void add_damage(const DamageBox& box);
    void add_full_damage();

    void accumulate_frame_damage();
    void clear() noexcept;

    [[nodiscard]] bool has_damage() const noexcept;
    [[nodiscard]] const DamageRegion& current_damage() const noexcept { return m_current_region; }
    [[nodiscard]] const DamageRegion& accumulated_damage() const noexcept { return m_accumulated_region; }

private:
    uint32_t m_screen_width{0};
    uint32_t m_screen_height{0};
    DamageRegion m_current_region;
    DamageRegion m_accumulated_region;
    static constexpr float FALLBACK_THRESHOLD = 0.80f;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_DAMAGE_TRACKER_HPP
